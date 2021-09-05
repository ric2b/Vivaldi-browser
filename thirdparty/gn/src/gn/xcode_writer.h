// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_XCODE_WRITER_H_
#define TOOLS_GN_XCODE_WRITER_H_

#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

class Builder;
class BuildSettings;
class Err;

// Writes an Xcode workspace to build and debug code.
class XcodeWriter {
 public:
  // Controls some parameters and behaviour of the RunAndWriteFiles().
  struct Options {
    // Name of the generated workspace file. Defaults to "all" is empty.
    std::string workspace_name;

    // Name of the ninja target to use for the "All" target in the generated
    // project. If empty, no target will be passed to ninja which will thus
    // try to build all defined targets.
    std::string root_target_name;

    // Name of the ninja executable. Defaults to "ninja" is empty.
    std::string ninja_executable;

    // Extra parameters to pass to ninja. Deprecated.
    std::string ninja_extra_args;

    // If specified, should be a semicolon-separated list of label patterns.
    // It will be used to filter the list of targets generated in the project
    // (in the same way that the other filtering is done, source and header
    // files for those target will still be listed in the generated project).
    std::string dir_filters_string;
  };

  // Writes an Xcode workspace with a single project file.
  //
  // The project will lists all files referenced for the build (including the
  // sources, headers and some supporting files). The project can be used to
  // build, develop and debug from Xcode (though adding files, changing build
  // settings, etc. still needs to be done via BUILD.gn files).
  //
  // The list of targets is filtered to only include relevant targets for
  // debugging (mostly binaries and bundles) so it is not possible to build
  // individuals targets (i.e. source_set) via Xcode. This filtering is done
  // to improve the performances when loading the solution in Xcode (project
  // like Chromium cannot be opened if all targets are generated).
  //
  // The source and header files are still listed in the generated generated
  // Xcode project, even if the target they are defined in are filtered (not
  // doing so would make it less pleasant to use Xcode to debug without any
  // significant performance improvement).
  //
  // Extra behaviour is controlled by the |options| parameter. See comments
  // of the Options type for more informations.
  //
  // Returns true on success, fails on failure. |err| is set in that case.
  static bool RunAndWriteFiles(const BuildSettings* build_settings,
                               const Builder& builder,
                               Options options,
                               Err* err);

 private:
  DISALLOW_COPY_AND_ASSIGN(XcodeWriter);
};

#endif  // TOOLS_GN_XCODE_WRITER_H_
