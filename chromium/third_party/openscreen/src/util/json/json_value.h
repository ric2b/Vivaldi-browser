// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_VALUE_H_
#define UTIL_JSON_JSON_VALUE_H_

#include <optional>
#include <string_view>

#include "json/value.h"

#define JSON_EXPAND_FIND_CONSTANT_ARGS(s) (s), ((s) + sizeof(s) - 1)

namespace openscreen {

std::optional<int> MaybeGetInt(const Json::Value& message,
                               const char* first,
                               const char* last);

std::optional<std::string_view> MaybeGetString(const Json::Value& message);

std::optional<std::string_view> MaybeGetString(const Json::Value& message,
                                               const char* first,
                                               const char* last);

}  // namespace openscreen

#endif  // UTIL_JSON_JSON_VALUE_H_
