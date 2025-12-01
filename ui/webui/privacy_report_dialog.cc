// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "ui/webui/privacy_report_dialog.h"

#include <string>

#include "app/vivaldi_constants.h"
#include "app/vivaldi_resources.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/privacy_report_ui.h"

namespace vivaldi {

PrivacyReportDialog::PrivacyReportDialog(
    content::BrowserContext* browser_context)
    : ui::WebDialogDelegate(), browser_context_(browser_context) {
  std::string privacy_report_url =
      base::StrCat({VIVALDI_DATA_URL_SCHEME, url::kStandardSchemeSeparator,
                    vivaldi::kVivaldiPrivacyReportHost});

  set_can_close(true);
  set_delete_on_close(true);
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_title(l10n_util::GetStringUTF16(IDS_APD_PRIVACY_REPORT_TITLE));
  set_dialog_content_url(GURL(privacy_report_url));
  set_dialog_size(gfx::Size(540, 640));
  set_show_dialog_title(true);
}

PrivacyReportDialog::~PrivacyReportDialog() = default;

void PrivacyReportDialog::Show() {
  chrome::ShowWebDialog(gfx::NativeView(), browser_context_,
                        new PrivacyReportDialog(browser_context_));
}

void PrivacyReportDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

}  // namespace vivaldi
