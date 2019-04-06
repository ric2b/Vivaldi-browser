// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//

#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/win/setup/setup_resource.h"

#include <Shellapi.h>
#include <Shlwapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"

using base::PathService;

namespace installer {

namespace {
static const uint32_t kuint32max = 0xFFFFFFFFu;

static const std::vector<std::wstring> kExtraLanguages = {
  {L"en-AU"},
  {L"en-IN"} };

std::map<const std::wstring, const std::wstring> kLanguages = {
  // please keep this map alphabetically sorted by language name!
  {L"sq", L"Albanian"},
  {L"hy", L"Armenian"},
  {L"bg", L"Bulgarian"},
  {L"zh_CN", L"Chinese (Simplified)"},
  {L"zh_TW", L"Chinese (Traditional)"},
  {L"hr", L"Croatian"},
  {L"cs", L"Czech"},
  {L"da", L"Danish"},
  {L"nl", L"Dutch"},
  {L"en-us", L"English"},
  {L"en-AU", L"English (Australia)"},
  {L"en-IN", L"English (India)"},
  {L"et", L"Estonian"},
  {L"fi", L"Finnish"},
  {L"fr", L"French"},
  {L"fy", L"Frisian"},
  {L"de", L"German"},
  {L"el", L"Greek"},
  {L"hu", L"Hungarian"},
  {L"is", L"Icelandic"},
  {L"id", L"Indonesian"},
  {L"it", L"Italian"},
  {L"ja", L"Japanese"},
  {L"ko", L"Korean"},
  {L"lt", L"Lithuanian"},
  {L"no", L"Norwegian (Bokm\U000000E5l)"},
  {L"nn", L"Norwegian (Nynorsk)"},
  {L"fa", L"Persian"},
  {L"pl", L"Polish"},
  {L"pt_BR", L"Portuguese (Brazil)"},
  {L"pt_PT", L"Portuguese (Portugal)"},
  {L"ro", L"Romanian"},
  {L"ru", L"Russian"},
  {L"gd", L"Scots Gaelic"},
  {L"sr", L"Serbian"},
  {L"sk", L"Slovak"},
  {L"sl", L"Slovenian"},
  {L"es", L"Spanish"},
  {L"sv", L"Swedish"},
  {L"tr", L"Turkish"},
  {L"uk", L"Ukrainian"},
  {L"vi", L"Vietnamese"} };
} //namespace

void GetDPI(int* dpi_x, int* dpi_y) {
  HDC screen_dc = GetDC(NULL);
  *dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
  *dpi_y = GetDeviceCaps(screen_dc, LOGPIXELSY);
  if (screen_dc)
    ReleaseDC(NULL, screen_dc);
}

VivaldiInstallDialog* VivaldiInstallDialog::this_ = NULL;

VivaldiInstallDialog::VivaldiInstallDialog(
    HINSTANCE instance,
    const bool set_as_default_browser,
    const InstallType default_install_type,
    const base::FilePath& destination_folder)
    : install_type_(default_install_type),
      set_as_default_browser_(set_as_default_browser),
      instance_(instance) {
  if (destination_folder.empty())
    SetDefaultDestinationFolder();
  else
    destination_folder_ = destination_folder;

  if (default_install_type == INSTALL_STANDALONE)
    last_standalone_folder_ = destination_folder_;

  language_code_ = GetCurrentTranslation();

  int dpi_x = 0;
  int dpi_y = 0;
  GetDPI(&dpi_x, &dpi_y);
  if (dpi_x <= 96)
    dpi_scale_ = DPI_NORMAL;
  else if (dpi_x <= 120)
    dpi_scale_ = DPI_MEDIUM;
  else if (dpi_x <= 144)
    dpi_scale_ = DPI_LARGE;
  else if (dpi_x <= 192)
    dpi_scale_ = DPI_XL;
  else
    dpi_scale_ = DPI_XXL;

  enable_set_as_default_checkbox_ =
      (base::win::GetVersion() < base::win::VERSION_WIN10);

  enable_register_browser_checkbox_ = IsRegisterBrowserValid();
}

VivaldiInstallDialog::~VivaldiInstallDialog() {
  ClearAll();
}

VivaldiInstallDialog::DlgResult VivaldiInstallDialog::ShowModal() {
  GetLastInstallValues();

  enable_register_browser_checkbox_ = IsRegisterBrowserValid();

  hdlg_ = CreateDialogParam(instance_, MAKEINTRESOURCE(IDD_DIALOG1), NULL,
                            DlgProc, (LPARAM)this);

  InitDialog();
  ShowWindow(hdlg_, SW_SHOW);
  ShowOptions(hdlg_, advanced_mode_);

  DoDialog();  // main message loop

  if (dlg_result_ == INSTALL_DLG_INSTALL)
    SaveInstallValues();

  return dlg_result_;
}

void VivaldiInstallDialog::SetDefaultDestinationFolder() {
  wchar_t szPath[MAX_PATH];
  int csidl = 0;

  switch (install_type_) {
    case INSTALL_FOR_ALL_USERS:
      csidl = CSIDL_PROGRAM_FILES;
      break;
    case INSTALL_FOR_CURRENT_USER:
      csidl = CSIDL_LOCAL_APPDATA;
      break;
    case INSTALL_STANDALONE:
      destination_folder_ = last_standalone_folder_;
      break;
    default:
      NOTREACHED();
  }

  if (csidl && (SUCCEEDED(SHGetFolderPath(NULL, csidl, NULL, 0, szPath)))) {
    destination_folder_ = base::FilePath(szPath);
    destination_folder_ = destination_folder_.Append(L"Vivaldi");
  }
  if (hdlg_) {
    SetDlgItemText(hdlg_, IDC_EDIT_DEST_FOLDER,
                   destination_folder_.value().c_str());
  }
}

bool VivaldiInstallDialog::GetLastInstallValues() {
  base::win::RegKey key(HKEY_CURRENT_USER,
                        vivaldi::constants::kVivaldiKey,
                        KEY_QUERY_VALUE);
  if (key.Valid()) {
    std::wstring str_dest_folder;
    if (key.ReadValue(vivaldi::constants::kVivaldiInstallerDestinationFolder,
                      &str_dest_folder) ==
        ERROR_SUCCESS) {
      destination_folder_ = base::FilePath(str_dest_folder);
    }

    key.ReadValueDW(vivaldi::constants::kVivaldiInstallerInstallType,
                    reinterpret_cast<DWORD*>(&install_type_));
    key.ReadValueDW(vivaldi::constants::kVivaldiInstallerDefaultBrowser,
                    reinterpret_cast<DWORD*>(&set_as_default_browser_));
    key.ReadValueDW(vivaldi::constants::kVivaldiInstallerAdvancedMode,
                    reinterpret_cast<DWORD*>(&advanced_mode_));

    if (install_type_ == INSTALL_STANDALONE)
      last_standalone_folder_ = destination_folder_;

    return true;
  }
  return false;
}

void VivaldiInstallDialog::SaveInstallValues() {
  base::win::RegKey key(HKEY_CURRENT_USER,
                        vivaldi::constants::kVivaldiKey,
                        KEY_ALL_ACCESS);
  if (key.Valid()) {
    key.WriteValue(vivaldi::constants::kVivaldiInstallerDestinationFolder,
                   destination_folder_.value().c_str());
    key.WriteValue(vivaldi::constants::kVivaldiInstallerInstallType,
                   (DWORD)install_type_);
    key.WriteValue(vivaldi::constants::kVivaldiInstallerDefaultBrowser,
                   set_as_default_browser_ ? 1 : 0);
    key.WriteValue(vivaldi::constants::kVivaldiInstallerAdvancedMode,
                   advanced_mode_ ? 1 : 0);
    key.WriteValue(google_update::kRegLangField, language_code_.c_str());
  }
}

bool VivaldiInstallDialog::InternalSelectLanguage(const std::wstring& code) {
  LOG(INFO) << "InternalSelectLanguage: code: " << code;
  bool found = false;
  std::map<const std::wstring, const std::wstring>::iterator it;
  for (it = kLanguages.begin(); it != kLanguages.end(); it++) {
    if (it->first == code) {
      ComboBox_SelectString(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), -1,
                            it->second.c_str());
      found = true;
      break;
    }
  }
  if (!found)
    LOG(WARNING) << "InternalSelectLanguage: language code undefined";
  return found;
}

