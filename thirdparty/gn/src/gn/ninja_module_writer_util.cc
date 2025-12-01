// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_module_writer_util.h"

#include <algorithm>
#include <set>

#include "gn/resolved_target_data.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

namespace {

// Returns the first source file in the target's sources that is a modulemap
// file. Returns nullptr if no modulemap file is found.
const SourceFile* GetModuleMapFromTargetSources(const Target* target) {
  for (const SourceFile& sf : target->sources()) {
    if (sf.IsModuleMapType())
      return &sf;
  }
  return nullptr;
}

}  // namespace

ClangModuleDep::ClangModuleDep(const SourceFile* modulemap,
                               const std::string& module_name,
                               const OutputFile& pcm,
                               bool is_self)
    : modulemap(modulemap),
      module_name(module_name),
      pcm(pcm),
      is_self(is_self) {}

std::vector<ClangModuleDep> GetModuleDepsInformation(
    const Target* target,
    const ResolvedTargetData& resolved) {
  std::vector<ClangModuleDep> ret;
  // Use a set to keep track of added PCM files to ensure uniqueness.
  std::set<OutputFile> added_pcms;

  auto add_if_new = [&added_pcms, &ret](const Target* t, bool is_self) {
    const SourceFile* modulemap = GetModuleMapFromTargetSources(t);
    if (!modulemap)  // Not a module or no .modulemap file.
      return;

    std::string label;
    CHECK(SubstitutionWriter::GetTargetSubstitution(
        t, &SubstitutionLabelNoToolchain, &label));

    const char* tool_type;
    std::vector<OutputFile> modulemap_outputs;
    CHECK(
        t->GetOutputFilesForSource(*modulemap, &tool_type, &modulemap_outputs));
    CHECK(modulemap_outputs.size() == 1u);  // Must be only one .pcm.
    const OutputFile& pcm_file = modulemap_outputs[0];

    if (added_pcms.insert(pcm_file).second) {
      ret.emplace_back(modulemap, label, pcm_file, is_self);
    }
  };

  if (target->source_types_used().Get(SourceFile::SOURCE_MODULEMAP))
    add_if_new(target, true);

  for (const auto& pair : resolved.GetModuleDepsInformation(target))
    add_if_new(pair.target(), false);

  // Sort by pcm path for deterministic output.
  std::sort(ret.begin(), ret.end(),
            [](const ClangModuleDep& a, const ClangModuleDep& b) {
              return a.pcm < b.pcm;
            });

  return ret;
}
