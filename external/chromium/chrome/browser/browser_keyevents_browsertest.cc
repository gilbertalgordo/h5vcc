// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"
#include "ui/base/keycodes/keyboard_codes.h"

using content::DomOperationNotificationDetails;
using content::NavigationController;
using content::RenderViewHost;

namespace {

const char kTestingPage[] = "files/keyevents_test.html";
const wchar_t kSuppressEventJS[] =
    L"window.domAutomationController.send(setDefaultAction('%ls', %ls));";
const wchar_t kGetResultJS[] =
    L"window.domAutomationController.send(keyEventResult[%d]);";
const wchar_t kGetResultLengthJS[] =
    L"window.domAutomationController.send(keyEventResult.length);";
const wchar_t kGetFocusedElementJS[] =
    L"window.domAutomationController.send(focusedElement);";
const wchar_t kSetFocusedElementJS[] =
    L"window.domAutomationController.send(setFocusedElement('%ls'));";
const wchar_t kGetTextBoxValueJS[] =
    L"window.domAutomationController.send("
    L"document.getElementById('%ls').value);";
const wchar_t kSetTextBoxValueJS[] =
    L"window.domAutomationController.send("
    L"document.getElementById('%ls').value = '%ls');";
const wchar_t kStartTestJS[] =
    L"window.domAutomationController.send(startTest(%d));";

// Maximum lenght of the result array in KeyEventTestData structure.
const size_t kMaxResultLength = 10;

// A structure holding test data of a keyboard event.
// Each keyboard event may generate multiple result strings representing
// the result of keydown, keypress, keyup and textInput events.
// For keydown, keypress and keyup events, the format of the result string is:
// <type> <keyCode> <charCode> <ctrlKey> <shiftKey> <altKey> <commandKey>
// where <type> may be 'D' (keydown), 'P' (keypress) or 'U' (keyup).
// <ctrlKey>, <shiftKey> <altKey> and <commandKey> are boolean value indicating
// the state of corresponding modifier key.
// For textInput event, the format is: T <text>, where <text> is the text to be
// input.
// Please refer to chrome/test/data/keyevents_test.html for details.
struct KeyEventTestData {
  ui::KeyboardCode key;
  bool ctrl;
  bool shift;
  bool alt;
  bool command;

  bool suppress_keydown;
  bool suppress_keypress;
  bool suppress_keyup;
  bool suppress_textinput;

  int result_length;
  const char* const result[kMaxResultLength];
};

const wchar_t* GetBoolString(bool value) {
  return value ? L"true" : L"false";
}

// A class to help wait for the finish of a key event test.
class TestFinishObserver : public content::NotificationObserver {
 public:
  explicit TestFinishObserver(RenderViewHost* render_view_host)
      : finished_(false), waiting_(false) {
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::Source<RenderViewHost>(render_view_host));
  }

  bool WaitForFinish() {
    if (!finished_) {
      waiting_ = true;
      content::RunMessageLoop();
      waiting_ = false;
    }
    return finished_;
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    DCHECK(type == content::NOTIFICATION_DOM_OPERATION_RESPONSE);
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    // We might receive responses for other script execution, but we only
    // care about the test finished message.
    if (dom_op_details->json == "\"FINISHED\"") {
      finished_ = true;
      if (waiting_)
        MessageLoopForUI::current()->Quit();
    }
  }

 private:
  bool finished_;
  bool waiting_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestFinishObserver);
};

