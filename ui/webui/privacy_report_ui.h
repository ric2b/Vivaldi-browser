// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WEBUI_PRIVACY_REPORT_UI_H_
#define UI_WEBUI_PRIVACY_REPORT_UI_H_

#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace adblock_filter {
class StatsData;
}

namespace vivaldi {

extern const char kVivaldiPrivacyReportHost[];

// The WebUI handler for vivaldi://privacy-report
class PrivacyReportUI : public content::WebUIController {
 public:
  explicit PrivacyReportUI(content::WebUI* web_ui);
  PrivacyReportUI(const PrivacyReportUI&) = delete;
  PrivacyReportUI& operator=(const PrivacyReportUI&) = delete;
  ~PrivacyReportUI() override;

 private:
  void OnStatsDataLoaded(std::unique_ptr<adblock_filter::StatsData> data,
                         const std::string& callback_id);
};

}  // namespace vivaldi

#endif  // UI_WEBUI_PRIVACY_REPORT_UI_H_
