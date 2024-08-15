// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(LabelMatchesTest, MatchesSubTarget) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//foo:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), true);
}

TEST(LabelMatchesTest, MatchesTargetInFile) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//bar:foo"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), true);
}

TEST(LabelMatchesTest, NoMatch) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//baz/foo:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), false);
}

TEST(LabelMatchesTest, LabelMustBeString) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, true));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "First argument must be a target label.");
}

TEST(LabelMatchesTest, PatternsMustBeList) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//baz/foo:bar"));

  Value patterns(nullptr, true);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "Second argument must be a list of label patterns.");
}

TEST(LabelMatchesTest, PatternsMustBeListOfStrings) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//baz/foo:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, true));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "Second argument must be a list of label patterns.");
}
