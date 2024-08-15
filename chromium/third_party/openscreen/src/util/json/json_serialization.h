// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_SERIALIZATION_H_
#define UTIL_JSON_JSON_SERIALIZATION_H_

#include <string>
#include <string_view>

#include "json/value.h"
#include "platform/base/error.h"

namespace openscreen {

namespace json {

ErrorOr<Json::Value> Parse(std::string_view value);
ErrorOr<std::string> Stringify(const Json::Value& value);

}  // namespace json
}  // namespace openscreen

#endif  // UTIL_JSON_JSON_SERIALIZATION_H_
