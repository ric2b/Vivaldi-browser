// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.
//

#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/win/setup/setup_resource.h"

#include <math.h>

#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/shell_util.h"

#include "installer/win/vivaldi_install_l10n.h"
#include "vivaldi/app/grit/vivaldi_installer_strings.h"
#include "vivaldi/installer/win/vivaldi_install_language_names.h"

#include <Windows.h>

#include <Shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <windowsx.h>

namespace installer {

namespace {
static const uint32_t kuint32max = 0xFFFFFFFFu;

struct {
  const wchar_t* code;
  const wchar_t* name;
} kLanguages[] = {
#define HANDLE_VIVALDI_LANGUAGE_NAME(code, name) {L"" code, L"" name},
    DO_VIVALDI_LANGUAGE_NAMES
#undef HANDLE_VIVALDI_LANGUAGE_NAME
};

std::optional<vivaldi::InstallType> GetInstallTypeFromComboIndex(int i) {
  static const vivaldi::InstallType selection_type_map[] = {
      vivaldi::InstallType::kForAllUsers,
      vivaldi::InstallType::kForCurrentUser,
      vivaldi::InstallType::kStandalone,
  };
  if (0 <= i && static_cast<unsigned>(i) < std::size(selection_type_map)) {
    return selection_type_map[i];
  }
  return std::nullopt;
}

DWORD ToComboIndex(vivaldi::InstallType install_type) {
  switch (install_type) {
    case vivaldi::InstallType::kForAllUsers:
      return 0;
    case vivaldi::InstallType::kForCurrentUser:
      return 1;
    case vivaldi::InstallType::kStandalone:
      return 2;
  }
}

int GetCurrentDpi(HWND window) {
  // GetDpiForMonitor() is available only in Windows 8.1.
  static auto get_dpi_for_monitor_func = []() {
    const HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    return reinterpret_cast<decltype(&::GetDpiForMonitor)>(
        shcore_dll ? ::GetProcAddress(shcore_dll, "GetDpiForMonitor")
                   : nullptr);
  }();
  if (get_dpi_for_monitor_func) {
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    UINT dpi_x;
    UINT dpi_y;
    HRESULT hr =
        get_dpi_for_monitor_func(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    if (SUCCEEDED(hr)) {
      return static_cast<int>(dpi_x);
    }
  }
  HDC screen_dc = GetDC(nullptr);
  int dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
  ReleaseDC(nullptr, screen_dc);
  return dpi_x;
}

}  // namespace

VivaldiInstallUIOptions::VivaldiInstallUIOptions() = default;
VivaldiInstallUIOptions::~VivaldiInstallUIOptions() = default;
VivaldiInstallUIOptions::VivaldiInstallUIOptions(VivaldiInstallUIOptions&&) =
    default;
VivaldiInstallUIOptions& VivaldiInstallUIOptions::operator=(
    VivaldiInstallUIOptions&&) = default;

VivaldiInstallDialog* VivaldiInstallDialog::this_ = nullptr;

VivaldiInstallDialog::VivaldiInstallDialog(HINSTANCE instance,
                                           VivaldiInstallUIOptions options)
    : options_(std::move(options)), instance_(instance) {
  ReadLastInstallValues();
  if (options_.install_dir.empty() &&
      options_.install_type != vivaldi::InstallType::kStandalone) {
    // For standalone install there is no default path so we keep it empty
    // forcing the user to make a choice.
    base::FilePath path =
        vivaldi::GetDefaultInstallTopDir(options_.install_type);
    if (!path.empty()) {
      options_.install_dir = std::move(path);
    }
  }

  if (!options_.install_dir.empty()) {
    // An existing type unconditionally overrides any option or registry.
    std::optional<vivaldi::InstallType> existing_install_type =
        vivaldi::FindInstallType(options_.install_dir);
    if (existing_install_type) {
      options_.install_type = *existing_install_type;
    }
  }

  if (!options_.given_register_browser) {
    options_.register_browser =
        (options_.install_type != vivaldi::InstallType::kStandalone);
  }
}

VivaldiInstallDialog::~VivaldiInstallDialog() {
  ClearAll();
}

VivaldiInstallDialog::DlgResult VivaldiInstallDialog::ShowModal() {

   if (base::win::OSInfo::IsRunningEmulatedOnArm64()) {
    MessageBox(
        nullptr,
        GetLocalizedString(IDS_INSTALL_RUNNING_EMULATED_ON_ARM64_BASE).c_str(),
        GetLocalizedString(IDS_INSTALL_INSTALLER_NAME_BASE).c_str(),
        MB_ICONINFORMATION | MB_SETFOREGROUND);
    // Fallthrough, let the user install.
  }

  if (!InstallUtil::IsOSSupported()) {
    MessageBox(nullptr,
        GetLocalizedString(IDS_INSTALL_OUTDATED_WINDOWS_VERSION_BASE).c_str(),
               GetLocalizedString(IDS_INSTALL_INSTALLER_NAME_BASE).c_str(),
        MB_ICONERROR | MB_SETFOREGROUND);
    return INSTALL_DLG_ERROR;
  }

  INITCOMMONCONTROLSEX iccx;
  iccx.dwSize = sizeof(iccx);
  iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES |
               ICC_USEREX_CLASSES;
  ::InitCommonControlsEx(&iccx);

  HWND hdlg =
      CreateDialogParam(instance_, MAKEINTRESOURCE(IDD_DIALOG1), nullptr,
                        DlgProc, reinterpret_cast<LPARAM>(this));
  DCHECK_EQ(hdlg, hdlg_);

  InitDialog();
  ShowWindow(hdlg_, SW_SHOW);
  ShowOptions();

  DoDialog();  // main message loop

  if (dlg_result_ == INSTALL_DLG_INSTALL) {
    SaveInstallValues();
    if (changed_language_) {
      vivaldi::WriteInstallerRegistryLanguage();
    }
  }

  return dlg_result_;
}

void VivaldiInstallDialog::ReadLastInstallValues() {
  base::win::RegKey key = vivaldi::OpenRegistryKeyToRead(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiKey);
  if (!key.Valid())
    return;

  base::FilePath registry_install_dir(vivaldi::ReadRegistryString(
      vivaldi::constants::kVivaldiInstallerDestinationFolder, key));

  std::optional<vivaldi::InstallType> registry_install_type;
  if (std::optional<uint32_t> value = vivaldi::ReadRegistryUint32(
          vivaldi::constants::kVivaldiInstallerInstallType, key)) {
    registry_install_type = GetInstallTypeFromComboIndex(*value);
    if (!registry_install_type) {
      LOG(ERROR) << "Unsupported install type in "
                 << vivaldi::constants::kVivaldiInstallerInstallType
                 << " registry value - " << *value;
    }
  }

  if (options_.install_dir.empty() && !registry_install_dir.empty()) {
    // The installation directory was not given on the command line. We use the
    // registry value unless the installation type was also given on the command
    // line and it does not match the registry type. In that case we want to use
    // the default value for the path.
    if (!options_.given_install_type || !registry_install_type ||
        *registry_install_type == options_.install_type) {
      options_.install_dir = std::move(registry_install_dir);
    }
  }

  if (!options_.given_install_type && registry_install_type) {
    options_.install_type = *registry_install_type;
    options_.given_install_type = true;
  }

  // Initialize the last standalone.
  if (options_.install_type == vivaldi::InstallType::kStandalone &&
      !options_.install_dir.empty()) {
    last_standalone_folder_ = options_.install_dir;
  } else if (registry_install_type &&
             *registry_install_type == vivaldi::InstallType::kStandalone) {
    // The type was overwritten from the command line, but the registry still
    // points to the standalone, so use that.
    last_standalone_folder_ = std::move(registry_install_dir);
  }

  if (!options_.given_register_browser) {
    if (std::optional<bool> bool_value = vivaldi::ReadRegistryBool(
            vivaldi::constants::kVivaldiInstallerDefaultBrowser, key)) {
      options_.register_browser = *bool_value;
      options_.given_register_browser = true;
    }
  }

  if (std::optional<bool> bool_value = vivaldi::ReadRegistryBool(
          vivaldi::constants::kVivaldiInstallerAdvancedMode, key)) {
    advanced_mode_ = *bool_value;
  }

  if (std::optional<bool> bool_value = vivaldi::ReadRegistryBool(
          vivaldi::constants::kVivaldiInstallerDisableStandaloneAutoupdate,
          key)) {
    disable_standalone_autoupdates_ = *bool_value;
  }
}

void VivaldiInstallDialog::SaveInstallValues() {
  base::win::RegKey key = vivaldi::OpenRegistryKeyToWrite(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiKey);
  if (!key.Valid())
    return;
  vivaldi::WriteRegistryString(
      vivaldi::constants::kVivaldiInstallerDestinationFolder,
      options_.install_dir.value(), key);
  vivaldi::WriteRegistryUint32(vivaldi::constants::kVivaldiInstallerInstallType,
                               ToComboIndex(options_.install_type), key);
  vivaldi::WriteRegistryBool(
      vivaldi::constants::kVivaldiInstallerDefaultBrowser,
      options_.register_browser, key);
  vivaldi::WriteRegistryBool(vivaldi::constants::kVivaldiInstallerAdvancedMode,
                             advanced_mode_, key);
  if (disable_standalone_autoupdates_) {
    vivaldi::WriteRegistryBool(
        vivaldi::constants::kVivaldiInstallerDisableStandaloneAutoupdate, true,
        key);
  } else {
    // Remove the key not to advertise this option.
    key.DeleteValue(
        vivaldi::constants::kVivaldiInstallerDisableStandaloneAutoupdate);
  }
}

bool VivaldiInstallDialog::InternalSelectLanguage() {
  if (DCHECK_IS_ON()) {
    for (const auto& pair : kLanguages) {
      std::wstring language_code = pair.code;
      vivaldi::NormalizeLanguageCode(language_code);
      DCHECK_EQ(language_code, pair.code) << "The language code " << pair.code
                                          << " in kLanguages is not normalized";
    }
  }
  std::wstring code = vivaldi::GetInstallerLanguage();
  bool found = false;
  std::map<const std::wstring, const std::wstring>::iterator it;
  for (const auto& pair : kLanguages) {
    if (pair.code == code) {
      ComboBox_SelectString(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), -1,
                            pair.name);
      found = true;
      break;
    }
  }
  return found;
}

void VivaldiInstallDialog::InitDialog() {
  dialog_ended_ = false;
  is_upgrade_ = vivaldi::IsVivaldiInstalled(options_.install_dir);

  std::map<const std::wstring, const std::wstring>::iterator it;
  for (const auto& pair : kLanguages) {
    ComboBox_AddString(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), pair.name);
  }
  if (!InternalSelectLanguage()) {
    ::vivaldi::SetInstallerLanguage(L"en-us");
    changed_language_ = true;
    InternalSelectLanguage();
  }

