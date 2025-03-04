//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// precompiled.h: Precompiled header file for libGLESv2.
#include <common/system.h>

#define GL_APICALL
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <assert.h>
#include <cstddef>
#include <float.h>
#include <intrin.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm> // for std::min and std::max
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if ANGLE_ENABLE_D3D11
#include <D3D11.h>
#include <dxgi.h>
#endif

#if !defined(ANGLE_ENABLE_D3D11_STRICT)
#include <d3d9.h>
#endif

#if !defined(__LB_XB360__)
#include <D3Dcompiler.h>
#endif

#ifdef _MSC_VER
#include <hash_map>
#endif
