// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCOPED_PTR_HASH_MAP_H_
#define CC_SCOPED_PTR_HASH_MAP_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/memory/scoped_ptr.h"

namespace cc {

// This type acts like a hash_map<K, scoped_ptr<V> >, based on top of
// std::hash_map. The ScopedPtrHashMap has ownership of all values in the data
// structure.
#if defined(__LB_SHELL__)
template <typename Key, typename Value, typename Alloc = std::allocator<std::pair<const Key, Value*> > >
#else
template <typename Key, typename Value>
#endif
class ScopedPtrHashMap {
#if defined(__LB_WIIU__) || defined(__LB_PS3__)
  typedef base::hash_map<Key, Value*, std::hash_compare<Key, std::less<Key> >, Alloc> Container;
#elif defined(__LB_LINUX__)
  typedef base::hash_map<Key, Value*, base::hash<Key>, std::equal_to<Key>, Alloc> Container;
#else
  typedef base::hash_map<Key, Value*> Container;
#endif
 public:
  typedef typename Container::iterator iterator;
  typedef typename Container::const_iterator const_iterator;

  ScopedPtrHashMap() {}

  ~ScopedPtrHashMap() { clear(); }

  void swap(ScopedPtrHashMap<Key, Value*>& other) {
    data_.swap(other.data_);
  }

  std::pair<iterator, bool> insert(
      std::pair<Key, const scoped_ptr<Value> > pair) {
    return data_.insert(
        std::pair<Key, Value*>(pair.first, pair.second.release()));
  }

  // Replaces value but not key if key is already present.
  std::pair<iterator, bool> set(Key key, scoped_ptr<Value> data) {
    iterator it = find(key);
    if (it != end())
      erase(it);
    Value* rawPtr = data.release();
    return data_.insert(std::pair<Key, Value*>(key, rawPtr));
  }

  // Does nothing if key is already present
  std::pair<iterator, bool> add(Key key, scoped_ptr<Value> data) {
    Value* rawPtr = data.release();
    return data_.insert(std::pair<Key, Value*>(key, rawPtr));
  }

  void erase(iterator it) {
    if (it->second)
      delete it->second;
    data_.erase(it);
  }

  size_t erase(const Key& k) {
    iterator it = data_.find(k);
    if (it == data_.end())
      return 0;
    erase(it);
    return 1;
  }

  scoped_ptr<Value> take(iterator it) {
    DCHECK(it != data_.end());
    if (it == data_.end())
      return scoped_ptr<Value>(NULL);

    Key key = it->first;
    scoped_ptr<Value> ret(it->second);
    data_.erase(it);
    data_.insert(std::pair<Key, Value*>(key, static_cast<Value*>(NULL)));
    return ret.Pass();
  }

  scoped_ptr<Value> take(const Key& k) {
    iterator it = find(k);
    if (it == data_.end())
      return scoped_ptr<Value>(NULL);

    return take(it);
  }

  scoped_ptr<Value> take_and_erase(iterator it) {
    DCHECK(it != data_.end());
    if (it == data_.end())
      return scoped_ptr<Value>(NULL);

    scoped_ptr<Value> ret(it->second);
    data_.erase(it);
    return ret.Pass();
  }

  scoped_ptr<Value> take_and_erase(const Key& k) {
    iterator it = find(k);
    if (it == data_.end())
      return scoped_ptr<Value>(NULL);

    return take_and_erase(it);
  }

  // Returns the first element in the hash_map that matches the given key.
  // If no such element exists it returns NULL.
  Value* get(const Key& k) const {
    const_iterator it = find(k);
    if (it == end())
      return 0;
    return it->second;
  }

  inline bool contains(const Key& k) const { return data_.count(k); }

  inline void clear() { STLDeleteValues(&data_); }

  inline const_iterator find(const Key& k) const { return data_.find(k); }
  inline iterator find(const Key& k) { return data_.find(k); }

  inline size_t count(const Key& k) const { return data_.count(k); }
  inline std::pair<const_iterator, const_iterator> equal_range(
      const Key& k) const {
    return data_.equal_range(k);
  }
  inline std::pair<iterator, iterator> equal_range(const Key& k) {
    return data_.equal_range(k);
  }

  inline size_t size() const { return data_.size(); }
  inline size_t max_size() const { return data_.max_size(); }

  inline bool empty() const { return data_.empty(); }

  inline size_t bucket_count() const { return data_.bucket_count(); }
  inline void resize(size_t size) const { return data_.resize(size); }

  inline iterator begin() { return data_.begin(); }
  inline const_iterator begin() const { return data_.begin(); }
  inline iterator end() { return data_.end(); }
  inline const_iterator end() const { return data_.end(); }

 private:
  Container data_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPtrHashMap);
};

}  // namespace cc

#endif  // CC_SCOPED_PTR_HASH_MAP_H_