// Special handling for locales not supported by the Chromium installer.
std::wstring VivaldiInstallDialog::GetCurrentTranslation() {
  std::unique_ptr<wchar_t[]> buffer(new wchar_t[LOCALE_NAME_MAX_LENGTH]);
  if (buffer.get()) {
    ::GetUserDefaultLocaleName(buffer.get(), LOCALE_NAME_MAX_LENGTH);
    const std::wstring locale_name(buffer.get());
    for (auto const &val : kExtraLanguages)
      if (locale_name.compare(val) == 0)
        return locale_name;
  }
  return installer::GetCurrentTranslation();
}

void VivaldiInstallDialog::InitDialog() {
  dialog_ended_ = false;

  std::map<const std::wstring, const std::wstring>::iterator it;
  for (it = kLanguages.begin(); it != kLanguages.end(); it++) {
    ComboBox_AddString(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE),
                       it->second.c_str());
  }
  if (!InternalSelectLanguage(language_code_))
    InternalSelectLanguage(L"en-us");

  ComboBox_AddString(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
                     L"Install for all users");  // TODO(jarle) localize
  ComboBox_AddString(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
                     L"Install per user");  // TODO(jarle) localize
  ComboBox_AddString(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES),
                     L"Install standalone");  // TODO(jarle) localize

  ComboBox_SetCurSel(GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES), install_type_);

  SetDlgItemText(hdlg_, IDC_EDIT_DEST_FOLDER,
                 destination_folder_.value().c_str());

  SendMessage(GetDlgItem(hdlg_, IDC_CHECK_DEFAULT), BM_SETCHECK,
              set_as_default_browser_ ? BST_CHECKED : BST_UNCHECKED, 0);

  SendMessage(GetDlgItem(hdlg_, IDC_CHECK_REGISTER), BM_SETCHECK,
              register_browser_ ? BST_CHECKED : BST_UNCHECKED, 0);
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
  *tree_view = NULL;
  return TRUE;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd,
                                       UINT msg,
                                       LPARAM lparam,
                                       LPARAM lpdata) {
  static HWND tree_view = NULL;
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
  bi.lpszTitle = L"Select a folder";  // TODO(jarle) localize
  bi.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
  bi.lpfn = BrowseCallbackProc;
  bi.lParam = (LPARAM)destination_folder_.value().c_str();

  OleInitialize(NULL);

  LPITEMIDLIST pIDL = SHBrowseForFolder(&bi);

  if (pIDL == NULL)
    return;

  std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
  if (!SHGetPathFromIDList(pIDL, buffer.get())) {
    CoTaskMemFree(pIDL);
    return;
  }
  destination_folder_ = base::FilePath(buffer.get());

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
  install_type_ = (InstallType)ComboBox_GetCurSel(
      GetDlgItem(hdlg_, IDC_COMBO_INSTALLTYPES));
  SetDefaultDestinationFolder();
  UpdateRegisterCheckboxVisibility();
}

