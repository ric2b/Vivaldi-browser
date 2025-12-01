// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WEBUI_PRIVACY_REPORT_DIALOG_H_
#define UI_WEBUI_PRIVACY_REPORT_DIALOG_H_

#include "ui/web_dialogs/web_dialog_delegate.h"

namespace vivaldi {

// The WebUI Dialog for vivaldi://privacy-report-dialog
class PrivacyReportDialog : public ui::WebDialogDelegate {
 public:
  explicit PrivacyReportDialog(content::BrowserContext* browser_context);
  PrivacyReportDialog(const PrivacyReportDialog&) = delete;
  PrivacyReportDialog& operator=(const PrivacyReportDialog&) = delete;
  ~PrivacyReportDialog() override;
  void Show();

 private:
  void OnDialogClosed(const std::string& json_retval) override;

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace vivaldi

#endif  // UI_WEBUI_PRIVACY_REPORT_DIALOG_H_
