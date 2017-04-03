//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"

namespace {
const char vivaldi_uuid_file_name[] = ".vivaldi_user_id";

base::FilePath GetUserIdFilePath() {
  return base::nix::GetXDGDirectory(base::Environment::Create().get(),
                                    "XDG_DATA_HOME", ".local/share")
      .AppendASCII(vivaldi_uuid_file_name);
}
}  // anonymous namespace

namespace extensions {
bool UtilitiesGetUniqueUserIdFunction::ReadUserIdFromOSProfile(
    std::string* user_id) {
  return base::ReadFileToString(GetUserIdFilePath(), user_id);
}

void UtilitiesGetUniqueUserIdFunction::WriteUserIdToOSProfile(
    const std::string& user_id) {
  base::WriteFile(GetUserIdFilePath(), user_id.c_str(), user_id.length());
}
}  // namespace extensions