void VivaldiInstallDialog::OnLanguageSelection() {
  int i = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE));
  if (i != CB_ERR) {
    const int len =
        ComboBox_GetLBTextLen(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), i);
    if (len <= 0)
      return;

    std::unique_ptr<wchar_t[]> buf(new wchar_t[len + 1]);
    ComboBox_GetLBText(GetDlgItem(hdlg_, IDC_COMBO_LANGUAGE), i, buf.get());
    std::map<const std::wstring, const std::wstring>::iterator it;
    for (it = kLanguages.begin(); it != kLanguages.end(); it++) {
      if (it->second == buf.get()) {
        language_code_ = it->first;
        break;
      }
    }
  }
}

bool VivaldiInstallDialog::GetRegisterBrowser() const {
  return register_browser_ ||
         (set_as_default_browser_ &&
          base::win::GetVersion() < base::win::VERSION_WIN10);
}

/* static */
bool VivaldiInstallDialog::IsVivaldiInstalled(const base::FilePath& path,
                                              InstallType* installed_type) {
  *installed_type = INSTALL_FOR_CURRENT_USER;

  base::FilePath vivaldi_exe_path(path);
  vivaldi_exe_path =
      vivaldi_exe_path.Append(kInstallBinaryDir).Append(kChromeExe);

  bool is_installed = base::PathExists(vivaldi_exe_path);
  if (is_installed) {
    base::FilePath vivaldi_sa_file_path(path);
    vivaldi_sa_file_path = vivaldi_sa_file_path.Append(kInstallBinaryDir)
                               .Append(kStandaloneProfileHelper);

    if (base::PathExists(vivaldi_sa_file_path)) {
      *installed_type = INSTALL_STANDALONE;
    } else {
      base::FilePath program_files_path;
      PathService::Get(base::DIR_PROGRAM_FILES, &program_files_path);

      if (vivaldi_exe_path.value().size() >=
          program_files_path.value().size()) {
        DWORD prefix_len =
            base::saturated_cast<DWORD>(program_files_path.value().size());
        if (::CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                            vivaldi_exe_path.value().data(), prefix_len,
                            program_files_path.value().data(),
                            prefix_len) == CSTR_EQUAL) {
          *installed_type = INSTALL_FOR_ALL_USERS;
        }
      }
    }
  }
  return is_installed;
}

