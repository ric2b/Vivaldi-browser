// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/c_substitution_type.h"

#include <stddef.h>
#include <stdlib.h>

#include "gn/err.h"

const SubstitutionTypes CSubstitutions = {
    &CSubstitutionAsmFlags,     &CSubstitutionCFlags,
    &CSubstitutionCFlagsC,      &CSubstitutionCFlagsCc,
    &CSubstitutionCFlagsObjC,   &CSubstitutionCFlagsObjCc,
    &CSubstitutionDefines,      &CSubstitutionFrameworkDirs,
    &CSubstitutionIncludeDirs,

    &CSubstitutionLinkerInputs, &CSubstitutionLinkerInputsNewline,
    &CSubstitutionLdFlags,      &CSubstitutionLibs,
    &CSubstitutionSoLibs,       &CSubstitutionFrameworks,

    &CSubstitutionArFlags,
};

// Valid for compiler tools.
const Substitution CSubstitutionAsmFlags = {"{{asmflags}}", "asmflags"};
const Substitution CSubstitutionCFlags = {"{{cflags}}", "cflags"};
const Substitution CSubstitutionCFlagsC = {"{{cflags_c}}", "cflags_c"};
const Substitution CSubstitutionCFlagsCc = {"{{cflags_cc}}", "cflags_cc"};
const Substitution CSubstitutionCFlagsObjC = {"{{cflags_objc}}", "cflags_objc"};
const Substitution CSubstitutionCFlagsObjCc = {"{{cflags_objcc}}",
                                              "cflags_objcc"};
const Substitution CSubstitutionDefines = {"{{defines}}", "defines"};
const Substitution CSubstitutionFrameworkDirs = {"{{framework_dirs}}",
                                                 "framework_dirs"};
const Substitution CSubstitutionIncludeDirs = {"{{include_dirs}}",
                                              "include_dirs"};

// Valid for linker tools.
const Substitution CSubstitutionLinkerInputs = {"{{inputs}}", "in"};
const Substitution CSubstitutionLinkerInputsNewline = {"{{inputs_newline}}",
                                                      "in_newline"};
const Substitution CSubstitutionLdFlags = {"{{ldflags}}", "ldflags"};
const Substitution CSubstitutionLibs = {"{{libs}}", "libs"};
const Substitution CSubstitutionSoLibs = {"{{solibs}}", "solibs"};
const Substitution CSubstitutionFrameworks = {"{{frameworks}}", "frameworks"};

// Valid for alink only.
const Substitution CSubstitutionArFlags = {"{{arflags}}", "arflags"};

bool IsValidCompilerSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || IsValidSourceSubstitution(type) ||
         type == &SubstitutionSource || type == &CSubstitutionAsmFlags ||
         type == &CSubstitutionCFlags || type == &CSubstitutionCFlagsC ||
         type == &CSubstitutionCFlagsCc || type == &CSubstitutionCFlagsObjC ||
         type == &CSubstitutionCFlagsObjCc || type == &CSubstitutionDefines ||
         type == &CSubstitutionFrameworkDirs ||
         type == &CSubstitutionIncludeDirs;
}

bool IsValidCompilerOutputsSubstitution(const Substitution* type) {
  // All tool types except "output" (which would be infinitely recursive).
  return (IsValidToolSubstitution(type) && type != &SubstitutionOutput) ||
         IsValidSourceSubstitution(type);
}

bool IsValidLinkerSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || type == &SubstitutionOutputDir ||
         type == &SubstitutionOutputExtension ||
         type == &CSubstitutionLinkerInputs ||
         type == &CSubstitutionLinkerInputsNewline ||
         type == &CSubstitutionLdFlags || type == &CSubstitutionLibs ||
         type == &CSubstitutionSoLibs || type == &CSubstitutionFrameworks;
}

bool IsValidLinkerOutputsSubstitution(const Substitution* type) {
  // All valid compiler outputs plus the output extension.
  return IsValidCompilerOutputsSubstitution(type) ||
         type == &SubstitutionOutputDir || type == &SubstitutionOutputExtension;
}

bool IsValidALinkSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) ||
         type == &SubstitutionOutputDir ||
         type == &SubstitutionOutputExtension ||
         type == &CSubstitutionLinkerInputs ||
         type == &CSubstitutionLinkerInputsNewline ||
         type == &CSubstitutionArFlags;
}
