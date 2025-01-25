// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/switches.h"

namespace switches {

const char kArgs[] = "args";
const char kArgs_HelpShort[] = "--args: Specifies build arguments overrides.";
const char kArgs_Help[] =
    R"(--args: Specifies build arguments overrides.

  See "gn help buildargs" for an overview of how build arguments work.

  Most operations take a build directory. The build arguments are taken from
  the previous build done in that directory. If a command specifies --args, it
  will override the previous arguments stored in the build directory, and use
  the specified ones.

  The args specified will be saved to the build directory for subsequent
  commands. Specifying --args="" will clear all build arguments.

Formatting

  The value of the switch is interpreted in GN syntax. For typical usage of
  string arguments, you will need to be careful about escaping of quotes.

Examples

  gn gen out/Default --args="foo=\"bar\""

  gn gen out/Default --args='foo="bar" enable=true blah=7'

  gn check out/Default --args=""
    Clears existing build args from the directory.

  gn desc out/Default --args="some_list=[1, false, \"foo\"]"
)";

#define COLOR_HELP_LONG                                                       \
  "--[no]color: Forces colored output on or off.\n"                           \
  "\n"                                                                        \
  "  Normally GN will try to detect whether it is outputting to a terminal\n" \
  "  and will enable or disable color accordingly. Use of these switches\n"   \
  "  will override the default.\n"                                            \
  "\n"                                                                        \
  "Examples\n"                                                                \
  "\n"                                                                        \
  "  gn gen out/Default --color\n"                                            \
  "\n"                                                                        \
  "  gn gen out/Default --nocolor\n"
const char kColor[] = "color";
const char kColor_HelpShort[] = "--color: Force colored output.";
const char kColor_Help[] = COLOR_HELP_LONG;

const char kDotfile[] = "dotfile";
const char kDotfile_HelpShort[] =
    "--dotfile: Override the name of the \".gn\" file.";
const char kDotfile_Help[] =
    R"(--dotfile: Override the name of the ".gn" file.

  Normally GN loads the ".gn" file from the source root for some basic
  configuration (see "gn help dotfile"). This flag allows you to
  use a different file.
)";

const char kFailOnUnusedArgs[] = "fail-on-unused-args";
const char kFailOnUnusedArgs_HelpShort[] =
    "--fail-on-unused-args: Treat unused build args as fatal errors.";
const char kFailOnUnusedArgs_Help[] =
    R"(--fail-on-unused-args: Treat unused build args as fatal errors.

  If you set a value in a build's "gn args" and never use it in the build (in
  a declare_args() block), GN will normally print an error but not fail the
  build.

  In many cases engineers would use build args to enable or disable features
  that would sometimes get removed. It would by annoying to block work for
  typically benign problems. In Chrome in particular, flags might be configured
  for build bots in a separate infrastructure repository, or a declare_args
  block might be changed in a third party repository. Treating these errors as
  blocking forced complex multi- way patches to land what would otherwise be
  simple changes.

  In some cases, such concerns are not as important, and a mismatch in build
  flags between the invoker of the build and the build files represents a
  critical mismatch that should be immediately fixed. Such users can set this
  flag to force GN to fail in that case.
)";

const char kMarkdown[] = "markdown";
const char kMarkdown_HelpShort[] =
    "--markdown: Write help output in the Markdown format.";
const char kMarkdown_Help[] =
    "--markdown: Write help output in the Markdown format.\n";

const char kNoColor[] = "nocolor";
const char kNoColor_HelpShort[] = "--nocolor: Force non-colored output.";
const char kNoColor_Help[] = COLOR_HELP_LONG;

const char kNinjaExecutable[] = "ninja-executable";
const char kNinjaExecutable_HelpShort[] =
    "--ninja-executable: Set the Ninja executable.";
const char kNinjaExecutable_Help[] =
    R"(--ninja-executable: Set the Ninja executable.

  When set specifies the Ninja executable that will be used to perform some
  post-processing on the generated files for more consistent builds.
)";

const char kScriptExecutable[] = "script-executable";
const char kScriptExecutable_HelpShort[] =
    "--script-executable: Set the executable used to execute scripts.";