bool VivaldiInstallDialog::IsInstallPathValid(const base::FilePath& path) {
  bool path_is_valid = base::PathIsWritable(path);
  if (!path_is_valid)
    MessageBox(hdlg_,
               L"The destination folder is invalid. Please choose another.",
               NULL, MB_ICONERROR);  // TODO(jarle) localize

  return path_is_valid;
}

installer::InstallStatus VivaldiInstallDialog::ShowEULADialog() {
  VLOG(1) << "About to show EULA";
  base::string16 eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  base::string16 inner_frame_path = GetInnerFrameEULAResource();
  if (inner_frame_path.empty()) {
    LOG(ERROR) << "No EULA inner frame path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  installer::EulaHTMLDialog dlg(eula_path, inner_frame_path);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return installer::EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return installer::EULA_ACCEPTED;
}

std::wstring VivaldiInstallDialog::GetInnerFrameEULAResource() {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileName(NULL, full_exe_path, MAX_PATH);
  if (len == 0 || len == MAX_PATH)
    return L"";

  const wchar_t* inner_frame_resource = L"IDR_OEM_EULA_VIV.HTML";
  if (NULL == FindResource(NULL, inner_frame_resource, RT_HTML))
    return L"";
  // spaces and DOS paths must be url encoded.
  std::wstring url_path = base::StringPrintf(
      L"res://%ls/#23/%ls", full_exe_path, inner_frame_resource);

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

void VivaldiInstallDialog::ShowOptions(HWND hdlg, bool show) {
  // TODO(jarle) localize
  SetDlgItemText(hdlg, IDC_BTN_MODE, (show) ? L"Simple" : L"Advanced");

  EnableWindow(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES), show ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_EDIT_DEST_FOLDER), show ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_CHECK_DEFAULT),
               (show && enable_set_as_default_checkbox_) ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_CHECK_REGISTER),
               (show && enable_register_browser_checkbox_) ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_BTN_BROWSE), show ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_COMBO_LANGUAGE), show ? TRUE : FALSE);
  ShowWindow(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_EDIT_DEST_FOLDER), show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_CHECK_DEFAULT),
             (show && enable_set_as_default_checkbox_) ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_CHECK_REGISTER),
             (show && enable_register_browser_checkbox_) ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_BTN_BROWSE), show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_COMBO_LANGUAGE), show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_STATIC_LANGUAGE), show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_STATIC_INSTALLTYPES),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_STATIC_DEST_FOLDER),
             show ? SW_SHOW : SW_HIDE);

  ShowWindow(GetDlgItem(hdlg, IDC_SYSLINK_TOS),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_STATIC_WARN),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDOK),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDCANCEL),
             show ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hdlg, IDC_SYSLINK_TOS_SIMPLE),
             show ? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY_SIMPLE),
             show ? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(hdlg, IDC_BTN_OK_SIMPLE),
             show ? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(hdlg, IDC_BTN_CANCEL_SIMPLE),
             show ? SW_HIDE : SW_SHOW);

  if (is_upgrade_ && show)
    EnableWindow(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES), FALSE);
}

void VivaldiInstallDialog::UpdateRegisterCheckboxVisibility() {
  enable_register_browser_checkbox_ = IsRegisterBrowserValid();
  EnableWindow(GetDlgItem(hdlg_, IDC_CHECK_REGISTER),
               (enable_register_browser_checkbox_) ? TRUE : FALSE);
  ShowWindow(GetDlgItem(hdlg_, IDC_CHECK_REGISTER),
             (enable_register_browser_checkbox_) ? SW_SHOW : SW_HIDE);
}

bool VivaldiInstallDialog::IsRegisterBrowserValid() const {
  return (install_type_ == INSTALL_STANDALONE &&
          base::win::GetVersion() >= base::win::VERSION_WIN10);
}