  TranslateDialog();

  SetDlgItemText(hdlg_, IDC_EDIT_DEST_FOLDER,
                 options_.install_dir.value().c_str());

  SendMessage(GetDlgItem(hdlg_, IDC_CHECK_REGISTER), BM_SETCHECK,
              options_.register_browser ? BST_CHECKED : BST_UNCHECKED, 0);

  SendMessage(GetDlgItem(hdlg_, IDC_CHECK_NO_AUTOUPDATE), BM_SETCHECK,
              disable_standalone_autoupdates_ ? BST_CHECKED : BST_UNCHECKED, 0);
}

void VivaldiInstallDialog::TranslateDialog() {
  txt_tos_accept_update_str_ =
      GetLocalizedString(IDS_INSTALL_TXT_TOS_ACCEPT_AND_UPDATE_BASE);
  btn_tos_accept_update_str_ =
      GetLocalizedString(IDS_INSTALL_TOS_ACCEPT_AND_UPDATE_BASE);
  txt_tos_accept_install_str_ =
      GetLocalizedString(IDS_INSTALL_TXT_TOS_ACCEPT_AND_INSTALL_BASE);
  btn_tos_accept_install_str_ =
      GetLocalizedString(IDS_INSTALL_TOS_ACCEPT_AND_INSTALL_BASE);
  btn_simple_mode_str_ = GetLocalizedString(IDS_INSTALL_MODE_SIMPLE_BASE);
  btn_advanced_mode_str_ = GetLocalizedString(IDS_INSTALL_MODE_ADVANCED_BASE);

  auto caption_string = GetLocalizedString(IDS_INSTALL_INSTALL_CAPTION_BASE);
#if !defined(NDEBUG)
  caption_string += L" [" + vivaldi::GetInstallerLanguage() + L"]";
#endif  // NDEBUG
  SetWindowText(hdlg_, caption_string.c_str());
  SetDlgItemText(hdlg_, IDC_STATIC_LANGUAGE,
                 GetLocalizedString(IDS_INSTALL_LANGUAGE_BASE).c_str());
  SetDlgItemText(
      hdlg_, IDC_STATIC_INSTALLTYPES,
      GetLocalizedString(IDS_INSTALL_INSTALLATION_TYPE_BASE).c_str());
  SetDlgItemText(hdlg_, IDC_STATIC_DEST_FOLDER,
                 GetLocalizedString(IDS_INSTALL_DEST_FOLDER_BASE).c_str());
  SetDlgItemText(hdlg_, IDC_BTN_BROWSE,
                 GetLocalizedString(IDS_INSTALL_BROWSE_BASE).c_str());
  SetDlgItemText(
      hdlg_, IDC_CHECK_REGISTER,
      GetLocalizedString(ShellUtil::CanMakeChromeDefaultUnattended()
                             ? IDS_INSTALL_SET_AS_DEFAULT_BASE
                             : IDS_INSTALL_MAKE_STANDALONE_AVAIL_BASE)
          .c_str());
  SetDlgItemText(hdlg_, IDC_CHECK_NO_AUTOUPDATE,
                 GetLocalizedString(IDS_DISABLE_AUTOUPDATE_BASE).c_str());
  SetDlgItemText(hdlg_, IDC_STATIC_WARN,
                 GetLocalizedString(IDS_INSTALL_RELAUNCH_WARNING_BASE).c_str());
  SetDlgItemText(hdlg_, IDCANCEL,
                 GetLocalizedString(IDS_INSTALL_CANCEL_BASE).c_str());
  SetDlgItemText(hdlg_, IDC_BTN_CANCEL_SIMPLE,
                 GetLocalizedString(IDS_INSTALL_CANCEL_BASE).c_str());
  SetDlgItemText(hdlg_, IDC_SYSLINK_TOS,
                 is_upgrade_ ? txt_tos_accept_update_str_.c_str()
                             : txt_tos_accept_install_str_.c_str());
  SetDlgItemText(hdlg_, IDC_SYSLINK_TOS_SIMPLE,
                 is_upgrade_ ? txt_tos_accept_update_str_.c_str()
                             : txt_tos_accept_install_str_.c_str());
  SetDlgItemText(hdlg_, IDOK,
                 is_upgrade_ ? btn_tos_accept_update_str_.c_str()
                             : btn_tos_accept_install_str_.c_str());
  SetDlgItemText(hdlg_, IDC_BTN_OK_SIMPLE,
                 is_upgrade_ ? btn_tos_accept_update_str_.c_str()
                             : btn_tos_accept_install_str_.c_str());
  SetDlgItemText(hdlg_, IDC_BTN_MODE,
                 advanced_mode_ ? btn_simple_mode_str_.c_str()
                                : btn_advanced_mode_str_.c_str());

  base::Time::Exploded time_exploded;
  base::Time::Now().LocalExplode(&time_exploded);
  auto copyright_year = std::to_wstring(time_exploded.year);

  SetDlgItemText(hdlg_, IDC_SYSLINK_PRIVACY_POLICY_SIMPLE,
                 vivaldi_installer::GetLocalizedStringF(
                     IDS_INSTALL_COPYRIGHT_AND_POLICY_BASE, copyright_year)
                     .c_str());
  SetDlgItemText(hdlg_, IDC_SYSLINK_PRIVACY_POLICY,
                 vivaldi_installer::GetLocalizedStringF(
                     IDS_INSTALL_COPYRIGHT_AND_POLICY_BASE, copyright_year)
                     .c_str());

  ComboBox_ResetContent(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES));
  ComboBox_AddString(
      GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
      GetLocalizedString(IDS_INSTALL_INSTALL_FOR_ALL_USERS_BASE).c_str());
  ComboBox_AddString(
      GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
      GetLocalizedString(IDS_INSTALL_INSTALL_PER_USER_BASE).c_str());
  ComboBox_AddString(
      GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
      GetLocalizedString(IDS_INSTALL_INSTALL_STANDALONE_BASE).c_str());
  ComboBox_SetCurSel(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
                     ToComboIndex(options_.install_type));

  // Reset background image after string changes
  UpdateSize();
}

