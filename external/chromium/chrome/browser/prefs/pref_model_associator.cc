// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_model_associator.h"

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

using syncer::PREFERENCES;

PrefModelAssociator::PrefModelAssociator()
    : models_associated_(false),
      processing_syncer_changes_(false),
      pref_service_(NULL) {
  DCHECK(CalledOnValidThread());
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK(CalledOnValidThread());
  pref_service_ = NULL;
}

void PrefModelAssociator::InitPrefAndAssociate(
    const syncer::SyncData& sync_pref,
    const std::string& pref_name,
    syncer::SyncChangeList* sync_changes) {
  const Value* user_pref_value = pref_service_->GetUserPrefValue(
      pref_name.c_str());
  VLOG(1) << "Associating preference " << pref_name;

  if (sync_pref.IsValid()) {
    const sync_pb::PreferenceSpecifics& preference =
        sync_pref.GetSpecifics().preference();
    DCHECK_EQ(pref_name, preference.name());

    base::JSONReader reader;
    scoped_ptr<Value> sync_value(reader.ReadToValue(preference.value()));
    if (!sync_value.get()) {
      LOG(ERROR) << "Failed to deserialize preference value: "
                 << reader.GetErrorMessage();
      return;
    }

    if (user_pref_value) {
      // We have both server and local values. Merge them.
      scoped_ptr<Value> new_value(
          MergePreference(pref_name, *user_pref_value, *sync_value));

      // Update the local preference based on what we got from the
      // sync server. Note: this only updates the user value store, which is
      // ignored if the preference is policy controlled.
      if (new_value->IsType(Value::TYPE_NULL)) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        pref_service_->ClearPref(pref_name.c_str());
      } else if (!new_value->IsType(user_pref_value->GetType())) {
        LOG(WARNING) << "Synced value for " << preference.name()
                     << " is of type " << new_value->GetType()
                     << " which doesn't match pref type "
                     << user_pref_value->GetType();
      } else if (!user_pref_value->Equals(new_value.get())) {
        pref_service_->Set(pref_name.c_str(), *new_value);
      }

      // If the merge resulted in an updated value, inform the syncer.
      if (!sync_value->Equals(new_value.get())) {
        syncer::SyncData sync_data;
        if (!CreatePrefSyncData(pref_name, *new_value, &sync_data)) {
          LOG(ERROR) << "Failed to update preference.";
          return;
        }
        sync_changes->push_back(
            syncer::SyncChange(FROM_HERE,
                               syncer::SyncChange::ACTION_UPDATE,
                               sync_data));
      }
    } else if (!sync_value->IsType(Value::TYPE_NULL)) {
      // Only a server value exists. Just set the local user value.
      pref_service_->Set(pref_name.c_str(), *sync_value);
    } else {
      LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
    }
  } else if (user_pref_value) {
    // The server does not know about this preference and should be added
    // to the syncer's database.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(pref_name, *user_pref_value, &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    sync_changes->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           sync_data));
  } else {
    // This pref does not have a sync value but also does not have a user
    // controlled value (either it's a default value or it's policy controlled,
    // either way it's not interesting). We can ignore it. Once it gets changed,
    // we'll send the new user controlled value to the syncer.
    return;
  }

  // Make sure we add it to our list of synced preferences so we know what
  // the server is aware of.
  synced_preferences_.insert(pref_name);
  return;
}

syncer::SyncMergeResult PrefModelAssociator::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_EQ(type, PREFERENCES);
  DCHECK(CalledOnValidThread());
  DCHECK(pref_service_);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  syncer::SyncChangeList new_changes;
  std::set<std::string> remaining_preferences = registered_preferences_;

  // Go through and check for all preferences we care about that sync already
  // knows about.
  for (syncer::SyncDataList::const_iterator sync_iter =
           initial_sync_data.begin();
       sync_iter != initial_sync_data.end();
       ++sync_iter) {
    DCHECK_EQ(PREFERENCES, sync_iter->GetDataType());
    std::string sync_pref_name = sync_iter->GetSpecifics().preference().name();
    if (remaining_preferences.count(sync_pref_name) == 0) {
      // We're not syncing this preference locally, ignore the sync data.
      // TODO(zea): Eventually we want to be able to have the syncable service
      // reconstruct all sync data for it's datatype (therefore having
      // GetAllSyncData be a complete representation). We should store this data
      // somewhere, even if we don't use it.
      continue;
    }

    remaining_preferences.erase(sync_pref_name);
    InitPrefAndAssociate(*sync_iter, sync_pref_name, &new_changes);
  }

  // Go through and build sync data for any remaining preferences.
  for (std::set<std::string>::iterator pref_name_iter =
          remaining_preferences.begin();
       pref_name_iter != remaining_preferences.end();
       ++pref_name_iter) {
    InitPrefAndAssociate(syncer::SyncData(), *pref_name_iter, &new_changes);
  }

  // Push updates to sync.
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

  models_associated_ = true;
  pref_service_->OnIsSyncingChanged();
  return merge_result;
}

