// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_DIALOG_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_DIALOG_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

class VivaldiInstallDialog {
 public:
  enum DlgResult {
    INSTALL_DLG_ERROR = -1,   // Dialog could not be shown.
    INSTALL_DLG_CANCEL = 0,   // The user cancelled install.
    INSTALL_DLG_INSTALL = 1,  // The user clicked the install button.
  };

  enum InstallType {
    INSTALL_UNDEFINED = -1,
    INSTALL_FOR_ALL_USERS = 0,     // Install for all users, system level.
    INSTALL_FOR_CURRENT_USER = 1,  // Install for current user.
    INSTALL_STANDALONE = 2         // Install Vivaldi standalone.
  };

  enum Scaling {
    DPI_NORMAL,  // 100%
    DPI_MEDIUM,  // 125%
    DPI_LARGE,   // 150%
    DPI_XL,      // 200%
    DPI_XXL      // 250%
  };

  VivaldiInstallDialog(HINSTANCE instance,
                       const bool set_as_default_browser,
                       const InstallType default_install_type,
                       const base::FilePath& destination_folder);

  virtual ~VivaldiInstallDialog();

  DlgResult ShowModal();

  const base::FilePath& GetDestinationFolder() const {
    return destination_folder_;
  }
  InstallType GetInstallType() const { return install_type_; }
  bool GetSetAsDefaultBrowser() const { return set_as_default_browser_; }
  std::wstring GetLanguageCode() const { return language_code_; }
  const bool GetRegisterBrowser() const;

  static bool IsVivaldiInstalled(const base::FilePath& path,
                                 InstallType* installed_type);

 private:
  void InitDialog();
  void ShowBrowseFolderDialog();
  void DoDialog();
  void SetDefaultDestinationFolder();
  bool GetLastInstallValues();
  void SaveInstallValues();
  bool InternalSelectLanguage(const std::wstring& code);
  std::wstring GetCurrentTranslation();

  void OnInstallTypeSelection();
  void OnLanguageSelection();
  bool IsInstallPathValid(const base::FilePath& path);
  installer::InstallStatus ShowEULADialog();
  std::wstring GetInnerFrameEULAResource();

  void ShowDlgControls(HWND hwnd_dlg, bool show = true);
  void ShowOptions(HWND hwnd_dlg, bool show = true);
  void UpdateRegisterCheckboxVisibility();
  const bool IsRegisterBrowserValid() const;

  void InitBkgnd(HWND hdlg, int cx, int cy);

  BOOL OnEraseBkgnd(HDC hdc);
  HBRUSH OnCtlColor(HWND hwnd_ctl, HDC hdc);

  HBRUSH CreateDIBrush(int x, int y, int cx, int cy);
  HBRUSH GetCtlBrush(HWND hdlg, int id_dlg_item);

  static INT_PTR CALLBACK DlgProc(HWND hdlg,
                                  UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam);

 private:
  std::wstring language_code_;
  base::FilePath destination_folder_;
  base::FilePath last_standalone_folder_;
  InstallType install_type_;
  bool set_as_default_browser_;
  bool register_browser_;

  bool is_upgrade_;

  bool dialog_ended_;
  bool advanced_mode_;
  HWND hdlg_;
  HINSTANCE instance_;
  DlgResult dlg_result_;

  bool enable_set_as_default_checkbox_;
  bool enable_register_browser_checkbox_;

  Scaling dpi_scale_;
  HBITMAP hbitmap_bkgnd_;
  HBITMAP back_bmp_;
  LPVOID back_bits_;
  int back_bmp_width_;
  int back_bmp_height_;

  HBRUSH syslink_tos_brush_;
  HBRUSH button_browse_brush_;
  HBRUSH button_ok_brush_;
  HBRUSH button_cancel_brush_;
  HBRUSH checkbox_default_brush_;
  HBRUSH checkbox_register_brush_;
  HBRUSH button_options_brush_;
  HBRUSH syslink_privacy_brush_;
  std::vector<HGLOBAL> dibs_;

  static VivaldiInstallDialog* this_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiInstallDialog);
};

}  // namespace installer
#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_DIALOG_H_
