// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "ui/about/vivaldi_about_version.h"

#include <string>

#include "app/vivaldi_version_info.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"


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

  html_source->AddString("productLicense",
                         l10n_util::GetStringFUTF16(
                             IDS_VIVALDI_VERSION_UI_LICENSE,
                             base::ASCIIToUTF16(chrome::kChromiumProjectURL),
                             base::ASCIIToUTF16(chrome::kChromeUICreditsURL)));
  base::string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, base::UTF8ToUTF16(chrome::kChromeUITermsURL));
  html_source->AddString("productTOS", tos);
  html_source->AddResourcePath("vivaldi_about_version.js",
                               IDR_VIVALDI_VERSION_UI_JS);
}
}  // namespace vivaldi
