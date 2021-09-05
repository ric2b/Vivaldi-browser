// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/json_project_writer.h"
#include "base/strings/string_util.h"
#include "gn/substitution_list.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

using JSONWriter = TestWithScheduler;

TEST_F(JSONWriter, ActionWithResponseFile) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::ACTION);

  target.sources().push_back(SourceFile("//foo/source1.txt"));
  target.config_values().inputs().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().set_script(SourceFile("//foo/script.py"));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  // Make sure we get interesting substitutions for both the args and the
  // response file contents.
  target.action_values().args() =
      SubstitutionList::MakeForTest("{{response_file_name}}");
  target.action_values().rsp_file_contents() =
      SubstitutionList::MakeForTest("-j", "3");
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/output1.out");

  setup.build_settings()->set_python_path(
      base::FilePath(FILE_PATH_LITERAL("/usr/bin/python")));
  std::vector<const Target*> targets;
  targets.push_back(&target);
#if defined(OS_WIN)
  base::FilePath root_path =
      base::FilePath(FILE_PATH_LITERAL("c:/path/to/src"));
#else
  base::FilePath root_path = base::FilePath(FILE_PATH_LITERAL("/path/to/src"));
#endif
  setup.build_settings()->SetRootPath(root_path);
  g_scheduler->AddGenDependency(root_path.Append(FILE_PATH_LITERAL(".gn")));
  g_scheduler->AddGenDependency(
      root_path.Append(FILE_PATH_LITERAL("BUILD.gn")));
  g_scheduler->AddGenDependency(
      root_path.Append(FILE_PATH_LITERAL("build/BUILD.gn")));
  std::string out =
      JSONProjectWriter::RenderJSON(setup.build_settings(), targets);
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "   \"build_settings\": {\n"
      "      \"build_dir\": \"//out/Debug/\",\n"
      "      \"default_toolchain\": \"//toolchain:default\",\n"
      "      \"gen_input_files\": [ \"//.gn\", \"//BUILD.gn\", "
      "\"//build/BUILD.gn\" ],\n"
#if defined(OS_WIN)
      "      \"root_path\": \"c:/path/to/src\"\n"
#else
      "      \"root_path\": \"/path/to/src\"\n"
#endif
      "   },\n"
      "   \"targets\": {\n"
      "      \"//foo:bar()\": {\n"
      "         \"args\": [ \"{{response_file_name}}\" ],\n"
      "         \"deps\": [  ],\n"
      "         \"inputs\": [ \"//foo/input1.txt\" ],\n"
      "         \"metadata\": {\n"
      "\n"
      "         },\n"
      "         \"outputs\": [ \"//out/Debug/output1.out\" ],\n"
      "         \"public\": \"*\",\n"
      "         \"response_file_contents\": [ \"-j\", \"3\" ],\n"
      "         \"script\": \"//foo/script.py\",\n"
      "         \"sources\": [ \"//foo/source1.txt\" ],\n"
      "         \"testonly\": false,\n"
      "         \"toolchain\": \"\",\n"
      "         \"type\": \"action\",\n"
      "         \"visibility\": [  ]\n"
      "      }\n"
      "   }\n"
      "}\n";
  EXPECT_EQ(expected_json, out) << out;
}

