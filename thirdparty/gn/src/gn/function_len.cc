// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "gn/err.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/value.h"

namespace functions {

Value RunLen(Scope* scope,
             const FunctionCallNode* function,
             const std::vector<Value>& args,
             Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Expecting exactly one argument.");
    return Value();
  }

  const Value& value = args[0];
  if (value.type() == Value::STRING) {
    return Value(function, static_cast<int64_t>(value.string_value().size()));
  }

  if (value.type() == Value::LIST) {
    return Value(function, static_cast<int64_t>(value.list_value().size()));
  }

  *err = Err(
      value.origin(), "len() expects a string or a list.",
      "Got " + std::string(Value::DescribeType(value.type())) + " instead.");
  return Value();
}

const char kLen[] = "len";
const char kLen_HelpShort[] = "len: Returns the length of a string or a list.";
const char kLen_Help[] =
    R"(len: Returns the length of a string or a list.

  len(item)

  The argument can be a string or a list.

Examples:
  len("foo")  # 3
  len([ "a", "b", "c" ])  # 3
)";

}  // namespace functions
