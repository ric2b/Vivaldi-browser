// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_binary_target_writer.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "gn/config_values_extractors.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_c_binary_target_writer.h"
#include "gn/ninja_rust_binary_target_writer.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/settings.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/variables.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  return opts;
}

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      rule_prefix_(GetNinjaRulePrefixForToolchain(settings_)) {}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() = default;

void NinjaBinaryTargetWriter::Run() {
  if (target_->source_types_used().RustSourceUsed()) {
    NinjaRustBinaryTargetWriter writer(target_, out_);
    writer.Run();
    return;
  }

  NinjaCBinaryTargetWriter writer(target_, out_);
  writer.Run();
}

OutputFile NinjaBinaryTargetWriter::WriteInputsStampAndGetDep() const {
  CHECK(target_->toolchain()) << "Toolchain not set on target "
                              << target_->label().GetUserVisibleName(true);

  UniqueVector<const SourceFile*> inputs;
  for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
    for (const auto& input : iter.cur().inputs()) {
      inputs.push_back(&input);
    }
  }

  if (inputs.size() == 0)
    return OutputFile();  // No inputs

  // If we only have one input, return it directly instead of writing a stamp
  // file for it.
  if (inputs.size() == 1)
    return OutputFile(settings_->build_settings(), *inputs[0]);

  // Make a stamp file.
  OutputFile stamp_file =
      GetBuildDirForTargetAsOutputFile(target_, BuildDirType::OBJ);
  stamp_file.value().append(target_->label().name());
  stamp_file.value().append(".inputs.stamp");

  out_ << "build ";
  path_output_.WriteFile(out_, stamp_file);
  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
       << GeneralTool::kGeneralToolStamp;

  // File inputs.
  for (const auto* input : inputs) {
    out_ << " ";
    path_output_.WriteFile(out_, *input);
  }

  out_ << std::endl;
  return stamp_file;
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  UniqueVector<const Target*> framework_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps,
          &framework_deps);

  // The classifier should never put extra object files in a source sets: any
  // source sets that we depend on should appear in our non-linkable deps
  // instead.
  DCHECK(extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (auto* dep : non_linkable_deps)
    order_only_deps.push_back(dep->dependency_output_file());

  WriteStampForTarget(object_files, order_only_deps);
}

void NinjaBinaryTargetWriter::GetDeps(
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps,
    UniqueVector<const Target*>* framework_deps) const {
  // Normal public/private deps.
  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
    ClassifyDependency(pair.ptr, extra_object_files, linkable_deps,
                       non_linkable_deps, framework_deps);
  }

  // Inherited libraries.
  for (auto* inherited_target : target_->inherited_libraries().GetOrdered()) {
    ClassifyDependency(inherited_target, extra_object_files, linkable_deps,
                       non_linkable_deps, framework_deps);
  }

  // Data deps.
  for (const auto& data_dep_pair : target_->data_deps())
    non_linkable_deps->push_back(data_dep_pair.ptr);
}

void NinjaBinaryTargetWriter::ClassifyDependency(
    const Target* dep,
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps,
    UniqueVector<const Target*>* framework_deps) const {
  // Only the following types of outputs have libraries linked into them:
  //  EXECUTABLE
  //  SHARED_LIBRARY
  //  _complete_ STATIC_LIBRARY
  //
  // Child deps of intermediate static libraries get pushed up the
  // dependency tree until one of these is reached, and source sets
  // don't link at all.
  bool can_link_libs = target_->IsFinal();

  if (dep->output_type() == Target::SOURCE_SET ||
      // If a complete static library depends on an incomplete static library,
      // manually link in the object files of the dependent library as if it
      // were a source set. This avoids problems with braindead tools such as
      // ar which don't properly link dependent static libraries.
      (target_->complete_static_lib() &&
       (dep->output_type() == Target::STATIC_LIBRARY &&
        !dep->complete_static_lib()))) {
    // Source sets have their object files linked into final targets
    // (shared libraries, executables, loadable modules, and complete static
    // libraries). Intermediate static libraries and other source sets
    // just forward the dependency, otherwise the files in the source
    // set can easily get linked more than once which will cause
    // multiple definition errors.
    if (can_link_libs)
      AddSourceSetFiles(dep, extra_object_files);

    // Add the source set itself as a non-linkable dependency on the current
    // target. This will make sure that anything the source set's stamp file
    // depends on (like data deps) are also built before the current target
    // can be complete. Otherwise, these will be skipped since this target
    // will depend only on the source set's object files.
    non_linkable_deps->push_back(dep);
  } else if (target_->output_type() == Target::RUST_LIBRARY &&
             dep->IsLinkable()) {
    // Rust libraries aren't final, but need to have the link lines of all
    // transitive deps specified.
    linkable_deps->push_back(dep);
  } else if (target_->complete_static_lib() && dep->IsFinal()) {
    non_linkable_deps->push_back(dep);
  } else if (can_link_libs && dep->IsLinkable()) {
    linkable_deps->push_back(dep);
  } else if (dep->output_type() == Target::CREATE_BUNDLE &&
             dep->bundle_data().is_framework()) {
    framework_deps->push_back(dep);
  } else {
    non_linkable_deps->push_back(dep);
  }
}

