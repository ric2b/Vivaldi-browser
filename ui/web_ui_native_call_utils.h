// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
#ifndef VIVALDI_UI_WEB_UI_NATIVE_CALL_UTILS_H_
#define VIVALDI_UI_WEB_UI_NATIVE_CALL_UTILS_H_

#include <string>

#include "content/public/browser/web_contents.h" // Needed for JNI_zero

namespace webUINativeCalls {
void openNewTab(std::string url);
void closeActivity(content::WebContents* webContents);
void createPrivacyReportNotification(int64_t adsBlocked, int64_t trackersBlocked);
}  // namespace webUINativeCalls
#endif