void PrefModelAssociator::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, PREFERENCES);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
  pref_service_->OnIsSyncingChanged();
}

scoped_ptr<Value> PrefModelAssociator::MergePreference(
    const std::string& name,
    const Value& local_value,
    const Value& server_value) {
  if (name == prefs::kURLsToRestoreOnStartup) {
    return scoped_ptr<Value>(MergeListValues(local_value, server_value)).Pass();
  }

  if (name == prefs::kContentSettingsPatternPairs) {
    return scoped_ptr<Value>(
        MergeDictionaryValues(local_value, server_value)).Pass();
  }

  // If this is not a specially handled preference, server wins.
  return scoped_ptr<Value>(server_value.DeepCopy()).Pass();
}

bool PrefModelAssociator::CreatePrefSyncData(
    const std::string& name,
    const Value& value,
    syncer::SyncData* sync_data) {
  if (value.IsType(Value::TYPE_NULL)) {
    LOG(ERROR) << "Attempting to sync a null pref value for " << name;
    return false;
  }

  std::string serialized;
  // TODO(zea): consider JSONWriter::Write since you don't have to check
  // failures to deserialize.
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    return false;
  }

  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name(name);
  pref_specifics->set_value(serialized);
  *sync_data = syncer::SyncData::CreateLocalData(name, name, specifics);
  return true;
}

Value* PrefModelAssociator::MergeListValues(const Value& from_value,
                                            const Value& to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK(from_value.GetType() == Value::TYPE_LIST);
  DCHECK(to_value.GetType() == Value::TYPE_LIST);
  const ListValue& from_list_value = static_cast<const ListValue&>(from_value);
  const ListValue& to_list_value = static_cast<const ListValue&>(to_value);
  ListValue* result = to_list_value.DeepCopy();

  for (ListValue::const_iterator i = from_list_value.begin();
       i != from_list_value.end(); ++i) {
    Value* value = (*i)->DeepCopy();
    result->AppendIfNotPresent(value);
  }
  return result;
}

