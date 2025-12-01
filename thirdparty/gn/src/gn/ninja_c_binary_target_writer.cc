// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_c_binary_target_writer.h"

#include <stddef.h>
#include <string.h>

#include <cstring>
#include <set>
#include <sstream>

#include "base/strings/string_util.h"
#include "gn/c_substitution_type.h"
#include "gn/config_values_extractors.h"
#include "gn/deps_iterator.h"
#include "gn/err.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/pool.h"
#include "gn/scheduler.h"
#include "gn/settings.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

struct ModuleDep {
  ModuleDep(const SourceFile* modulemap,
            const std::string& module_name,
            const OutputFile& pcm,
            bool is_self)
      : modulemap(modulemap),
        module_name(module_name),
        pcm(pcm),
        is_self(is_self) {}

  // The input module.modulemap source file.
  const SourceFile* modulemap;

  // The internal module name, in GN this is the target's label.
  std::string module_name;

  // The compiled version of the module.
  OutputFile pcm;

  // Is this the module for the current target.
  bool is_self;
};

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  return opts;
}

// Returns the language-specific lang recognized by gcc’s -x flag for
// precompiled header files.
const char* GetPCHLangForToolType(const char* name) {
  if (name == CTool::kCToolCc)
    return "c-header";
  if (name == CTool::kCToolCxx)
    return "c++-header";
  if (name == CTool::kCToolObjC)
    return "objective-c-header";
  if (name == CTool::kCToolObjCxx)
    return "objective-c++-header";
  NOTREACHED() << "Not a valid PCH tool type: " << name;
  return "";
}

const SourceFile* GetModuleMapFromTargetSources(const Target* target) {
  for (const SourceFile& sf : target->sources()) {
    if (sf.IsModuleMapType())
      return &sf;
  }
  return nullptr;
}

std::vector<ModuleDep> GetModuleDepsInformation(
    const Target* target,
    const ResolvedTargetData& resolved) {
  std::vector<ModuleDep> ret;
  // Use a set to keep track of added PCM files to ensure uniqueness.
  std::set<OutputFile> added_pcms;

  auto add_if_new = [&added_pcms, &ret](const Target* t, bool is_self) {
    const SourceFile* modulemap = GetModuleMapFromTargetSources(t);
    if (!modulemap)  // Not a module or no .modulemap file.
      return;

    std::string label;
    CHECK(SubstitutionWriter::GetTargetSubstitution(
        t, &SubstitutionLabelNoToolchain, &label));

    const char* tool_type;
    std::vector<OutputFile> modulemap_outputs;
    CHECK(
        t->GetOutputFilesForSource(*modulemap, &tool_type, &modulemap_outputs));
    // Must be only one .pcm from .modulemap.
    CHECK(modulemap_outputs.size() == 1u);
    const OutputFile& pcm_file = modulemap_outputs[0];

    if (added_pcms.insert(pcm_file).second) {
      ret.emplace_back(modulemap, label, pcm_file, is_self);
    }
  };

  if (target->source_types_used().Get(SourceFile::SOURCE_MODULEMAP)) {
    add_if_new(target, true);
  }

  // Process direct dependencies and their publicly inherited modules.
  for (const auto& pairs : resolved.GetModuleDepsInformation(target)) {
    const Target* dep = pairs.target();
    if (dep->source_types_used().Get(SourceFile::SOURCE_MODULEMAP)) {
      add_if_new(dep, false);
    }
  }

  // Sort by pcm path for deterministic output.
  std::sort(ret.begin(), ret.end(), [](const ModuleDep& a, const ModuleDep& b) {
    return a.pcm < b.pcm;
  });

  return ret;
}

}  // namespace

NinjaCBinaryTargetWriter::NinjaCBinaryTargetWriter(const Target* target,
                                                   std::ostream& out)
    : NinjaBinaryTargetWriter(target, out),
      tool_(target->toolchain()->GetToolForTargetFinalOutputAsC(target)) {}

NinjaCBinaryTargetWriter::~NinjaCBinaryTargetWriter() = default;

