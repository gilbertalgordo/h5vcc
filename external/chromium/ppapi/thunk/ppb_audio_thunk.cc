// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_audio.idl modified Tue Dec  4 10:41:02 2012.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Resource config,
                   PPB_Audio_Callback audio_callback,
                   void* user_data) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudio(instance,
                                        config,
                                        audio_callback,
                                        user_data);
}

PP_Bool IsAudio(PP_Resource resource) {
  EnterResource<PPB_Audio_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Resource GetCurrentConfig(PP_Resource audio) {
  EnterResource<PPB_Audio_API> enter(audio, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCurrentConfig();
}

PP_Bool StartPlayback(PP_Resource audio) {
  EnterResource<PPB_Audio_API> enter(audio, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StartPlayback();
}

PP_Bool StopPlayback(PP_Resource audio) {
  EnterResource<PPB_Audio_API> enter(audio, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StopPlayback();
}

const PPB_Audio_1_0 g_ppb_audio_thunk_1_0 = {
  &Create,
  &IsAudio,
  &GetCurrentConfig,
  &StartPlayback,
  &StopPlayback,
};

}  // namespace

const PPB_Audio_1_0* GetPPB_Audio_1_0_Thunk() {
  return &g_ppb_audio_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
