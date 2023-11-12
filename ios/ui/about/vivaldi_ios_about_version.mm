// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/about/vivaldi_ios_about_version.h"

#import "app/vivaldi_resources.h"
#import "app/vivaldi_version_info.h"
#import "base/strings/utf_string_conversions.h"
#import "components/grit/components_scaled_resources.h"
#import "components/version_ui/version_ui_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

namespace vivaldi {

void UpdateVersionUIIOSDataSource(web::WebUIIOSDataSource* html_source) {

  html_source->AddResourcePath("images/product_logo_white.png",
                               IDR_PRODUCT_LOGO);

  html_source->AddString(version_ui::kVersion,
                         vivaldi::GetVivaldiVersionString());
#if defined(OFFICIAL_BUILD) && \
    (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
  html_source->AddString("official",
                         std::string(VIVALDI_PRODUCT_VERSION).empty()
                             ? "Stable channel"
                             : VIVALDI_PRODUCT_VERSION);
#endif

  html_source->AddLocalizedString("productLicense",
                                  IDS_VIVALDI_VERSION_UI_LICENSE_NEW);

  html_source->AddLocalizedString("productCredits",
                                  IDS_VIVALDI_VERSION_UI_CREDITS);

  html_source->AddString("productLicenseChromiumURL",
                         u"https://www.chromium.org/");

  html_source->AddString("productLicenseCreditsURL",
                         base::UTF8ToUTF16(kChromeUICreditsURL));

  html_source->AddLocalizedString("productTOS",
                                  IDS_VIVALDI_ABOUT_TERMS_OF_SERVICE);
}
}  // namespace vivaldi
