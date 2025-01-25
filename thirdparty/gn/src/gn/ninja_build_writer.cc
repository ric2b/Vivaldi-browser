// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_build_writer.h"

#include <stddef.h>

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "gn/build_settings.h"
#include "gn/builder.h"
#include "gn/err.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/input_file_manager.h"
#include "gn/loader.h"
#include "gn/ninja_utils.h"
#include "gn/pool.h"
#include "gn/scheduler.h"
#include "gn/string_atom.h"
#include "gn/switches.h"
#include "gn/target.h"
#include "gn/trace.h"
#include "util/atomic_write.h"
#include "util/build_config.h"
#include "util/exe_path.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

struct Counts {
  Counts() : count(0), last_seen(nullptr) {}

  // Number of targets of this type.
  int count;

  // The last one we encountered.
  const Target* last_seen;
};

}  // namespace

base::CommandLine GetSelfInvocationCommandLine(
    const BuildSettings* build_settings) {
  const base::FilePath build_path =
      build_settings->build_dir().Resolve(build_settings->root_path(), true);

  base::FilePath exe_path = GetExePath();
  if (build_path.IsAbsolute())
    exe_path = MakeAbsoluteFilePathRelativeIfPossible(build_path, exe_path);

  base::CommandLine cmdline(exe_path.NormalizePathSeparatorsTo('/'));

  // Use "." for the directory to generate. When Ninja runs the command it
  // will have the build directory as the current one. Coding it explicitly
  // will cause everything to get confused if the user renames the directory.
  cmdline.AppendArg("gen");
  cmdline.AppendArg(".");

  base::FilePath root_path = build_settings->root_path();
  if (build_path.IsAbsolute())
    root_path = MakeAbsoluteFilePathRelativeIfPossible(build_path, root_path);

  cmdline.AppendSwitchPath(std::string("--") + switches::kRoot,
                           root_path.NormalizePathSeparatorsTo('/'));
  // Successful automatic invocations shouldn't print output.
  cmdline.AppendSwitch(std::string("-") + switches::kQuiet);

  EscapeOptions escape_shell;
  escape_shell.mode = ESCAPE_NINJA_COMMAND;
#if defined(OS_WIN)
  // The command line code quoting varies by platform. We have one string,
  // possibly with spaces, that we want to quote. The Windows command line
  // quotes again, so we don't want quoting. The Posix one doesn't.
  escape_shell.inhibit_quoting = true;
#endif

  // If both --root and --dotfile are passed, make sure the --dotfile is
  // made relative to the build dir here.
  base::FilePath dotfile_path = build_settings->dotfile_name();
  if (!dotfile_path.empty()) {
    if (build_path.IsAbsolute()) {
      dotfile_path =
          MakeAbsoluteFilePathRelativeIfPossible(build_path, dotfile_path);
    }
    cmdline.AppendSwitchPath(std::string("--") + switches::kDotfile,
                             dotfile_path.NormalizePathSeparatorsTo('/'));
  }

  const base::CommandLine& our_cmdline =
      *base::CommandLine::ForCurrentProcess();
  const base::CommandLine::SwitchMap& switches = our_cmdline.GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
    // Only write arguments we haven't already written. Always skip "args"
    // since those will have been written to the file and will be used
    // implicitly in the future. Keeping --args would mean changes to the file
    // would be ignored.
    if (i->first != switches::kQuiet && i->first != switches::kRoot &&
        i->first != switches::kDotfile && i->first != switches::kArgs) {
      std::string escaped_value =
          EscapeString(FilePathToUTF8(i->second), escape_shell, nullptr);
      cmdline.AppendSwitch(i->first, escaped_value);
    }
  }

  // Add the regeneration switch if not already present. This is so that when
  // the regeneration is invoked by ninja, the gen command is aware that it is a
  // regeneration invocation and not an user invocation. This allows the gen
  // command to elide ninja post processing steps that ninja will perform
  // itself.
  if (!cmdline.HasSwitch(switches::kRegeneration)) {
    cmdline.AppendSwitch(switches::kRegeneration);
  }

  return cmdline;
}

