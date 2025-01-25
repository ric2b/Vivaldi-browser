// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_outputs_writer.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/builder_record.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_writer.h"
#include "gn/setup.h"
#include "gn/switches.h"
#include "gn/test_with_scheduler.h"
#include "util/test/test.h"

using NinjaOutputsWriterTest = TestWithScheduler;
using NinjaOutputsMap = NinjaOutputsWriter::MapType;

static void WriteFile(const base::FilePath& file, const std::string& data) {
  CHECK_EQ(static_cast<int>(data.size()),  // Way smaller than INT_MAX.
           base::WriteFile(file, data.data(), data.size()));
}

// Collects Ninja outputs for each target. Used by multiple background threads.
struct TargetWriteInfo {
  std::mutex lock;
  NinjaOutputsMap ninja_outputs_map;
};

// Called on worker thread to write the ninja file.
void BackgroundDoWrite(TargetWriteInfo* write_info, const Target* target) {
  std::vector<OutputFile> target_ninja_outputs;
  std::string rule = NinjaTargetWriter::RunAndWriteFile(target, nullptr,
                                                        &target_ninja_outputs);

  std::lock_guard<std::mutex> lock(write_info->lock);
  write_info->ninja_outputs_map.emplace(target,
                                        std::move(target_ninja_outputs));
}

static void ItemResolvedAndGeneratedCallback(TargetWriteInfo* write_info,
                                             const BuilderRecord* record) {
  const Item* item = record->item();
  const Target* target = item->AsTarget();
  if (target) {
    g_scheduler->ScheduleWork(
        [write_info, target]() { BackgroundDoWrite(write_info, target); });
  }
}

TEST_F(NinjaOutputsWriterTest, OutputsFile) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  const char kDotfileContents[] = R"(
buildconfig = "//BUILDCONFIG.gn"
)";

  const char kBuildConfigContents[] = R"(
set_default_toolchain("//toolchain:default")
)";

  const char kToolchainBuildContents[] = R"##(
toolchain("default") {
  tool("stamp") {
    command = "stamp"
  }
}

toolchain("secondary") {
  tool("stamp") {
    command = "stamp2"
  }
}
)##";

  const char kBuildGnContents[] = R"##(
group("foo") {
  deps = [ ":bar", ":zoo(//toolchain:secondary)" ]
}

action("bar") {
  script = "//:run_bar_script.py"
  outputs = [ "$root_build_dir/bar.output" ]
  args = []
}

group("zoo") {
}
)##";

  // Create a temp directory containing the build.
  base::ScopedTempDir in_temp_dir;
  ASSERT_TRUE(in_temp_dir.CreateUniqueTempDir());
  base::FilePath in_path = in_temp_dir.GetPath();

  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILD.gn")), kBuildGnContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL("BUILDCONFIG.gn")),
            kBuildConfigContents);
  WriteFile(in_path.Append(FILE_PATH_LITERAL(".gn")), kDotfileContents);

  EXPECT_TRUE(
      base::CreateDirectory(in_path.Append(FILE_PATH_LITERAL("toolchain"))));

  WriteFile(in_path.Append(FILE_PATH_LITERAL("toolchain/BUILD.gn")),
            kToolchainBuildContents);

  cmdline.AppendSwitch(switches::kRoot, FilePathToUTF8(in_path));

  base::FilePath outputs_json_path(FILE_PATH_LITERAL("ninja_outputs.json"));
  cmdline.AppendSwitch("--ninja-outputs-file",
                       FilePathToUTF8(outputs_json_path));

  // Create another temp dir for writing the generated files to.
  base::ScopedTempDir build_temp_dir;
  ASSERT_TRUE(build_temp_dir.CreateUniqueTempDir());

  // Run setup
  Setup setup;
  EXPECT_TRUE(
      setup.DoSetup(FilePathToUTF8(build_temp_dir.GetPath()), true, cmdline));

  TargetWriteInfo write_info;

  setup.builder().set_resolved_and_generated_callback(
      [&write_info](const BuilderRecord* record) {
        ItemResolvedAndGeneratedCallback(&write_info, record);
      });

  // Do the actual load.
  ASSERT_TRUE(setup.Run());

  StringOutputBuffer out =
      NinjaOutputsWriter::GenerateJSON(write_info.ninja_outputs_map);

  // Verify that the generated file is here.
  std::string generated = out.str();
  std::string expected = R"##({
  "//:bar": [
    "bar.output",
    "phony/bar"
  ],
  "//:foo": [
    "phony/foo"
  ],
  "//:zoo": [
  ],
  "//:zoo(//toolchain:secondary)": [
  ]
})##";

  EXPECT_EQ(generated, expected) << generated << "\n" << expected;
}
