// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/functions.h"
#include "gn/label_pattern.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/settings.h"
#include "gn/value.h"

namespace functions {

const char kLabelMatches[] = "label_matches";
const char kLabelMatches_HelpShort[] =
    "label_matches: Returns whether a label matches any of a list of patterns.";
const char kLabelMatches_Help[] =
    R"(label_matches: Returns true if the label matches any of a set of patterns.

  label_matches(target_label, patterns)

  The argument patterns must be a list of label patterns (see
  "gn help label_pattern"). If the target_label matches any of the patterns,
  the function returns the value true.

Examples
  result = label_matches("//baz:bar", [ "//foo/bar/*", "//baz:*" ])
  # result will be true
)";

Value RunLabelMatches(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expecting exactly two arguments.");
    return Value();
  }

  // Extract "label"
  if (args[0].type() != Value::STRING) {
    *err = Err(args[0], "First argument must be a target label.");
    return Value();
  }
  Label label =
      Label::Resolve(scope->GetSourceDir(),
                     scope->settings()->build_settings()->root_path_utf8(),
                     ToolchainLabelForScope(scope), args[0], err);
  if (label.is_null()) {
    return Value();
  }

  // Extract "patterns".
  if (args[1].type() != Value::LIST) {
    *err = Err(args[1], "Second argument must be a list of label patterns.");
    return Value();
  }
  std::vector<LabelPattern> patterns;
  patterns.reserve(args[1].list_value().size());

  for (const auto& pattern_string : args[1].list_value()) {
    if (pattern_string.type() != Value::STRING) {
      *err = Err(pattern_string,
                 "Second argument must be a list of label patterns.");
      return Value();
    }
    LabelPattern pattern = LabelPattern::GetPattern(
        scope->GetSourceDir(),
        scope->settings()->build_settings()->root_path_utf8(), pattern_string,
        err);
    if (err->has_error()) {
      return Value();
    }
    patterns.push_back(std::move(pattern));
  }

  return Value(function, LabelPattern::VectorMatches(patterns, label));
}

}  // namespace functions