class BrowserKeyEventsTest : public InProcessBrowserTest {
 public:
  BrowserKeyEventsTest() {}

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) {
    ui_test_utils::ClickOnView(browser(), vid);
  }

  // Set the suppress flag of an event specified by |type|. If |suppress| is
  // true then the web page will suppress all events with |type|. Following
  // event types are supported: keydown, keypress, keyup and textInput.
  void SuppressEventByType(int tab_index, const wchar_t* type, bool suppress) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"",
        base::StringPrintf(kSuppressEventJS, type, GetBoolString(!suppress)),
        &actual));
    ASSERT_EQ(!suppress, actual);
  }

  void SuppressEvents(int tab_index, bool keydown, bool keypress,
                      bool keyup, bool textinput) {
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keydown", keydown));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keypress", keypress));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"keyup", keyup));
    ASSERT_NO_FATAL_FAILURE(
        SuppressEventByType(tab_index, L"textInput", textinput));
  }

  void SuppressAllEvents(int tab_index, bool suppress) {
    SuppressEvents(tab_index, suppress, suppress, suppress, suppress);
  }

  void GetResultLength(int tab_index, int* length) {
    ASSERT_LT(tab_index, browser()->tab_count());
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractInt(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"", kGetResultLengthJS, length));
  }

  void CheckResult(int tab_index, int length, const char* const result[]) {
    ASSERT_LT(tab_index, browser()->tab_count());
    int actual_length;
    ASSERT_NO_FATAL_FAILURE(GetResultLength(tab_index, &actual_length));
    ASSERT_GE(actual_length, length);
    for (int i = 0; i < actual_length; ++i) {
      std::string actual;
      ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
          chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
          L"", base::StringPrintf(kGetResultJS, i), &actual));

      // If more events were received than expected, then the additional events
      // must be keyup events.
      if (i < length)
        ASSERT_STREQ(result[i], actual.c_str());
      else
        ASSERT_EQ('U', actual[0]);
    }
  }

  void CheckFocusedElement(int tab_index, const wchar_t* focused) {
    ASSERT_LT(tab_index, browser()->tab_count());
    std::string actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"", kGetFocusedElementJS, &actual));
    ASSERT_EQ(WideToUTF8(focused), actual);
  }

  void SetFocusedElement(int tab_index, const wchar_t* focused) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"",
        base::StringPrintf(kSetFocusedElementJS, focused),
        &actual));
    ASSERT_TRUE(actual);
  }

  void CheckTextBoxValue(int tab_index, const wchar_t* id,
                         const wchar_t* value) {
    ASSERT_LT(tab_index, browser()->tab_count());
    std::string actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"",
        base::StringPrintf(kGetTextBoxValueJS, id),
        &actual));
    ASSERT_EQ(WideToUTF8(value), actual);
  }

  void SetTextBoxValue(int tab_index, const wchar_t* id,
                       const wchar_t* value) {
    ASSERT_LT(tab_index, browser()->tab_count());
    std::string actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"",
        base::StringPrintf(kSetTextBoxValueJS, id, value),
        &actual));
    ASSERT_EQ(WideToUTF8(value), actual);
  }

  void StartTest(int tab_index, int result_length) {
    ASSERT_LT(tab_index, browser()->tab_count());
    bool actual;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost(),
        L"", base::StringPrintf(kStartTestJS, result_length), &actual));
    ASSERT_TRUE(actual);
  }

  void TestKeyEvent(int tab_index, const KeyEventTestData& test) {
    ASSERT_LT(tab_index, browser()->tab_count());
    ASSERT_EQ(tab_index, browser()->active_index());

    // Inform our testing web page that we are about to start testing a key
    // event.
    ASSERT_NO_FATAL_FAILURE(StartTest(tab_index, test.result_length));
    ASSERT_NO_FATAL_FAILURE(SuppressEvents(
        tab_index, test.suppress_keydown, test.suppress_keypress,
        test.suppress_keyup, test.suppress_textinput));

    // We need to create a finish observer before sending the key event,
    // because the test finished message might be arrived before returning
    // from the SendKeyPressSync() method.
    TestFinishObserver finish_observer(
        chrome::GetWebContentsAt(browser(), tab_index)->GetRenderViewHost());

    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), test.key, test.ctrl, test.shift, test.alt, test.command));
    ASSERT_TRUE(finish_observer.WaitForFinish());
    ASSERT_NO_FATAL_FAILURE(CheckResult(
        tab_index, test.result_length, test.result));
  }

  std::string GetTestDataDescription(const KeyEventTestData& data) {
    std::string desc = base::StringPrintf(
        " VKEY:0x%02x, ctrl:%d, shift:%d, alt:%d, command:%d\n"
        " Suppress: keydown:%d, keypress:%d, keyup:%d, textInput:%d\n"
        " Expected results(%d):\n",
        data.key, data.ctrl, data.shift, data.alt, data.command,
        data.suppress_keydown, data.suppress_keypress, data.suppress_keyup,
        data.suppress_textinput, data.result_length);
    for (int i = 0; i < data.result_length; ++i) {
      desc.append("  ");
      desc.append(data.result[i]);
      desc.append("\n");
    }
    return desc;
  }
};

