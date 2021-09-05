// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_project_writer.h"

#include <fstream>
#include <sstream>
#include <tuple>

#include "base/json/string_escape.h"
#include "gn/builder.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_command_util.h"
#include "gn/rust_project_writer_helpers.h"
#include "gn/rust_tool.h"
#include "gn/source_file.h"
#include "gn/string_output_buffer.h"
#include "gn/tool.h"

#if defined(OS_WINDOWS)
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
#endif

// Current structure of rust-project.json output file
//
// {
//    "roots": [
//      "some/source/root"  // each crate's source root
//    ],
//    "crates": [
//        {
//            "deps": [
//                {
//                    "crate": 1, // index into crate array
//                    "name": "alloc" // extern name of dependency
//                },
//            ],
//            "edition": "2018", // edition of crate
//            "cfg": [
//              "unix", // "atomic" value config options
//              "rust_panic=\"abort\""", // key="value" config options
//            ]
//            "root_module": "absolute path to crate"
//        },
// }
//

bool RustProjectWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                         const Builder& builder,
                                         const std::string& file_name,
                                         bool quiet,
                                         Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null())
    return false;

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();

  StringOutputBuffer out_buffer;
  std::ostream out(&out_buffer);

  RenderJSON(build_settings, all_targets, out);

  if (out_buffer.ContentsEqual(output_path)) {
    return true;
  }

  return out_buffer.WriteToFile(output_path, err);
}

// Map of Targets to their index in the crates list (for linking dependencies to
// their indexes).
using TargetIndexMap = std::unordered_map<const Target*, uint32_t>;

// A collection of Targets.
using TargetsVector = UniqueVector<const Target*>;

// Get the Rust deps for a target, recursively expanding OutputType::GROUPS
// that are present in the GN structure.  This will return a flattened list of
// deps from the groups, but will not expand a Rust lib dependency to find any
// transitive Rust dependencies.
void GetRustDeps(const Target* target, TargetsVector* rust_deps) {
  for (const auto& pair : target->GetDeps(Target::DEPS_LINKED)) {
    const Target* dep = pair.ptr;

    if (dep->source_types_used().RustSourceUsed()) {
      // Include any Rust dep.
      rust_deps->push_back(dep);
    } else if (dep->output_type() == Target::OutputType::GROUP) {
      // Inspect (recursively) any group to see if it contains Rust deps.
      GetRustDeps(dep, rust_deps);
    }
  }
}
TargetsVector GetRustDeps(const Target* target) {
  TargetsVector deps;
  GetRustDeps(target, &deps);
  return deps;
}

// TODO(bwb) Parse sysroot structure from toml files. This is fragile and might
// break if upstream changes the dependency structure.
const std::string_view sysroot_crates[] = {"std",
                                           "core",
                                           "alloc",
                                           "collections",
                                           "libc",
                                           "panic_unwind",
                                           "proc_macro",
                                           "rustc_unicode",
                                           "std_unicode",
                                           "test",
                                           "alloc_jemalloc",
                                           "alloc_system",
                                           "compiler_builtins",
                                           "getopts",
                                           "panic_abort",
                                           "unwind",
                                           "build_helper",
                                           "rustc_asan",
                                           "rustc_lsan",
                                           "rustc_msan",
                                           "rustc_tsan",
                                           "syntax"};

// Multiple sysroot crates have dependenices on each other.  This provides a
// mechanism for specifiying that in an extendible manner.
const std::unordered_map<std::string_view, std::vector<std::string_view>>
    sysroot_deps_map = {{"alloc", {"core"}},
                        {"std", {"alloc", "core", "panic_abort", "unwind"}}};