void VivaldiInstallDialog::InitCtlBrushes(HWND hdlg) {
  syslink_tos_brush_ = GetCtlBrush(hdlg,
      advanced_mode_ ? IDC_SYSLINK_TOS : IDC_SYSLINK_TOS_SIMPLE);
  button_browse_brush_ = GetCtlBrush(hdlg, IDC_BTN_BROWSE);
  button_ok_brush_ = GetCtlBrush(hdlg,
      advanced_mode_ ? IDOK : IDC_BTN_OK_SIMPLE);
  button_cancel_brush_ = GetCtlBrush(hdlg,
      advanced_mode_ ? IDCANCEL : IDC_BTN_CANCEL_SIMPLE);
  checkbox_default_brush_ = GetCtlBrush(hdlg, IDC_CHECK_DEFAULT);
  checkbox_register_brush_ = GetCtlBrush(hdlg, IDC_CHECK_REGISTER);
  button_options_brush_ = GetCtlBrush(hdlg, IDC_BTN_MODE);
  syslink_privacy_brush_ = GetCtlBrush(hdlg,
      advanced_mode_ ? IDC_SYSLINK_PRIVACY_POLICY
                     : IDC_SYSLINK_PRIVACY_POLICY_SIMPLE);
}

void VivaldiInstallDialog::ClearAll() {
  if (syslink_tos_brush_) {
    DeleteObject(syslink_tos_brush_);
    syslink_tos_brush_ = NULL;
  }
  if (button_browse_brush_) {
    DeleteObject(button_browse_brush_);
    button_browse_brush_ = NULL;
  }
  if (button_ok_brush_) {
    DeleteObject(button_ok_brush_);
    button_ok_brush_ = NULL;
  }
  if (button_cancel_brush_) {
    DeleteObject(button_cancel_brush_);
    button_cancel_brush_ = NULL;
  }
  if (checkbox_default_brush_) {
    DeleteObject(checkbox_default_brush_);
    checkbox_default_brush_ = NULL;
  }
  if (checkbox_register_brush_) {
    DeleteObject(checkbox_register_brush_);
    checkbox_register_brush_ = NULL;
  }
  if (button_options_brush_) {
    DeleteObject(button_options_brush_);
    button_options_brush_ = NULL;
  }
  if (syslink_privacy_brush_) {
    DeleteObject(syslink_privacy_brush_);
    syslink_privacy_brush_ = NULL;
  }
  if (back_bmp_) {
    DeleteObject(back_bmp_);
    back_bmp_ = NULL;
  }
  if (hbitmap_bkgnd_) {
    DeleteObject(hbitmap_bkgnd_);
    hbitmap_bkgnd_ = NULL;
  }
  std::vector<HGLOBAL>::iterator it;
  for (it = dibs_.begin(); it != dibs_.end(); it++) {
    HGLOBAL hDIB = *it;
    GlobalFree(hDIB);
  }
  dibs_.clear();
}

void VivaldiInstallDialog::InitBkgnd(HWND hdlg) {
  if (back_bmp_)
    ClearAll();

  DCHECK(instance_);
  switch (dpi_scale_) {
    case DPI_NORMAL:
      hbitmap_bkgnd_ =
          LoadBitmap(instance_,
                     (advanced_mode_) ?
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_100_ADVANCED) :
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_100_SIMPLE));
      break;
    case DPI_MEDIUM:
       hbitmap_bkgnd_ =
          LoadBitmap(instance_,
                     (advanced_mode_) ?
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_125_ADVANCED) :
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_125_SIMPLE));
      break;
    case DPI_LARGE:
       hbitmap_bkgnd_ =
          LoadBitmap(instance_,
                     (advanced_mode_) ?
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_150_ADVANCED) :
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_150_SIMPLE));
      break;
    case DPI_XL:
      hbitmap_bkgnd_ =
          LoadBitmap(instance_,
                     (advanced_mode_) ?
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_200_ADVANCED) :
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_200_SIMPLE));
      break;
    case DPI_XXL:
      hbitmap_bkgnd_ =
          LoadBitmap(instance_,
                     (advanced_mode_) ?
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_250_ADVANCED) :
                          MAKEINTRESOURCE(IDB_BITMAP_BKGND_250_SIMPLE));
      break;
  }
  DCHECK(hbitmap_bkgnd_);
  if (!hbitmap_bkgnd_)
    return;

  BITMAP bm = { 0 };
  GetObject(hbitmap_bkgnd_, sizeof(bm), &bm);

  back_bmp_width_ = bm.bmWidth;
  back_bmp_height_ = bm.bmHeight;

  HDC hdc = GetDC(NULL);
  if (hdc) {
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biWidth = back_bmp_width_;
    bmi.bmiHeader.biHeight = back_bmp_height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biSizeImage = back_bmp_width_ * back_bmp_height_ * 4;

    back_bmp_ =
        CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &back_bits_, NULL, 0);
    if (back_bmp_) {
      GetDIBits(hdc, hbitmap_bkgnd_, 0, back_bmp_height_,
                back_bits_, &bmi, DIB_RGB_COLORS);
    }
    ReleaseDC(NULL, hdc);

    InitCtlBrushes(hdlg);
    Resize(hdlg);
  }
}