// Finds the tree view of the SHBrowseForFolder dialog
static BOOL CALLBACK EnumChildProcFindTreeView(HWND hwnd, LPARAM lparam) {
  HWND* tree_view = reinterpret_cast<HWND*>(lparam);
  DCHECK(tree_view);

  const int MAX_BUF_SIZE = 80;
  const wchar_t TREE_VIEW_CLASS_NAME[] = L"SysTreeView32";
  std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_BUF_SIZE]);

  GetClassName(hwnd, buffer.get(), MAX_BUF_SIZE - 1);
  if (std::wstring(buffer.get()) == TREE_VIEW_CLASS_NAME) {
    *tree_view = hwnd;
    return FALSE;
  }
  *tree_view = nullptr;
  return TRUE;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd,
                                       UINT msg,
                                       LPARAM lparam,
                                       LPARAM lpdata) {
  static HWND tree_view = nullptr;
  switch (msg) {
    case BFFM_INITIALIZED:
      if (lpdata)
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpdata);
      EnumChildWindows(hwnd, EnumChildProcFindTreeView, (LPARAM)&tree_view);
      break;
    case BFFM_SELCHANGED:
      if (IsWindow(tree_view)) {
        // Make sure the current selection is scrolled into view
        HTREEITEM item = TreeView_GetSelection(tree_view);
        if (item)
          TreeView_EnsureVisible(tree_view, item);
      }
      break;
  }
  return 0;
}

