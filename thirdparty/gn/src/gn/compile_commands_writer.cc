// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/compile_commands_writer.h"

#include <sstream>

#include "base/json/string_escape.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "gn/builder.h"
#include "gn/c_substitution_type.h"
#include "gn/c_tool.h"
#include "gn/config_values_extractors.h"
#include "gn/deps_iterator.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_command_util.h"
#include "gn/path_output.h"
#include "gn/substitution_writer.h"

// Structure of JSON output file
// [
//   {
//      "directory": "The build directory."
//      "file": "The main source file processed by this compilation step.
//               Must be absolute or relative to the above build directory."
//      "command": "The compile command executed."
//   }
//   ...
// ]

namespace {

#if defined(OS_WIN)
const char kPrettyPrintLineEnding[] = "\r\n";
#else
const char kPrettyPrintLineEnding[] = "\n";
#endif

struct CompileFlags {
  std::string includes;
  std::string defines;
  std::string cflags;
  std::string cflags_c;
  std::string cflags_cc;
  std::string cflags_objc;
  std::string cflags_objcc;
  std::string framework_dirs;
  std::string frameworks;
};

void SetupCompileFlags(const Target* target,
                       PathOutput& path_output,
                       EscapeOptions opts,
                       CompileFlags& flags) {
  bool has_precompiled_headers =
      target->config_values().has_precompiled_headers();

  std::ostringstream defines_out;
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::defines,
                                             DefineWriter(ESCAPE_SPACE, true),
                                             defines_out);
  base::EscapeJSONString(defines_out.str(), false, &flags.defines);

  std::ostringstream framework_dirs_out;
  RecursiveTargetConfigToStream<SourceDir>(
      target, &ConfigValues::framework_dirs,
      FrameworkDirsWriter(path_output, "-F"), framework_dirs_out);
  base::EscapeJSONString(framework_dirs_out.str(), false,
                         &flags.framework_dirs);

  std::ostringstream frameworks_out;
  RecursiveTargetConfigToStream<std::string>(
      target, &ConfigValues::frameworks,
      FrameworksWriter(ESCAPE_SPACE, true, "-framework "), frameworks_out);
  base::EscapeJSONString(frameworks_out.str(), false, &flags.frameworks);

  std::ostringstream includes_out;
  RecursiveTargetConfigToStream<SourceDir>(target, &ConfigValues::include_dirs,
                                           IncludeWriter(path_output),
                                           includes_out);
  base::EscapeJSONString(includes_out.str(), false, &flags.includes);

  std::ostringstream cflags_out;
  WriteOneFlag(target, &CSubstitutionCFlags, false, Tool::kToolNone,
               &ConfigValues::cflags, opts, path_output, cflags_out,
               /*write_substitution=*/false);
  base::EscapeJSONString(cflags_out.str(), false, &flags.cflags);

  std::ostringstream cflags_c_out;
  WriteOneFlag(target, &CSubstitutionCFlagsC, has_precompiled_headers,
               CTool::kCToolCc, &ConfigValues::cflags_c, opts, path_output,
               cflags_c_out, /*write_substitution=*/false);
  base::EscapeJSONString(cflags_c_out.str(), false, &flags.cflags_c);

  std::ostringstream cflags_cc_out;
  WriteOneFlag(target, &CSubstitutionCFlagsCc, has_precompiled_headers,
               CTool::kCToolCxx, &ConfigValues::cflags_cc, opts, path_output,
               cflags_cc_out, /*write_substitution=*/false);
  base::EscapeJSONString(cflags_cc_out.str(), false, &flags.cflags_cc);

  std::ostringstream cflags_objc_out;
  WriteOneFlag(target, &CSubstitutionCFlagsObjC, has_precompiled_headers,
               CTool::kCToolObjC, &ConfigValues::cflags_objc, opts, path_output,
               cflags_objc_out,
               /*write_substitution=*/false);
  base::EscapeJSONString(cflags_objc_out.str(), false, &flags.cflags_objc);

  std::ostringstream cflags_objcc_out;
  WriteOneFlag(target, &CSubstitutionCFlagsObjCc, has_precompiled_headers,
               CTool::kCToolObjCxx, &ConfigValues::cflags_objcc, opts,
               path_output, cflags_objcc_out, /*write_substitution=*/false);
  base::EscapeJSONString(cflags_objcc_out.str(), false, &flags.cflags_objcc);
}

