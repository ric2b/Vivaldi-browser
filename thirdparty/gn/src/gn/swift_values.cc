// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/swift_values.h"

#include <algorithm>

#include "gn/deps_iterator.h"
#include "gn/err.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/tool.h"

SwiftValues::SwiftValues() = default;

SwiftValues::~SwiftValues() = default;

// static
bool SwiftValues::OnTargetResolved(Target* target, Err* err) {
  if (!target->builds_swift_module())
    return true;

  return target->swift_values().FillModuleOutputFile(target, err);
}

const Tool* SwiftValues::GetTool(const Target* target) const {
  DCHECK(target->builds_swift_module());
  return target->toolchain()->GetToolForSourceType(SourceFile::SOURCE_SWIFT);
}

void SwiftValues::GetOutputs(const Target* target,
                             std::vector<OutputFile>* result) const {
  const Tool* tool = GetTool(target);

  // Expand tool's outputs().
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(target, tool,
                                                    tool->outputs(), result);

  // Expand tool's partial_outputs() for each .swift source file.
  for (const SourceFile& source : target->sources()) {
    if (!source.IsSwiftType()) {
      continue;
    }

    SubstitutionWriter::ApplyListToCompilerAsOutputFile(
        target, source, tool->partial_outputs(), result);
  }
}

void SwiftValues::GetOutputsAsSourceFiles(
    const Target* target,
    std::vector<SourceFile>* result) const {
  std::vector<OutputFile> outputs;
  GetOutputs(target, &outputs);

  result->reserve(outputs.size());

  const BuildSettings* build_settings = target->settings()->build_settings();
  for (const OutputFile& output : outputs) {
    result->push_back(output.AsSourceFile(build_settings));
  }
}

bool SwiftValues::FillModuleOutputFile(Target* target, Err* err) {
  std::vector<OutputFile> outputs;
  GetOutputs(target, &outputs);

  // Keep only .swiftmodule output files.
  const BuildSettings* build_settings = target->settings()->build_settings();
  const auto iter = std::remove_if(
      outputs.begin(), outputs.end(),
      [build_settings](const OutputFile& output) -> bool {
        SourceFile output_as_source_file = output.AsSourceFile(build_settings);
        return !output_as_source_file.IsSwiftModuleType();
      });
  outputs.erase(iter, outputs.end());

  // A target should generate exactly one .swiftmodule file.
  if (outputs.size() != 1) {
    const Tool* tool = GetTool(target);
    *err = Err(tool->defined_from(), "Incorrect outputs for tool",
               "The outputs of tool " + std::string(tool->name()) +
                   " must list exactly one .swiftmodule file");
    return false;
  }

  module_output_file_ = outputs.front();
  module_output_dir_ =
      module_output_file_.AsSourceFile(build_settings).GetDir();

  return true;
}