void VivaldiInstallDialog::ShowBrowseFolderDialog() {
  BROWSEINFO bi;
  memset(&bi, 0, sizeof(bi));

  bi.hwndOwner = hdlg_;
  bi.lpszTitle = GetLocalizedString(IDS_INSTALL_SELECT_A_FOLDER_BASE).c_str();
  bi.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
  bi.lpfn = BrowseCallbackProc;
  bi.lParam = (LPARAM)options_.install_dir.value().c_str();

  OleInitialize(nullptr);

  LPITEMIDLIST pIDL = SHBrowseForFolder(&bi);

  if (!pIDL)
    return;

  std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
  if (!SHGetPathFromIDList(pIDL, buffer.get())) {
    CoTaskMemFree(pIDL);
    return;
  }
  options_.install_dir = base::FilePath(buffer.get());

  CoTaskMemFree(pIDL);
  OleUninitialize();
}

void VivaldiInstallDialog::DoDialog() {
  MSG msg;
  BOOL ret;
  while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
    if (ret == -1) {
      dlg_result_ = INSTALL_DLG_ERROR;
      return;
    }

    if (!IsDialogMessage(hdlg_, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (dialog_ended_)
      return;
  }
}

void VivaldiInstallDialog::OnInstallTypeSelection() {
  int i = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES));
  std::optional<vivaldi::InstallType> type = GetInstallTypeFromComboIndex(i);
  if (!type || *type == options_.install_type)
    return;

  if (options_.install_type == vivaldi::InstallType::kStandalone) {
    last_standalone_folder_ = options_.install_dir;
  }
  options_.install_type = *type;
  if (options_.install_type == vivaldi::InstallType::kStandalone) {
    options_.install_dir = last_standalone_folder_;
  } else {
    base::FilePath path =
        vivaldi::GetDefaultInstallTopDir(options_.install_type);
    if (!path.empty()) {
      options_.install_dir = std::move(path);
    }
  }
  SetDlgItemText(hdlg_, IDC_EDIT_DEST_FOLDER,
                 options_.install_dir.value().c_str());
  UpdateTypeSpecificOptionsVisibility();
}

