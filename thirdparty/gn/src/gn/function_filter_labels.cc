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

enum FilterSelection {
  kExcludeFilter,
  kIncludeFilter,
};

Value RunFilterLabels(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      FilterSelection selection,
                      Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expecting exactly two arguments.");
    return Value();
  }

  // Validate "labels" and "patterns" are both lists
  if (args[0].type() != Value::LIST) {
    *err = Err(args[0], "First argument must be a list of target labels.");
    return Value();
  }
  if (args[1].type() != Value::LIST) {
    *err = Err(args[1], "Second argument must be a list of label patterns.");
    return Value();
  }

  // Extract "patterns"
  std::vector<LabelPattern> patterns;
  patterns.reserve(args[1].list_value().size());

  for (const auto& value : args[1].list_value()) {
    if (value.type() != Value::STRING) {
      *err = Err(args[1], "Second argument must be a list of label patterns.");
      return Value();
    }
    LabelPattern pattern = LabelPattern::GetPattern(
        scope->GetSourceDir(),
        scope->settings()->build_settings()->root_path_utf8(), value, err);
    if (err->has_error()) {
      return Value();
    }
    patterns.push_back(std::move(pattern));
  }

  // Iterate over "labels", resolving and matching against the list of patterns.
  Value result(function, Value::LIST);
  for (const auto& value : args[0].list_value()) {
    Label label =
        Label::Resolve(scope->GetSourceDir(),
                       scope->settings()->build_settings()->root_path_utf8(),
                       ToolchainLabelForScope(scope), value, err);
    if (err->has_error()) {
      // Change the error message to be more applicable than what Resolve will
      // produce.
      *err = Err(value, "First argument must be a list of target labels.");
      return Value();
    }

    const bool matches_pattern = LabelPattern::VectorMatches(patterns, label);
    switch (selection) {
      case kIncludeFilter:
        if (matches_pattern)
          result.list_value().push_back(value);
        break;

      case kExcludeFilter:
        if (!matches_pattern)
          result.list_value().push_back(value);
        break;
    }
  }
  return result;
}

const char kFilterLabelsInclude[] = "filter_labels_include";
const char kFilterLabelsInclude_HelpShort[] =
    "filter_labels_include: Remove labels that do not match a set of patterns.";
const char kFilterLabelsInclude_Help[] =
    R"(filter_labels_include: Remove labels that do not match a set of patterns.

  filter_labels_include(labels, include_patterns)

  The argument labels must be a list of strings.

  The argument include_patterns must be a list of label patterns (see
  "gn help label_pattern"). Only elements from labels matching at least
  one of the patterns will be included.

Examples
  labels = [ "//foo:baz", "//foo/bar:baz", "//bar:baz" ]
  result = filter_labels_include(labels, [ "//foo:*" ])
  # result will be [ "//foo:baz" ]
)";

Value RunFilterLabelsInclude(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err) {
  return RunFilterLabels(scope, function, args, kIncludeFilter, err);
}

const char kFilterLabelsExclude[] = "filter_labels_exclude";
const char kFilterLabelsExclude_HelpShort[] =
    "filter_labels_exclude: Remove labels that match a set of patterns.";
const char kFilterLabelsExclude_Help[] =
    R"(filter_labels_exclude: Remove labels that match a set of patterns.

  filter_labels_exclude(labels, exclude_patterns)

  The argument labels must be a list of strings.

  The argument exclude_patterns must be a list of label patterns (see
  "gn help label_pattern"). Only elements from labels matching at least
  one of the patterns will be excluded.

Examples
  labels = [ "//foo:baz", "//foo/bar:baz", "//bar:baz" ]
  result = filter_labels_exclude(labels, [ "//foo:*" ])
  # result will be [ "//foo/bar:baz", "//bar:baz" ]
)";

Value RunFilterLabelsExclude(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err) {
  return RunFilterLabels(scope, function, args, kExcludeFilter, err);
}

}  // namespace functions
