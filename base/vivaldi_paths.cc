// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "base/vivaldi_paths.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

using base::PathService;

namespace vivaldi {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_VIVALDI_TEST_DATA:
      {
        // PathExists() triggers IO restriction.
        base::VivaldiScopedAllowBlocking allow_blocking;

        if (!PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, result))
          return false;
        // Src dir is in the vivaldi chromium folder
        *result = result->DirName();
        *result = result->Append(FILE_PATH_LITERAL("testdata"));
        *result = result->Append(FILE_PATH_LITERAL("data"));
        if (!PathExists(*result))  // We don't want to create this.
          return false;
        return true;
      }
    default:
      return false;
  }
}

void RegisterVivaldiPaths() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  //  namespace vivaldi
