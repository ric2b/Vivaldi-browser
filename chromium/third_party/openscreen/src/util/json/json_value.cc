// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_value.h"

namespace openscreen {

std::optional<int> MaybeGetInt(const Json::Value& message,
                               const char* first,
                               const char* last) {
  const Json::Value* value = message.find(first, last);
  std::optional<int> result;
  if (value && value->isInt()) {
    result = value->asInt();
  }
  return result;
}

std::optional<std::string_view> MaybeGetString(const Json::Value& message) {
  if (message.isString()) {
    const char* begin = nullptr;
    const char* end = nullptr;
    message.getString(&begin, &end);
    if (begin && end >= begin) {
      return std::string_view(begin, end - begin);
    }
  }
  return std::nullopt;
}

std::optional<std::string_view> MaybeGetString(const Json::Value& message,
                                               const char* first,
                                               const char* last) {
  const Json::Value* value = message.find(first, last);
  std::optional<std::string_view> result;
  if (value && value->isString()) {
    return MaybeGetString(*value);
  }
  return result;
}

}  // namespace openscreen
