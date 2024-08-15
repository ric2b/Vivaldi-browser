// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STRINGUTIL_H_
#define UTIL_STRINGUTIL_H_

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

// String query and manipulation utilities.
namespace openscreen::stringutil {

namespace internal {
extern const unsigned char kPropertyBits[256];
extern const char kToLower[256];
extern const char kToUpper[256];
}  // namespace internal

// Determines whether the given character is an alphabetic character.
inline bool ascii_isalpha(unsigned char c) {
  return (internal::kPropertyBits[c] & 0x01) != 0;
}

// Determines whether the given character can be represented as a decimal
// digit character (i.e. {0-9}).
inline bool ascii_isdigit(unsigned char c) {
  return c >= '0' && c <= '9';
}

// Determines whether the given character is printable, including spaces.
inline bool ascii_isprint(unsigned char c) {
  return c >= 32 && c < 127;
}

// Determines whether the given character is a whitespace character (space,
// tab, vertical tab, formfeed, linefeed, or carriage return).
inline bool ascii_isspace(unsigned char c) {
  return (internal::kPropertyBits[c] & 0x08) != 0;
}

inline char ascii_tolower(unsigned char c) {
  return internal::kToLower[c];
}

// Converts s to lowercase.
void AsciiStrToLower(std::string& s);

// Creates a lowercase string from a given string_view.
std::string AsciiStrToLower(std::string_view s);

inline char ascii_toupper(unsigned char c) {
  return internal::kToUpper[c];
}

// Converts s to uppercase.
void AsciiStrToUpper(std::string& s);

// Creates a uppercase string from a given string_view.
std::string AsciiStrToUpper(std::string_view s);

// Returns whether a given string `text` begins with `prefix`.
//
// NOTE: Replace with std::{string,string_view}::starts_with() once C++20 is the
// default.
inline bool starts_with(std::string_view text, std::string_view prefix) {
  return prefix.empty() ||
         (text.size() >= prefix.size() &&
          memcmp(text.data(), prefix.data(), prefix.size()) == 0);
}

// Returns whether a given string `text` ends with `suffix`.
//
// NOTE: Replace with std::{string,string_view}::ends_with() once C++20 is the
// default.
inline bool ends_with(std::string_view text, std::string_view suffix) {
  return suffix.empty() || (text.size() >= suffix.size() &&
                            memcmp(text.data() + (text.size() - suffix.size()),
                                   suffix.data(), suffix.size()) == 0);
}

// Returns whether given ASCII strings `piece1` and `piece2` are equal, ignoring
// case in the comparison.
bool EqualsIgnoreCase(std::string_view piece1, std::string_view piece2);

// Returns std::string_view with whitespace stripped from the beginning of the
// given string_view.
inline std::string_view StripLeadingAsciiWhitespace(std::string_view str) {
  auto it = std::find_if_not(str.cbegin(), str.cend(), ascii_isspace);
  return str.substr(static_cast<size_t>(it - str.begin()));
}

}  // namespace openscreen::stringutil

#endif  // UTIL_STRINGUTIL_H_
