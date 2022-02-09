// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/source_dir.h"

#include <string>

#include "base/logging.h"
#include "gn/filesystem_utils.h"
#include "gn/source_file.h"
#include "util/build_config.h"

namespace {

void AssertValueSourceDirString(std::string_view s) {
  if (!s.empty()) {
#if defined(OS_WIN)
    DCHECK(s[0] == '/' ||
           (s.size() > 2 && s[0] != '/' && s[1] == ':' && IsSlash(s[2])));
#else
    DCHECK(s[0] == '/');
#endif
    DCHECK(EndsWithSlash(s)) << s;
  }
}

// Validates input value (input_value) and sets proper error message.
// Note: Parameter blame_input is used only for generating error message.
bool ValidateResolveInput(bool as_file,
                          const Value& blame_input_value,
                          std::string_view input_value,
                          Err* err) {
  if (as_file) {
    // It's an error to resolve an empty string or one that is a directory
    // (indicated by a trailing slash) because this is the function that expects
    // to return a file.
    if (input_value.empty()) {
      *err = Err(blame_input_value, "Empty file path.",
                 "You can't use empty strings as file paths.");
      return false;
    } else if (input_value[input_value.size() - 1] == '/') {
      std::string help = "You specified the path\n  ";
      help.append(std::string(input_value));
      help.append(
          "\nand it ends in a slash, indicating you think it's a directory."
          "\nBut here you're supposed to be listing a file.");
      *err = Err(blame_input_value, "File path ends in a slash.", help);
      return false;
    }
  } else if (input_value.empty()) {
    *err = Err(blame_input_value, "Empty directory path.",
               "You can't use empty strings as directories.");
    return false;
  }
  return true;
}

static StringAtom SourceDirStringAtom(std::string_view s) {
  if (EndsWithSlash(s)) {  // Avoid allocation when possible.
    AssertValueSourceDirString(s);
    return StringAtom(s);
  }

  std::string str;
  str.reserve(s.size() + 1);
  str += s;
  str.push_back('/');
  AssertValueSourceDirString(str);
  return StringAtom(str);
}

}  // namespace

SourceDir::SourceDir(std::string_view s)
    : value_(SourceDirStringAtom(s)) {
  actual_path_ = StringAtom(BuildSettings::RemapSourcePathToActual(value_.str()));
}

SourceDir::SourceDir(std::string_view p, const std::string_view& p_act)
    : value_(SourceDirStringAtom(p)),
      actual_path_(SourceDirStringAtom(p_act).str()) {}

std::string SourceDir::ResolveRelativeAs(bool as_file,
                                         const Value& blame_input_value,
                                         std::string_view input_value,
                                         Err* err,
                                         std::string_view source_root,
                                         std::string_view actual_path_in,
                                         StringAtom* actual_path_out) const {
  if (!ValidateResolveInput(as_file, blame_input_value, input_value,
                                        err)) {
    return std::string();
  }
  return ResolveRelative(input_value, value_.str(), as_file, source_root,
                         actual_path_in, actual_path_out);
}

SourceFile SourceDir::ResolveRelativeFile(const Value& p,
                                          Err* err,
                                          std::string_view source_root) const {
  SourceFile ret;

  if (!p.VerifyTypeIs(Value::STRING, err))
    return ret;

  const std::string& input_string = p.string_value();
  if (!ValidateResolveInput(true, p, input_string, err))
    return ret;

  ret.SetValue(ResolveRelative(input_string, value_.str(), true, source_root,
                               actual_path_.str(), &ret.actual_path_));
  return ret;
}

SourceDir SourceDir::ResolveRelativeDir(const Value& blame_input_value,
                                        std::string_view input_value,
                                        Err* err,
                                        std::string_view source_root) const {
  SourceDir ret;
  ret.value_ = StringAtom(ResolveRelativeAs(false, blame_input_value,
                                            input_value, err, source_root,
                                            actual_path_, &ret.actual_path_));
  return ret;
}

std::string SourceDir::ResolveRelativeAs(bool as_file,
                                         const Value& v,
                                         Err* err,
                                         std::string_view source_root,
                                         const std::string* v_value) const {
  if (!v.VerifyTypeIs(Value::STRING, err))
    return std::string();

  if (!v_value) {
    v_value = &v.string_value();
  }
  std::string result =
      ResolveRelativeAs(as_file, v, *v_value, err, source_root, actual_path_.str());
  if (!as_file)
    AssertValueSourceDirString(result);
  return result;
}

SourceDir SourceDir::ResolveRelativeDir(const Value& v,
                                        Err* err,
                                        std::string_view source_root) const {
  if (!v.VerifyTypeIs(Value::STRING, err))
    return SourceDir();

  return ResolveRelativeDir(v, v.string_value(), err, source_root);
}

base::FilePath SourceDir::Resolve(const base::FilePath& source_root,
                                  bool use_actual_path) const {
  return ResolvePath(use_actual_path ? actual_path_.str() : value_.str(), false,
                     source_root);
}