TEST_F(JSONWriter, RustTarget) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::RUST_LIBRARY);
  target.visibility().SetPublic();
  SourceFile lib("//foo/lib.rs");
  target.sources().push_back(lib);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(lib);
  target.rust_values().crate_name() = "foo";
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::vector<const Target*> targets;
  targets.push_back(&target);
  std::string out =
      JSONProjectWriter::RenderJSON(setup.build_settings(), targets);
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "   \"build_settings\": {\n"
      "      \"build_dir\": \"//out/Debug/\",\n"
      "      \"default_toolchain\": \"//toolchain:default\",\n"
      "      \"gen_input_files\": [  ],\n"
      "      \"root_path\": \"\"\n"
      "   },\n"
      "   \"targets\": {\n"
      "      \"//foo:bar()\": {\n"
      "         \"allow_circular_includes_from\": [  ],\n"
      "         \"check_includes\": true,\n"
      "         \"crate_name\": \"foo\",\n"
      "         \"crate_root\": \"//foo/lib.rs\",\n"
      "         \"deps\": [  ],\n"
      "         \"externs\": {\n"
      "\n"
      "         },\n"
      "         \"metadata\": {\n"
      "\n"
      "         },\n"
      "         \"outputs\": [ \"//out/Debug/obj/foo/libbar.rlib\" ],\n"
      "         \"public\": \"*\",\n"
      "         \"sources\": [ \"//foo/lib.rs\" ],\n"
      "         \"testonly\": false,\n"
      "         \"toolchain\": \"\",\n"
      "         \"type\": \"rust_library\",\n"
      "         \"visibility\": [ \"*\" ]\n"
      "      }\n"
      "   }\n"
      "}\n";
  EXPECT_EQ(expected_json, out);
}

TEST_F(JSONWriter, ForEachWithResponseFile) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::ACTION_FOREACH);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.action_values().set_script(SourceFile("//foo/script.py"));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  // Make sure we get interesting substitutions for both the args and the
  // response file contents.
  target.action_values().args() = SubstitutionList::MakeForTest(
      "{{source}}", "{{source_file_part}}", "{{response_file_name}}");
  target.action_values().rsp_file_contents() =
      SubstitutionList::MakeForTest("-j", "{{source_name_part}}");
  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  setup.build_settings()->set_python_path(
      base::FilePath(FILE_PATH_LITERAL("/usr/bin/python")));
  std::vector<const Target*> targets;
  targets.push_back(&target);
#if defined(OS_WIN)
  base::FilePath root_path =
      base::FilePath(FILE_PATH_LITERAL("c:/path/to/src"));
#else
  base::FilePath root_path = base::FilePath(FILE_PATH_LITERAL("/path/to/src"));
#endif
  setup.build_settings()->SetRootPath(root_path);
  g_scheduler->AddGenDependency(root_path.Append(FILE_PATH_LITERAL(".gn")));
  g_scheduler->AddGenDependency(
      root_path.Append(FILE_PATH_LITERAL("BUILD.gn")));
  std::string out =
      JSONProjectWriter::RenderJSON(setup.build_settings(), targets);
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "   \"build_settings\": {\n"
      "      \"build_dir\": \"//out/Debug/\",\n"
      "      \"default_toolchain\": \"//toolchain:default\",\n"
      "      \"gen_input_files\": [ \"//.gn\", \"//BUILD.gn\" ],\n"
#if defined(OS_WIN)
      "      \"root_path\": \"c:/path/to/src\"\n"
#else
      "      \"root_path\": \"/path/to/src\"\n"
#endif
      "   },\n"
      "   \"targets\": {\n"
      "      \"//foo:bar()\": {\n"
      "         \"args\": [ \"{{source}}\", \"{{source_file_part}}\", "
      "\"{{response_file_name}}\" ],\n"
      "         \"deps\": [  ],\n"
      "         \"metadata\": {\n"
      "\n"
      "         },\n"
      "         \"output_patterns\": [ "
      "\"//out/Debug/{{source_name_part}}.out\" ],\n"
      "         \"outputs\": [ \"//out/Debug/input1.out\" ],\n"
      "         \"public\": \"*\",\n"
      "         \"response_file_contents\": [ \"-j\", \"{{source_name_part}}\" "
      "],\n"
      "         \"script\": \"//foo/script.py\",\n"
      "         \"source_outputs\": {\n"
      "            \"//foo/input1.txt\": [ \"input1.out\" ]\n"
      "         },\n"
      "         \"sources\": [ \"//foo/input1.txt\" ],\n"
      "         \"testonly\": false,\n"
      "         \"toolchain\": \"\",\n"
      "         \"type\": \"action_foreach\",\n"
      "         \"visibility\": [  ]\n"
      "      }\n"
      "   }\n"
      "}\n";
  EXPECT_EQ(expected_json, out);
}
