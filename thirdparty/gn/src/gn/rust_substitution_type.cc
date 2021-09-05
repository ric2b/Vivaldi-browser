// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_substitution_type.h"

#include <stddef.h>
#include <stdlib.h>

#include "gn/err.h"
#include "gn/substitution_type.h"

const SubstitutionTypes RustSubstitutions = {
    &kRustSubstitutionCrateName,       &kRustSubstitutionCrateType,
    &kRustSubstitutionRustDeps,        &kRustSubstitutionRustFlags,
    &kRustSubstitutionRustEnv,         &kRustSubstitutionExterns,
};

// Valid for Rust tools.
const Substitution kRustSubstitutionCrateName = {"{{crate_name}}",
                                                 "crate_name"};
const Substitution kRustSubstitutionCrateType = {"{{crate_type}}",
                                                 "crate_type"};
const Substitution kRustSubstitutionExterns = {"{{externs}}", "externs"};
const Substitution kRustSubstitutionRustDeps = {"{{rustdeps}}", "rustdeps"};
const Substitution kRustSubstitutionRustEnv = {"{{rustenv}}", "rustenv"};
const Substitution kRustSubstitutionRustFlags = {"{{rustflags}}", "rustflags"};

bool IsValidRustSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || IsValidSourceSubstitution(type) ||
         type == &SubstitutionOutputDir ||
         type == &SubstitutionOutputExtension ||
         type == &kRustSubstitutionCrateName ||
         type == &kRustSubstitutionCrateType ||
         type == &kRustSubstitutionExterns ||
         type == &kRustSubstitutionRustDeps ||
         type == &kRustSubstitutionRustEnv ||
         type == &kRustSubstitutionRustFlags;
}