#if defined(OS_MACOSX)
// http://crbug.com/81451
#define MAYBE_NormalKeyEvents DISABLED_NormalKeyEvents
#elif defined(OS_LINUX)
// http://crbug.com/129235
#define MAYBE_NormalKeyEvents DISABLED_NormalKeyEvents
#else
#define MAYBE_NormalKeyEvents NormalKeyEvents
#endif

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, MAYBE_NormalKeyEvents) {
  static const KeyEventTestData kTestNoInput[] = {
    // a
    { ui::VKEY_A, false, false, false, false,
      false, false, false, false, 3,
      { "D 65 0 false false false false",
        "P 97 97 false false false false",
        "U 65 0 false false false false" } },
    // shift-a
    { ui::VKEY_A, false, true, false, false,
      false, false, false, false, 5,
      { "D 16 0 false true false false",
        "D 65 0 false true false false",
        "P 65 65 false true false false",
        "U 65 0 false true false false",
        "U 16 0 false true false false" } },
    // a, suppress keydown
    { ui::VKEY_A, false, false, false, false,
      true, false, false, false, 2,
      { "D 65 0 false false false false",
        "U 65 0 false false false false" } },
  };

  static const KeyEventTestData kTestWithInput[] = {
    // a
    { ui::VKEY_A, false, false, false, false,
      false, false, false, false, 4,
      { "D 65 0 false false false false",
        "P 97 97 false false false false",
        "T a",
        "U 65 0 false false false false" } },
    // shift-a
    { ui::VKEY_A, false, true, false, false,
      false, false, false, false, 6,
      { "D 16 0 false true false false",
        "D 65 0 false true false false",
        "P 65 65 false true false false",
        "T A",
        "U 65 0 false true false false",
        "U 16 0 false true false false" } },
    // a, suppress keydown
    { ui::VKEY_A, false, false, false, false,
      true, false, false, false, 2,
      { "D 65 0 false false false false",
        "U 65 0 false false false false" } },
    // a, suppress keypress
    { ui::VKEY_A, false, false, false, false,
      false, true, false, false, 3,
      { "D 65 0 false false false false",
        "P 97 97 false false false false",
        "U 65 0 false false false false" } },
    // a, suppress textInput
    { ui::VKEY_A, false, false, false, false,
      false, false, false, true, 4,
      { "D 65 0 false false false false",
        "P 97 97 false false false false",
        "T a",
        "U 65 0 false false false false" } },
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  for (size_t i = 0; i < arraysize(kTestNoInput); ++i) {
    EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestNoInput[i]))
        << "kTestNoInput[" << i << "] failed:\n"
        << GetTestDataDescription(kTestNoInput[i]);
  }

  // Input in normal text box.
  ASSERT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L"A"));
  for (size_t i = 0; i < arraysize(kTestWithInput); ++i) {
    EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestWithInput[i]))
        << "kTestWithInput[" << i << "] in text box failed:\n"
        << GetTestDataDescription(kTestWithInput[i]);
  }
  EXPECT_NO_FATAL_FAILURE(CheckTextBoxValue(tab_index, L"A", L"aA"));

  // Input in password box.
  ASSERT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L"B"));
  for (size_t i = 0; i < arraysize(kTestWithInput); ++i) {
    EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestWithInput[i]))
        << "kTestWithInput[" << i << "] in password box failed:\n"
        << GetTestDataDescription(kTestWithInput[i]);
  }
  EXPECT_NO_FATAL_FAILURE(CheckTextBoxValue(tab_index, L"B", L"aA"));
}

