// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Class for finding and caching Windows explorer icons. The IconManager
// lives on the UI thread but performs icon extraction work on the file thread
// to avoid blocking the UI thread with potentially expensive COM and disk
// operations.
//
// Terminology
//
// Windows files have icons associated with them that can be of two types:
//   1. "Per class": the icon used for this file is used for all files with the
//      same file extension or class. Examples are PDF or MP3 files, which use
//      the same icon for all files of that type.
//   2. "Per instance": the icon used for this file is embedded in the file
//      itself and is unique. Executable files are typically "per instance".
//
// Files that end in the following extensions are considered "per instance":
//   .exe
//   .dll
//   .ico
// The IconManager will do explicit icon loads on the full path of these files
// and cache the results per file. All other file types will be looked up by
// file extension and the results will be cached per extension. That way, all
// .mp3 files will share one icon, but all .exe files will have their own icon.
//
// POSIX files don't have associated icons. We query the OS by the file's
// mime type.
//
// The IconManager can be queried in two ways:
//   1. A quick, synchronous check of its caches which does not touch the disk:
//      IconManager::LookupIcon()
//   2. An asynchronous icon load from a file on the file thread:
//      IconManager::LoadIcon()
//
// When using the second (asychronous) method, callers must supply a callback
// which will be run once the icon has been extracted. The icon manager will
// cache the results of the icon extraction so that subsequent lookups will be
// fast.
//
// Icon bitmaps returned should be treated as const since they may be referenced
// by other clients. Make a copy of the icon if you need to modify it.

#ifndef CHROME_BROWSER_ICON_MANAGER_H_
#define CHROME_BROWSER_ICON_MANAGER_H_

#include <map>

#include "chrome/browser/icon_loader.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "ui/gfx/image/image.h"

class FilePath;

class IconManager : public IconLoader::Delegate {
 public:
  IconManager();
  virtual ~IconManager();

  // Synchronous call to examine the internal caches for the icon. Returns the
  // icon if we have already loaded it, NULL if we don't have it and must load
  // it via 'LoadIcon'. The returned bitmap is owned by the IconManager and must
  // not be free'd by the caller. If the caller needs to modify the icon, it
  // must make a copy and modify the copy.
  gfx::Image* LookupIcon(const FilePath& file_name, IconLoader::IconSize size);

  typedef base::Callback<void(gfx::Image*)> IconRequestCallback;

  // Asynchronous call to lookup and return the icon associated with file. The
  // work is done on the file thread, with the callbacks running on the thread
  // this function is called.
  //
  // Note:
  // 1. This does *not* check the cache.
  // 2. The returned bitmap pointer is *not* owned by callback. So callback
  //    should never keep it or delete it.
  // 3. The gfx::Image pointer passed to the callback may be NULL if decoding
  //    failed.
  CancelableTaskTracker::TaskId LoadIcon(const FilePath& file_name,
                                         IconLoader::IconSize size,
                                         const IconRequestCallback& callback,
                                         CancelableTaskTracker* tracker);

  // IconLoader::Delegate interface.
  virtual bool OnImageLoaded(IconLoader* loader, gfx::Image* result) OVERRIDE;

  // Get the identifying string for the given file. The implementation
  // is in icon_manager_[platform].cc.
  static IconGroupID GetGroupIDFromFilepath(const FilePath& path);

 private:
  struct CacheKey {
    CacheKey(const IconGroupID& group, IconLoader::IconSize size);

    // Used as a key in the map below, so we need this comparator.
    bool operator<(const CacheKey &other) const;

    IconGroupID group;
    IconLoader::IconSize size;
  };

  typedef std::map<CacheKey, gfx::Image*> IconMap;
  IconMap icon_cache_;

  // Asynchronous requests that have not yet been completed.
  struct ClientRequest;
  typedef std::map<IconLoader*, ClientRequest> ClientRequests;
  ClientRequests requests_;

  DISALLOW_COPY_AND_ASSIGN(IconManager);
};

#endif  // CHROME_BROWSER_ICON_MANAGER_H_