void VivaldiInstallDialog::OnLanguageSelection() {
  int i = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE));
  if (i != CB_ERR) {
    const int len =
        ComboBox_GetLBTextLen(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), i);
    if (len <= 0)
      return;

    std::wstring buf;
    buf.resize(len);
    ComboBox_GetLBText(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), i, &buf[0]);
    std::map<const std::wstring, const std::wstring>::iterator it;
    for (const auto& pair : kLanguages) {
      if (pair.name == buf) {
        vivaldi::SetInstallerLanguage(pair.code);
        changed_language_ = true;
        TranslateDialog();
        break;
      }
    }
  }
}

bool VivaldiInstallDialog::IsInstallPathValid(const base::FilePath& path) {
  bool path_is_valid = base::PathIsWritable(path);
  if (!path_is_valid)
    MessageBox(hdlg_,
               GetLocalizedString(IDS_INSTALL_DEST_FOLDER_INVALID_BASE).c_str(),
               nullptr, MB_ICONERROR);
  return path_is_valid;
}

InstallStatus VivaldiInstallDialog::ShowEULADialog() {
  VLOG(1) << "About to show EULA";
  std::wstring eula_path = GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return EULA_REJECTED;
  }
  std::wstring inner_frame_path = GetInnerFrameEULAResource();
  if (inner_frame_path.empty()) {
    LOG(ERROR) << "No EULA inner frame path available";
    return EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  EulaHTMLDialog dlg(eula_path, inner_frame_path);
  EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return EULA_REJECTED;
  }
  if (EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return EULA_ACCEPTED;
}

std::wstring VivaldiInstallDialog::GetInnerFrameEULAResource() {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileName(nullptr, full_exe_path, MAX_PATH);
  if (len == 0 || len == MAX_PATH)
    return L"";

  const wchar_t* inner_frame_resource = L"IDR_OEM_EULA_VIV.HTML";
  if (!FindResource(nullptr, inner_frame_resource, RT_HTML))
    return L"";
  // spaces and DOS paths must be url encoded.
  std::wstring url_path = base::UTF8ToWide(base::StringPrintf(
      "res://%ls/#23/%ls", full_exe_path, inner_frame_resource));

  // the cast is safe because url_path has limited length
  // (see the definition of full_exe_path and resource).
  DCHECK(kuint32max > (url_path.size() * 3));
  DWORD count = static_cast<DWORD>(url_path.size() * 3);
  std::unique_ptr<wchar_t[]> url_canon(new wchar_t[count]);
  HRESULT hr = ::UrlCanonicalizeW(url_path.c_str(), url_canon.get(), &count,
                                  URL_ESCAPE_UNSAFE);
  if (SUCCEEDED(hr))
    return std::wstring(url_canon.get());
  return url_path;
}

void VivaldiInstallDialog::ShowOptions() {
  SetDlgItemText(hdlg_, IDC_BTN_MODE,
                 advanced_mode_ ? btn_simple_mode_str_.c_str()
                                : btn_advanced_mode_str_.c_str());

  // Advanced mode controls
  EnableAndShowControl(IDC_COMBO_INSTALLTYPES, advanced_mode_);
  EnableAndShowControl(IDC_EDIT_DEST_FOLDER, advanced_mode_);
  EnableAndShowControl(IDC_BTN_BROWSE, advanced_mode_);
  EnableAndShowControl(IDC_COMBO_LANGUAGE, advanced_mode_);
  ShowControl(IDC_STATIC_LANGUAGE, advanced_mode_);
  ShowControl(IDC_STATIC_INSTALLTYPES, advanced_mode_);
  ShowControl(IDC_STATIC_DEST_FOLDER, advanced_mode_);
  ShowControl(IDC_SYSLINK_TOS, advanced_mode_);
  ShowControl(IDC_SYSLINK_PRIVACY_POLICY, advanced_mode_);
  ShowControl(IDOK, advanced_mode_);
  ShowControl(IDCANCEL, advanced_mode_);
  ShowControl(IDC_STATIC_WARN, is_upgrade_ && advanced_mode_);

  // Simple mode controls
  ShowControl(IDC_SYSLINK_TOS_SIMPLE, !advanced_mode_);
  ShowControl(IDC_SYSLINK_PRIVACY_POLICY_SIMPLE, !advanced_mode_);
  ShowControl(IDC_BTN_OK_SIMPLE, !advanced_mode_);
  ShowControl(IDC_BTN_CANCEL_SIMPLE, !advanced_mode_);

  UpdateTypeSpecificOptionsVisibility();
}

