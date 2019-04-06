// Copyright (c) 2016 Vivaldi. All rights reserved.
// included in chromium/chrome/installer/util/shell_util.cc

#include "components/base32/base32.h"

namespace vivaldi {

namespace {
const wchar_t kRegShellOpenNewWindow[] = L"\\shell\\opennewwindow\\command";
}

// Cleans up DelegateExecute registry values from old Vivaldi installs.
void RemoveDelegateExecuteForVivaldi(BrowserDistribution* dist,
                                     const base::FilePath& chrome_exe,
                                     const base::string16& suffix) {
  bool is_per_user_install = InstallUtil::IsPerUserInstall();
  HKEY root_key = DetermineRegistrationRoot(is_per_user_install);

  // <root>\Software\Classes\Vivaldi<.suffix>\.exe\shell\open\command
  base::string16 delegate_exe_key(ShellUtil::kRegClasses);
  delegate_exe_key.push_back(base::FilePath::kSeparators[0]);
  delegate_exe_key.append(install_static::GetBaseAppId() + suffix);
  delegate_exe_key.append(ShellUtil::kRegExePath);
  delegate_exe_key.append(ShellUtil::kRegShellOpen);
  InstallUtil::DeleteRegistryValue(root_key, delegate_exe_key,
                                   WorkItem::kWow64Default,
                                   ShellUtil::kRegDelegateExecute);

  // <root>\Software\Classes\Vivaldi<.suffix>\.exe\shell\opennewwindow\command
  delegate_exe_key = ShellUtil::kRegClasses;
  delegate_exe_key.push_back(base::FilePath::kSeparators[0]);
  delegate_exe_key.append(install_static::GetBaseAppId() + suffix);
  delegate_exe_key.append(ShellUtil::kRegExePath);
  delegate_exe_key.append(kRegShellOpenNewWindow);
  InstallUtil::DeleteRegistryValue(root_key, delegate_exe_key,
                                   WorkItem::kWow64Default,
                                   ShellUtil::kRegDelegateExecute);

  // <root>\Software\Classes\VivaldiHTM<.suffix>\shell\open\command
  delegate_exe_key = ShellUtil::kRegClasses;
  delegate_exe_key.push_back(base::FilePath::kSeparators[0]);
  delegate_exe_key.append(GetBrowserProgId(suffix));
  delegate_exe_key.append(ShellUtil::kRegShellOpen);
  InstallUtil::DeleteRegistryValue(root_key, delegate_exe_key,
                                   WorkItem::kWow64Default,
                                   ShellUtil::kRegDelegateExecute);
}

// Constructs a suffix based on path.
void GetPathSpecificSuffix(const base::FilePath& path, base::string16* suffix) {
  DCHECK(!path.empty());
  DCHECK(suffix);
  base::MD5Digest md5_digest;
  std::string path_utf8(base::UTF16ToUTF8(path.value()));
  base::MD5Sum(path_utf8.c_str(), strlen(path_utf8.c_str()), &md5_digest);
  const std::string base32_md5(base32::Base32Encode(
      base::StringPiece(reinterpret_cast<char*>(md5_digest.a),
                        arraysize(md5_digest.a)),
      base32::Base32EncodePolicy::OMIT_PADDING));
  DCHECK_EQ(base32_md5.length(), 26U);
  suffix->reserve(base32_md5.length() + 1);
  suffix->assign(1, L'.');
  suffix->append(base::ASCIIToUTF16(base32_md5));
}

// Returns the application name, "Vivaldi" or "Vivaldi S [<folder>]".
base::string16 GetAppName(BrowserDistribution* dist,
                          const base::string16& install_path) {
  base::string16 app_name(dist->GetDisplayName());
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(vivaldi::constants::kVivaldiStandalone)) {
    // extract the install folder name for the standalone install
    base::string16::size_type pos = install_path.rfind(L"\\Application");
    if (pos != base::string16::npos) {
      app_name.append(L" S [");
      app_name.append(install_path.substr(0, pos));
      app_name.append(1, L']');
    }
  }
  return app_name;
}

}  // namespace vivaldi