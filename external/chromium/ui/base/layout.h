// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LAYOUT_H_
#define UI_BASE_LAYOUT_H_

#include <vector>

#include "build/build_config.h"
#include "ui/base/ui_export.h"
#if !defined(__LB_SHELL__)
#include "ui/gfx/native_widget_types.h"
#endif

namespace ui {

enum DisplayLayout {
  // Layout optimized for ASH.  This enum value should go away as soon as
  // LAYOUT_DESKTOP and LAYOUT_ASH are the same.
  LAYOUT_ASH,

  // The typical layout for e.g. Windows, Mac and Linux.
  LAYOUT_DESKTOP,

  // Layout optimized for touch.  Used e.g. for Windows 8 Metro mode.
  LAYOUT_TOUCH,
};

// Returns the display layout that should be used.  This could be used
// e.g. to tweak hard-coded padding that's layout specific, or choose
// the .pak file of theme resources to load.
UI_EXPORT DisplayLayout GetDisplayLayout();

// Supported UI scale factors for the platform. This is used as an index
// into the array |kScaleFactorScales| which maps the enum value to a float.
// SCALE_FACTOR_NONE is used for density independent resources such as
// string, html/js files or an image that can be used for any scale factors
// (such as wallpapers).
enum ScaleFactor {
  SCALE_FACTOR_NONE = 0,
  SCALE_FACTOR_100P,
  SCALE_FACTOR_140P,
  SCALE_FACTOR_180P,
  SCALE_FACTOR_200P,

  NUM_SCALE_FACTORS  // This always appears last.
};

// Returns the float scale value for |scale_factor|.
UI_EXPORT float GetScaleFactorScale(ScaleFactor scale_factor);

// Returns the supported ScaleFactor which most closely matches |scale|.
// Converting from float to ScaleFactor is inefficient and should be done as
// little as possible.
// TODO(oshima): Make ScaleFactor a class and remove this.
UI_EXPORT ScaleFactor GetScaleFactorFromScale(float scale);

#if !defined(__LB_SHELL__)
// Returns the ScaleFactor used by |view|.
UI_EXPORT ScaleFactor GetScaleFactorForNativeView(gfx::NativeView view);
#endif

// Returns the maximum device scale factor supported by this platform.
UI_EXPORT ScaleFactor GetMaxScaleFactor();

// Returns a vector with the scale factors which are supported by this
// platform, in ascending order.
UI_EXPORT std::vector<ScaleFactor> GetSupportedScaleFactors();

// Returns true if |scale_factor| is supported by this platform.
UI_EXPORT bool IsScaleFactorSupported(ScaleFactor scale_factor);

namespace test {

UI_EXPORT void SetSupportedScaleFactors(
    const std::vector<ScaleFactor>& scale_factors);

}  // namespace test

}  // namespace ui

#endif  // UI_BASE_LAYOUT_H_