void VivaldiInstallDialog::Resize(HWND hdlg) {
  RECT size_rect = { 0, 0, back_bmp_width_ - 1, back_bmp_height_ - 1 };
  AdjustWindowRectEx(&size_rect, GetWindowLong(hdlg, GWL_STYLE), FALSE,
                     GetWindowLong(hdlg, GWL_EXSTYLE));
  back_bmp_width_ = size_rect.right - size_rect.left;
  back_bmp_height_ = size_rect.bottom - size_rect.top;
  SetWindowPos(hdlg, NULL, 0, 0,
               size_rect.right - size_rect.left,
               size_rect.bottom - size_rect.top,
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
  InvalidateRect(hdlg, NULL, TRUE);
}

void VivaldiInstallDialog::Center(HWND hdlg) {
  RECT rect_window;
  GetWindowRect(hdlg, &rect_window);

  RECT rect_parent;
  HWND hwnd_parent = GetDesktopWindow();
  GetWindowRect(hwnd_parent, &rect_parent);

  int w = rect_window.right - rect_window.left;
  int h = rect_window.bottom - rect_window.top;
  int x = ((rect_parent.right - rect_parent.left) - w) / 2 + rect_parent.left;
  int y = ((rect_parent.bottom - rect_parent.top) - h) / 2 + rect_parent.top;

  MoveWindow(hdlg, x, y, w, h, FALSE);
}

BOOL VivaldiInstallDialog::OnEraseBkgnd(HDC hdc) {
  if (back_bmp_) {
    HDC hdc_mem = CreateCompatibleDC(hdc);
    HGDIOBJ old_bmp = SelectObject(hdc_mem, back_bmp_);
    BitBlt(hdc, 0, 0, back_bmp_width_, back_bmp_height_,
           hdc_mem, 0, 0, SRCCOPY);
    SelectObject(hdc_mem, old_bmp);
    DeleteDC(hdc_mem);
  }
  return TRUE;
}

HBRUSH VivaldiInstallDialog::CreateDIBrush(int x, int y, int cx, int cy) {
  if ((x < 0) || (y < 0) || (0 == cx) || (0 == cy) ||
      ((x + cx) > back_bmp_width_) || ((y + cy) > back_bmp_height_) ||
      (NULL == back_bits_)) {
    DCHECK(false);
    return NULL;
  }
  HGLOBAL hDIB = GlobalAlloc(GHND, sizeof(BITMAPINFOHEADER) + cx * cy * 4);

  if (NULL == hDIB)
    return NULL;

  LPVOID lpvBits = GlobalLock(hDIB);

  if (NULL == lpvBits) {
    GlobalFree(hDIB);
    return NULL;
  }

  BITMAPINFOHEADER* bih = reinterpret_cast<BITMAPINFOHEADER*>(lpvBits);

  bih->biBitCount = 32;
  bih->biCompression = BI_RGB;
  bih->biWidth = cx;
  bih->biHeight = cy;
  bih->biPlanes = 1;
  bih->biSize = sizeof(BITMAPINFOHEADER);
  bih->biSizeImage = cx * cy * 4;

  PDWORD pdwData = (PDWORD)(bih + 1);

  size_t cnt = cx << 2;
  int src_start_offset = back_bmp_height_ - 1 - y;

  for (int j = 0; j < cy; j++) {
    int dst_off = (cy - 1 - j) * cx;
    int src_off = (src_start_offset - j) * back_bmp_width_ + x;

    PDWORD pdwDst = pdwData + dst_off;
    PDWORD pdwSrc = ((PDWORD)back_bits_) + src_off;
    memcpy(pdwDst, pdwSrc, cnt);
  }

  GlobalUnlock(hDIB);

  LOGBRUSH lb;

  lb.lbStyle = BS_DIBPATTERN;
  lb.lbColor = DIB_RGB_COLORS;
  lb.lbHatch = (ULONG_PTR)hDIB;

  HBRUSH hBrush = CreateBrushIndirect(&lb);

  if (NULL == hBrush)
    GlobalFree(hDIB);

  dibs_.push_back(hDIB);
  return hBrush;
}

HBRUSH VivaldiInstallDialog::GetCtlBrush(HWND hdlg, int id_dlg_item) {
  RECT rcControl;
  GetWindowRect(GetDlgItem(hdlg, id_dlg_item), &rcControl);
  int w = rcControl.right - rcControl.left;
  int h = rcControl.bottom - rcControl.top;
  POINT pt = {rcControl.left, rcControl.top};
  ScreenToClient(hdlg, &pt);
  return CreateDIBrush(pt.x, pt.y, w, h);
}

HBRUSH VivaldiInstallDialog::OnCtlColor(HWND hwnd_ctl, HDC hdc) {
  SetBkMode(hdc, TRANSPARENT);

  if (GetDlgItem(hdlg_,
      advanced_mode_ ? IDC_SYSLINK_TOS : IDC_SYSLINK_TOS_SIMPLE) == hwnd_ctl)
    return syslink_tos_brush_;
  else if (GetDlgItem(hdlg_, IDC_BTN_BROWSE) == hwnd_ctl)
    return button_browse_brush_;
  else if (GetDlgItem(hdlg_,
      advanced_mode_ ? IDOK : IDC_BTN_OK_SIMPLE) == hwnd_ctl)
    return button_ok_brush_;
  else if (GetDlgItem(hdlg_,
      advanced_mode_ ? IDCANCEL : IDC_BTN_CANCEL_SIMPLE) == hwnd_ctl)
    return button_cancel_brush_;
  else if (GetDlgItem(hdlg_, IDC_CHECK_DEFAULT) == hwnd_ctl)
    return checkbox_default_brush_;
  else if (GetDlgItem(hdlg_, IDC_CHECK_REGISTER) == hwnd_ctl)
    return checkbox_register_brush_;
  else if (GetDlgItem(hdlg_, IDC_BTN_MODE) == hwnd_ctl)
    return button_options_brush_;
  else if (GetDlgItem(hdlg_,
      advanced_mode_ ? IDC_SYSLINK_PRIVACY_POLICY
                     : IDC_SYSLINK_PRIVACY_POLICY_SIMPLE) == hwnd_ctl)
    return syslink_privacy_brush_;

  return (HBRUSH)GetStockObject(NULL_BRUSH);
}

const WCHAR TXT_TOS_ACCEPT_AND_INSTALL[] =
    L"By clicking on the 'Accept and Install' button you are agreeing to "
    L"Vivaldi's <a>Terms of Service</a>";
const WCHAR TXT_TOS_ACCEPT_AND_UPDATE[] =
    L"By clicking on the 'Accept and Update' button you are agreeing to "
    L"Vivaldi's <a>Terms of Service</a>";
const WCHAR TXT_BTN_ACCEPT_AND_INSTALL[] = L"Accept and Install";
const WCHAR TXT_BTN_ACCEPT_AND_UPDATE[] = L"Accept and Update";

/*static*/
INT_PTR CALLBACK VivaldiInstallDialog::DlgProc(HWND hdlg,
                                               UINT msg,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      this_ = reinterpret_cast<VivaldiInstallDialog*>(lparam);
      DCHECK(this_);
      {
        // Tell the syslink control to accept color change.
        LITEM item = { 0 };
        item.iLink = 0;
        item.mask = LIF_ITEMINDEX | LIF_STATE;
        item.state = LIS_DEFAULTCOLORS;
        item.stateMask = LIS_DEFAULTCOLORS;
        SendMessage(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY),
                    LM_SETITEM, 0, (LPARAM)&item);
        SendMessage(GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY_SIMPLE),
                    LM_SETITEM, 0, (LPARAM)&item);
      }
      this_->InitBkgnd(hdlg);
      this_->Center(hdlg);
      return (INT_PTR)TRUE;

    case WM_ERASEBKGND:
      if (this_)
        return (INT_PTR)this_->OnEraseBkgnd((HDC)wparam);
      break;

    case WM_CTLCOLORSTATIC:
      if ((HWND)lparam == GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY) ||
          (HWND)lparam == GetDlgItem(hdlg, IDC_SYSLINK_PRIVACY_POLICY_SIMPLE))
        SetTextColor((HDC)wparam, RGB(0xff, 0xff, 0xff)); // White
    FALLTHROUGH;
    case WM_CTLCOLORBTN:
     if (this_)
       return (INT_PTR)this_->OnCtlColor((HWND)lparam, (HDC)wparam);
     break;

    case WM_NOTIFY: {
      LPNMHDR pnmh = (LPNMHDR)lparam;
      if (pnmh->idFrom == IDC_SYSLINK_TOS ||
          pnmh->idFrom == IDC_SYSLINK_TOS_SIMPLE) {
        if ((pnmh->code == NM_CLICK) || (pnmh->code == NM_RETURN)) {
          // TODO(jarle): check the return code and act upon it
          /*int exit_code =*/this_->ShowEULADialog();
        }
      } else if (pnmh->idFrom == IDC_SYSLINK_PRIVACY_POLICY ||
                 pnmh->idFrom == IDC_SYSLINK_PRIVACY_POLICY_SIMPLE) {
        if ((pnmh->code == NM_CLICK) || (pnmh->code == NM_RETURN)) {
          ShellExecute(NULL, L"open", L"https://vivaldi.com/privacy", NULL,
                       NULL, SW_SHOWNORMAL);
        }
      }
    } break;

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDOK:
        case IDC_BTN_OK_SIMPLE:
          {
          this_->dlg_result_ = INSTALL_DLG_INSTALL;
          std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH]);
          if (buffer.get()) {
            GetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER, buffer.get(),
                           MAX_PATH - 1);
            this_->destination_folder_ = base::FilePath(buffer.get());
          }

          this_->set_as_default_browser_ =
              SendMessage(GetDlgItem(hdlg, IDC_CHECK_DEFAULT), BM_GETCHECK, 0,
                          0) != 0;
          this_->register_browser_ =
              SendMessage(GetDlgItem(hdlg, IDC_CHECK_REGISTER), BM_GETCHECK, 0,
                          0) != 0;
          this_->install_type_ = (InstallType)SendMessage(
              GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES), CB_GETCURSEL, 0, 0);
          }
          EndDialog(hdlg, 0);
          this_->dialog_ended_ = true;
          break;
        case IDCANCEL:
        case IDC_BTN_CANCEL_SIMPLE:
          // TODO(jarle) localize
          if (MessageBox(
                  hdlg,
                  L"The Vivaldi Installer is not finished installing "
                  L"the Vivaldi Browser. Are you sure you want to exit now?",
                  L"Vivaldi Installer", MB_YESNO | MB_ICONQUESTION) == IDYES) {
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
            this_->destination_folder_ = base::FilePath(buffer.get());
          }
          this_->ShowBrowseFolderDialog();
          SetDlgItemText(hdlg, IDC_EDIT_DEST_FOLDER,
                         this_->destination_folder_.value().c_str());
        } break;
        case IDC_BTN_MODE:
          this_->advanced_mode_ = !this_->advanced_mode_;
          this_->ShowOptions(hdlg, this_->advanced_mode_);
          this_->InitBkgnd(hdlg);
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

              InstallType installed_type;
              if (IsVivaldiInstalled(new_path, &installed_type)) {
                SetDlgItemText(
                    hdlg, IDC_SYSLINK_TOS, TXT_TOS_ACCEPT_AND_UPDATE);
                SetDlgItemText(
                    hdlg, IDC_SYSLINK_TOS_SIMPLE, TXT_TOS_ACCEPT_AND_UPDATE);
                SetDlgItemText(
                    hdlg, IDOK, TXT_BTN_ACCEPT_AND_UPDATE);
                SetDlgItemText(
                    hdlg, IDC_BTN_OK_SIMPLE, TXT_BTN_ACCEPT_AND_UPDATE);

                ShowWindow(GetDlgItem(hdlg, IDC_STATIC_WARN), SW_SHOW);

                // If not standalone install selected, override current.
                if (this_->install_type_ != INSTALL_STANDALONE) {
                  this_->install_type_ = installed_type;
                  ComboBox_SetCurSel(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES),
                                        this_->install_type_);
                }

                this_->UpdateRegisterCheckboxVisibility();
              } else {
                this_->is_upgrade_ = false;
                SetDlgItemText(
                    hdlg, IDC_SYSLINK_TOS, TXT_TOS_ACCEPT_AND_INSTALL);
                SetDlgItemText(
                    hdlg, IDC_SYSLINK_TOS_SIMPLE, TXT_TOS_ACCEPT_AND_INSTALL);
                SetDlgItemText(
                    hdlg, IDOK, TXT_BTN_ACCEPT_AND_INSTALL);
                SetDlgItemText(
                    hdlg, IDC_BTN_OK_SIMPLE, TXT_BTN_ACCEPT_AND_INSTALL);

                EnableWindow(GetDlgItem(hdlg, IDC_COMBO_INSTALLTYPES), TRUE);
                ShowWindow(GetDlgItem(hdlg, IDC_STATIC_WARN), SW_HIDE);
              }
            }
          }
        } break;
      }
      break;
  }
  return (INT_PTR)FALSE;
}

}  // namespace installer
