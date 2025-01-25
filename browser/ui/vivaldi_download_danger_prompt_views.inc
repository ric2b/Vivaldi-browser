// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

bool DownloadDangerPromptViews::RunKeepAndClose() {
  OnDone done = std::move(done_);
  if (download_) {
    download_->RemoveObserver(this);
    download_ = nullptr;
  }
/* note:
    // The user chose to proceed down the dangerous path.
    ACCEPT, "ok"
    // The user chose not to proceed down the dangerous path.
    CANCEL, "cancel"
    // The user dismissed the dialog without making an explicit choice.
    DISMISS, "close dialog"
*/
  if (done) {
    std::move(done).Run(ACCEPT);
  }
  return true;
}

bool DownloadDangerPromptViews::RunScanAndClose() {

  if (download_ &&
      download_->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING) {
    safe_browsing::DownloadProtectionService::CheckDownloadWithLocalDecryption(
        download_, base::UTF16ToUTF8(password_prompt_->GetText()));
  }
  return true;  // Close the dialog. We might get a new one asking to keep or
                // not with the result after scanning.
}

void DownloadDangerPromptViews::UpdatePasswordPrompt() {
  password_prompt_->SetState(
      DownloadItemWarningData::HasIncorrectPassword(download_)
          ? DownloadBubblePasswordPromptView::State::kInvalid
          : DownloadBubblePasswordPromptView::State::kValid);

}
