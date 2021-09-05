// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_C_SUBSTITUTION_TYPE_H_
#define TOOLS_GN_C_SUBSTITUTION_TYPE_H_

#include <set>
#include <vector>

#include "gn/substitution_type.h"

// The set of substitutions available to all tools.
extern const SubstitutionTypes CSubstitutions;

// Valid for compiler tools.
extern const Substitution CSubstitutionAsmFlags;
extern const Substitution CSubstitutionCFlags;
extern const Substitution CSubstitutionCFlagsC;
extern const Substitution CSubstitutionCFlagsCc;
extern const Substitution CSubstitutionCFlagsObjC;
extern const Substitution CSubstitutionCFlagsObjCc;
extern const Substitution CSubstitutionDefines;
extern const Substitution CSubstitutionFrameworkDirs;
extern const Substitution CSubstitutionIncludeDirs;

// Valid for linker tools.
extern const Substitution CSubstitutionLinkerInputs;
extern const Substitution CSubstitutionLinkerInputsNewline;
extern const Substitution CSubstitutionLdFlags;
extern const Substitution CSubstitutionLibs;
extern const Substitution CSubstitutionSoLibs;
extern const Substitution CSubstitutionFrameworks;

// Valid for alink only.
extern const Substitution CSubstitutionArFlags;

// Both compiler and linker tools.
bool IsValidCompilerSubstitution(const Substitution* type);
bool IsValidCompilerOutputsSubstitution(const Substitution* type);
bool IsValidLinkerSubstitution(const Substitution* type);
bool IsValidLinkerOutputsSubstitution(const Substitution* type);
bool IsValidALinkSubstitution(const Substitution* type);

#endif  // TOOLS_GN_C_SUBSTITUTION_TYPE_H_