Value* PrefModelAssociator::MergeDictionaryValues(
    const Value& from_value,
    const Value& to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK_EQ(from_value.GetType(), Value::TYPE_DICTIONARY);
  DCHECK_EQ(to_value.GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue& from_dict_value =
      static_cast<const DictionaryValue&>(from_value);
  const DictionaryValue& to_dict_value =
      static_cast<const DictionaryValue&>(to_value);
  DictionaryValue* result = to_dict_value.DeepCopy();

  for (DictionaryValue::key_iterator key = from_dict_value.begin_keys();
       key != from_dict_value.end_keys(); ++key) {
    const Value* from_value;
    bool success = from_dict_value.GetWithoutPathExpansion(*key, &from_value);
    DCHECK(success);

    Value* to_key_value;
    if (result->GetWithoutPathExpansion(*key, &to_key_value)) {
      if (to_key_value->GetType() == Value::TYPE_DICTIONARY) {
        Value* merged_value = MergeDictionaryValues(*from_value, *to_key_value);
        result->SetWithoutPathExpansion(*key, merged_value);
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      result->SetWithoutPathExpansion(*key, from_value->DeepCopy());
    }
  }
  return result;
}

// Note: This will build a model of all preferences registered as syncable
// with user controlled data. We do not track any information for preferences
// not registered locally as syncable and do not inform the syncer of
// non-user controlled preferences.
syncer::SyncDataList PrefModelAssociator::GetAllSyncData(
    syncer::ModelType type)
    const {
  DCHECK_EQ(PREFERENCES, type);
  syncer::SyncDataList current_data;
  for (PreferenceSet::const_iterator iter = synced_preferences_.begin();
       iter != synced_preferences_.end();
       ++iter) {
    std::string name = *iter;
    const PrefService::Preference* pref =
      pref_service_->FindPreference(name.c_str());
    DCHECK(pref);
    if (!pref->IsUserControlled() || pref->IsDefaultValue())
      continue;  // This is not data we care about.
    // TODO(zea): plumb a way to read the user controlled value.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *pref->GetValue(), &sync_data))
      continue;
    current_data.push_back(sync_data);
  }
  return current_data;
}

syncer::SyncError PrefModelAssociator::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE,
                    "Models not yet associated.",
                    PREFERENCES);
    return error;
  }
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  syncer::SyncChangeList::const_iterator iter;
  for (iter = change_list.begin(); iter != change_list.end(); ++iter) {
    DCHECK_EQ(PREFERENCES, iter->sync_data().GetDataType());

    std::string name;
    sync_pb::PreferenceSpecifics pref_specifics =
        iter->sync_data().GetSpecifics().preference();
    scoped_ptr<Value> value(ReadPreferenceSpecifics(pref_specifics,
                                                    &name));

    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      // We never delete preferences.
      NOTREACHED() << "Attempted to process sync delete change for " << name
                   << ". Skipping.";
      continue;
    }

    // Skip values we can't deserialize.
    // TODO(zea): consider taking some further action such as erasing the bad
    // data.
    if (!value.get())
      continue;

    // It is possible that we may receive a change to a preference we do not
    // want to sync. For example if the user is syncing a Mac client and a
    // Windows client, the Windows client does not support
    // kConfirmToQuitEnabled. Ignore updates from these preferences.
    const char* pref_name = name.c_str();
    if (!IsPrefRegistered(pref_name))
      continue;

    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    DCHECK(pref);

    // This will only modify the user controlled value store, which takes
    // priority over the default value but is ignored if the preference is
    // policy controlled.
    pref_service_->Set(pref_name, *value);

    // Keep track of any newly synced preferences.
    if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      synced_preferences_.insert(name);
    }
  }
  return syncer::SyncError();
}

Value* PrefModelAssociator::ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference,
    std::string* name) {
  base::JSONReader reader;
  scoped_ptr<Value> value(reader.ReadToValue(preference.value()));
  if (!value.get()) {
    std::string err = "Failed to deserialize preference value: " +
        reader.GetErrorMessage();
    LOG(ERROR) << err;
    return NULL;
  }
  *name = preference.name();
  return value.release();
}

std::set<std::string> PrefModelAssociator::registered_preferences() const {
  return registered_preferences_;
}

void PrefModelAssociator::RegisterPref(const char* name) {
  DCHECK(!models_associated_ && registered_preferences_.count(name) == 0);
  registered_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const char* name) {
  return registered_preferences_.count(name) > 0;
}

void PrefModelAssociator::UnregisterPref(const char* name) {
  DCHECK(synced_preferences_.count(name) == 0);
  registered_preferences_.erase(name);
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  // We only process changes if we've already associated models.
  if (!models_associated_)
    return;

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name.c_str());
  if (!preference)
    return;

  if (!IsPrefRegistered(name.c_str()))
    return;  // We are not syncing this preference.

  syncer::SyncChangeList changes;

  if (!preference->IsUserModifiable()) {
    // If the preference is no longer user modifiable, it must now be controlled
    // by policy, whose values we do not sync. Just return. If the preference
    // stops being controlled by policy, it will revert back to the user value
    // (which we continue to update with sync changes).
    return;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  if (synced_preferences_.count(name) == 0) {
    // Not in synced_preferences_ means no synced data. InitPrefAndAssociate(..)
    // will determine if we care about its data (e.g. if it has a default value
    // and hasn't been changed yet we don't) and take care syncing any new data.
    InitPrefAndAssociate(syncer::SyncData(), name, &changes);
  } else {
    // We are already syncing this preference, just update it's sync node.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *preference->GetValue(), &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_UPDATE,
                           sync_data));
  }

  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void PrefModelAssociator::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service_ == NULL);
  pref_service_ = pref_service;
}
