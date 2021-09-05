// Included in "chrome/install_static/user_data_dir.cc"

#include "installer/util/vivaldi_install_constants.h"

namespace {

bool VivaldiCheckStandaloneUserDir(std::wstring* result) {
  wchar_t exe_path[MAX_PATH] = {};
  ::GetModuleFileName(nullptr, exe_path, MAX_PATH);

  // Do not use PathRemoveFileSpec() as only kernel.lib API is allowed here.
  wchar_t* last_backslash = wcsrchr(exe_path, L'\\');
  if (!last_backslash)
    return false;

  *last_backslash = L'\0';
  std::wstring stp_viv_path(exe_path);
  stp_viv_path.push_back(L'\\');
  stp_viv_path.append(vivaldi::constants::kStandaloneMarkerFile);

  bool is_standalone =
      ::GetFileAttributes(stp_viv_path.c_str()) != INVALID_FILE_ATTRIBUTES;

  if (!is_standalone)
    return false;

  last_backslash = wcsrchr(exe_path, L'\\');
  if (!last_backslash)
    return false;

  *last_backslash = L'\0';
  *result = exe_path;
  result->push_back(L'\\');
  result->append(L"User Data");
  return true;
}

}  // namespace