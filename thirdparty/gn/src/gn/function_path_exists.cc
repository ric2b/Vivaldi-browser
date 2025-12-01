// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_util.h"
#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/value.h"

namespace functions {

Value RunPathExists(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  Value result;

  if (args.size() != 1) {
    *err = Err(function->function(), "Expecting exactly one argument.");
    return result;
  }
  const Value& value = args[0];
  if (!value.VerifyTypeIs(Value::STRING, err)) {
    return result;
  }

  const std::string& input_string = value.string_value();
  const SourceDir& cur_dir = scope->GetSourceDir();
  bool as_dir =
      !input_string.empty() && input_string[input_string.size() - 1] == '/';

  base::FilePath system_path;
  if (as_dir) {
    system_path = scope->settings()->build_settings()->GetFullPath(
        cur_dir.ResolveRelativeDir(
            value, err, scope->settings()->build_settings()->root_path_utf8()));
  } else {
    system_path = scope->settings()->build_settings()->GetFullPath(
        cur_dir.ResolveRelativeFile(
            value, err, scope->settings()->build_settings()->root_path_utf8()));
  }
  if (err->has_error()) {
    return value;
  }

  bool exists = PathExists(system_path);
  return Value(function, exists);
}

const char kPathExists[] = "path_exists";
const char kPathExists_HelpShort[] =
    "path_exists: Returns whether the given path exists.";
const char kPathExists_Help[] =
    R"(path_exists: Returns whether the given path exists.

  path_exists(path)

Examples:
  path_exists("//")  # true
  path_exists("BUILD.gn")  # true
  path_exists("/abs-non-existent")  # false
)";

}  // namespace functions