void NinjaCBinaryTargetWriter::Run() {
  std::vector<ModuleDep> module_dep_info =
      GetModuleDepsInformation(target_, resolved());

  WriteCompilerVars(module_dep_info);

  size_t num_output_uses = target_->sources().size();

  std::vector<OutputFile> input_deps =
      WriteInputsStampOrPhonyAndGetDep(num_output_uses);

  // The input dependencies will be an order-only dependency. This will cause
  // Ninja to make sure the inputs are up to date before compiling this source,
  // but changes in the inputs deps won't cause the file to be recompiled.
  //
  // This is important to prevent changes in unrelated actions that are
  // upstream of this target from causing everything to be recompiled.
  //
  // Why can we get away with this rather than using implicit deps ("|", which
  // will force rebuilds when the inputs change)? For source code, the
  // computed dependencies of all headers will be computed by the compiler,
  // which will cause source rebuilds if any "real" upstream dependencies
  // change.
  //
  // If a .cc file is generated by an input dependency, Ninja will see the
  // input to the build rule doesn't exist, and that it is an output from a
  // previous step, and build the previous step first. This is a "real"
  // dependency and doesn't need | or || to express.
  //
  // The only case where this rule matters is for the first build where no .d
  // files exist, and Ninja doesn't know what that source file depends on. In
  // this case it's sufficient to ensure that the upstream dependencies are
  // built first. This is exactly what Ninja's order-only dependencies
  // expresses.
  //
  // The order only deps are referenced by each source file compile,
  // but also by PCH compiles.  The latter are annoying to count, so omit
  // them here.  This means that binary targets with a single source file
  // that also use PCH files won't have a phony target even though having
  // one would make output ninja file size a bit lower. That's ok, binary
  // targets with a single source are rare.
  std::vector<OutputFile> order_only_deps = WriteInputDepsStampOrPhonyAndGetDep(
      std::vector<const Target*>(), num_output_uses);

  // For GCC builds, the .gch files are not object files, but still need to be
  // added as explicit dependencies below. The .gch output files are placed in
  // |pch_other_files|. This is to prevent linking against them.
  std::vector<OutputFile> pch_obj_files;
  std::vector<OutputFile> pch_other_files;
  WritePCHCommands(input_deps, order_only_deps, &pch_obj_files,
                   &pch_other_files);
  std::vector<OutputFile>* pch_files =
      !pch_obj_files.empty() ? &pch_obj_files : &pch_other_files;

  // Treat all pch output files as explicit dependencies of all
  // compiles that support them. Some notes:
  //
  //  - On Windows, the .pch file is the input to the compile, not the
  //    precompiled header's corresponding object file that we're using here.
  //    But Ninja's depslog doesn't support multiple outputs from the
  //    precompiled header compile step (it outputs both the .pch file and a
  //    corresponding .obj file). So we consistently list the .obj file and the
  //    .pch file we really need comes along with it.
  //
  //  - GCC .gch files are not object files, therefore they are not added to the
  //    object file list.
  std::vector<OutputFile> obj_files;
  std::vector<OutputFile> extra_files;
  std::vector<SourceFile> other_files;
  std::vector<OutputFile>* stamp_files = &obj_files;  // default
  if (!target_->source_types_used().SwiftSourceUsed()) {
    WriteSources(*pch_files, input_deps, order_only_deps, module_dep_info,
                 &obj_files, &other_files);
  } else {
    stamp_files = &extra_files;  // Swift generates more than object files
    WriteSwiftSources(input_deps, order_only_deps, &obj_files, &extra_files);
  }

  // Link all MSVC pch object files. The vector will be empty on GCC toolchains.
  obj_files.insert(obj_files.end(), pch_obj_files.begin(), pch_obj_files.end());
  if (!CheckForDuplicateObjectFiles(obj_files))
    return;

  if (target_->output_type() == Target::SOURCE_SET) {
    WriteSourceSetStamp(*stamp_files);
#ifndef NDEBUG
    // Verify that the function that separately computes a source set's object
    // files match the object files just computed.
    UniqueVector<OutputFile> computed_obj;
    AddSourceSetFiles(target_, &computed_obj);
    DCHECK_EQ(obj_files.size(), computed_obj.size());
    for (const auto& obj : obj_files)
      DCHECK(computed_obj.Contains(obj));
#endif
  } else {
    WriteLinkerStuff(obj_files, other_files, input_deps);
  }
}

