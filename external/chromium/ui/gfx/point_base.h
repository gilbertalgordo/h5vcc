// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_BASE_H_
#define UI_GFX_POINT_BASE_H_

#include <string>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/base/ui_export.h"

namespace gfx {

// A point has an x and y coordinate.
template<typename Class, typename Type, typename VectorClass>
class UI_EXPORT PointBase {
 public:
  Type x() const { return x_; }
  Type y() const { return y_; }

  void SetPoint(Type x, Type y) {
    x_ = x;
    y_ = y;
  }

  void set_x(Type x) { x_ = x; }
  void set_y(Type y) { y_ = y; }

  void Offset(Type delta_x, Type delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  void operator+=(const VectorClass& vector) {
    x_ += vector.x();
    y_ += vector.y();
  }

  void operator-=(const VectorClass& vector) {
    x_ -= vector.x();
    y_ -= vector.y();
  }

  void ClampToMax(const Class& max) {
    x_ = x_ <= max.x_ ? x_ : max.x_;
    y_ = y_ <= max.y_ ? y_ : max.y_;
  }

  void ClampToMin(const Class& min) {
    x_ = x_ >= min.x_ ? x_ : min.x_;
    y_ = y_ >= min.y_ ? y_ : min.y_;
  }

  bool IsOrigin() const {
    return x_ == 0 && y_ == 0;
  }

  VectorClass OffsetFromOrigin() const {
    return VectorClass(x_, y_);
  }

  // A point is less than another point if its y-value is closer
  // to the origin. If the y-values are the same, then point with
  // the x-value closer to the origin is considered less than the
  // other.
  // This comparison is required to use Point in sets, or sorted
  // vectors.
  bool operator<(const Class& rhs) const {
    return (y_ == rhs.y_) ? (x_ < rhs.x_) : (y_ < rhs.y_);
  }

#if defined(__LB_SHELL__)
  virtual ~PointBase() {}
#endif

 protected:
  PointBase(Type x, Type y) : x_(x), y_(y) {}
#if !defined(__LB_SHELL__)
  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~PointBase() {}
#endif

 private:
  Type x_;
  Type y_;
};

}  // namespace gfx

#endif  // UI_GFX_POINT_BASE_H_
