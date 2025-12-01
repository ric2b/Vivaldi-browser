// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_
#define TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_

#include <string>
#include <vector>

#include "gn/output_file.h"

class ResolvedTargetData;
class SourceFile;
class Target;

struct ClangModuleDep {
  ClangModuleDep(const SourceFile* modulemap,
                 const std::string& module_name,
                 const OutputFile& pcm,
                 bool is_self);

  // The input module.modulemap source file.
  const SourceFile* modulemap;

  // The internal module name, in GN this is the target's label.
  std::string module_name;

  // The compiled version of the module.
  OutputFile pcm;

  // Is this the module for the current target.
  bool is_self;
};

// Gathers information about all module dependencies for a given target.
std::vector<ClangModuleDep> GetModuleDepsInformation(
    const Target* target,
    const ResolvedTargetData& resolved);

#endif  // TOOLS_GN_NINJA_MODULE_WRITER_UTIL_H_