// Add each of the crates a sysroot has, including their dependencies.
void AddSysrootCrate(const BuildSettings* build_settings,
                     const std::string_view crate,
                     const std::string_view current_sysroot,
                     SysrootCrateIndexMap& sysroot_crate_lookup,
                     CrateList& crate_list) {
  if (sysroot_crate_lookup.find(crate) != sysroot_crate_lookup.end()) {
    // If this sysroot crate is already in the lookup, we don't add it again.
    return;
  }

  // Add any crates that this sysroot crate depends on.
  auto deps_lookup = sysroot_deps_map.find(crate);
  if (deps_lookup != sysroot_deps_map.end()) {
    auto deps = (*deps_lookup).second;
    for (auto dep : deps) {
      AddSysrootCrate(build_settings, dep, current_sysroot,
                      sysroot_crate_lookup, crate_list);
    }
  }

  size_t crate_index = crate_list.size();
  sysroot_crate_lookup.insert(std::make_pair(crate, crate_index));

  base::FilePath rebased_out_dir =
      build_settings->GetFullPath(build_settings->build_dir());
  auto crate_path =
      FilePathToUTF8(rebased_out_dir) + std::string(current_sysroot) +
      "/lib/rustlib/src/rust/src/lib" + std::string(crate) + "/lib.rs";

  Crate sysroot_crate =
      Crate(SourceFile(crate_path), crate_index, std::string(crate), "2018");

  sysroot_crate.AddConfigItem("debug_assertions");

  if (deps_lookup != sysroot_deps_map.end()) {
    auto deps = (*deps_lookup).second;
    for (auto dep : deps) {
      auto idx = sysroot_crate_lookup[dep];
      sysroot_crate.AddDependency(idx, std::string(dep));
    }
  }

  crate_list.push_back(sysroot_crate);
}

// Add the given sysroot to the project, if it hasn't already been added.
void AddSysroot(const BuildSettings* build_settings,
                const std::string_view sysroot,
                SysrootIndexMap& sysroot_lookup,
                CrateList& crate_list) {
  // If this sysroot is already in the lookup, we don't add it again.
  if (sysroot_lookup.find(sysroot) != sysroot_lookup.end()) {
    return;
  }

  // Otherwise, add all of its crates
  for (auto crate : sysroot_crates) {
    AddSysrootCrate(build_settings, crate, sysroot, sysroot_lookup[sysroot],
                    crate_list);
  }
}

void AddTarget(const BuildSettings* build_settings,
               const Target* target,
               TargetIndexMap& lookup,
               SysrootIndexMap& sysroot_lookup,
               CrateList& crate_list) {
  if (lookup.find(target) != lookup.end()) {
    // If target is already in the lookup, we don't add it again.
    return;
  }

  // Check what sysroot this target needs.  Add it to the crate list if it
  // hasn't already been added.
  auto rust_tool =
      target->toolchain()->GetToolForSourceTypeAsRust(SourceFile::SOURCE_RS);
  auto current_sysroot = rust_tool->GetSysroot();
  if (current_sysroot != "" && sysroot_lookup.count(current_sysroot) == 0) {
    AddSysroot(build_settings, current_sysroot, sysroot_lookup, crate_list);
  }

  auto crate_deps = GetRustDeps(target);

  // Add all dependencies of this crate, before this crate.
  for (const auto& dep : crate_deps) {
    AddTarget(build_settings, dep, lookup, sysroot_lookup, crate_list);
  }

  // The index of a crate is its position (0-based) in the list of crates.
  size_t crate_id = crate_list.size();

  // Add the target to the crate lookup.
  lookup.insert(std::make_pair(target, crate_id));

  SourceFile crate_root = target->rust_values().crate_root();
  std::string crate_label = target->label().GetUserVisibleName(false);

  std::string cfg_prefix("--cfg=");
  std::string edition_prefix("--edition=");
  ConfigList cfgs;

  std::string edition = "2015";  // default to be overridden as needed

  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    for (const auto& flag : iter.cur().rustflags()) {
      // extract the edition of this target
      if (!flag.compare(0, edition_prefix.size(), edition_prefix)) {
        edition = flag.substr(edition_prefix.size());
      }
      if (!flag.compare(0, cfg_prefix.size(), cfg_prefix)) {
        auto cfg = flag.substr(cfg_prefix.size());
        std::string escaped_config;
        base::EscapeJSONString(cfg, false, &escaped_config);
        cfgs.push_back(escaped_config);
      }
    }
  }

  Crate crate = Crate(crate_root, crate_id, crate_label, edition);

  crate.AddConfigItem("test");
  crate.AddConfigItem("debug_assertions");

  for (auto& cfg : cfgs) {
    crate.AddConfigItem(cfg);
  }

  // Add the sysroot dependency, if there is one.
  if (current_sysroot != "") {
    // TODO(bwb) If this library doesn't depend on std, use core instead
    auto std_idx = sysroot_lookup[current_sysroot].find("std");
    if (std_idx != sysroot_lookup[current_sysroot].end()) {
      crate.AddDependency(std_idx->second, "std");
    }
  }

  // Add the rest of the crate dependencies.
  for (const auto& dep : crate_deps) {
    auto idx = lookup[dep];
    crate.AddDependency(idx, dep->rust_values().crate_name());
  }

  crate_list.push_back(crate);
}

