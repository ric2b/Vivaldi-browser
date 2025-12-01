// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

void CreatePrivacyReportUIAssets(content::WebUIDataSource* html_source) {
  html_source->AddResourcePath("favicon.ico", IDR_VIVALDI_PRIVACY_REPORT_FAVICON);
  html_source->AddResourcePath("privacy_report.html", IDR_VIVALDI_PRIVACY_REPORT_FILE);
  html_source->AddResourcePath("js/tabs.js", IDR_VIVALDI_PRIVACY_REPORT_JS_TABS);
  html_source->AddResourcePath("img/bandwidth_saved.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_BANDWIDTH_SAVED);
  html_source->AddResourcePath("img/bandwidth_saved_darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_BANDWIDTH_SAVED_DARKMODE);
  html_source->AddResourcePath("img/chevron-down.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CHEVRON_DOWN);
  html_source->AddResourcePath("img/chevron-left.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CHEVRON_LEFT);
  html_source->AddResourcePath("img/chevron-left-darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CHEVRON_LEFT_DARKMODE);
  html_source->AddResourcePath("img/close.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CLOSE);
  html_source->AddResourcePath("img/close-darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CLOSE_DARKMODE);
  html_source->AddResourcePath("img/chevron-up.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_CHEVRON_UP);
  html_source->AddResourcePath("img/report_icon_disabled.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_REPORT_ICON_DISABLED);
  html_source->AddResourcePath("img/report_icon_full.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_REPORT_ICON_FULL);
  html_source->AddResourcePath("img/report_icon_half.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_REPORT_ICON_HALF);
  html_source->AddResourcePath("img/date_range_picker.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_DATE_RANGE_PICKER);
  html_source->AddResourcePath("img/graphic_blocker_shield_trackers.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_GRAPHIC_BLOCKER_SHIELD_TRACKERS);
  html_source->AddResourcePath("img/graphic_blocker_shield_trackers_darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_GRAPHIC_BLOCKER_SHIELD_TRACKERS_DARKMODE);
  html_source->AddResourcePath("img/graphic_blocker_shield_trackers_and_ads.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_GRAPHIC_BLOCKER_SHIELD_TRACKERS_AND_ADS);
  html_source->AddResourcePath("img/graphic_blocker_shield_trackers_and_ads_darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_GRAPHIC_BLOCKER_SHIELD_TRACKERS_AND_ADS_DARKMODE);
  html_source->AddResourcePath("img/time_saved.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_TIME_SAVED);
  html_source->AddResourcePath("img/time_saved_darkmode.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_TIME_SAVED_DARKMODE);
  html_source->AddResourcePath("css/privacy_report.css", IDR_VIVALDI_PRIVACY_REPORT_CSS_FILE);
  html_source->AddResourcePath("js/privacy_report.js", IDR_VIVALDI_PRIVACY_REPORT_JS_FILE);
  html_source->AddResourcePath("img/notification.svg", IDR_VIVALDI_PRIVACY_REPORT_IMG_NOTIFICATION);
  html_source->AddResourcePath("img/No-Tracking-Hero_1-960x540.png", IDR_VIVALDI_PRIVACY_REPORT_IMG_NO_TRACKING_HERO_1_960X540);
  html_source->AddResourcePath("img/image-27.png", IDR_VIVALDI_PRIVACY_REPORT_IMG_IMAGE_27);
  html_source->AddResourcePath("img/Topics-980x551-3-640x360.jpeg", IDR_VIVALDI_PRIVACY_REPORT_IMG_TOPICS_980X551_3_640X360);
  html_source->AddResourcePath("img/860w_v3-manifest-640x360.png", IDR_VIVALDI_PRIVACY_REPORT_IMG_860W_V3_MANIFEST_640X360);
}