const char kScriptExecutable_Help[] =
    R"(--script-executable: Set the executable used to execute scripts.

  Path to specific Python executable or other interpreter to use in
  action targets and exec_script calls. By default GN searches the
  PATH for Python to execute these scripts.

  If set to the empty string, the path of scripts specified in action
  targets and exec_script calls will be executed directly.
)";

const char kQuiet[] = "q";
const char kQuiet_HelpShort[] =
    "-q: Quiet mode. Don't print output on success.";
const char kQuiet_Help[] =
    R"(-q: Quiet mode. Don't print output on success.

  This is useful when running as a part of another script.
)";

const char kRoot[] = "root";
const char kRoot_HelpShort[] = "--root: Explicitly specify source root.";
const char kRoot_Help[] =
    R"(--root: Explicitly specify source root.

  Normally GN will look up in the directory tree from the current directory to
  find a ".gn" file. The source root directory specifies the meaning of "//"
  beginning with paths, and the BUILD.gn file in that directory will be the
  first thing loaded.

  Specifying --root allows GN to do builds in a specific directory regardless
  of the current directory.

Examples

  gn gen //out/Default --root=/home/baracko/src

  gn desc //out/Default --root="C:\Users\BObama\My Documents\foo"
)";

const char kRootTarget[] = "root-target";
const char kRootTarget_HelpShort[] = "--root-target: Override the root target.";
const char kRootTarget_Help[] =
    R"(--root-target: Override the root target.

  The root target is the target initially loaded to begin population of the
  build graph. It defaults to "//:" which normally causes the "//BUILD.gn" file
  to be loaded. It can be specified in the .gn file via the "root" variable (see
  "gn help dotfile").

  If specified, the value of this switch will be take precedence over the value
  in ".gn". The target name (after the colon) is ignored, only the directory
  name is required. Relative paths will be resolved relative to the current "//"
  directory.

  Specifying a different initial BUILD.gn file does not change the meaning of
  the source root (the "//" directory) which can be independently set via the
  --root switch. It also does not prevent the build file located at "//BUILD.gn"
  from being loaded if a target in the build references that directory.

  One use-case of this feature is to load a different set of initial targets
  from project that uses GN without modifying any files.

Examples

  gn gen //out/Default --root-target="//third_party/icu"

  gn gen //out/Default --root-target="//third_party/grpc"
)";

const char kRootPattern[] = "root-pattern";
const char kRootPattern_HelpShort[] =
    "--root-pattern: Add root pattern override.";
const char kRootPattern_Help[] =
    R"(--root-pattern: Add root pattern override.

  The root patterns is a list of label patterns used to control which
  targets are defined when evaluating BUILD.gn files in the default toolchain.

  The list is empty by default, meaning that all targets defined in all
  BUILD.gn files evaluated in the default toolchain will be added to the
  final GN build graph (GN's default behavior for historical reasons).

  When this list is not empty, only targets matching any of the root patterns,
  as well as their transitive dependencies, will be defined in the default
  toolchain instead. This is a way to restrict the size of the final build graph
  for projects with a very large number of target definitions per BUILD.gn file.

  Using --root-pattern overrides the root_patterns value specified in the .gn file.

  The argument must be a GN label pattern, and each --root-pattern=<pattern>
  on the command-line will append a pattern to the list.

Example

  gn gen //out/Default --root-pattern="//:*"
)";

const char kRuntimeDepsListFile[] = "runtime-deps-list-file";
const char kRuntimeDepsListFile_HelpShort[] =
    "--runtime-deps-list-file: Save runtime dependencies for targets in file.";
const char kRuntimeDepsListFile_Help[] =
    R"(--runtime-deps-list-file: Save runtime dependencies for targets in file.

  --runtime-deps-list-file=<filename>

  Where <filename> is a text file consisting of the labels, one per line, of
  the targets for which runtime dependencies are desired.

  See "gn help runtime_deps" for a description of how runtime dependencies are
  computed.