#if defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_LINUX)
// http://crbug.com/129235
#define MAYBE_CtrlKeyEvents DISABLED_CtrlKeyEvents
#else
#define MAYBE_CtrlKeyEvents CtrlKeyEvents
#endif

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, MAYBE_CtrlKeyEvents) {
  static const KeyEventTestData kTestCtrlF = {
    ui::VKEY_F, true, false, false, false,
    false, false, false, false, 2,
    { "D 17 0 true false false false",
      "D 70 0 true false false false" }
  };

  static const KeyEventTestData kTestCtrlFSuppressKeyDown = {
    ui::VKEY_F, true, false, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 70 0 true false false false",
      "U 70 0 true false false false",
      "U 17 0 true false false false" }
  };

  // Ctrl+Z doesn't bind to any accelerators, which then should generate a
  // keypress event with charCode=26.
  static const KeyEventTestData kTestCtrlZ = {
    ui::VKEY_Z, true, false, false, false,
    false, false, false, false, 5,
    { "D 17 0 true false false false",
      "D 90 0 true false false false",
      "P 26 26 true false false false",
      "U 90 0 true false false false",
      "U 17 0 true false false false" }
  };

  static const KeyEventTestData kTestCtrlZSuppressKeyDown = {
    ui::VKEY_Z, true, false, false, false,
    true, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 90 0 true false false false",
      "U 90 0 true false false false",
      "U 17 0 true false false false" }
  };

  // Ctrl+Enter shall generate a keypress event with charCode=10 (LF).
  static const KeyEventTestData kTestCtrlEnter = {
    ui::VKEY_RETURN, true, false, false, false,
    false, false, false, false, 5,
    { "D 17 0 true false false false",
      "D 13 0 true false false false",
      "P 10 10 true false false false",
      "U 13 0 true false false false",
      "U 17 0 true false false false" }
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  // Press Ctrl+F, which will make the Find box open and request focus.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlF));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Press Escape to close the Find box and move the focus back to the web page.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Press Ctrl+F with keydown suppressed shall not open the find box.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlFSuppressKeyDown));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlZ));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlZSuppressKeyDown));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlEnter));
}
#elif defined(OS_MACOSX)
// http://crbug.com/81451
IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, DISABLED_CommandKeyEvents) {
  static const KeyEventTestData kTestCmdF = {
    ui::VKEY_F, false, false, false, true,
    false, false, false, false, 2,
    { "D 91 0 false false false true",
      "D 70 0 false false false true" }
  };

  // On Mac we don't send key up events when command modifier is down.
  static const KeyEventTestData kTestCmdFSuppressKeyDown = {
    ui::VKEY_F, false, false, false, true,
    true, false, false, false, 3,
    { "D 91 0 false false false true",
      "D 70 0 false false false true",
      "U 91 0 false false false true" }
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  // Press Cmd+F, which will make the Find box open and request focus.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCmdF));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Press Escape to close the Find box and move the focus back to the web page.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Press Cmd+F with keydown suppressed shall not open the find box.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCmdFSuppressKeyDown));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}
#endif

#if defined(OS_MACOSX)
// http://crbug.com/81451 for mac
#define MAYBE_AccessKeys DISABLED_AccessKeys
#elif defined(OS_LINUX)
// http://crbug.com/129235
#define MAYBE_AccessKeys DISABLED_AccessKeys
#else
#define MAYBE_AccessKeys AccessKeys
#endif

IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, MAYBE_AccessKeys) {
#if defined(OS_MACOSX)
  // On Mac, access keys use ctrl+alt modifiers.
  static const KeyEventTestData kTestAccessA = {
    ui::VKEY_A, true, false, true, false,
    false, false, false, false, 6,
    { "D 17 0 true false false false",
      "D 18 0 true false true false",
      "D 65 0 true false true false",
      "U 65 0 true false true false",
      "U 18 0 true false true false",
      "U 17 0 true false false false" }
  };

  static const KeyEventTestData kTestAccessDSuppress = {
    ui::VKEY_D, true, false, true, false,
    true, true, true, false, 6,
    { "D 17 0 true false false false",
      "D 18 0 true false true false",
      "D 68 0 true false true false",
      "U 68 0 true false true false",
      "U 18 0 true false true false",
      "U 17 0 true false false false" }
  };

  static const KeyEventTestData kTestAccess1 = {
    ui::VKEY_1, true, false, true, false,
    false, false, false, false, 6,
    { "D 17 0 true false false false",
      "D 18 0 true false true false",
      "D 49 0 true false true false",
      "U 49 0 true false true false",
      "U 18 0 true false true false",
      "U 17 0 true false false false" }
  };
#else
  static const KeyEventTestData kTestAccessA = {
    ui::VKEY_A, false, false, true, false,
    false, false, false, false, 4,
    { "D 18 0 false false true false",
      "D 65 0 false false true false",
      "U 65 0 false false true false",
      "U 18 0 false false true false" }
  };

  static const KeyEventTestData kTestAccessD = {
    ui::VKEY_D, false, false, true, false,
    false, false, false, false, 2,
    { "D 18 0 false false true false",
      "D 68 0 false false true false" }
  };

  static const KeyEventTestData kTestAccessDSuppress = {
    ui::VKEY_D, false, false, true, false,
    true, true, true, false, 4,
    { "D 18 0 false false true false",
      "D 68 0 false false true false",
      "U 68 0 false false true false",
      "U 18 0 false false true false" }
  };

#if !defined(USE_ASH)
  static const KeyEventTestData kTestAccess1 = {
    ui::VKEY_1, false, false, true, false,
    false, false, false, false, 4,
    { "D 18 0 false false true false",
      "D 49 0 false false true false",
      "U 49 0 false false true false",
      "U 18 0 false false true false" }
  };
#endif
#endif

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  content::RunAllPendingInMessageLoop();
  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
  // Alt+A should focus the element with accesskey = "A".
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAccessA));
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"A"));

  // Blur the focused element.
  EXPECT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L""));
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));

