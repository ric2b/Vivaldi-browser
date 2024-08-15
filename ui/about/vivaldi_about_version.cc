// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "ui/about/vivaldi_about_version.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

#include "app/vivaldi_resources.h"
#include "app/vivaldi_version_info.h"

#if BUILDFLAG(IS_WIN)
#include "installer/util/vivaldi_install_util.h"
#endif

namespace vivaldi {

void UpdateVersionUIDataSource(content::WebUIDataSource* html_source) {
  html_source->AddString(version_ui::kVersion,
                         vivaldi::GetVivaldiVersionString());
#if defined(OFFICIAL_BUILD) && \
    (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
  html_source->AddString("official",
                         std::string(VIVALDI_PRODUCT_VERSION).empty()
                             ? "Stable channel"
                             : VIVALDI_PRODUCT_VERSION);
#endif

  std::string pending_update;
#ifdef OS_WIN
  std::optional<base::Version> pending_version;
  {
    // TODO(igor@vivaldi): Use a worker thread to get the pending version.
    base::VivaldiScopedAllowBlocking allow_blocking;
    pending_version = GetPendingUpdateVersion();
  }
  if (pending_version) {
    // The error should be very rare and it is better to inform the user about
    // it than showing nothing to get a chance of a feedback.
    std::string pending_version_string = pending_version->IsValid()
                                             ? pending_version->GetString()
                                             : "Version Error";
    pending_update = "(";
    pending_update += base::UTF16ToUTF8(
        l10n_util::GetStringFUTF16(IDS_VIVALDI_VERSION_UI_PENDING_VERSION,
                                   base::UTF8ToUTF16(pending_version_string)));
    pending_update += ")";
  }
#endif
  html_source->AddString("vivaldi_pending_update", pending_update);

  html_source->AddLocalizedString("productLicense",
                                  IDS_VIVALDI_VERSION_UI_LICENSE_NEW);

  html_source->AddLocalizedString("productCredits",
                                  IDS_VIVALDI_VERSION_UI_CREDITS);

  html_source->AddString("productLicenseChromiumURL",
                         chrome::kChromiumProjectURL);

  html_source->AddString("productLicenseCreditsURL",
                         chrome::kChromeUICreditsURL);

  html_source->AddLocalizedString("productTOS", IDS_ABOUT_TERMS_OF_SERVICE);
}
}  // namespace vivaldi
