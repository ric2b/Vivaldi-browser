// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/string_parse.h"

#include <limits>
#include <string_view>

#include "gtest/gtest.h"

namespace openscreen::string_parse {

void ExpectParseInt(std::string_view number, int expected_value) {
  int result = 0;
  EXPECT_TRUE(string_parse::ParseAsciiNumber(number, result));
  EXPECT_EQ(expected_value, result);
}

TEST(StringParseTest, ParseAsciiNumberInt) {
  ExpectParseInt("0", 0);
  ExpectParseInt("0100", 100);
  ExpectParseInt("13245", 13245);
  ExpectParseInt("-77377", -77377);
  ExpectParseInt("-2147483648", std::numeric_limits<int>::min());
  ExpectParseInt("2147483647", std::numeric_limits<int>::max());
}

TEST(StringParseTest, ParseAsciiNumberFails) {
  int result = 0;
  EXPECT_FALSE(string_parse::ParseAsciiNumber("", result));
  EXPECT_FALSE(string_parse::ParseAsciiNumber("- 100", result));
  EXPECT_FALSE(string_parse::ParseAsciiNumber("ASXD", result));
  EXPECT_FALSE(string_parse::ParseAsciiNumber("  100", result));
  EXPECT_FALSE(string_parse::ParseAsciiNumber("-2147483649", result));
  EXPECT_FALSE(string_parse::ParseAsciiNumber("2147483648", result));
}

}  // namespace openscreen::string_parse