void VivaldiInstallDialog::UpdateTypeSpecificOptionsVisibility() {
  bool register_browser_valid =
      (options_.install_type == vivaldi::InstallType::kStandalone ||
       ShellUtil::CanMakeChromeDefaultUnattended());
  bool no_autoupdate_valid =
      (options_.install_type == vivaldi::InstallType::kStandalone);
  bool for_all_users =
      (options_.install_type == vivaldi::InstallType::kForAllUsers);
  EnableAndShowControl(IDC_CHECK_REGISTER,
                       advanced_mode_ && register_browser_valid);
  EnableAndShowControl(IDC_CHECK_NO_AUTOUPDATE,
                       advanced_mode_ && no_autoupdate_valid);

  // Do not provide the option to change the destination folder from the default
  // in UI as this may allow for insecure installations. To alter the
  // destination the user needs to pass --vivaldi-install-dir option to the
  // installer.
  EnableAndShowControl(IDC_STATIC_DEST_FOLDER, !for_all_users);
  EnableAndShowControl(IDC_EDIT_DEST_FOLDER, !for_all_users);
  EnableAndShowControl(IDC_BTN_BROWSE, !for_all_users);
}

void VivaldiInstallDialog::EnableAndShowControl(int id, bool show) {
  HWND hControl = GetDlgItem(hdlg_, id);
  EnableWindow(hControl, show ? TRUE : FALSE);
  ShowWindow(hControl, show ? SW_SHOW : SW_HIDE);
}

void VivaldiInstallDialog::ShowControl(int id, bool show) {
  HWND hControl = GetDlgItem(hdlg_, id);
  ShowWindow(hControl, show ? SW_SHOW : SW_HIDE);
}

void VivaldiInstallDialog::ClearAll() {
  if (back_bmp_) {
    DeleteObject(back_bmp_);
    back_bmp_ = nullptr;
  }
}

