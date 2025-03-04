// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_
#define CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {
namespace test_utils {

// Read a JSON dictionary from |filename| and return it as a
// DictionaryValue. CHECKs if any error occurs.
scoped_ptr<base::DictionaryValue> ReadTestDictionary(
    const std::string& filename);

// Checks that the pointer |actual| is not NULL but points to a dictionary that
// equals |expected| (using Value::Equals). The intended use case is:
// EXPECT_TRUE(test_utils::Equals(expected, actual));
::testing::AssertionResult Equals(const base::DictionaryValue* expected,
                                  const base::DictionaryValue* actual);

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_
