// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace views {

namespace {

// View implementation that allows setting the preferred size.
class CustomView : public View {
 public:
  CustomView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return preferred_size_;
  }

  virtual void Layout() OVERRIDE {
    gfx::Size pref = GetPreferredSize();
    int width = pref.width();
    int height = pref.height();
    if (parent()) {
      width = std::max(parent()->width(), width);
      height = std::max(parent()->height(), height);
    }
    SetBounds(x(), y(), width, height);
  }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomView);
};

}  // namespace

// Verifies the viewport is sized to fit the available space.
TEST(ScrollViewTest, ViewportSizedToFit) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary.
TEST(ScrollViewTest, ScrollBars) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view.vertical_scroll_bar() ||
              !scroll_view.vertical_scroll_bar()->visible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
}

// Assertions around adding a header.
TEST(ScrollViewTest, Header) {
  ScrollView scroll_view;
  View* contents = new View;
  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  View* header_parent = header->parent();
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  // |header|s preferred size is empty, which should result in all space going
  // to contents.
  EXPECT_EQ("0,0 100x0", header->parent()->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());

  // Get the header a height of 20.
  header->SetPreferredSize(gfx::Size(10, 20));
  EXPECT_EQ("0,0 100x20", header->parent()->bounds().ToString());
  EXPECT_EQ("0,20 100x80", contents->parent()->bounds().ToString());

  // Remove the header.
  scroll_view.SetHeader(NULL);
  // SetHeader(NULL) deletes header.
  header = NULL;
  EXPECT_EQ("0,0 100x0", header_parent->bounds().ToString());
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary when a header is present.
TEST(ScrollViewTest, ScrollBarsWithHeader) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  header->SetPreferredSize(gfx::Size(10, 20));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(80, contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());


  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100, header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view.vertical_scroll_bar() ||
              !scroll_view.vertical_scroll_bar()->visible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(0, contents->parent()->x());
  EXPECT_EQ(20, contents->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight() - 20,
            contents->parent()->height());
  EXPECT_EQ(0, header->parent()->x());
  EXPECT_EQ(0, header->parent()->y());
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), header->parent()->width());
  EXPECT_EQ(20, header->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
}

// Verifies the header scrolls horizontally with the content.
TEST(ScrollViewTest, HeaderScrollsWithContent) {
  ScrollView scroll_view;
  CustomView* contents = new CustomView;
  scroll_view.SetContents(contents);
  contents->SetPreferredSize(gfx::Size(500, 500));

  CustomView* header = new CustomView;
  scroll_view.SetHeader(header);
  header->SetPreferredSize(gfx::Size(500, 20));

  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ("0,0", contents->bounds().origin().ToString());
  EXPECT_EQ("0,0", header->bounds().origin().ToString());

  // Scroll the horizontal scrollbar.
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar());
  scroll_view.ScrollToPosition(
      const_cast<ScrollBar*>(scroll_view.horizontal_scroll_bar()), 1);
  EXPECT_EQ("-1,0", contents->bounds().origin().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());

  // Scrolling the vertical scrollbar shouldn't effect the header.
  ASSERT_TRUE(scroll_view.vertical_scroll_bar());
  scroll_view.ScrollToPosition(
      const_cast<ScrollBar*>(scroll_view.vertical_scroll_bar()), 1);
  EXPECT_EQ("-1,-1", contents->bounds().origin().ToString());
  EXPECT_EQ("-1,0", header->bounds().origin().ToString());
}

}  // namespace views