void NinjaCBinaryTargetWriter::WriteCompilerVars(
    const std::vector<ModuleDep>& module_dep_info) {
  const SubstitutionBits& subst = target_->toolchain()->substitution_bits();

  WriteCCompilerVars(subst, /*indent=*/false,
                     /*respect_source_types_used=*/true);

  if (!module_dep_info.empty()) {
    // TODO(scottmg): Currently clang modules only working for C++.
    if (target_->source_types_used().Get(SourceFile::SOURCE_CPP) ||
        target_->source_types_used().Get(SourceFile::SOURCE_MODULEMAP)) {
      WriteModuleDepsSubstitution(&CSubstitutionModuleDeps, module_dep_info,
                                  true);
      WriteModuleDepsSubstitution(&CSubstitutionModuleDepsNoSelf,
                                  module_dep_info, false);
    }
  }

  WriteSharedVars(subst);
}

void NinjaCBinaryTargetWriter::WriteModuleDepsSubstitution(
    const Substitution* substitution,
    const std::vector<ModuleDep>& module_dep_info,
    bool include_self) {
  if (target_->toolchain()->substitution_bits().used.count(substitution)) {
    EscapeOptions options;
    options.mode = ESCAPE_NINJA_COMMAND;

    out_ << substitution->ninja_name << " =";
    for (const auto& module_dep : module_dep_info) {
      if (!module_dep.is_self || include_self) {
        out_ << " ";
        EscapeStringToStream(out_, "-fmodule-file=", options);
        path_output_.WriteFile(out_, module_dep.pcm);
      }
    }

    out_ << std::endl;
  }
}

void NinjaCBinaryTargetWriter::WritePCHCommands(
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    std::vector<OutputFile>* object_files,
    std::vector<OutputFile>* other_files) {
  if (!target_->config_values().has_precompiled_headers())
    return;

  const CTool* tool_c = target_->toolchain()->GetToolAsC(CTool::kCToolCc);
  if (tool_c && tool_c->precompiled_header_type() != CTool::PCH_NONE &&
      target_->source_types_used().Get(SourceFile::SOURCE_C)) {
    WritePCHCommand(&CSubstitutionCFlagsC, CTool::kCToolCc,
                    tool_c->precompiled_header_type(), input_deps,
                    order_only_deps, object_files, other_files);
  }
  const CTool* tool_cxx = target_->toolchain()->GetToolAsC(CTool::kCToolCxx);
  if (tool_cxx && tool_cxx->precompiled_header_type() != CTool::PCH_NONE &&
      target_->source_types_used().Get(SourceFile::SOURCE_CPP)) {
    WritePCHCommand(&CSubstitutionCFlagsCc, CTool::kCToolCxx,
                    tool_cxx->precompiled_header_type(), input_deps,
                    order_only_deps, object_files, other_files);
  }

  const CTool* tool_objc = target_->toolchain()->GetToolAsC(CTool::kCToolObjC);
  if (tool_objc && tool_objc->precompiled_header_type() == CTool::PCH_GCC &&
      target_->source_types_used().Get(SourceFile::SOURCE_M)) {
    WritePCHCommand(&CSubstitutionCFlagsObjC, CTool::kCToolObjC,
                    tool_objc->precompiled_header_type(), input_deps,
                    order_only_deps, object_files, other_files);
  }

  const CTool* tool_objcxx =
      target_->toolchain()->GetToolAsC(CTool::kCToolObjCxx);
  if (tool_objcxx && tool_objcxx->precompiled_header_type() == CTool::PCH_GCC &&
      target_->source_types_used().Get(SourceFile::SOURCE_MM)) {
    WritePCHCommand(&CSubstitutionCFlagsObjCc, CTool::kCToolObjCxx,
                    tool_objcxx->precompiled_header_type(), input_deps,
                    order_only_deps, object_files, other_files);
  }
}

void NinjaCBinaryTargetWriter::WritePCHCommand(
    const Substitution* flag_type,
    const char* tool_name,
    CTool::PrecompiledHeaderType header_type,
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    std::vector<OutputFile>* object_files,
    std::vector<OutputFile>* other_files) {
  switch (header_type) {
    case CTool::PCH_MSVC:
      WriteWindowsPCHCommand(flag_type, tool_name, input_deps, order_only_deps,
                             object_files);
      break;
    case CTool::PCH_GCC:
      WriteGCCPCHCommand(flag_type, tool_name, input_deps, order_only_deps,
                         other_files);
      break;
    case CTool::PCH_NONE:
      NOTREACHED() << "Cannot write a PCH command with no PCH header type";
      break;
  }
}

