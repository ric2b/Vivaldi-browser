#include "browser/stats_reporter_impl.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace {
constexpr wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
constexpr wchar_t kUniqueUserValue[] = L"unique_user_id";
}  // anonymous namespace

namespace vivaldi {
// static
std::string StatsReporterImpl::GetUserIdFromLegacyStorage() {
  base::win::RegKey key(HKEY_CURRENT_USER, kVivaldiKey, KEY_READ);

  if (!key.Valid())
    return std::string();

  base::string16 reg_user_id;
  if (key.ReadValue(kUniqueUserValue, &reg_user_id) == ERROR_SUCCESS) {
    return base::UTF16ToUTF8(reg_user_id);
  }
  return std::string();
}

// static
base::FilePath StatsReporterImpl::GetReportingDataFileDir() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  return dir;
}
}  // namespace vivaldi