void WriteFile(const SourceFile& source,
               PathOutput& path_output,
               std::string* compile_commands) {
  std::ostringstream rel_source_path;
  path_output.WriteFile(rel_source_path, source);
  compile_commands->append("    \"file\": \"");
  compile_commands->append(rel_source_path.str());
}

void WriteDirectory(std::string build_dir, std::string* compile_commands) {
  compile_commands->append("\",");
  compile_commands->append(kPrettyPrintLineEnding);
  compile_commands->append("    \"directory\": \"");
  compile_commands->append(build_dir);
  compile_commands->append("\",");
}

void WriteCommand(const Target* target,
                  const SourceFile& source,
                  const CompileFlags& flags,
                  std::vector<OutputFile>& tool_outputs,
                  PathOutput& path_output,
                  SourceFile::Type source_type,
                  const char* tool_name,
                  EscapeOptions opts,
                  std::string* compile_commands) {
  EscapeOptions no_quoting(opts);
  no_quoting.inhibit_quoting = true;
  const Tool* tool = target->toolchain()->GetTool(tool_name);
  std::ostringstream command_out;

  for (const auto& range : tool->command().ranges()) {
    // TODO: this is emitting a bonus space prior to each substitution.
    if (range.type == &SubstitutionLiteral) {
      EscapeStringToStream(command_out, range.literal, no_quoting);
    } else if (range.type == &SubstitutionOutput) {
      path_output.WriteFiles(command_out, tool_outputs);
    } else if (range.type == &CSubstitutionDefines) {
      command_out << flags.defines;
    } else if (range.type == &CSubstitutionFrameworkDirs) {
      command_out << flags.framework_dirs;
    } else if (range.type == &CSubstitutionFrameworks) {
      command_out << flags.frameworks;
    } else if (range.type == &CSubstitutionIncludeDirs) {
      command_out << flags.includes;
    } else if (range.type == &CSubstitutionCFlags) {
      command_out << flags.cflags;
    } else if (range.type == &CSubstitutionCFlagsC) {
      if (source_type == SourceFile::SOURCE_C)
        command_out << flags.cflags_c;
    } else if (range.type == &CSubstitutionCFlagsCc) {
      if (source_type == SourceFile::SOURCE_CPP)
        command_out << flags.cflags_cc;
    } else if (range.type == &CSubstitutionCFlagsObjC) {
      if (source_type == SourceFile::SOURCE_M)
        command_out << flags.cflags_objc;
    } else if (range.type == &CSubstitutionCFlagsObjCc) {
      if (source_type == SourceFile::SOURCE_MM)
        command_out << flags.cflags_objcc;
    } else if (range.type == &SubstitutionLabel ||
               range.type == &SubstitutionLabelName ||
               range.type == &SubstitutionRootGenDir ||
               range.type == &SubstitutionRootOutDir ||
               range.type == &SubstitutionTargetGenDir ||
               range.type == &SubstitutionTargetOutDir ||
               range.type == &SubstitutionTargetOutputName ||
               range.type == &SubstitutionSource ||
               range.type == &SubstitutionSourceNamePart ||
               range.type == &SubstitutionSourceFilePart ||
               range.type == &SubstitutionSourceDir ||
               range.type == &SubstitutionSourceRootRelativeDir ||
               range.type == &SubstitutionSourceGenDir ||
               range.type == &SubstitutionSourceOutDir ||
               range.type == &SubstitutionSourceTargetRelative) {
      EscapeStringToStream(command_out,
                           SubstitutionWriter::GetCompilerSubstitution(
                               target, source, range.type),
                           opts);
    } else {
      // Other flags shouldn't be relevant to compiling C/C++/ObjC/ObjC++
      // source files.
      NOTREACHED() << "Unsupported substitution for this type of target : "
                   << range.type->name;
      continue;
    }
  }
  compile_commands->append(kPrettyPrintLineEnding);
  compile_commands->append("    \"command\": \"");
  compile_commands->append(command_out.str());
}

}  // namespace