namespace {

std::string GetSelfInvocationCommand(const BuildSettings* build_settings) {
  base::CommandLine cmdline = GetSelfInvocationCommandLine(build_settings);
#if defined(OS_WIN)
  return base::UTF16ToUTF8(cmdline.GetCommandLineString());
#else
  return cmdline.GetCommandLineString();
#endif
}

// Given an output that appears more than once, generates an error message
// that describes the problem and which targets generate it.
Err GetDuplicateOutputError(const std::vector<const Target*>& all_targets,
                            const OutputFile& bad_output) {
  std::vector<const Target*> matches;
  for (const Target* target : all_targets) {
    for (const auto& output : target->computed_outputs()) {
      if (output == bad_output) {
        matches.push_back(target);
        break;
      }
    }
  }

  // There should always be at least two targets generating this file for this
  // function to be called in the first place.
  DCHECK(matches.size() >= 2);
  std::string matches_string;
  for (const Target* target : matches)
    matches_string += "  " + target->label().GetUserVisibleName(true) + "\n";

  Err result(matches[0]->defined_from(), "Duplicate output file.",
             "Two or more targets generate the same output:\n  " +
                 bad_output.value() +
                 "\n\n"
                 "This is can often be fixed by changing one of the target "
                 "names, or by \n"
                 "setting an output_name on one of them.\n"
                 "\nCollisions:\n" +
                 matches_string);
  for (size_t i = 1; i < matches.size(); i++)
    result.AppendSubErr(Err(matches[i]->defined_from(), "Collision."));
  return result;
}

// Given two toolchains with the same name, generates an error message
// that describes the problem.
Err GetDuplicateToolchainError(const SourceFile& source_file,
                               const Toolchain* previous_toolchain,
                               const Toolchain* toolchain) {
  Err result(
      toolchain->defined_from(), "Duplicate toolchain.",
      "Two or more toolchains write to the same directory:\n  " +
          source_file.GetDir().value() +
          "\n\n"
          "This can be fixed by making sure that distinct toolchains have\n"
          "distinct names.\n");
  result.AppendSubErr(
      Err(previous_toolchain->defined_from(), "Previous toolchain."));
  return result;
}

}  // namespace

NinjaBuildWriter::NinjaBuildWriter(
    const BuildSettings* build_settings,
    const std::unordered_map<const Settings*, const Toolchain*>&
        used_toolchains,
    const std::vector<const Target*>& all_targets,
    const Toolchain* default_toolchain,
    const std::vector<const Target*>& default_toolchain_targets,
    std::ostream& out,
    std::ostream& dep_out)
    : build_settings_(build_settings),
      used_toolchains_(used_toolchains),
      all_targets_(all_targets),
      default_toolchain_(default_toolchain),
      default_toolchain_targets_(default_toolchain_targets),
      out_(out),
      dep_out_(dep_out),
      path_output_(build_settings->build_dir(),
                   build_settings->root_path_utf8(),
                   ESCAPE_NINJA) {}

NinjaBuildWriter::~NinjaBuildWriter() = default;

bool NinjaBuildWriter::Run(Err* err) {
  WriteNinjaRules();
  WriteAllPools();
  return WriteSubninjas(err) && WritePhonyAndAllRules(err);
}