#if !defined(OS_MACOSX)
  // Alt+D should move the focus to the location entry.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAccessD));

  // TODO(isherman): This is an experimental change to help diagnose
  // http://crbug.com/55713
  content::RunAllPendingInMessageLoop();
#if defined(USE_AURA)
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
#else
  EXPECT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
#endif
  // No element should be focused, as Alt+D was handled by the browser.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));

  // Move the focus back to the web page.
  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
#endif

  // If the keydown event is suppressed, then Alt+D should be handled as an
  // accesskey rather than an accelerator key. Activation of an accesskey is not
  // a part of the default action of the key event, so it should not be
  // suppressed at all.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAccessDSuppress));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"D"));

  // Blur the focused element.
  EXPECT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L""));
  // Make sure no element is focused.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
#if !defined(USE_ASH)
  // On Ash, alt-1..9 are assigned as window selection global accelerators, so
  // they can not be used as accesskeys.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAccess1));
#if defined(TOOLKIT_GTK)
  // On GTK, alt-0..9 are assigned as tab selection accelerators, so they can
  // not be used as accesskeys.
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L""));
#else
  EXPECT_NO_FATAL_FAILURE(CheckFocusedElement(tab_index, L"1"));
#endif
#endif
}

// Flaky, http://crbug.com/69475.
#if defined(OS_LINUX)
#define MAYBE_ReservedAccelerators DISABLED_ReservedAccelerators
#else
#define MAYBE_ReservedAccelerators ReservedAccelerators
#endif
IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, MAYBE_ReservedAccelerators) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  ASSERT_EQ(1, browser()->tab_count());

  static const KeyEventTestData kTestCtrlOrCmdT = {
#if defined(OS_MACOSX)
    ui::VKEY_T, false, false, false, true,
    true, false, false, false, 1,
    { "D 91 0 false false false true" }
#else
    ui::VKEY_T, true, false, false, false,
    true, false, false, false, 1,
    { "D 17 0 true false false false" }
#endif
  };

  content::WindowedNotificationObserver wait_for_new_tab(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());

  // Press Ctrl/Cmd+T, which will open a new tab. It cannot be suppressed.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(0, kTestCtrlOrCmdT));
  wait_for_new_tab.Wait();

  int result_length;
  ASSERT_NO_FATAL_FAILURE(GetResultLength(0, &result_length));
  EXPECT_EQ(1, result_length);

  EXPECT_EQ(2, browser()->tab_count());
  ASSERT_EQ(1, browser()->active_index());

  // Because of issue <http://crbug.com/65375>, switching back to the first tab
  // may cause the focus to be grabbed by omnibox. So instead, we load our
  // testing page in the newly created tab and try Cmd-W here.
  ui_test_utils::NavigateToURL(browser(), url);

  // Make sure the focus is in the testing page.
  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Reserved accelerators can't be suppressed.
  ASSERT_NO_FATAL_FAILURE(SuppressAllEvents(1, true));

  content::WindowedNotificationObserver wait_for_tab_closed(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(chrome::GetWebContentsAt(browser(), 1)));

  // Press Ctrl/Cmd+W, which will close the tab.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, true, false, false, false));
