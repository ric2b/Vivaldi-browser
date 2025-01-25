// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "gn/ninja_copy_target_writer.h"
#include "gn/target.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

// Tests multiple files with an output pattern and no toolchain dependency.
TEST(NinjaCopyTargetWriter, Run) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt\n"
      "build input2.out: copy ../../foo/input2.txt\n"
      "\n"
      "build phony/foo/bar: phony input1.out input2.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

// Tests a single file with no output pattern.
TEST(NinjaCopyTargetWriter, ToolchainDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/output.out");

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build output.out: copy ../../foo/input1.txt\n"
      "\n"
      "build phony/foo/bar: phony output.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

TEST(NinjaCopyTargetWriter, OrderOnlyDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);
  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");
  target.config_values().inputs().push_back(SourceFile("//foo/script.py"));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt || ../../foo/script.py\n"
      "\n"
      "build phony/foo/bar: phony input1.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

TEST(NinjaCopyTargetWriter, DataDeps) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);
  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  Target data_dep(setup.settings(), Label(SourceDir("//foo/"), "datadep"));
  data_dep.set_output_type(Target::ACTION);
  data_dep.visibility().SetPublic();
  data_dep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(data_dep.OnResolved(&err));

  target.data_deps().push_back(LabelTargetPair(&data_dep));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt || phony/foo/datadep\n"
      "\n"
      "build phony/foo/bar: phony input1.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}
