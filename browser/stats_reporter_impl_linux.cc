// Copyright (c) 2019 Vivaldi. All rights reserved.

#include "browser/stats_reporter_impl.h"

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"

namespace {
constexpr char kVivaldiUuidFileName[] = ".vivaldi_user_id";

base::FilePath GetLegacyUserIdFilePath() {
  return base::nix::GetXDGDirectory(base::Environment::Create().get(),
                                    "XDG_DATA_HOME", ".local/share")
      .AppendASCII(kVivaldiUuidFileName);
}
}  // anonymous namespace

namespace vivaldi {
// static
std::string StatsReporterImpl::GetUserIdFromLegacyStorage() {
  std::string user_id;
  if (!base::ReadFileToString(GetLegacyUserIdFilePath(), &user_id))
    return "";
  return user_id;
}

// static
base::FilePath StatsReporterImpl::GetReportingDataFileDir() {
  return base::nix::GetXDGDirectory(base::Environment::Create().get(),
                                    "XDG_DATA_HOME", ".local/share");
}

}  // namespace vivaldi