#endif

  ASSERT_NO_FATAL_FAILURE(wait_for_tab_closed.Wait());

  EXPECT_EQ(1, browser()->tab_count());
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, EditorKeyBindings) {
  static const KeyEventTestData kTestCtrlA = {
    ui::VKEY_A, true, false, false, false,
    false, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 65 0 true false false false",
      "U 65 0 true false false false",
      "U 17 0 true false false false" }
  };

  static const KeyEventTestData kTestCtrlF = {
    ui::VKEY_F, true, false, false, false,
    false, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 70 0 true false false false",
      "U 70 0 true false false false",
      "U 17 0 true false false false" }
  };

  static const KeyEventTestData kTestCtrlK = {
    ui::VKEY_K, true, false, false, false,
    false, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 75 0 true false false false",
      "U 75 0 true false false false",
      "U 17 0 true false false false" }
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  ASSERT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L"A"));
  ASSERT_NO_FATAL_FAILURE(SetTextBoxValue(tab_index, L"A", L"Hello"));
  // Move the caret to the beginning of the line.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlA));
  // Forward one character
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlF));
  // Delete to the end of the line.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlK));
  EXPECT_NO_FATAL_FAILURE(CheckTextBoxValue(tab_index, L"A", L"H"));
}
#endif

// See http://crbug.com/147579
IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, FLAKY_PageUpDownKeys) {
  static const KeyEventTestData kTestPageUp = {
    ui::VKEY_PRIOR, false, false, false, false,
    false, false, false, false, 2,
    { "D 33 0 false false false false",
      "U 33 0 false false false false" }
  };

  static const KeyEventTestData kTestPageDown = {
    ui::VKEY_NEXT, false, false, false, false,
    false, false, false, false, 2,
    { "D 34 0 false false false false",
      "U 34 0 false false false false" }
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  ASSERT_NO_FATAL_FAILURE(SetFocusedElement(tab_index, L"A"));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestPageUp));
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestPageDown));
  EXPECT_NO_FATAL_FAILURE(CheckTextBoxValue(tab_index, L"A", L""));
}

#if defined(OS_WIN)
// AltKey is enabled only on Windows. See crbug.com/114537.
IN_PROC_BROWSER_TEST_F(BrowserKeyEventsTest, FocusMenuBarByAltKey) {
  static const KeyEventTestData kTestAltKey = {
    ui::VKEY_MENU, false, false, false, false,
    false, false, false, false, 2,
    { "D 18 0 false false true false",
      "U 18 0 false false true false" }
  };

  static const KeyEventTestData kTestAltKeySuppress = {
    ui::VKEY_MENU, false, false, false, false,
    true, false, false, false, 2,
    { "D 18 0 false false true false",
      "U 18 0 false false true false" }
  };

  static const KeyEventTestData kTestCtrlAltKey = {
    ui::VKEY_MENU, true, false, false, false,
    false, false, false, false, 4,
    { "D 17 0 true false false false",
      "D 18 0 true false true false",
      "U 18 0 true false true false",
      "U 17 0 true false false false" }
  };

  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  GURL url = test_server()->GetURL(kTestingPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  int tab_index = browser()->active_index();
  // Press and release Alt key to focus wrench menu button.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAltKey));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_APP_MENU));

  ASSERT_NO_FATAL_FAILURE(ClickOnView(VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Alt key can be suppressed.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestAltKeySuppress));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Ctrl+Alt should have no effect.
  EXPECT_NO_FATAL_FAILURE(TestKeyEvent(tab_index, kTestCtrlAltKey));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}
#endif

}  // namespace