void CompileCommandsWriter::RenderJSON(const BuildSettings* build_settings,
                                       std::vector<const Target*>& all_targets,
                                       std::string* compile_commands) {
  // TODO: Determine out an appropriate size to reserve.
  compile_commands->reserve(all_targets.size() * 100);
  compile_commands->append("[");
  compile_commands->append(kPrettyPrintLineEnding);
  bool first = true;
  auto build_dir = build_settings->GetFullPath(build_settings->build_dir())
                       .StripTrailingSeparators();
  std::vector<OutputFile> tool_outputs;  // Prevent reallocation in loop.

  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_PREFORMATTED_COMMAND;

  for (const auto* target : all_targets) {
    if (!target->IsBinary())
      continue;

    // Precompute values that are the same for all sources in a target to avoid
    // computing for every source.

    PathOutput path_output(
        target->settings()->build_settings()->build_dir(),
        target->settings()->build_settings()->root_path_utf8(),
        ESCAPE_NINJA_COMMAND);

    CompileFlags flags;
    SetupCompileFlags(target, path_output, opts, flags);

    for (const auto& source : target->sources()) {
      // If this source is not a C/C++/ObjC/ObjC++ source (not header) file,
      // continue as it does not belong in the compilation database.
      SourceFile::Type source_type = source.type();
      if (source_type != SourceFile::SOURCE_CPP &&
          source_type != SourceFile::SOURCE_C &&
          source_type != SourceFile::SOURCE_M &&
          source_type != SourceFile::SOURCE_MM)
        continue;

      const char* tool_name = Tool::kToolNone;
      if (!target->GetOutputFilesForSource(source, &tool_name, &tool_outputs))
        continue;

      if (!first) {
        compile_commands->append(",");
        compile_commands->append(kPrettyPrintLineEnding);
      }
      first = false;
      compile_commands->append("  {");
      compile_commands->append(kPrettyPrintLineEnding);

      WriteFile(source, path_output, compile_commands);
      WriteDirectory(base::StringPrintf("%" PRIsFP, PATH_CSTR(build_dir)),
                     compile_commands);
      WriteCommand(target, source, flags, tool_outputs, path_output,
                   source_type, tool_name, opts, compile_commands);
      compile_commands->append("\"");
      compile_commands->append(kPrettyPrintLineEnding);
      compile_commands->append("  }");
    }
  }

  compile_commands->append(kPrettyPrintLineEnding);
  compile_commands->append("]");
  compile_commands->append(kPrettyPrintLineEnding);
}

bool CompileCommandsWriter::RunAndWriteFiles(
    const BuildSettings* build_settings,
    const Builder& builder,
    const std::string& file_name,
    const std::string& target_filters,
    bool quiet,
    Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null())
    return false;

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();

  std::set<std::string> target_filters_set;
  for (auto& target :
       base::SplitString(target_filters, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    target_filters_set.insert(target);
  }
  std::string json;
  if (target_filters_set.empty()) {
    RenderJSON(build_settings, all_targets, &json);
  } else {
    std::vector<const Target*> preserved_targets =
        FilterTargets(all_targets, target_filters_set);
    RenderJSON(build_settings, preserved_targets, &json);
  }

  if (!WriteFileIfChanged(output_path, json, err))
    return false;
  return true;
}

std::vector<const Target*> CompileCommandsWriter::FilterTargets(
    const std::vector<const Target*>& all_targets,
    const std::set<std::string>& target_filters_set) {
  std::vector<const Target*> preserved_targets;

  std::set<const Target*> visited;
  for (auto& target : all_targets) {
    if (target_filters_set.count(target->label().name())) {
      VisitDeps(target, &visited);
    }
  }

  preserved_targets.reserve(visited.size());
  // Preserve the original ordering of all_targets
  // to allow easier debugging and testing.
  for (auto& target : all_targets) {
    if (visited.count(target)) {
      preserved_targets.push_back(target);
    }
  }
  return preserved_targets;
}

void CompileCommandsWriter::VisitDeps(const Target* target,
                                      std::set<const Target*>* visited) {
  if (!visited->count(target)) {
    visited->insert(target);
    for (const auto& pair : target->GetDeps(Target::DEPS_ALL)) {
      VisitDeps(pair.ptr, visited);
    }
  }
}