// static
bool NinjaBuildWriter::RunAndWriteFile(const BuildSettings* build_settings,
                                       const Builder& builder,
                                       Err* err) {
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE_NINJA, "build.ninja");

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();
  std::unordered_map<const Settings*, const Toolchain*> used_toolchains;

  // Find the default toolchain info.
  Label default_toolchain_label = builder.loader()->GetDefaultToolchain();
  const Settings* default_toolchain_settings =
      builder.loader()->GetToolchainSettings(default_toolchain_label);
  const Toolchain* default_toolchain =
      builder.GetToolchain(default_toolchain_label);

  // Most targets will be in the default toolchain. Add it at the beginning and
  // skip adding it to the list every time in the loop.
  used_toolchains[default_toolchain_settings] = default_toolchain;

  std::vector<const Target*> default_toolchain_targets;
  default_toolchain_targets.reserve(all_targets.size());
  for (const Target* target : all_targets) {
    if (target->settings() == default_toolchain_settings) {
      default_toolchain_targets.push_back(target);
      // The default toolchain will already have been added to the used
      // settings array.
    } else if (used_toolchains.find(target->settings()) ==
               used_toolchains.end()) {
      used_toolchains[target->settings()] =
          builder.GetToolchain(target->settings()->toolchain_label());
    }
  }

  std::stringstream file;
  std::stringstream depfile;
  NinjaBuildWriter gen(build_settings, used_toolchains, all_targets,
                       default_toolchain, default_toolchain_targets, file,
                       depfile);
  if (!gen.Run(err))
    return false;

  // Unconditionally write the build.ninja. Ninja's build-out-of-date
  // checking will re-run GN when any build input is newer than build.ninja, so
  // any time the build is updated, build.ninja's timestamp needs to updated
  // also, even if the contents haven't been changed.
  base::FilePath ninja_file_name(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja")));
  base::CreateDirectory(ninja_file_name.DirName());
  std::string ninja_contents = file.str();
  if (util::WriteFileAtomically(ninja_file_name, ninja_contents.data(),
                                static_cast<int>(ninja_contents.size())) !=
      static_cast<int>(ninja_contents.size()))
    return false;

  // Dep file listing build dependencies.
  base::FilePath dep_file_name(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja.d")));
  std::string dep_contents = depfile.str();
  if (util::WriteFileAtomically(dep_file_name, dep_contents.data(),
                                static_cast<int>(dep_contents.size())) !=
      static_cast<int>(dep_contents.size()))
    return false;

  // Finally, write the empty build.ninja.stamp file. This is the output
  // expected by the first of the two ninja rules used to accomplish
  // regeneration.

  base::FilePath stamp_file_name(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja.stamp")));
  std::string stamp_contents;
  if (util::WriteFileAtomically(stamp_file_name, stamp_contents.data(),
                                static_cast<int>(stamp_contents.size())) !=
      static_cast<int>(stamp_contents.size()))
    return false;

  return true;
}

// static
std::string NinjaBuildWriter::ExtractRegenerationCommands(
    std::istream& build_ninja_in) {
  std::ostringstream out;
  int num_blank_lines = 0;
  for (std::string line; std::getline(build_ninja_in, line);) {
    out << line << '\n';
    if (line.empty())
      ++num_blank_lines;
    // Warning: Significant magic number. Represents the number of blank lines
    // in the ninja rules written by NinjaBuildWriter::WriteNinjaRules below.
    if (num_blank_lines == 4)
      return out.str();
  }
  return std::string{};
}

void NinjaBuildWriter::WriteNinjaRules() {
  out_ << "ninja_required_version = "
       << build_settings_->ninja_required_version().Describe() << "\n\n";
  out_ << "rule gn\n";
  out_ << "  command = " << GetSelfInvocationCommand(build_settings_) << "\n";
  // Putting gn rule to console pool for colorful output on regeneration
  out_ << "  pool = console\n";
  out_ << "  description = Regenerating ninja files\n\n";

  // A comment is left in the build.ninja explaining the two statement setup to
  // avoid confusion, since build.ninja is written earlier than the ninja rules
  // might make someone think.
  out_ << "# The 'gn' rule also writes build.ninja, unbeknownst to ninja. The\n"
       << "# build.ninja edge is separate to prevent ninja from deleting it\n"
       << "# (due to depfile usage) if interrupted. gn uses atomic writes to\n"
       << "# ensure that build.ninja is always valid even if interrupted.\n"
       << "build build.ninja.stamp: gn\n"
       << "  generator = 1\n"
       << "  depfile = build.ninja.d\n"
       << "\n"
       << "build build.ninja: phony build.ninja.stamp\n"
       << "  generator = 1\n";

  // Input build files. These go in the ".d" file. If we write them
  // as dependencies in the .ninja file itself, ninja will expect
  // the files to exist and will error if they don't. When files are
  // listed in a depfile, missing files are ignored.
  dep_out_ << "build.ninja.stamp:";

  // Other files read by the build.
  std::vector<base::FilePath> other_files = g_scheduler->GetGenDependencies();

  const InputFileManager* input_file_manager =
      g_scheduler->input_file_manager();

  VectorSetSorter<base::FilePath> sorter(
      input_file_manager->GetInputFileCount() + other_files.size());

  input_file_manager->AddAllPhysicalInputFileNamesToVectorSetSorter(&sorter);
  sorter.Add(other_files.begin(), other_files.end());

  const base::FilePath build_path =
      build_settings_->build_dir().Resolve(build_settings_->root_path(), true);

  EscapeOptions depfile_escape;
  depfile_escape.mode = ESCAPE_DEPFILE;
  auto item_callback = [this, &depfile_escape,
                        &build_path](const base::FilePath& input_file) {
    const base::FilePath file =
        MakeAbsoluteFilePathRelativeIfPossible(build_path, input_file);
    dep_out_ << " ";
    EscapeStringToStream(dep_out_,
                         FilePathToUTF8(file.NormalizePathSeparatorsTo('/')),
                         depfile_escape);
  };

  sorter.IterateOver(item_callback);

  out_ << std::endl;
}

void NinjaBuildWriter::WriteAllPools() {
  // Compute the pools referenced by all tools of all used toolchains.
  std::set<const Pool*> used_pools;
  for (const auto& pair : used_toolchains_) {
    for (const auto& tool : pair.second->tools()) {
      if (tool.second->pool().ptr)
        used_pools.insert(tool.second->pool().ptr);
    }
  }

  for (const Target* target : all_targets_) {
    if (target->IsBinary() || target->output_type() == Target::ACTION ||
        target->output_type() == Target::ACTION_FOREACH) {
      const LabelPtrPair<Pool>& pool = target->pool();
      if (pool.ptr)
        used_pools.insert(pool.ptr);
    }
  }

  // Write pools sorted by their name, to make output deterministic.
  std::vector<const Pool*> sorted_pools(used_pools.begin(), used_pools.end());
  auto pool_name = [this](const Pool* pool) {
    return pool->GetNinjaName(default_toolchain_->label());
  };
  std::sort(sorted_pools.begin(), sorted_pools.end(),
            [&pool_name](const Pool* a, const Pool* b) {
              return pool_name(a) < pool_name(b);
            });
  for (const Pool* pool : sorted_pools) {
    std::string name = pool_name(pool);
    if (name == "console")
      continue;
    out_ << "pool " << name << std::endl
         << "  depth = " << pool->depth() << std::endl
         << std::endl;
  }
}

bool NinjaBuildWriter::WriteSubninjas(Err* err) {
  // Write toolchains sorted by their name, to make output deterministic.
  std::vector<std::pair<const Settings*, const Toolchain*>> sorted_settings(
      used_toolchains_.begin(), used_toolchains_.end());
  std::sort(sorted_settings.begin(), sorted_settings.end(),
            [this](const std::pair<const Settings*, const Toolchain*>& a,
                   const std::pair<const Settings*, const Toolchain*>& b) {
              // Always put the default toolchain first.
              if (b.second == default_toolchain_)
                return false;
              if (a.second == default_toolchain_)
                return true;
              return GetNinjaFileForToolchain(a.first) <
                     GetNinjaFileForToolchain(b.first);
            });

  SourceFile previous_subninja;
  const Toolchain* previous_toolchain = nullptr;

  for (const auto& pair : sorted_settings) {
    SourceFile subninja = GetNinjaFileForToolchain(pair.first);

    // Since the toolchains are sorted, comparing to the previous subninja is
    // enough to find duplicates.
    if (subninja == previous_subninja) {
      *err =
          GetDuplicateToolchainError(subninja, previous_toolchain, pair.second);
      return false;
    }

    out_ << "subninja ";
    path_output_.WriteFile(out_, subninja);
    out_ << std::endl;
    previous_subninja = subninja;
    previous_toolchain = pair.second;
  }
  out_ << std::endl;
  return true;
}

const char kNinjaRules_Help[] =
    R"(Ninja build rules

The "all" and "default" rules

  All generated targets (see "gn help execution") will be added to an implicit
  build rule called "all" so "ninja all" will always compile everything. The
  default rule will be used by Ninja if no specific target is specified (just
  typing "ninja"). If there is a target named "default" in the root build file,
  it will be the default build rule, otherwise the implicit "all" rule will be
  used.

Phony rules

  GN generates Ninja "phony" rules for targets in the default toolchain.  The
  phony rules can collide with each other and with the names of generated files
  so are generated with the following priority:

    1. Actual files generated by the build always take precedence.

    2. Targets in the toplevel //BUILD.gn file.

    3. Targets in toplevel directories matching the names of the directories.
       So "ninja foo" can be used to compile "//foo:foo". This only applies to
       the first level of directories since usually these are the most
       important (so this won't apply to "//foo/bar:bar").

    4. The short names of executables if there is only one executable with that
       short name. Use "ninja doom_melon" to compile the
       "//tools/fruit:doom_melon" executable.

       Note that for Apple platforms, create_bundle targets with a product_type
       of "com.apple.product-type.application" are considered as executable
       for this rule (as they define application bundles).

    5. The short names of all targets if there is only one target with that
       short name.

    6. Full label name with no leading slashes. So you can use
       "ninja tools/fruit:doom_melon" to build "//tools/fruit:doom_melon".

    7. Labels with an implicit name part (when the short names match the
       directory). So you can use "ninja foo/bar" to compile "//foo/bar:bar".

  These "phony" rules are provided only for running Ninja since this matches
  people's historical expectations for building. For consistency with the rest
  of the program, GN introspection commands accept explicit labels.

  To explicitly compile a target in a non-default toolchain, you must give
  Ninja the exact name of the output file relative to the build directory.
)";

bool NinjaBuildWriter::WritePhonyAndAllRules(Err* err) {
  // Track rules as we generate them so we don't accidentally write a phony
  // rule that collides with something else.
  // GN internally generates an "all" target, so don't duplicate it.
  std::set<StringAtom, StringAtom::PtrCompare> written_rules;
  written_rules.insert(StringAtom("all"));

  // Set if we encounter a target named "//:default".
  const Target* default_target = nullptr;

  // Targets in the root build file.
  std::vector<const Target*> toplevel_targets;

  // Targets with names matching their toplevel directories. For example
  // "//foo:foo". Expect this is the naming scheme for "big components."
  std::vector<const Target*> toplevel_dir_targets;

  // Tracks the number of each target with the given short name, as well
  // as the short names of executables (which will be a subset of short_names).
  std::map<std::string, Counts> short_names;
  std::map<std::string, Counts> exes;

  // ----------------------------------------------------
  // If you change this algorithm, update the help above!
  // ----------------------------------------------------

  for (const Target* target : default_toolchain_targets_) {
    const Label& label = target->label();
    const std::string& short_name = label.name();

    if (label.dir() == build_settings_->root_target_label().dir() &&
        short_name == "default")
      default_target = target;

    // Count the number of targets with the given short name.
    Counts& short_names_counts = short_names[short_name];
    short_names_counts.count++;
    short_names_counts.last_seen = target;

    // Count executables with the given short name.
    if (target->output_type() == Target::EXECUTABLE) {
      Counts& exes_counts = exes[short_name];
      exes_counts.count++;
      exes_counts.last_seen = target;
    }

    if (target->output_type() == Target::CREATE_BUNDLE &&
        target->bundle_data().is_application()) {
      Counts& exes_counts = exes[short_name];
      exes_counts.count++;
      exes_counts.last_seen = target;
    }

    // Find targets in "important" directories.
    const std::string& dir_string = label.dir().value();
    if (dir_string.size() == 2 && dir_string[0] == '/' &&
        dir_string[1] == '/') {
      toplevel_targets.push_back(target);
    } else if (dir_string.size() == label.name().size() + 3 &&  // Size matches.
               dir_string[0] == '/' &&
               dir_string[1] == '/' &&  // "//" at beginning.
               dir_string[dir_string.size() - 1] == '/' &&  // "/" at end.
               dir_string.compare(2, label.name().size(), label.name()) == 0) {
      toplevel_dir_targets.push_back(target);
    }

    // Add the output files from each target to the written rules so that
    // we don't write phony rules that collide with anything generated by the
    // build.
    //
    // If at this point there is a collision (no phony rules have been
    // generated yet), two targets make the same output so throw an error.
    for (const auto& output : target->computed_outputs()) {
      // Need to normalize because many toolchain outputs will be preceded
      // with "./".
      std::string output_string(output.value());
      NormalizePath(&output_string);
      const StringAtom output_string_atom(output_string);

      if (!written_rules.insert(output_string_atom).second) {
        *err = GetDuplicateOutputError(default_toolchain_targets_, output);
        return false;
      }
    }
  }

  // First prefer the short names of toplevel targets.
  for (const Target* target : toplevel_targets) {
    if (written_rules.insert(target->label().name_atom()).second)
      WritePhonyRule(target, target->label().name_atom());
  }

  // Next prefer short names of toplevel dir targets.
  for (const Target* target : toplevel_dir_targets) {
    if (written_rules.insert(target->label().name_atom()).second)
      WritePhonyRule(target, target->label().name_atom());
  }

  // Write out the names labels of executables. Many toolchains will produce
  // executables in the root build directory with no extensions, so the names
  // will already exist and this will be a no-op.  But on Windows such programs
  // will have extensions, and executables may override the output directory to
  // go into some other place.
  //
  // Putting this after the "toplevel" rules above also means that you can
  // steal the short name from an executable by outputting the executable to
  // a different directory or using a different output name, and writing a
  // toplevel build rule.
  for (const auto& pair : exes) {
    const Counts& counts = pair.second;
    const StringAtom& short_name = counts.last_seen->label().name_atom();
    if (counts.count == 1 && written_rules.insert(short_name).second)
      WritePhonyRule(counts.last_seen, short_name);
  }

  // Write short names when those names are unique and not already taken.
  for (const auto& pair : short_names) {
    const Counts& counts = pair.second;
    const StringAtom& short_name = counts.last_seen->label().name_atom();
    if (counts.count == 1 && written_rules.insert(short_name).second)
      WritePhonyRule(counts.last_seen, short_name);
  }

  // Write the label variants of the target name.
  for (const Target* target : default_toolchain_targets_) {
    const Label& label = target->label();

    // Write the long name "foo/bar:baz" for the target "//foo/bar:baz".
    std::string long_name = label.GetUserVisibleName(false);
    base::TrimString(long_name, "/", &long_name);
    const StringAtom long_name_atom(long_name);
    if (written_rules.insert(long_name_atom).second)
      WritePhonyRule(target, long_name_atom);

    // Write the directory name with no target name if they match
    // (e.g. "//foo/bar:bar" -> "foo/bar").
    if (FindLastDirComponent(label.dir()) == label.name()) {
      std::string medium_name = DirectoryWithNoLastSlash(label.dir());
      base::TrimString(medium_name, "/", &medium_name);
      const StringAtom medium_name_atom(medium_name);

      // That may have generated a name the same as the short name of the
      // target which we already wrote.
      if (medium_name != label.name() &&
          written_rules.insert(medium_name_atom).second)
        WritePhonyRule(target, medium_name_atom);
    }
  }

  // Write the autogenerated "all" rule.
  if (!default_toolchain_targets_.empty()) {
    out_ << "\nbuild all: phony";

    EscapeOptions ninja_escape;
    ninja_escape.mode = ESCAPE_NINJA;
    for (const Target* target : default_toolchain_targets_) {
      if (target->has_dependency_output()) {
        out_ << " $\n    ";
        path_output_.WriteFile(out_, target->dependency_output());
      }
    }
  }
  out_ << std::endl;

  if (default_target) {
    // Use the short name when available
    if (written_rules.find(StringAtom("default")) != written_rules.end()) {
      out_ << "\ndefault default" << std::endl;
    } else if (default_target->has_dependency_output()) {
      // If the default target does not have a dependency output file or phony,
      // then the target specified as default is a no-op. We omit the default
      // statement entirely to avoid ninja runtime failure.
      out_ << "\ndefault ";
      path_output_.WriteFile(out_, default_target->dependency_output());
      out_ << std::endl;
    }
  } else if (!default_toolchain_targets_.empty()) {
    out_ << "\ndefault all" << std::endl;
  }

  return true;
}

void NinjaBuildWriter::WritePhonyRule(const Target* target,
                                      std::string_view phony_name) {
  EscapeOptions ninja_escape;
  ninja_escape.mode = ESCAPE_NINJA;

  // Escape for special chars Ninja will handle.
  std::string escaped = EscapeString(phony_name, ninja_escape, nullptr);

  // If the target doesn't have a dependency_output(), we should
  // still emit the phony rule, but with no dependencies. This allows users to
  // continue to use the phony rule, but it will effectively be a no-op.
  out_ << "build " << escaped << ": phony ";
  if (target->has_dependency_output()) {
    path_output_.WriteFile(out_, target->dependency_output());
  }
  out_ << std::endl;
}
