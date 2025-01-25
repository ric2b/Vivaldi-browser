// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_tool.h"

#include "gn/rust_substitution_type.h"
#include "gn/target.h"

const char* RustTool::kRsToolBin = "rust_bin";
const char* RustTool::kRsToolCDylib = "rust_cdylib";
const char* RustTool::kRsToolDylib = "rust_dylib";
const char* RustTool::kRsToolMacro = "rust_macro";
const char* RustTool::kRsToolRlib = "rust_rlib";
const char* RustTool::kRsToolStaticlib = "rust_staticlib";

RustTool::RustTool(const char* n) : Tool(n) {
  CHECK(ValidateName(n));
  // TODO: should these be settable in toolchain definition? They would collide
  // with the same switch names for C tools, however. So reading them from the
  // toolchain definition would produce the incorrect switch unless we separate
  // their names somehow.
  set_framework_switch("-lframework=");
  // NOTE: No support for weak_framework in rustc, so we just use `-lframework`
  // for now, to avoid more cryptic compiler errors.
  // https://doc.rust-lang.org/rustc/command-line-arguments.html#-l-link-the-generated-crate-to-a-native-library
  set_weak_framework_switch("-lframework=");
  set_framework_dir_switch("-Lframework=");
  set_lib_dir_switch("-Lnative=");
  set_lib_switch("-l");
  set_linker_arg("-Clink-arg=");
  set_dynamic_link_switch("-Clink-arg=-Bdynamic");
}

RustTool::~RustTool() = default;

RustTool* RustTool::AsRust() {
  return this;
}
const RustTool* RustTool::AsRust() const {
  return this;
}

bool RustTool::ValidateName(const char* name) const {
  return name == kRsToolBin || name == kRsToolCDylib || name == kRsToolDylib ||
         name == kRsToolMacro || name == kRsToolRlib ||
         name == kRsToolStaticlib;
}

bool RustTool::MayLink() const {
  return name_ == kRsToolBin || name_ == kRsToolCDylib ||
         name_ == kRsToolDylib || name_ == kRsToolMacro;
}

void RustTool::SetComplete() {
  SetToolComplete();
  link_output_.FillRequiredTypes(&substitution_bits_);
  depend_output_.FillRequiredTypes(&substitution_bits_);
}

std::string_view RustTool::GetSysroot() const {
  return rust_sysroot_;
}

bool RustTool::SetOutputExtension(const Value* value,
                                  std::string* var,
                                  Err* err) {
  DCHECK(!complete_);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::STRING, err))
    return false;
  if (value->string_value().empty())
    return true;

  *var = std::move(value->string_value());
  return true;
}

bool RustTool::ReadOutputsPatternList(Scope* scope,
                                      const char* var,
                                      SubstitutionList* field,
                                      Err* err) {
  DCHECK(!complete_);
  const Value* value = scope->GetValue(var, true);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::LIST, err))
    return false;

  SubstitutionList list;
  if (!list.Parse(*value, err))
    return false;

  // Validate the right kinds of patterns are used.
  if (list.list().empty()) {
    *err = Err(defined_from(), "\"outputs\" must be specified for this tool.");
    return false;
  }

  for (const auto& cur_type : list.required_types()) {
    if (!IsValidRustSubstitution(cur_type)) {
      *err = Err(*value, "Pattern not valid here.",
                 "You used the pattern " + std::string(cur_type->name) +
                     " which is not valid\nfor this variable.");
      return false;
    }
  }

  *field = std::move(list);
  return true;
}

bool RustTool::ValidateRuntimeOutputs(Err* err) {
  if (runtime_outputs().list().empty())
    return true;  // Empty is always OK.

  if (name_ == kRsToolRlib || name_ == kRsToolStaticlib) {
    *err =
        Err(defined_from(), "This tool specifies runtime_outputs.",
            "This is only valid for linker tools (rust_rlib doesn't count).");
    return false;
  }

  for (const SubstitutionPattern& pattern : runtime_outputs().list()) {
    if (!IsPatternInOutputList(outputs(), pattern)) {
      *err = Err(defined_from(), "This tool's runtime_outputs is bad.",
                 "It must be a subset of the outputs. The bad one is:\n  " +
                     pattern.AsString());
      return false;
    }
  }
  return true;
}

// Validates either link_output or depend_output. To generalize to either, pass
// the associated pattern, and the variable name that should appear in error
// messages.
bool RustTool::ValidateLinkAndDependOutput(const SubstitutionPattern& pattern,
                                           const char* variable_name,
                                           Err* err) {
  if (pattern.empty())
    return true;  // Empty is always OK.

  // It should only be specified for certain tool types.
  if (name_ == kRsToolRlib || name_ == kRsToolStaticlib) {
    *err = Err(defined_from(),
               "This tool specifies a " + std::string(variable_name) + ".",
               "This is only valid for linking tools, not rust_rlib or "
               "rust_staticlib.");
    return false;
  }

  if (!IsPatternInOutputList(outputs(), pattern)) {
    *err = Err(defined_from(), "This tool's link_output is bad.",
               "It must match one of the outputs.");
    return false;
  }

  return true;
}

bool RustTool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  // Initialize default vars.
  if (!Tool::InitTool(scope, toolchain, err)) {
    return false;
  }

  // All Rust tools should have outputs.
  if (!ReadOutputsPatternList(scope, "outputs", &outputs_, err)) {
    return false;
  }

  // Check for a sysroot. Sets an empty string when not explicitly set.
  if (!ReadString(scope, "rust_sysroot", &rust_sysroot_, err)) {
    return false;
  }

  if (MayLink()) {
    if (!ReadString(scope, "rust_swiftmodule_switch", &swiftmodule_switch_,
                    err) ||
        !ReadString(scope, "dynamic_link_switch", &dynamic_link_switch_, err)) {
      return false;
    }
  }

  if (!ValidateRuntimeOutputs(err)) {
    return false;
  }

  if (!ReadPattern(scope, "link_output", &link_output_, err) ||
      !ReadPattern(scope, "depend_output", &depend_output_, err)) {
    return false;
  }

  // Validate link_output and depend_output.
  if (!ValidateLinkAndDependOutput(link_output(), "link_output", err)) {
    return false;
  }
  if (!ValidateLinkAndDependOutput(depend_output(), "depend_output", err)) {
    return false;
  }
  if (link_output().empty() != depend_output().empty()) {
    *err = Err(defined_from(),
               "Both link_output and depend_output should either "
               "be specified or they should both be empty.");
    return false;
  }

  return true;
}

bool RustTool::ValidateSubstitution(const Substitution* sub_type) const {
  if (MayLink())
    return IsValidRustLinkerSubstitution(sub_type);
  if (ValidateName(name_))
    return IsValidRustSubstitution(sub_type);
  NOTREACHED();
  return false;
}