void NinjaCBinaryTargetWriter::WriteGCCPCHCommand(
    const Substitution* flag_type,
    const char* tool_name,
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    std::vector<OutputFile>* gch_files) {
  // Compute the pch output file (it will be language-specific).
  std::vector<OutputFile> outputs;
  GetPCHOutputFiles(target_, tool_name, &outputs);
  if (outputs.empty())
    return;

  gch_files->insert(gch_files->end(), outputs.begin(), outputs.end());

  std::vector<OutputFile> extra_deps;
  std::copy(input_deps.begin(), input_deps.end(),
            std::back_inserter(extra_deps));

  // Build line to compile the file.
  WriteCompilerBuildLine({target_->config_values().precompiled_source()},
                         extra_deps, order_only_deps, tool_name, outputs);

  // This build line needs a custom language-specific flags value. Rule-specific
  // variables are just indented underneath the rule line.
  out_ << "  " << flag_type->ninja_name << " =";

  // Each substitution flag is overwritten in the target rule to replace the
  // implicitly generated -include flag with the -x <header lang> flag required
  // for .gch targets.
  EscapeOptions opts = GetFlagOptions();
  if (tool_name == CTool::kCToolCc) {
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::cflags_c, opts,
                                         out_);
  } else if (tool_name == CTool::kCToolCxx) {
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::cflags_cc,
                                         opts, out_);
  } else if (tool_name == CTool::kCToolObjC) {
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::cflags_objc,
                                         opts, out_);
  } else if (tool_name == CTool::kCToolObjCxx) {
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::cflags_objcc,
                                         opts, out_);
  }

  // Append the command to specify the language of the .gch file.
  out_ << " -x " << GetPCHLangForToolType(tool_name);

  // Write two blank lines to help separate the PCH build lines from the
  // regular source build lines.
  out_ << std::endl << std::endl;
}

void NinjaCBinaryTargetWriter::WriteWindowsPCHCommand(
    const Substitution* flag_type,
    const char* tool_name,
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    std::vector<OutputFile>* object_files) {
  // Compute the pch output file (it will be language-specific).
  std::vector<OutputFile> outputs;
  GetPCHOutputFiles(target_, tool_name, &outputs);
  if (outputs.empty())
    return;

  object_files->insert(object_files->end(), outputs.begin(), outputs.end());

  std::vector<OutputFile> extra_deps;
  std::copy(input_deps.begin(), input_deps.end(),
            std::back_inserter(extra_deps));

  // Build line to compile the file.
  WriteCompilerBuildLine({target_->config_values().precompiled_source()},
                         extra_deps, order_only_deps, tool_name, outputs);

  // This build line needs a custom language-specific flags value. Rule-specific
  // variables are just indented underneath the rule line.
  out_ << "  " << flag_type->ninja_name << " =";

  // Append the command to generate the .pch file.
  // This adds the value to the existing flag instead of overwriting it.
  out_ << " ${" << flag_type->ninja_name << "}";
  out_ << " /Yc" << target_->config_values().precompiled_header();

  // Write two blank lines to help separate the PCH build lines from the
  // regular source build lines.
  out_ << std::endl << std::endl;
}