void VivaldiInstallDialog::UpdateSize() {
  if (back_bmp_)
    ClearAll();

  DCHECK(instance_);

  // Currently we do not scale the controls on dynamic DPI updates. Thus we want
  // to keep the background image and the window size at the initial DPI to
  // continue to match controls even after DPI change. So cache the DPI.
  //
  // TODO(igor@vivaldi.com): Call base::win::EnableHighDPISupport() somewhere
  // and react to WM_DPICHANGED to rescale everything dynamically.
  static const int initial_dpi = GetCurrentDpi(hdlg_);

  // For DPI above 240 we scale bitmaps, but to avoid artifacts we limit the
  // scale factor only to integers.
  int dpi = initial_dpi;
  int bitmap_scale = 1.0;
  constexpr int kMaxUnscaledDpi = 240;
  if (dpi > kMaxUnscaledDpi) {
    bitmap_scale = (dpi + kMaxUnscaledDpi - 1) / kMaxUnscaledDpi;
    dpi /= bitmap_scale;
  }
  int bitmap_id;
  if (dpi <= 96) {
    bitmap_id = advanced_mode_ ? IDB_BITMAP_BKGND_100_ADVANCED
                               : IDB_BITMAP_BKGND_100_SIMPLE;
  } else if (dpi <= 120) {
    bitmap_id = advanced_mode_ ? IDB_BITMAP_BKGND_125_ADVANCED
                               : IDB_BITMAP_BKGND_125_SIMPLE;
  } else if (dpi <= 144) {
    bitmap_id = advanced_mode_ ? IDB_BITMAP_BKGND_150_ADVANCED
                               : IDB_BITMAP_BKGND_150_SIMPLE;
  } else if (dpi <= 192) {
    bitmap_id = advanced_mode_ ? IDB_BITMAP_BKGND_200_ADVANCED
                               : IDB_BITMAP_BKGND_200_SIMPLE;
  } else {
    bitmap_id = advanced_mode_ ? IDB_BITMAP_BKGND_250_ADVANCED
                               : IDB_BITMAP_BKGND_250_SIMPLE;
  }

  back_bmp_ = LoadBitmap(instance_, MAKEINTRESOURCE(bitmap_id));
  CHECK(back_bmp_);  // We cannot recover from the out-of-memory.

  BITMAP bm = {0};
  GetObject(back_bmp_, sizeof(bm), &bm);

  background_bitmap_width_ = bm.bmWidth;
  background_bitmap_height_ = bm.bmHeight;
  window_client_width_ = background_bitmap_width_ * bitmap_scale;
  window_client_height_ = background_bitmap_height_ * bitmap_scale;

  RECT size_rect = {0, 0, window_client_width_, window_client_height_};
  AdjustWindowRectEx(&size_rect, GetWindowLong(hdlg_, GWL_STYLE), FALSE,
                     GetWindowLong(hdlg_, GWL_EXSTYLE));
  SetWindowPos(hdlg_, nullptr, 0, 0, size_rect.right - size_rect.left,
               size_rect.bottom - size_rect.top,
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
  InvalidateRect(hdlg_, nullptr, TRUE);
}

void VivaldiInstallDialog::Center() {
  RECT rect_window;
  GetWindowRect(hdlg_, &rect_window);

  RECT rect_parent;
  HWND hwnd_parent = GetDesktopWindow();
  GetWindowRect(hwnd_parent, &rect_parent);

  int w = rect_window.right - rect_window.left;
  int h = rect_window.bottom - rect_window.top;
  int x = ((rect_parent.right - rect_parent.left) - w) / 2 + rect_parent.left;
  int y = ((rect_parent.bottom - rect_parent.top) - h) / 2 + rect_parent.top;

  MoveWindow(hdlg_, x, y, w, h, FALSE);
}

BOOL VivaldiInstallDialog::OnEraseBkgnd(HDC hdc) {
  if (back_bmp_) {
    HDC hdc_mem = CreateCompatibleDC(hdc);
    HGDIOBJ old_bmp = SelectObject(hdc_mem, back_bmp_);
    StretchBlt(hdc, 0, 0, window_client_width_, window_client_height_, hdc_mem,
               0, 0, background_bitmap_width_, background_bitmap_height_,
               SRCCOPY);
    SelectObject(hdc_mem, old_bmp);
    DeleteDC(hdc_mem);
  }
  return TRUE;
}

HBRUSH VivaldiInstallDialog::OnCtlColor(HWND hwnd_ctl, HDC hdc) {
  int id = GetDlgCtrlID(hwnd_ctl);
  bool is_link = (id == IDC_SYSLINK_PRIVACY_POLICY ||
                  id == IDC_SYSLINK_PRIVACY_POLICY_SIMPLE);
  if (is_link) {
    SetTextColor(hdc, RGB(0x0e, 0x0e, 0x0e));  // dark gray
  }

  // Make controls transparent.
  SetBkMode(hdc, TRANSPARENT);

  bool is_checkbox =
      (id == IDC_CHECK_NO_AUTOUPDATE || id == IDC_CHECK_REGISTER);
  if (is_checkbox) {
    // With checkboxes Windows already painted the background with the parent
    // window background color at this point. Apparently there is no flags like
    // LWS_TRANSPARENT but that works for checkboxes. So to get the effect of a
    // transparent background draw that background again against the area of the
    // control.
    RECT r{};
    GetWindowRect(hwnd_ctl, &r);

    // the above is a window rect; convert to client rect
    POINT p = {.x = r.left, .y = r.top};
    ScreenToClient(hdlg_, &p);
    int saved = SaveDC(hdc);
    if (saved != 0) {
      SetViewportOrgEx(hdc, -p.x, -p.y, nullptr);
      POINT p2 = {.x = r.right, .y = r.bottom};
      ScreenToClient(hdlg_, &p2);
      IntersectClipRect(hdc, p.x, p.y, p2.x, p2.y);
      OnEraseBkgnd(hdc);
      RestoreDC(hdc, saved);
    }
  }

  // Prevent any futher background rendering.
  return (HBRUSH)GetStockObject(NULL_BRUSH);
}

/*static*/
INT_PTR CALLBACK VivaldiInstallDialog::DlgProc(HWND hdlg,
                                               UINT msg,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      this_ = reinterpret_cast<VivaldiInstallDialog*>(lparam);
      DCHECK(this_);
      DCHECK(!this_->hdlg_);
      this_->hdlg_ = hdlg;
      {
        // Tell the syslink control to accept color change.
        LITEM item = {0};
        item.iLink = 0;
        item.mask = LIF_ITEMINDEX | LIF_STATE;
        item.state = LIS_DEFAULTCOLORS;
        item.stateMask = LIS_DEFAULTCOLORS;
        SendMessage(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY), LM_SETITEM, 0,
                    (LPARAM)&item);
        SendMessage(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY_SIMPLE),
                    LM_SETITEM, 0, (LPARAM)&item);
      }
      this_->UpdateSize();
      this_->Center();
      return (INT_PTR)TRUE;

    case WM_ERASEBKGND:
      if (this_)
        return (INT_PTR)this_->OnEraseBkgnd((HDC)wparam);
      break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
      if (this_)
        return (INT_PTR)this_->OnCtlColor((HWND)lparam, (HDC)wparam);
      break;

    case WM_NOTIFY: {
      LPNMHDR pnmh = (LPNMHDR)lparam;
      if (pnmh->idFrom == IDC_SYSLINK_TOS ||
          pnmh->idFrom == IDC_SYSLINK_TOS_SIMPLE) {
        if ((pnmh->code == NM_CLICK) || (pnmh->code == NM_RETURN)) {
          this_->ShowEULADialog();
        }
      } else if (pnmh->idFrom == IDC_SYSLINK_PRIVACY_POLICY ||
                 pnmh->idFrom == IDC_SYSLINK_PRIVACY_POLICY_SIMPLE) {
        if ((pnmh->code == NM_CLICK) || (pnmh->code == NM_RETURN)) {
          ShellExecute(nullptr, L"open", L"https://vivaldi.com/privacy",
                       nullptr, nullptr, SW_SHOWNORMAL);
        }
      }
    } break;

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDOK:
        case IDC_BTN_OK_SIMPLE: {
          this_->dlg_result_ = INSTALL_DLG_INSTALL;
          std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
          if (buffer.get()) {
            GetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER, buffer.get(),
                           MAX_PATH - 1);
            this_->options_.install_dir = base::FilePath(buffer.get());
          }
          this_->options_.register_browser =
              SendMessage(GetDlgItem(hdlg, IDC_CHECK_REGISTER), BM_GETCHECK, 0,
                          0) != 0;
          this_->disable_standalone_autoupdates_ =
              SendMessage(GetDlgItem(hdlg, IDC_CHECK_NO_AUTOUPDATE),
                          BM_GETCHECK, 0, 0) != 0;
          int i = SendMessage(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES),
                              CB_GETCURSEL, 0, 0);
          std::optional<vivaldi::InstallType> type =
              GetInstallTypeFromComboIndex(i);
          if (type) {
            this_->options_.install_type = *type;
          }
        }
          EndDialog(hdlg, 0);
          this_->dialog_ended_ = true;
          break;
        case IDCANCEL:
        case IDC_BTN_CANCEL_SIMPLE:
          if (MessageBox(
                  hdlg,
                  GetLocalizedString(IDS_INSTALL_NOT_FINISHED_PROMPT_BASE)
                      .c_str(),
                  GetLocalizedString(IDS_INSTALL_INSTALLER_NAME_BASE).c_str(),
                  MB_YESNO | MB_ICONQUESTION) == IDYES) {
            this_->dlg_result_ = INSTALL_DLG_CANCEL;
            EndDialog(hdlg, 0);
            this_->dialog_ended_ = true;
          }
          break;
        case IDC_BTN_BROWSE: {
          std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
          if (buffer.get()) {
            GetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER, buffer.get(),
                           MAX_PATH - 1);
            this_->options_.install_dir = base::FilePath(buffer.get());
          }
          this_->ShowBrowseFolderDialog();
          SetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER,
                         this_->options_.install_dir.value().c_str());
        } break;
        case IDC_BTN_MODE:
          this_->advanced_mode_ = !this_->advanced_mode_;
          this_->ShowOptions();
          this_->UpdateSize();
          break;
        case IDC_COMBO_INSTALLTYPES:
          if (HIWORD(wparam) == CBN_SELCHANGE)
            this_->OnInstallTypeSelection();
          break;
        case IDC_COMBO_LANGUAGE:
          if (HIWORD(wparam) == CBN_SELCHANGE)
            this_->OnLanguageSelection();
          break;
        case IDC_EDIT_DEST_FOLDER: {
          if (HIWORD(wparam) == EN_CHANGE) {
            std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
            if (buffer.get()) {
              UINT chars_count = GetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER,
                                                buffer.get(), MAX_PATH - 1);

              base::FilePath new_path(buffer.get());

              if (chars_count == 0 && IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
                EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
              else if (chars_count > 0 &&
                       !IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
                EnableWindow(GetDlgItem(hdlg, IDOK), TRUE);

              this_->is_upgrade_ = vivaldi::IsVivaldiInstalled(new_path);
              SetDlgItemText(hdlg, IDC_SYSLINK_TOS,
                             this_->is_upgrade_
                                 ? this_->txt_tos_accept_update_str_.c_str()
                                 : this_->txt_tos_accept_install_str_.c_str());
              SetDlgItemText(hdlg, IDC_SYSLINK_TOS_SIMPLE,
                             this_->is_upgrade_
                                 ? this_->txt_tos_accept_update_str_.c_str()
                                 : this_->txt_tos_accept_install_str_.c_str());
              SetDlgItemText(hdlg, IDOK,
                             this_->is_upgrade_
                                 ? this_->btn_tos_accept_update_str_.c_str()
                                 : this_->btn_tos_accept_install_str_.c_str());
              SetDlgItemText(hdlg, IDC_BTN_OK_SIMPLE,
                             this_->is_upgrade_
                                 ? this_->btn_tos_accept_update_str_.c_str()
                                 : this_->btn_tos_accept_install_str_.c_str());
              ShowWindow(GetDlgItem(hdlg, IDC_STATIC_WARN),
                         this_->is_upgrade_ ? SW_SHOW : SW_HIDE);
              this_->UpdateTypeSpecificOptionsVisibility();
            }
          }
        } break;
      }
      break;
  }
  return (INT_PTR)FALSE;
}

}  // namespace installer