Runtime deps output file

  For each target requested, GN will write a separate runtime dependency file.
  The runtime dependency file will be in the output directory alongside the
  output file of the target, with a ".runtime_deps" extension. For example, if
  the target "//foo:bar" is listed in the input file, and that target produces
  an output file "bar.so", GN will create a file "bar.so.runtime_deps" in the
  build directory.

  For targets that don't generate an output file (such as source set, action,
  copy or group), the runtime deps file will be in the output directory where an
  output file would have been located. For example, the source_set target
  "//foo:bar" would result in a runtime dependency file being written to
  "<output_dir>/obj/foo/bar.runtime_deps". This is probably not useful; the
  use-case for this feature is generally executable targets.

  The runtime dependency file will list one file per line, with no escaping.
  The files will be relative to the root_build_dir. The first line of the file
  will be the main output file of the target itself (in the above example,
  "bar.so").
)";

const char kThreads[] = "threads";
const char kThreads_HelpShort[] =
    "--threads: Specify number of worker threads.";
const char kThreads_Help[] =
    R"(--threads: Specify number of worker threads.

  GN runs many threads to load and run build files. This can make debugging
  challenging. Or you may want to experiment with different values to see how
  it affects performance.

  The parameter is the number of worker threads. This does not count the main
  thread (so there are always at least two).

Examples

  gen gen out/Default --threads=1
)";

const char kTime[] = "time";
const char kTime_HelpShort[] =
    "--time: Outputs a summary of how long everything took.";
const char kTime_Help[] =
    R"(--time: Outputs a summary of how long everything took.

  Hopefully self-explanatory.

Examples

  gn gen out/Default --time
)";

const char kTracelog[] = "tracelog";
const char kTracelog_HelpShort[] =
    "--tracelog: Writes a Chrome-compatible trace log to the given file.";
const char kTracelog_Help[] =
    R"(--tracelog: Writes a Chrome-compatible trace log to the given file.

  The trace log will show file loads, executions, scripts, and writes. This
  allows performance analysis of the generation step.

  To view the trace, open Chrome and navigate to "chrome://tracing/", then
  press "Load" and specify the file you passed to this parameter.

Examples

  gn gen out/Default --tracelog=mytrace.trace
)";

const char kVerbose[] = "v";
const char kVerbose_HelpShort[] = "-v: Verbose logging.";
const char kVerbose_Help[] =
    R"(-v: Verbose logging.

  This will spew logging events to the console for debugging issues.

  Good luck!
)";

const char kVersion[] = "version";
const char kVersion_HelpShort[] =
    "--version: Prints the GN version number and exits.";
// It's impossible to see this since gn_main prints the version and exits
// immediately if this switch is used.
const char kVersion_Help[] = "";

const char kDefaultToolchain[] = "default-toolchain";

const char kRegeneration[] = "regeneration";

const char kAddExportCompileCommands[] = "add-export-compile-commands";

// -----------------------------------------------------------------------------

SwitchInfo::SwitchInfo() : short_help(""), long_help("") {}

SwitchInfo::SwitchInfo(const char* short_help, const char* long_help)
    : short_help(short_help), long_help(long_help) {}

#define INSERT_VARIABLE(var) \
  info_map[k##var] = SwitchInfo(k##var##_HelpShort, k##var##_Help);

const SwitchInfoMap& GetSwitches() {
  static SwitchInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(Args)
    INSERT_VARIABLE(Color)
    INSERT_VARIABLE(Dotfile)
    INSERT_VARIABLE(FailOnUnusedArgs)
    INSERT_VARIABLE(Markdown)
    INSERT_VARIABLE(NinjaExecutable)
    INSERT_VARIABLE(NoColor)
    INSERT_VARIABLE(Root)
    INSERT_VARIABLE(RootTarget)
    INSERT_VARIABLE(Quiet)
    INSERT_VARIABLE(RuntimeDepsListFile)
    INSERT_VARIABLE(ScriptExecutable)
    INSERT_VARIABLE(Threads)
    INSERT_VARIABLE(Time)
    INSERT_VARIABLE(Tracelog)
    INSERT_VARIABLE(Verbose)
    INSERT_VARIABLE(Version)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace switches
