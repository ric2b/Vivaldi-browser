// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/group_target_generator.h"

#include "tools/gn/target.h"
#include "tools/gn/variables.h"

GroupTargetGenerator::GroupTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Err* err)
    : TargetGenerator(target, scope, function_call, err) {
}

GroupTargetGenerator::~GroupTargetGenerator() {
}

void GroupTargetGenerator::DoRun() {
  target_->set_output_type(Target::GROUP);
  // Groups only have the default types filled in by the target generator
  // base class.
  // However, groups can be used to create aliases for executable targets
  // so retrieve output_name, if it is present
  FillOutputName();
}

bool GroupTargetGenerator::FillOutputName() {
  const Value* value = scope_->GetValue(variables::kOutputName, true);
  if (!value)
    return true;
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;
  target_->set_output_name(value->string_value());
  return true;
}
