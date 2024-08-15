// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_outputs_writer.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/string_escape.h"
#include "gn/builder.h"
#include "gn/commands.h"
#include "gn/filesystem_utils.h"
#include "gn/invoke_python.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"

// NOTE: Intentional macro definition allows compile-time string concatenation.
// (see usage below).
#if defined(OS_WINDOWS)
#define LINE_ENDING "\r\n"
#else
#define LINE_ENDING "\n"
#endif

namespace {

using MapType = NinjaOutputsWriter::MapType;

// Sort the targets according to their human visible labels first.
struct TargetLabelPair {
  TargetLabelPair(const Target* target, const Label& default_toolchain_label)
      : target(target),
        label(std::make_unique<std::string>(
            target->label().GetUserVisibleName(default_toolchain_label))) {}

  const Target* target;
  std::unique_ptr<std::string> label;

  bool operator<(const TargetLabelPair& other) const {
    return *label < *other.label;
  }

  using List = std::vector<TargetLabelPair>;

  // Create list of TargetLabelPairs sorted by their target labels.
  static List CreateSortedList(const MapType& outputs_map,
                               const Label& default_toolchain_label) {
    List result;
    result.reserve(outputs_map.size());

    for (const auto& output_pair : outputs_map)
      result.emplace_back(output_pair.first, default_toolchain_label);

    std::sort(result.begin(), result.end());
    return result;
  }
};

}  // namespace

// static
StringOutputBuffer NinjaOutputsWriter::GenerateJSON(
    const MapType& outputs_map) {
  Label default_toolchain_label;
  if (!outputs_map.empty()) {
    default_toolchain_label =
        outputs_map.begin()->first->settings()->default_toolchain_label();
  }

  auto sorted_pairs =
      TargetLabelPair::CreateSortedList(outputs_map, default_toolchain_label);

  StringOutputBuffer out;
  out.Append('{');

  auto escape = [](std::string_view str) -> std::string {
    std::string result;
    base::EscapeJSONString(str, true, &result);
    return result;
  };

  bool first_label = true;
  for (const auto& pair : sorted_pairs) {
    const Target* target = pair.target;
    const std::string& label = *pair.label;

    auto it = outputs_map.find(target);
    CHECK(it != outputs_map.end());

    if (!first_label)
      out.Append(',');
    first_label = false;

    out.Append("\n  ");
    out.Append(escape(label));
    out.Append(": [");
    bool first_path = true;
    for (const auto& output : it->second) {
      if (!first_path)
        out.Append(',');
      first_path = false;
      out.Append("\n    ");
      out.Append(escape(output.value()));
    }
    out.Append("\n  ]");
  }

  out.Append("\n}");
  return out;
}

bool NinjaOutputsWriter::RunAndWriteFiles(
    const MapType& outputs_map,
    const BuildSettings* build_settings,
    const std::string& file_name,
    const std::string& exec_script,
    const std::string& exec_script_extra_args,
    bool quiet,
    Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null()) {
    return false;
  }

  StringOutputBuffer outputs = GenerateJSON(outputs_map);

  base::FilePath output_path = build_settings->GetFullPath(output_file);
  if (!outputs.ContentsEqual(output_path)) {
    if (!outputs.WriteToFile(output_path, err)) {
      return false;
    }

    if (!exec_script.empty()) {
      SourceFile script_file;
      if (exec_script[0] != '/') {
        // Relative path, assume the base is in build_dir.
        script_file = build_settings->build_dir().ResolveRelativeFile(
            Value(nullptr, exec_script), err);
        if (script_file.is_null()) {
          return false;
        }
      } else {
        script_file = SourceFile(exec_script);
      }
      base::FilePath script_path = build_settings->GetFullPath(script_file);
      return internal::InvokePython(build_settings, script_path,
                                    exec_script_extra_args, output_path, quiet,
                                    err);
    }
  }

  return true;
}
