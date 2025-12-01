// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

namespace {
bool RunPathExists(Scope* scope, const std::string& path) {
  Err err;
  std::vector<Value> args;
  args.push_back(Value(nullptr, path));

  FunctionCallNode function_call;
  Value result = functions::RunPathExists(scope, &function_call, args, &err);
  EXPECT_FALSE(err.has_error());
  return !err.has_error() && result.boolean_value();
}
}  // namespace

TEST(PathExistsTest, FileExists) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  // Make a real directory for the test.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  std::string data = "foo";
  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::FilePath file_path = dir_path.AppendASCII("foo.txt");
  base::WriteFile(file_path, data.c_str(), static_cast<int>(data.size()));

  EXPECT_TRUE(RunPathExists(setup.scope(), "//"));
  EXPECT_TRUE(RunPathExists(setup.scope(), "//some-dir"));
  EXPECT_TRUE(RunPathExists(setup.scope(), "//some-dir/"));
  EXPECT_TRUE(RunPathExists(setup.scope(), "../some-dir"));
  EXPECT_TRUE(RunPathExists(setup.scope(), "//some-dir/foo.txt"));
  EXPECT_TRUE(RunPathExists(setup.scope(), temp_dir.GetPath().As8Bit()));
  EXPECT_FALSE(RunPathExists(setup.scope(), "//bar"));
  EXPECT_FALSE(RunPathExists(setup.scope(), "bar"));
}

TEST(PathExistsTest, FileExistsInvalidValues) {
  TestWithScope setup;
  FunctionCallNode function_call;

  {
    // No arg.
    Err err;
    std::vector<Value> args;
    functions::RunPathExists(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }
  {
    // Extra arg.
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "a"));
    args.push_back(Value(nullptr, "b"));
    functions::RunPathExists(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }
  {
    // Wrong type.
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, Value::LIST));
    functions::RunPathExists(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }
  {
    // Empty string.
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, ""));
    functions::RunPathExists(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }
}
