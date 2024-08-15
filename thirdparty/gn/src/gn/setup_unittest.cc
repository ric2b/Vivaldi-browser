// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/setup.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/builder_record.h"
#include "gn/filesystem_utils.h"
#include "gn/switches.h"
#include "gn/test_with_scheduler.h"
#include "util/build_config.h"

using SetupTest = TestWithScheduler;

static void WriteFile(const base::FilePath& file, const std::string& data) {
  CHECK_EQ(static_cast<int>(data.size()),  // Way smaller than INT_MAX.
           base::WriteFile(file, data.data(), data.size()));
}

TEST_F(SetupTest, DotGNFileIsGenDep) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  // Create a temp directory containing a .gn file and a BUILDCONFIG.gn file,
  // pass it as --root.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();
  base::FilePath dot_gn_name = in_path.Append(FILE_PATH_LITERAL(".gn"));
  WriteFile(dot_gn_name, "buildconfig = \"//BUILDCONFIG.gn\"\n");
  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  cmdline.AppendSwitchPath(switches::kRoot, in_path);

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  EXPECT_TRUE(
      setup.DoSetup(FilePathToUTF8(build_temp_dir.GetPath()), true, cmdline));
  std::vector<base::FilePath> gen_deps = g_scheduler->GetGenDependencies();
  ASSERT_EQ(1u, gen_deps.size());
  EXPECT_EQ(gen_deps[0], base::MakeAbsoluteFilePath(dot_gn_name));
}

TEST_F(SetupTest, EmptyScriptExecutableDoesNotGenerateError) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
script_executable = ""
)";

  // Create a temp directory containing a .gn file and a BUILDCONFIG.gn file,
  // pass it as --root.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();
  base::FilePath dot_gn_name = in_path.Append(FILE_PATH_LITERAL(".gn"));
  WriteFile(dot_gn_name, kDotfileContents);

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  cmdline.AppendSwitchPath(switches::kRoot, in_path);

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_TRUE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                   true, cmdline, &err));
}

#if defined(OS_WIN)
TEST_F(SetupTest, MissingScriptExeGeneratesSetupErrorOnWindows) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
script_executable = "this_does_not_exist"
)";

  // Create a temp directory containing a .gn file and a BUILDCONFIG.gn file,
  // pass it as --root.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();
  base::FilePath dot_gn_name = in_path.Append(FILE_PATH_LITERAL(".gn"));
  WriteFile(dot_gn_name, kDotfileContents);

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  cmdline.AppendSwitchPath(switches::kRoot, in_path);

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_FALSE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                    true, cmdline, &err));
  EXPECT_TRUE(err.has_error());
}
#endif  // defined(OS_WIN)

static void RunExtensionCheckTest(std::string extension,
                                  bool success,
                                  const std::string& expected_error_message) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  // Create a temp directory containing a .gn file and a BUILDCONFIG.gn file,
  // pass it as --root.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();
  base::FilePath dot_gn_name = in_path.Append(FILE_PATH_LITERAL(".gn"));
  WriteFile(dot_gn_name,
            "buildconfig = \"//BUILDCONFIG.gn\"\n\
      build_file_extension = \"" +
                extension + "\"");
  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  cmdline.AppendSwitchPath(switches::kRoot, in_path);

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that its status.
  Setup setup;
  Err err;
  EXPECT_EQ(success,
            setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()), true,
                                 cmdline, &err));
  EXPECT_EQ(success, !err.has_error());
}

TEST_F(SetupTest, NoSeparatorInExtension) {
  RunExtensionCheckTest(
      "hello" + std::string(1, base::FilePath::kSeparators[0]) + "world", false,
#if defined(OS_WIN)
      "Build file extension 'hello\\world' cannot contain a path separator"
#else
      "Build file extension 'hello/world' cannot contain a path separator"
#endif
  );
}

TEST_F(SetupTest, Extension) {
  RunExtensionCheckTest("yay", true, "");
}

TEST_F(SetupTest, AddExportCompileCommands) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  // Provide a project default export compile command list.
  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
