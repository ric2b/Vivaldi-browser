// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

TEST(LenTest, StringLen) {
  TestWithScope setup;
  FunctionCallNode function_call;
  Err err;
  std::vector<Value> args;
  args.push_back(Value(nullptr, "foo"));
  Value result = functions::RunLen(setup.scope(), &function_call, args, &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(result.type(), Value::INTEGER);
  EXPECT_EQ(result.int_value(), 3);
}

TEST(LenTest, ListLen) {
  TestWithScope setup;
  FunctionCallNode function_call;
  Err err;
  std::vector<Value> args;
  Value list_value(nullptr, Value::LIST);
  list_value.list_value().push_back(Value(nullptr, "a"));
  list_value.list_value().push_back(Value(nullptr, "b"));
  args.push_back(list_value);
  Value result = functions::RunLen(setup.scope(), &function_call, args, &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(result.type(), Value::INTEGER);
  EXPECT_EQ(result.int_value(), 2);
}

TEST(LenTest, InvalidType) {
  TestWithScope setup;
  FunctionCallNode function_call;
  Err err;
  std::vector<Value> args;
  args.push_back(Value(nullptr, static_cast<int64_t>(123)));
  functions::RunLen(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(LenTest, WrongArugmentCount) {
  TestWithScope setup;
  FunctionCallNode function_call;
  Err err;
  std::vector<Value> args;
  // No Args:
  functions::RunLen(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(err.has_error());

  // Two Args:
  args.push_back(Value(nullptr, "a"));
  args.push_back(Value(nullptr, "b"));
  functions::RunLen(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(err.has_error());
}
