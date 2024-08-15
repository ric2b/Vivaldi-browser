// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "base/test/vivaldi_testinit.h"

#include "app/vivaldi_apptools.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/vivaldi_switches.h"

namespace vivaldi {
namespace {
bool done_init_path = false;
}

void InitTestEnvironment() {
  vivaldi::CommandLineAppendSwitchNoDup(base::CommandLine::ForCurrentProcess(),
                                        switches::kDisableVivaldi);
  InitTestPathEnvironment();
}

void InitTestPathEnvironment() {
  if (!done_init_path) {
    base::FilePath src_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir);

    base::PathService::Override(base::DIR_SRC_TEST_DATA_ROOT,
                                src_dir.Append(FILE_PATH_LITERAL("chromium")));
    done_init_path = true;
  }
}

}  // namespace vivaldi
