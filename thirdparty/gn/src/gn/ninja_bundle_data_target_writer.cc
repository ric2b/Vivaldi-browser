// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_bundle_data_target_writer.h"

#include "gn/output_file.h"
#include "gn/settings.h"
#include "gn/target.h"

NinjaBundleDataTargetWriter::NinjaBundleDataTargetWriter(const Target* target,
                                                         std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaBundleDataTargetWriter::~NinjaBundleDataTargetWriter() = default;

void NinjaBundleDataTargetWriter::Run() {
  std::vector<OutputFile> output_files;
  for (const SourceFile& source_file : target_->sources()) {
    output_files.push_back(
        OutputFile(settings_->build_settings(), source_file));
  }

  std::vector<OutputFile> input_deps = WriteInputDepsStampOrPhonyAndGetDep(
      std::vector<const Target*>(), /*num_output_uses=*/1);
  output_files.insert(output_files.end(), input_deps.begin(), input_deps.end());

  std::vector<OutputFile> order_only_deps;
  for (const Target* data_dep : resolved().GetDataDeps(target_)) {
    if (data_dep->has_dependency_output())
      order_only_deps.push_back(data_dep->dependency_output());
  }

  WriteStampOrPhonyForTarget(output_files, order_only_deps);
}
