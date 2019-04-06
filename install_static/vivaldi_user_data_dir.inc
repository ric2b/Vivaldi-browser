// Included in "chrome/install_static/user_data_dir.cc"

const wchar_t kStandaloneProfileHelper[] = L"stp.viv";
wchar_t exe_path[MAX_PATH] = {};
::GetModuleFileName(nullptr, exe_path, MAX_PATH);
::PathRemoveFileSpec(exe_path);

std::wstring stp_viv_path(exe_path);
stp_viv_path.push_back(L'\\');
stp_viv_path.append(kStandaloneProfileHelper);

bool is_standalone =
    ::GetFileAttributes(stp_viv_path.c_str()) != INVALID_FILE_ATTRIBUTES;

if (is_standalone) {
  std::wstring stp_user_data(exe_path);
  stp_user_data.push_back(L'\\');
  stp_user_data.append(L"..");
  stp_user_data = MakeAbsoluteFilePath(stp_user_data);
  result->swap(stp_user_data);
  result->push_back(L'\\');
  result->append(L"User Data");
  return true;
}