void WriteCrates(const BuildSettings* build_settings,
                 CrateList& crate_list,
                 std::ostream& rust_project) {
  rust_project << "{" NEWLINE;
  rust_project << "  \"roots\": [";
  bool first_root = true;
  for (auto& crate : crate_list) {
    if (!first_root)
      rust_project << ",";
    first_root = false;

    std::string root_dir =
        FilePathToUTF8(build_settings->GetFullPath(crate.root().GetDir()));
    rust_project << NEWLINE "    \"" << root_dir << "\"";
  }
  rust_project << NEWLINE "  ]," NEWLINE;
  rust_project << "  \"crates\": [";
  bool first_crate = true;
  for (auto& crate : crate_list) {
    if (!first_crate)
      rust_project << ",";
    first_crate = false;

    auto crate_root = FilePathToUTF8(build_settings->GetFullPath(crate.root()));

    rust_project << NEWLINE << "    {" NEWLINE
                 << "      \"crate_id\": " << crate.index() << "," NEWLINE
                 << "      \"root_module\": \"" << crate_root << "\"," NEWLINE
                 << "      \"label\": \"" << crate.label() << "\"," NEWLINE
                 << "      \"deps\": [";

    bool first_dep = true;
    for (auto& dep : crate.dependencies()) {
      if (!first_dep)
        rust_project << ",";
      first_dep = false;

      rust_project << NEWLINE << "        {" NEWLINE
                   << "          \"crate\": " << dep.first << "," NEWLINE
                   << "          \"name\": \"" << dep.second << "\"" NEWLINE
                   << "        }";
    }
    rust_project << NEWLINE "      ]," NEWLINE;  // end dep list

    rust_project << "      \"edition\": \"" << crate.edition() << "\"," NEWLINE;

    rust_project << "      \"cfg\": [";
    bool first_cfg = true;
    for (const auto& cfg : crate.configs()) {
      if (!first_cfg)
        rust_project << ",";
      first_cfg = false;

      rust_project << NEWLINE;
      rust_project << "        \"" << cfg << "\"";
    }
    rust_project << NEWLINE;
    rust_project << "      ]" NEWLINE;  // end cfgs

    rust_project << "    }";  // end crate
  }
  rust_project << NEWLINE "  ]" NEWLINE;  // end crate list
  rust_project << "}" NEWLINE;
}

void RustProjectWriter::RenderJSON(const BuildSettings* build_settings,
                                   std::vector<const Target*>& all_targets,
                                   std::ostream& rust_project) {
  TargetIndexMap lookup;
  SysrootIndexMap sysroot_lookup;
  CrateList crate_list;

  // All the crates defined in the project.
  for (const auto* target : all_targets) {
    if (!target->IsBinary() || !target->source_types_used().RustSourceUsed())
      continue;

    AddTarget(build_settings, target, lookup, sysroot_lookup, crate_list);
  }

  WriteCrates(build_settings, crate_list, rust_project);
}