void NinjaBinaryTargetWriter::AddSourceSetFiles(
    const Target* source_set,
    UniqueVector<OutputFile>* obj_files) const {
  // Just add all sources to the list.
  for (const auto& source : source_set->sources()) {
    obj_files->push_back(OutputFile(settings_->build_settings(), source));
  }
}

void NinjaBinaryTargetWriter::WriteCompilerBuildLine(
    const SourceFile& source,
    const std::vector<OutputFile>& extra_deps,
    const std::vector<OutputFile>& order_only_deps,
    const char* tool_name,
    const std::vector<OutputFile>& outputs) {
  out_ << "build";
  path_output_.WriteFiles(out_, outputs);

  out_ << ": " << rule_prefix_ << tool_name;
  out_ << " ";
  path_output_.WriteFile(out_, source);

  if (!extra_deps.empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, extra_deps);
  }

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerFlags(
    std::ostream& out,
    const Tool* tool,
    const SourceFile* optional_def_file) {
  if (tool->AsC()) {
    // First the ldflags from the target and its config.
    RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags,
                                       GetFlagOptions(), out);
  }

  // Followed by library search paths that have been recursively pushed
  // through the dependency tree.
  const OrderedSet<SourceDir> all_lib_dirs = target_->all_lib_dirs();
  if (!all_lib_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput lib_path_output(path_output_.current_dir(),
                               settings_->build_settings()->root_path_utf8(),
                               ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out << " " << tool->lib_dir_switch();
      lib_path_output.WriteDir(out, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }

  const auto& all_framework_dirs = target_->all_framework_dirs();
  if (!all_framework_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput framework_path_output(
        path_output_.current_dir(),
        settings_->build_settings()->root_path_utf8(), ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_framework_dirs.size(); i++) {
      out << " " << tool->framework_dir_switch();
      framework_path_output.WriteDir(out, all_framework_dirs[i],
                                     PathOutput::DIR_NO_LAST_SLASH);
    }
  }

  if (optional_def_file) {
    out_ << " /DEF:";
    path_output_.WriteFile(out, *optional_def_file);
  }
}

void NinjaBinaryTargetWriter::WriteLibs(std::ostream& out, const Tool* tool) {
  // Libraries that have been recursively pushed through the dependency tree.
  EscapeOptions lib_escape_opts;
  lib_escape_opts.mode = ESCAPE_NINJA_COMMAND;
  const OrderedSet<LibFile> all_libs = target_->all_libs();
  for (size_t i = 0; i < all_libs.size(); i++) {
    const LibFile& lib_file = all_libs[i];
    const std::string& lib_value = lib_file.value();
    std::string_view framework_name = GetFrameworkName(lib_value);
    if (lib_file.is_source_file()) {
      out << " " << tool->linker_arg();
      path_output_.WriteFile(out, lib_file.source_file());
    } else if (!framework_name.empty()) {
      // Special-case libraries ending in ".framework" to support Mac: Add the
      // -framework switch and don't add the extension to the output.
      // TODO(crbug.com/gn/119): remove this once all code has been ported to
      // use "frameworks" and "framework_dirs" instead.
      out << " " << tool->framework_switch();
      EscapeStringToStream(out, framework_name, lib_escape_opts);
    } else {
      out << " " << tool->lib_switch();
      EscapeStringToStream(out, lib_value, lib_escape_opts);
    }
  }
}

void NinjaBinaryTargetWriter::WriteFrameworks(std::ostream& out,
                                              const Tool* tool) {
  FrameworksWriter writer(tool->framework_switch());

  // Frameworks that have been recursively pushed through the dependency tree.
  const auto& all_frameworks = target_->all_frameworks();
  for (size_t i = 0; i < all_frameworks.size(); i++) {
    writer(all_frameworks[i], out);
  }
}
