// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_constants.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace remoting {

const char kDaemonPipeSwitchName[] = "daemon-pipe";

const FilePath::CharType kDaemonBinaryName[] =
    FILE_PATH_LITERAL("remoting_daemon");

const FilePath::CharType kHostBinaryName[] = FILE_PATH_LITERAL("remoting_host");

bool GetInstalledBinaryPath(const FilePath::StringType& binary,
                            FilePath* full_path) {
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }

  FilePath path = dir_path.Append(binary);

#if defined(OS_WIN)
  path = path.ReplaceExtension(FILE_PATH_LITERAL("exe"));
#endif  // defined(OS_WIN)

  *full_path = path;
  return true;
}

}  // namespace remoting
