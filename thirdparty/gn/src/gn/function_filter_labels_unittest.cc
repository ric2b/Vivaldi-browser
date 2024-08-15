// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(FilterLabelsTest, OneIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(1u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//foo:bar", result.list_value()[0].string_value());
}

TEST(FilterLabelsTest, TwoIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(2u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//foo:bar", result.list_value()[0].string_value());
  ASSERT_TRUE(result.list_value()[1].type() == Value::STRING);
  ASSERT_EQ("//bar", result.list_value()[1].string_value());
}

TEST(FilterLabelsTest, NoneIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//fooz/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(0u, result.list_value().size());
}

TEST(FilterLabelsTest, OneExcluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsExclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(1u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//baz:bar", result.list_value()[0].string_value());
}

TEST(FilterLabelsTest, TwoExcluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsExclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(1u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//baz:bar", result.list_value()[0].string_value());
}

TEST(FilterLabelsTest, NoneExcluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//fooz/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsExclude(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(2u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//foo:bar", result.list_value()[0].string_value());
  ASSERT_TRUE(result.list_value()[1].type() == Value::STRING);
  ASSERT_EQ("//baz:bar", result.list_value()[1].string_value());
}

TEST(FilterLabelsTest, LabelsIsList) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, true);

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "First argument must be a list of target labels.");
}

TEST(FilterLabelsTest, PatternsIsList) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, true);

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "Second argument must be a list of label patterns.");
}

TEST(FilterLabelsTest, LabelsAreLabels) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, true));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "First argument must be a list of target labels.");
}

TEST(FilterLabelsTest, PatternsArePatterns) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*:foo"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabelsInclude(setup.scope(), &function, args, &err);
  ASSERT_TRUE(err.has_error());
  ASSERT_EQ(err.message(), "Invalid label pattern.");
}