void NinjaCBinaryTargetWriter::WriteSources(
    const std::vector<OutputFile>& pch_deps,
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    const std::vector<ModuleDep>& module_dep_info,
    std::vector<OutputFile>* object_files,
    std::vector<SourceFile>* other_files) {
  DCHECK(!target_->source_types_used().SwiftSourceUsed());
  object_files->reserve(object_files->size() + target_->sources().size());

  std::vector<OutputFile> tool_outputs;  // Prevent reallocation in loop.
  std::vector<OutputFile> deps;
  for (const auto& source : target_->sources()) {
    DCHECK_NE(source.GetType(), SourceFile::SOURCE_SWIFT);

    // Clear the vector but maintain the max capacity to prevent reallocations.
    deps.resize(0);
    const char* tool_name = Tool::kToolNone;
    if (!target_->GetOutputFilesForSource(source, &tool_name, &tool_outputs)) {
      if (source.IsDefType())
        other_files->push_back(source);
      continue;  // No output for this source.
    }

    std::copy(input_deps.begin(), input_deps.end(), std::back_inserter(deps));

    if (tool_name != Tool::kToolNone) {
      // Only include PCH deps that correspond to the tool type, for instance,
      // do not specify target_name.precompile.cc.obj (a CXX PCH file) as a dep
      // for the output of a C tool type.
      //
      // This makes the assumption that pch_deps only contains pch output files
      // with the naming scheme specified in GetWindowsPCHObjectExtension or
      // GetGCCPCHOutputExtension.
      const CTool* tool = target_->toolchain()->GetToolAsC(tool_name);
      if (tool->precompiled_header_type() != CTool::PCH_NONE) {
        for (const auto& dep : pch_deps) {
          const std::string& output_value = dep.value();
          size_t extension_offset = FindExtensionOffset(output_value);
          if (extension_offset == std::string::npos)
            continue;
          std::string output_extension;
          if (tool->precompiled_header_type() == CTool::PCH_MSVC) {
            output_extension = GetWindowsPCHObjectExtension(
                tool_name, output_value.substr(extension_offset - 1));
          } else if (tool->precompiled_header_type() == CTool::PCH_GCC) {
            output_extension = GetGCCPCHOutputExtension(tool_name);
          }
          if (output_value.compare(
                  output_value.size() - output_extension.size(),
                  output_extension.size(), output_extension) == 0) {
            deps.push_back(dep);
          }
        }
      }

      for (const auto& module_dep : module_dep_info) {
        if (tool_outputs[0] != module_dep.pcm)
          deps.push_back(module_dep.pcm);
      }

      WriteCompilerBuildLine({source}, deps, order_only_deps, tool_name,
                             tool_outputs);
      WritePool(out_);
    }

    // It's theoretically possible for a compiler to produce more than one
    // output, but we'll only link to the first output.
    if (!source.IsModuleMapType()) {
      object_files->push_back(tool_outputs[0]);
    }
  }

  out_ << std::endl;
}

void NinjaCBinaryTargetWriter::WriteSwiftSources(
    const std::vector<OutputFile>& input_deps,
    const std::vector<OutputFile>& order_only_deps,
    std::vector<OutputFile>* object_files,
    std::vector<OutputFile>* output_files) {
  DCHECK(target_->builds_swift_module());
  target_->swift_values().GetOutputs(target_, output_files);

  const BuildSettings* build_settings = settings_->build_settings();
  for (const OutputFile& output : *output_files) {
    const SourceFile output_as_source = output.AsSourceFile(build_settings);
    if (output_as_source.IsObjectType()) {
      object_files->push_back(output);
    }
  }

  UniqueVector<OutputFile> swift_order_only_deps;
  swift_order_only_deps.reserve(order_only_deps.size());
  swift_order_only_deps.Append(order_only_deps.begin(), order_only_deps.end());

  for (const Target* swiftmodule :
       resolved().GetSwiftModuleDependencies(target_)) {
    CHECK(swiftmodule->has_dependency_output());
    {
      swift_order_only_deps.push_back(swiftmodule->dependency_output());
    }
  }

  const Tool* tool = target_->swift_values().GetTool(target_);
  WriteCompilerBuildLine(target_->sources(), input_deps,
                         swift_order_only_deps.vector(), tool->name(),
                         *output_files,
                         /*can_write_source_info=*/false,
                         /*restat_output_allowed=*/true);

  out_ << std::endl;
}

void NinjaCBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  ClassifiedDeps classified_deps = GetClassifiedDeps();

  // The classifier should never put extra object files in a source sets: any
  // source sets that we depend on should appear in our non-linkable deps
  // instead.
  DCHECK(classified_deps.extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (auto* dep : classified_deps.non_linkable_deps) {
    if (dep->has_dependency_output()) {
      order_only_deps.push_back(dep->dependency_output());
    }
  }

  WriteStampOrPhonyForTarget(object_files, order_only_deps);
}

void NinjaCBinaryTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files,
    const std::vector<SourceFile>& other_files,
    const std::vector<OutputFile>& input_deps) {
  std::vector<OutputFile> output_files;
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(
      target_, tool_, tool_->outputs(), &output_files);

  out_ << "build";
  WriteOutputs(output_files);

  out_ << ": " << rule_prefix_ << tool_->name();

  ClassifiedDeps classified_deps = GetClassifiedDeps();

  // Object files.
  path_output_.WriteFiles(out_, object_files);
  path_output_.WriteFiles(out_, classified_deps.extra_object_files);

  // Dependencies.
  std::vector<OutputFile> implicit_deps;
  std::vector<OutputFile> solibs;
  for (const Target* cur : classified_deps.linkable_deps) {
    // All linkable deps should have a link output file.
    DCHECK(!cur->link_output_file().value().empty())
        << "No link output file for "
        << target_->label().GetUserVisibleName(false);

    if (cur->output_type() == Target::RUST_LIBRARY ||
        cur->output_type() == Target::RUST_PROC_MACRO)
      continue;

    if (cur->has_dependency_output() &&
        cur->dependency_output().value() != cur->link_output_file().value()) {
      // This is a shared library with separate link and deps files. Save for
      // later.
      implicit_deps.push_back(cur->dependency_output());
      solibs.push_back(cur->link_output_file());
    } else {
      // Normal case, just link to this target.
      out_ << " ";
      path_output_.WriteFile(out_, cur->link_output_file());
    }
  }

  const SourceFile* optional_def_file = nullptr;
  if (!other_files.empty()) {
    for (const SourceFile& src_file : other_files) {
      if (src_file.IsDefType()) {
        optional_def_file = &src_file;
        implicit_deps.push_back(
            OutputFile(settings_->build_settings(), src_file));
        break;  // Only one def file is allowed.
      }
    }
  }

  // Libraries specified by paths.
  for (const auto& lib : resolved().GetLinkedLibraries(target_)) {
    if (lib.is_source_file()) {
      implicit_deps.push_back(
          OutputFile(settings_->build_settings(), lib.source_file()));
    }
  }

  // If any target creates a framework bundle, then treat it as an implicit
  // dependency via the phony target. This is a pessimisation as it is not
  // always necessary to relink the current target if one of the framework
  // is regenerated, but it ensure that if one of the framework API changes,
  // any dependent target will relink it (see crbug.com/1037607).
  for (const Target* dep : classified_deps.framework_deps) {
    if (dep->has_dependency_output())
      implicit_deps.push_back(dep->dependency_output());
  }

  // The input dependency is only needed if there are no object files, as the
  // dependency is normally provided transitively by the source files.
  std::copy(input_deps.begin(), input_deps.end(),
            std::back_inserter(implicit_deps));

  // Any C++ target which depends on a Rust .rlib has to depend on its entire
  // tree of transitive rlibs found inside the linking target (which excludes
  // rlibs only depended on inside a shared library dependency).
  std::vector<OutputFile> transitive_rustlibs;
  if (target_->IsFinal()) {
    for (const auto& inherited : resolved().GetInheritedLibraries(target_)) {
      const Target* dep = inherited.target();
      if (dep->output_type() == Target::RUST_LIBRARY) {
        CHECK(dep->has_dependency_output_file());
        transitive_rustlibs.push_back(dep->dependency_output_file());
        implicit_deps.push_back(dep->dependency_output_file());
      }
    }
  }

  // Swift modules from dependencies (and possibly self).
  std::vector<OutputFile> swiftmodules;
  if (target_->IsFinal()) {
    for (const Target* dep : classified_deps.swiftmodule_deps) {
      swiftmodules.push_back(dep->swift_values().module_output_file());
      implicit_deps.push_back(dep->swift_values().module_output_file());
    }
    if (target_->builds_swift_module()) {
      swiftmodules.push_back(target_->swift_values().module_output_file());
      implicit_deps.push_back(target_->swift_values().module_output_file());
    }
  }

  // Append implicit dependencies collected above.
  if (!implicit_deps.empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, implicit_deps);
  }

  // Append data dependencies as order-only dependencies.
  // If `async_non_linkable_deps` flag is set, it uses
  // validations instead.
  //
  // This will include data dependencies and input dependencies (like when
  // this target depends on an action). Having the data dependencies in this
  // list ensures that the data is available at runtime when the user builds
  // this target.
  //
  // The action dependencies are not strictly necessary in this case. They
  // should also have been collected via the input deps phony alias that each
  // source file has for an order-only dependency, and since this target depends
  // on the sources, there is already an implicit order-only dependency.
  // However, it's extra work to separate these out and there's no disadvantage
  // to listing them again.
  if (settings_->build_settings()->async_non_linkable_deps()) {
    WriteValidations(classified_deps.non_linkable_deps);
  } else {
    WriteOrderOnlyDependencies(classified_deps.non_linkable_deps);
  }

  // End of the link "build" line.
  out_ << std::endl;

  // The remaining things go in the inner scope of the link line.
  if (target_->output_type() == Target::EXECUTABLE ||
      target_->output_type() == Target::SHARED_LIBRARY ||
      target_->output_type() == Target::LOADABLE_MODULE) {
    out_ << "  ldflags =";
    WriteLinkerFlags(out_, tool_, optional_def_file);
    out_ << std::endl;
    out_ << "  libs =";
    WriteLibs(out_, tool_);
    out_ << std::endl;
    out_ << "  frameworks =";
    WriteFrameworks(out_, tool_);
    out_ << std::endl;
    out_ << "  swiftmodules =";
    WriteSwiftModules(out_, tool_, swiftmodules);
    out_ << std::endl;
  } else if (target_->output_type() == Target::STATIC_LIBRARY) {
    out_ << "  arflags =";
    RecursiveTargetConfigStringsToStream(kRecursiveWriterKeepDuplicates,
                                         target_, &ConfigValues::arflags,
                                         GetFlagOptions(), out_);
    out_ << std::endl;
  }
  WriteOutputSubstitutions();
  WriteLibsList("solibs", solibs);
  WriteLibsList("rlibs", transitive_rustlibs);
  WritePool(out_);
}

