// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TOOL_H_
#define TOOLS_GN_RUST_TOOL_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "gn/label.h"
#include "gn/label_ptr.h"
#include "gn/rust_values.h"
#include "gn/source_file.h"
#include "gn/substitution_list.h"
#include "gn/substitution_pattern.h"
#include "gn/target.h"
#include "gn/tool.h"

class RustTool : public Tool {
 public:
  // Rust tools
  static const char* kRsToolBin;
  static const char* kRsToolCDylib;
  static const char* kRsToolDylib;
  static const char* kRsToolMacro;
  static const char* kRsToolRlib;
  static const char* kRsToolStaticlib;

  explicit RustTool(const char* n);
  ~RustTool();

  // Manual RTTI and required functions ---------------------------------------

  bool InitTool(Scope* block_scope, Toolchain* toolchain, Err* err);
  bool ValidateName(const char* name) const override;
  void SetComplete() override;
  bool ValidateSubstitution(const Substitution* sub_type) const override;

  RustTool* AsRust() override;
  const RustTool* AsRust() const override;

 private:
  bool SetOutputExtension(const Value* value, std::string* var, Err* err);
  bool ReadOutputsPatternList(Scope* scope,
                              const char* var,
                              SubstitutionList* field,
                              Err* err);

  DISALLOW_COPY_AND_ASSIGN(RustTool);
};

#endif  // TOOLS_GN_RUST_TOOL_H_
