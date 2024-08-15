// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/invoke_python.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/exec_process.h"
#include "gn/filesystem_utils.h"

namespace internal {

bool InvokePython(const BuildSettings* build_settings,
                  const base::FilePath& python_script_path,
                  const std::string& python_script_extra_args,
                  const base::FilePath& output_path,
                  bool quiet,
                  Err* err) {
  const base::FilePath& python_path = build_settings->python_path();
  base::CommandLine cmdline(python_path);
  cmdline.AppendArg("--");
  cmdline.AppendArgPath(python_script_path);
  cmdline.AppendArgPath(output_path);
  if (!python_script_extra_args.empty()) {
    cmdline.AppendArg(python_script_extra_args);
  }
  base::FilePath startup_dir =
      build_settings->GetFullPath(build_settings->build_dir());

  std::string output;
  std::string stderr_output;

  int exit_code = 0;
  if (!internal::ExecProcess(cmdline, startup_dir, &output, &stderr_output,
                             &exit_code)) {
    *err =
        Err(Location(), "Could not execute python.",
            "I was trying to execute \"" + FilePathToUTF8(python_path) + "\".");
    return false;
  }

  if (!quiet) {
    printf("%s", output.c_str());
    fprintf(stderr, "%s", stderr_output.c_str());
  }

  if (exit_code != 0) {
    *err = Err(Location(), "Python has quit with exit code " +
                               base::IntToString(exit_code) + ".");
    return false;
  }

  return true;
}

}  // namespace internal
