// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "base/vivaldi_paths.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

using base::PathService;

namespace vivaldi {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_VIVALDI_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, result))
        return false;
      *result = result->DirName();  // Src dir is in the vivaldi chromium folder
      *result = result->Append(FILE_PATH_LITERAL("testdata"));
      *result = result->Append(FILE_PATH_LITERAL("data"));
      if (!PathExists(*result))  // We don't want to create this.
        return false;
      return true;
    default:
      return false;
  }
}

void RegisterVivaldiPaths() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  //  namespace vivaldi
