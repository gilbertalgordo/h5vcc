// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FORMAT_MACROS_H_
#define BASE_FORMAT_MACROS_H_

// This file defines the format macros for some integer types.

// To print a 64-bit value in a portable way:
//   int64_t value;
//   printf("xyz:%" PRId64, value);
// The "d" in the macro corresponds to %d; you can also use PRIu64 etc.
//
// For wide strings, prepend "Wide" to the macro:
//   int64_t value;
//   StringPrintf(L"xyz: %" WidePRId64, value);
//
// To print a size_t value in a portable way:
//   size_t size;
//   printf("xyz: %" PRIuS, size);
// The "u" in the macro corresponds to %u, and S is for "size".

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(__LB_XB1__) && !defined(__LB_XB360__)

#if (defined(_INTTYPES_H) || defined(_INTTYPES_H_)) && !defined(PRId64)
#error "inttypes.h has already been included before this header file, but "
#error "without __STDC_FORMAT_MACROS defined."
#endif

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

// GCC will concatenate wide and narrow strings correctly, so nothing needs to
// be done here.
#define WidePRId64 PRId64
#define WidePRIu64 PRIu64
#define WidePRIx64 PRIx64

#if !defined(PRIuS)
#define PRIuS "zu"
#endif

#else  // OS_WIN || __LB_XB1__ || __LB_XB360__

#if !defined(PRId64)
#define PRId64 "I64d"
#endif

#if !defined(PRIu64)
#define PRIu64 "I64u"
#endif

#if !defined(PRIx64)
#define PRIx64 "I64x"
#endif

#define WidePRId64 L"I64d"
#define WidePRIu64 L"I64u"
#define WidePRIx64 L"I64x"

#if !defined(PRIuS)
#define PRIuS "Iu"
#endif

#endif

#endif  // BASE_FORMAT_MACROS_H_