void NinjaCBinaryTargetWriter::WriteOutputSubstitutions() {
  const std::string output_extension =
      SubstitutionWriter::GetLinkerSubstitution(target_, tool_,
                                                &SubstitutionOutputExtension);
  out_ << "  output_extension =";
  if (!output_extension.empty()) {
    out_ << " " << output_extension;
  }
  out_ << std::endl;

  const std::string output_dir = SubstitutionWriter::GetLinkerSubstitution(
      target_, tool_, &SubstitutionOutputDir);
  out_ << "  output_dir =";
  if (!output_dir.empty()) {
    out_ << " " << output_dir;
  }
  out_ << std::endl;
}

void NinjaCBinaryTargetWriter::WriteLibsList(
    const std::string& label,
    const std::vector<OutputFile>& libs) {
  if (libs.empty())
    return;

  out_ << "  " << label << " =";
  PathOutput output(path_output_.current_dir(),
                    settings_->build_settings()->root_path_utf8(),
                    ESCAPE_NINJA_COMMAND);
  output.WriteFiles(out_, libs);
  out_ << std::endl;
}

void NinjaCBinaryTargetWriter::WriteOrderOnlyDependencies(
    const UniqueVector<const Target*>& non_linkable_deps) {
  if (non_linkable_deps.empty())
    return;

  out_ << " ||";

  // Non-linkable targets.
  for (auto* non_linkable_dep : non_linkable_deps) {
    if (non_linkable_dep->has_dependency_output()) {
      out_ << " ";
      path_output_.WriteFile(out_, non_linkable_dep->dependency_output());
    }
  }
}

void NinjaCBinaryTargetWriter::WriteValidations(
    const UniqueVector<const Target*>& non_linkable_deps) {
  if (non_linkable_deps.empty())
    return;

  out_ << " |@";

  // Non-linkable targets.
  for (auto* non_linkable_dep : non_linkable_deps) {
    if (non_linkable_dep->has_dependency_output()) {
      out_ << " ";
      path_output_.WriteFile(out_, non_linkable_dep->dependency_output());
    }
  }
}

bool NinjaCBinaryTargetWriter::CheckForDuplicateObjectFiles(
    const std::vector<OutputFile>& files) const {
  std::set<std::string> set;
  for (const auto& file : files) {
    if (!set.insert(file.value()).second) {
      Err err(
          target_->defined_from(), "Duplicate object file",
          "The target " + target_->label().GetUserVisibleName(false) +
              "\ngenerates two object files with the same name:\n  " +
              file.value() +
              "\n"
              "\n"
              "It could be you accidentally have a file listed twice in the\n"
              "sources. Or, depending on how your toolchain maps sources to\n"
              "object files, two source files with the same name in different\n"
              "directories could map to the same object file.\n"
              "\n"
              "In the latter case, either rename one of the files or move one "
              "of\n"
              "the sources to a separate source_set to avoid them both being "
              "in\n"
              "the same target.");
      g_scheduler->FailWithError(err);
      return false;
    }
  }
  return true;
}
