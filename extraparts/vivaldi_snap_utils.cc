#include "base/environment.h"
#include "base/files/file_path.h"

namespace vivaldi {

bool IsRunningInSnap() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar("SNAP");
}

bool GetSnapDesktopPathOverride(base::FilePath *path) {
  if (!IsRunningInSnap()) return false;

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string realhome;

  if (!env->GetVar("SNAP_REAL_HOME", &realhome))
    return true;

  *path = base::FilePath(realhome).Append(FILE_PATH_LITERAL("/.local/share/applications"));

  return true;
}

} // namespace vivaldi
