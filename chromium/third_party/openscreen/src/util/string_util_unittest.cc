// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/string_util.h"

#include "gtest/gtest.h"

namespace openscreen::string_util {

// Reference: https://ascii-code.com
TEST(StringUtilTest, AsciiTest) {
  constexpr char kAlpha[] = "aAzZ";
  constexpr char kDigits[] = "09";
  constexpr char kPrintable[] = "*&$^ ";
  constexpr char kNonPrintable[] = "\000\010\015\177\202";

  for (size_t i = 0; i < sizeof(kAlpha) - 1; i++) {
    EXPECT_TRUE(ascii_isalpha(kAlpha[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kAlpha[i])) << i;
    EXPECT_TRUE(ascii_isprint(kAlpha[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kDigits) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kDigits[i])) << i;
    EXPECT_TRUE(ascii_isdigit(kDigits[i])) << i;
    EXPECT_TRUE(ascii_isprint(kDigits[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kPrintable) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kPrintable[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kPrintable[i])) << i;
    EXPECT_TRUE(ascii_isprint(kPrintable[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kNonPrintable) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kNonPrintable[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kNonPrintable[i])) << i;
    EXPECT_FALSE(ascii_isprint(kNonPrintable[i])) << i;
  }

  EXPECT_EQ(ascii_tolower('A'), 'a');
  EXPECT_EQ(ascii_tolower('a'), 'a');
  EXPECT_EQ(ascii_tolower('0'), '0');
  EXPECT_EQ(ascii_toupper('A'), 'A');
  EXPECT_EQ(ascii_toupper('a'), 'A');
  EXPECT_EQ(ascii_toupper('0'), '0');
}

TEST(StringUtilTest, StartsAndEndsWith) {
  constexpr char kString[] = "United Federation of Planets";
  EXPECT_TRUE(starts_with("", ""));
  EXPECT_TRUE(starts_with(kString, ""));
  EXPECT_TRUE(starts_with(kString, "United"));
  EXPECT_FALSE(starts_with(kString, "Klingons"));

  EXPECT_TRUE(ends_with("", ""));
  EXPECT_TRUE(ends_with(kString, ""));
  EXPECT_TRUE(ends_with(kString, "Planets"));
  EXPECT_FALSE(ends_with(kString, "Borg"));
}

TEST(StringUtilTest, EqualsIgnoreCase) {
  constexpr char kString[] = "Vulcans!";
  EXPECT_TRUE(EqualsIgnoreCase("", ""));
  EXPECT_FALSE(EqualsIgnoreCase("", kString));
  EXPECT_FALSE(EqualsIgnoreCase("planet vulcan", kString));
  EXPECT_TRUE(EqualsIgnoreCase("Vulcans!", kString));
  EXPECT_TRUE(EqualsIgnoreCase("vUlCaNs!", kString));
  EXPECT_FALSE(EqualsIgnoreCase("vUlKaNs!", kString));
}

TEST(StringUtilTest, AsciiStrToUpperLower) {
  constexpr char kString[] = "Vulcans!";
  EXPECT_EQ("", AsciiStrToUpper(""));
  EXPECT_EQ("", AsciiStrToLower(""));

  EXPECT_EQ("VULCANS!", AsciiStrToUpper("Vulcans!"));
  std::string s1(kString);
  AsciiStrToUpper(s1);
  EXPECT_EQ("VULCANS!", s1);

  EXPECT_EQ("vulcans!", AsciiStrToLower("Vulcans!"));
  std::string s2(kString);
  AsciiStrToLower(s2);
  EXPECT_EQ("vulcans!", s2);
}

TEST(StringUtilTest, StrCat) {
  EXPECT_EQ(std::string(), StrCat({}));
  EXPECT_EQ(std::string(), StrCat({"", ""}));
  EXPECT_EQ(std::string("abcdef"), StrCat({"abc", std::string("def")}));
}

TEST(StringUtilTest, Split) {
  std::vector<std::string_view> result;
  std::vector<std::string_view> empty;
  auto single = std::vector<std::string_view>({"donut"});
  auto expected = std::vector<std::string_view>({"a", "b", "ccc"});

  result = Split("", ';');
  EXPECT_EQ(result, empty);
  result = Split(";;;;;", ';');
  EXPECT_EQ(result, empty);
  result = Split("donut", ';');
  EXPECT_EQ(result, single);
  result = Split(";;;donut", ';');
  EXPECT_EQ(result, single);
  result = Split("donut;;;", ';');
  EXPECT_EQ(result, single);
  result = Split("a;;b;;;ccc", ';');
  EXPECT_EQ(result, expected);
  result = Split(";;;a;;b;;;ccc", ';');
  EXPECT_EQ(result, expected);
  result = Split(";;;a;;b;;;ccc;;;;", ';');
  EXPECT_EQ(result, expected);
}

TEST(StringUtilTest, Join) {
  std::vector<std::string_view> empty;
  auto single = std::vector<std::string_view>({"donut"});
  auto input = std::vector<std::string_view>({"a", "b", "ccc"});

  EXPECT_EQ("", Join(empty.begin(), empty.end(), ","));
  EXPECT_EQ("donut", Join(single.begin(), single.end(), ","));
  EXPECT_EQ("abccc", Join(input.begin(), input.end(), ""));
  EXPECT_EQ("a,b,ccc", Join(input.begin(), input.end(), ","));
  EXPECT_EQ("a<->b<->ccc", Join(input.begin(), input.end(), "<->"));
}

}  // namespace openscreen::string_util
