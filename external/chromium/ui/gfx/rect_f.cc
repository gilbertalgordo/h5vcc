// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_f.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "ui/gfx/insets_f.h"
#include "ui/gfx/rect_base_impl.h"
#include "ui/gfx/safe_integer_conversions.h"

namespace gfx {

template class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>;

typedef class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF,
                       float> RectBaseT;

bool RectF::IsExpressibleAsRect() const {
  return IsExpressibleAsInt(x()) && IsExpressibleAsInt(y()) &&
      IsExpressibleAsInt(width()) && IsExpressibleAsInt(height()) &&
      IsExpressibleAsInt(right()) && IsExpressibleAsInt(bottom());
}

std::string RectF::ToString() const {
  return base::StringPrintf("%s %s",
                            origin().ToString().c_str(),
                            size().ToString().c_str());
}

RectF operator+(const RectF& lhs, const Vector2dF& rhs) {
  RectF result(lhs);
  result += rhs;
  return result;
}

RectF operator-(const RectF& lhs, const Vector2dF& rhs) {
  RectF result(lhs);
  result -= rhs;
  return result;
}

RectF IntersectRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Intersect(b);
  return result;
}

RectF UnionRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Union(b);
  return result;
}

RectF SubtractRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Subtract(b);
  return result;
}

RectF ScaleRect(const RectF& r, float x_scale, float y_scale) {
  RectF result = r;
  result.Scale(x_scale, y_scale);
  return result;
}

RectF BoundingRect(const PointF& p1, const PointF& p2) {
  float rx = std::min(p1.x(), p2.x());
  float ry = std::min(p1.y(), p2.y());
  float rr = std::max(p1.x(), p2.x());
  float rb = std::max(p1.y(), p2.y());
  return RectF(rx, ry, rr - rx, rb - ry);
}

}  // namespace gfx
