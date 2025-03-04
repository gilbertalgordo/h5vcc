// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "skia/ext/platform_canvas.h"
#include "ui/native_theme/native_theme.h"

namespace gfx {
class ImageSkia;
class Rect;
class Size;
}

namespace ui {

// Theme support for non-Windows toolkits.
class NATIVE_THEME_EXPORT NativeThemeBase : public NativeTheme {
 public:
  // NativeTheme implementation:
  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra) const OVERRIDE;
  virtual void Paint(SkCanvas* canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra) const OVERRIDE;

 protected:
  NativeThemeBase();
  virtual ~NativeThemeBase();

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const;
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(
      SkCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const;
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(
      SkCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect) const;

  virtual void PaintCheckbox(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintRadio(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintTextField(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text) const;

  virtual void PaintMenuList(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintMenuPopupBackground(SkCanvas* canvas,
                                        const gfx::Size& size) const;

  virtual void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintSliderTrack(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintSliderThumb(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintInnerSpinButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) const;

  virtual void PaintProgressBar(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) const;

 protected:
  void set_scrollbar_button_length(unsigned int length) {
    scrollbar_button_length_ = length;
  }

  bool IntersectsClipRectInt(SkCanvas* canvas,
                             int x, int y, int w, int h) const;

  void DrawImageInt(SkCanvas* canvas, const gfx::ImageSkia& image,
                    int src_x, int src_y, int src_w, int src_h,
                    int dest_x, int dest_y, int dest_w, int dest_h) const;

  void DrawTiledImage(SkCanvas* canvas,
                      const gfx::ImageSkia& image,
                      int src_x, int src_y,
                      float tile_scale_x, float tile_scale_y,
                      int dest_x, int dest_y, int w, int h) const;

  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;
 private:
  void DrawVertLine(SkCanvas* canvas,
                    int x,
                    int y1,
                    int y2,
                    const SkPaint& paint) const;
  void DrawHorizLine(SkCanvas* canvas,
                     int x1,
                     int x2,
                     int y,
                     const SkPaint& paint) const;
  void DrawBox(SkCanvas* canvas,
               const gfx::Rect& rect,
               const SkPaint& paint) const;
  SkScalar Clamp(SkScalar value,
                 SkScalar min,
                 SkScalar max) const;
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  // Returns whether the new vector-graphics based checkbox and radio button
  // style is enabled.
  bool IsNewCheckboxStyleEnabled(SkCanvas* canvas) const;

  // Paint the common parts of the new (experimental) checkboxes and radio
  // buttons.
  // borderRadius specifies how rounded the corners should be.
  SkRect PaintCheckboxRadioNewCommon(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SkScalar borderRadius) const;

  // Paint an (experimental) vector-graphics based checkbox on the supplied
  // canvas at the specified co-ordinates.
  void PaintCheckboxNew(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  // Paint an (experimental) vector-graphics based radio button on the
  // supplied canbas at the specified co-ordinates.
  void PaintRadioNew(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  unsigned int scrollbar_width_;
  unsigned int scrollbar_button_length_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeBase);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
