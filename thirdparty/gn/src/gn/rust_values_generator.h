// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_VALUES_GENERATOR_H_
#define TOOLS_GN_RUST_VALUES_GENERATOR_H_

#include "base/macros.h"
#include "gn/target.h"

class FunctionCallNode;

// Collects and writes specified data.
class RustTargetGenerator {
 public:
  RustTargetGenerator(Target* target,
                      Scope* scope,
                      const FunctionCallNode* function_call,
                      Err* err);
  ~RustTargetGenerator();

  void Run();

 private:
  bool FillCrateName();
  bool FillCrateRoot();
  bool FillCrateType();
  bool FillEdition();
  bool FillAliasedDeps();

  Target* target_;
  Scope* scope_;
  const FunctionCallNode* function_call_;
  Err* err_;

  DISALLOW_COPY_AND_ASSIGN(RustTargetGenerator);
};

#endif  // TOOLS_GN_RUST_VALUES_GENERATOR_H_
