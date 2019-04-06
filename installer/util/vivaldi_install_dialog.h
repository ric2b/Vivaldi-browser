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
  bool GetRegisterBrowser() const;

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

  void ShowOptions(HWND hwnd_dlg, bool show = true);
  void UpdateRegisterCheckboxVisibility();
  bool IsRegisterBrowserValid() const;

  void InitBkgnd(HWND hdlg);
  void InitCtlBrushes(HWND hdlg);
  void ClearAll();
  void Resize(HWND hdlg);
  void Center(HWND hdlg);

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
  InstallType install_type_ = INSTALL_UNDEFINED;
  bool set_as_default_browser_ = false;
  bool register_browser_ = false;
  bool is_upgrade_ = false;
  bool dialog_ended_ = false;
  bool advanced_mode_ = false;
  HWND hdlg_ = NULL;
  HINSTANCE instance_ = NULL;
  DlgResult dlg_result_ = INSTALL_DLG_ERROR;
  bool enable_set_as_default_checkbox_ = false;
  bool enable_register_browser_checkbox_ = false;
  Scaling dpi_scale_ = DPI_NORMAL;
  HBITMAP hbitmap_bkgnd_ = NULL;
  HBITMAP back_bmp_ = NULL;
  LPVOID back_bits_ = NULL;
  LONG back_bmp_width_ = 0;
  LONG back_bmp_height_ = 0;
  HBRUSH syslink_tos_brush_ = NULL;
  HBRUSH button_browse_brush_ = NULL;
  HBRUSH button_ok_brush_ = NULL;
  HBRUSH button_cancel_brush_ = NULL;
  HBRUSH checkbox_default_brush_ = NULL;
  HBRUSH checkbox_register_brush_ = NULL;
  HBRUSH button_options_brush_ = NULL;
  HBRUSH syslink_privacy_brush_ = NULL;
  std::vector<HGLOBAL> dibs_;

  static VivaldiInstallDialog* this_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiInstallDialog);
};

}  // namespace installer
#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_DIALOG_H_
