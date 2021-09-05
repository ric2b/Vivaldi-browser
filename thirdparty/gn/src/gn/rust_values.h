// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TARGET_VALUES_H_
#define TOOLS_GN_RUST_TARGET_VALUES_H_

#include <map>

#include "base/containers/flat_map.h"
#include "gn/inherited_libraries.h"
#include "gn/label.h"
#include "gn/source_file.h"

// Holds the values (outputs, args, script name, etc.) for either an action or
// an action_foreach target.
class RustValues {
 public:
  RustValues();
  ~RustValues();

  // Library crate types are specified here. Shared library crate types must be
  // specified, all other crate types can be automatically deduced from the
  // target type (e.g. executables use crate_type = "bin", static_libraries use
  // crate_type = "staticlib") unless explicitly set.
  enum CrateType {
    CRATE_AUTO = 0,
    CRATE_BIN,
    CRATE_CDYLIB,
    CRATE_DYLIB,
    CRATE_PROC_MACRO,
    CRATE_RLIB,
    CRATE_STATICLIB,
  };

  // Name of this crate.
  std::string& crate_name() { return crate_name_; }
  const std::string& crate_name() const { return crate_name_; }

  // Main source file for this crate.
  const SourceFile& crate_root() const { return crate_root_; }
  void set_crate_root(SourceFile& s) { crate_root_ = s; }

  // Crate type for compilation.
  CrateType crate_type() const { return crate_type_; }
  void set_crate_type(CrateType s) { crate_type_ = s; }

  // Any renamed dependencies for the `extern` flags.
  const std::map<Label, std::string>& aliased_deps() const {
    return aliased_deps_;
  }
  std::map<Label, std::string>& aliased_deps() { return aliased_deps_; }

  // Transitive closure of libraries that are depended on by this target
  InheritedLibraries& transitive_libs() { return rust_libs_; }
  const InheritedLibraries& transitive_libs() const { return rust_libs_; }

 private:
  std::string crate_name_;
  SourceFile crate_root_;
  CrateType crate_type_ = CRATE_AUTO;
  std::map<Label, std::string> aliased_deps_;
  InheritedLibraries rust_libs_;

  DISALLOW_COPY_AND_ASSIGN(RustValues);
};

#endif  // TOOLS_GN_RUST_TARGET_VALUES_H_
