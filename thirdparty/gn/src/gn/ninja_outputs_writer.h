// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_OUTPUTS_WRITER_H_
#define TOOLS_GN_NINJA_OUTPUTS_WRITER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "gn/err.h"
#include "gn/output_file.h"
#include "gn/string_output_buffer.h"
#include "gn/target.h"

class Builder;
class BuildSettings;
class StringOutputBuffer;

// Generates the --ninja-outputs-file content
class NinjaOutputsWriter {
 public:
  // A map from targets to list of corresponding Ninja output paths.
  using MapType = std::unordered_map<const Target*, std::vector<OutputFile>>;

  static bool RunAndWriteFiles(const MapType& outputs_map,
                               const BuildSettings* build_setting,
                               const std::string& file_name,
                               const std::string& exec_script,
                               const std::string& exec_script_extra_args,
                               bool quiet,
                               Err* err);

 private:
  FRIEND_TEST_ALL_PREFIXES(NinjaOutputsWriterTest, OutputsFile);

  static StringOutputBuffer GenerateJSON(const MapType& outputs_map);
};

#endif