export_compile_commands = [ "//base/*" ]
)";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();
  base::FilePath dot_gn_name = in_path.Append(FILE_PATH_LITERAL(".gn"));
  WriteFile(dot_gn_name, kDotfileContents);

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  // Two additions to the compile commands list.
  cmdline.AppendSwitch(switches::kAddExportCompileCommands,
                       "//tools:doom_melon");
  cmdline.AppendSwitch(switches::kAddExportCompileCommands, "//src/gn:*");

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_TRUE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                   true, cmdline, &err));

  // The export compile commands should have three items.
  const std::vector<LabelPattern>& export_cc = setup.export_compile_commands();
  ASSERT_EQ(3u, export_cc.size());
  EXPECT_EQ("//base/*", export_cc[0].Describe());
  EXPECT_EQ("//tools:doom_melon", export_cc[1].Describe());
  EXPECT_EQ("//src/gn:*", export_cc[2].Describe());
}

TEST_F(SetupTest, RootPatternsInGnConfig) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  // Provide a default root pattern for all top-level targets from //BUILD.gn
  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
root_patterns = [ "//:*" ]
)";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  WriteFile(in_path.Append(FILE_PATH_LITERAL(".gn")), kDotfileContents);

  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_TRUE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                   true, cmdline, &err));

  const std::vector<LabelPattern>& root_patterns =
      setup.build_settings().root_patterns();
  ASSERT_EQ(1u, root_patterns.size());
  EXPECT_EQ("//.:*", root_patterns[0].Describe());
}

TEST_F(SetupTest, RootPatternsOnCommandLineOverrideGnConfig) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  // Provide a default root pattern for only //:foo
  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
root_patterns = [ "//:foo" ]
)";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")), "");
  WriteFile(in_path.Append(FILE_PATH_LITERAL(".gn")), kDotfileContents);

  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  // Override the default root pattern list.
  cmdline.AppendSwitch(switches::kRootPattern, "//:bar");
  cmdline.AppendSwitch(switches::kRootPattern, "//:qux");

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_TRUE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                   true, cmdline, &err));

  const std::vector<LabelPattern>& root_patterns =
      setup.build_settings().root_patterns();
  ASSERT_EQ(2u, root_patterns.size());
  EXPECT_EQ("//.:bar", root_patterns[0].Describe());
  EXPECT_EQ("//.:qux", root_patterns[1].Describe());
}

TEST_F(SetupTest, RootPatternsFiltersPatterns) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
root_patterns = [ "//:foo" ]
)";

  const char kBuildConfigContents[] = R"(
set_default_toolchain("//:toolchain")
)";

  const char kBuildGnContents[] = R"(
group("foo") {
  deps = [ ":bar" ]
}

group("bar") {
}

group("zoo") {
}

group("qux") {
}

# Minimal default toolchain definition for this test. Non-functional.
toolchain("toolchain") {
  tool("stamp") {
    command = "stamp"
  }
}
)";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILD.gn")), kBuildGnContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")),
            kBuildConfigContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL(".gn")), kDotfileContents);

  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup and check that the .gn file is in the scheduler's gen deps.
  Setup setup;
  Err err;
  EXPECT_TRUE(setup.DoSetupWithErr(FilePathToUTF8(build_temp_dir.GetPath()),
                                   true, cmdline, &err));

  const std::vector<LabelPattern>& root_patterns =
      setup.build_settings().root_patterns();
  ASSERT_EQ(1u, root_patterns.size());
  EXPECT_EQ("//.:foo", root_patterns[0].Describe());

  // Now build the graph, then verify it only includes //:foo and //:bar
  ASSERT_TRUE(setup.Run(cmdline));

  SourceDir top_dir("//");

  const BuilderRecord* foo_record =
      setup.builder().GetRecord(Label(top_dir, "foo", top_dir, "toolchain"));
  const BuilderRecord* bar_record =
      setup.builder().GetRecord(Label(top_dir, "bar", top_dir, "toolchain"));
  const BuilderRecord* qux_record =
      setup.builder().GetRecord(Label(top_dir, "qux", top_dir, "toolchain"));
  const BuilderRecord* zoo_record =
      setup.builder().GetRecord(Label(top_dir, "zoo", top_dir, "toolchain"));

  // All four targets were added as build graph records.
  ASSERT_TRUE(foo_record);
  ASSERT_TRUE(bar_record);
  ASSERT_TRUE(zoo_record);
  ASSERT_TRUE(qux_record);

  // But only foo and bar should be generated in the Ninja plan.
  EXPECT_TRUE(foo_record->should_generate());
  EXPECT_TRUE(bar_record->should_generate());
  EXPECT_FALSE(qux_record->should_generate());
  EXPECT_FALSE(zoo_record->should_generate());
}
