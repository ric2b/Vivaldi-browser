// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "ui/webui/privacy_report_ui.h"

#include <string>

#include "app/vivaldi_constants.h"
#include "app/vivaldi_resources.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/adblock_stats_store.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs_impl.h"
#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "ui/web_ui_native_call_utils.h"
#endif
namespace {

using content::BrowserThread;

#include "vivaldi_privacy_report_resources.inc"

void CreateAndAddPrivacyReportUIHTMLSource(Profile* profile) {
  // Get the base HTML source and address
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, vivaldi::kVivaldiPrivacyReportHost);

  // Add Localized Strings.
  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order) <- TODO.
      {"privacy_report_header_title", IDS_PRIVACY_REPORT_HEADER_TITLE},
      {"privacy_report_title", IDS_APD_PRIVACY_REPORT_TITLE},
      {"privacy_report_title_description",
       IDS_APD_PRIVACY_REPORT_TITLE_DESCRIPTION},
      {"time_interval_7", IDS_APD_INTERVAL_7},
      {"time_interval_1_month", IDS_APD_INTERVAL_1_MONTH},
      {"time_interval_all_time", IDS_APD_INTERVAL_ALL_TIME},
      {"privacy_statistics_title", IDS_APD_PRIVACY_STATISTICS_TITLE},
      {"privacy_statistics_ads_blocked",
       IDS_APD_PRIVACY_STATISTICS_ADS_BLOCKED},
      {"privacy_statistics_trackers_blocked",
       IDS_APD_PRIVACY_STATISTICS_TRACKERS_BLOCKED},
      {"privacy_statistics_bandwidth_saved",
       IDS_APD_PRIVACY_STATISTICS_BANDWIDTH_SAVED},
      {"privacy_statistics_time_saved", IDS_APD_PRIVACY_STATISTICS_TIME_SAVED},
      {"privacy_report_tracker_blocker_disabled_p1",
       IDS_APD_PRIVACY_REPORT_TRACKER_BLOCKER_DISABLED_P1},
      {"privacy_report_tracker_blocker_disabled_p2",
       IDS_APD_PRIVACY_REPORT_TRACKER_BLOCKER_DISABLED_P2},
      {"privacy_statistics_description",
       IDS_APD_PRIVACY_STATISTICS_DESCRIPTION},
      {"privacy_statistics_description_link",
       IDS_APD_PRIVACY_STATISTICS_DESCRIPTION_LINK},
      {"website_details_title", IDS_APD_PRIVACY_WEBSITE_DETAILS_TITLE},
      {"website_details_sites_title",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_SITES_TITLE},
      {"website_details_trackers_title",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_TRACKERS_TITLE},
      {"website_details_sites_address",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_SITES_ADDRESS},
      {"website_details_sites_ads", IDS_APD_PRIVACY_WEBSITE_DETAILS_SITES_ADS},
      {"website_details_sites_trackers",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_SITES_TRACKERS},
      {"website_details_trackers_address",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_TRACKERS_ADDRESS},
      {"website_details_trackers_ads",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_TRACKERS_ADS},
      {"website_details_trackers_trackers",
       IDS_APD_PRIVACY_WEBSITE_DETAILS_TRACKERS_TRACKERS},
      {"article_commitment_title", IDS_APD_PRIVACY_ARTICLE_COMMITMENT_TITLE},
      {"article_commitment_description",
       IDS_APD_PRIVACY_ARTICLE_COMMITMENT_DESCRIPTION},
      {"article_1_title", IDS_APD_PRIVACY_ARTICLE_1_TITLE},
      {"article_1_description", IDS_APD_PRIVACY_ARTICLE_1_DESCRIPTION},
      {"article_2_title", IDS_APD_PRIVACY_ARTICLE_2_TITLE},
      {"article_2_description", IDS_APD_PRIVACY_ARTICLE_2_DESCRIPTION},
      {"article_3_title", IDS_APD_PRIVACY_ARTICLE_3_TITLE},
      {"article_3_description", IDS_APD_PRIVACY_ARTICLE_3_DESCRIPTION},
      {"article_4_title", IDS_APD_PRIVACY_ARTICLE_4_TITLE},
      {"article_4_description", IDS_APD_PRIVACY_ARTICLE_4_DESCRIPTION},
      {"privacy_statistics_link_learn_more",
       IDS_APD_PRIVACY_STATISTICS_LINK_LEARN_MORE},
      {"privacy_statistics_article_link_1",
       IDS_APD_PRIVACY_STATISTICS_ARTICLE_LINK_1},
      {"privacy_statistics_article_link_2",
       IDS_APD_PRIVACY_STATISTICS_ARTICLE_LINK_2},
      {"privacy_statistics_article_link_3",
       IDS_APD_PRIVACY_STATISTICS_ARTICLE_LINK_3},
      {"privacy_statistics_article_link_4",
       IDS_APD_PRIVACY_STATISTICS_ARTICLE_LINK_4},
      {"privacy_statistics_nav_bar_title",
       IDS_APD_PRIVACY_STATISTICS_NAV_BAR_TITLE},
      {"privacy_statistics_nav_bar_back",
       IDS_APD_PRIVACY_STATISTICS_NAV_BAR_BACK}};

  source->AddLocalizedStrings(kStrings);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  // Add required resources.
  CreatePrivacyReportUIAssets(source);
  source->SetDefaultResource(IDR_VIVALDI_PRIVACY_REPORT_FILE);

  // Allow scripts from vivaldi://privacy-report
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src vivaldi://privacy-report chrome://resources 'unsafe-inline' "
      "'self';");

  // Allow workers from vivaldi://privacy-report
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src vivaldi://privacy-report;");

  source->DisableTrustedTypesCSP();
}  // CreateAndAddPrivacyReportUIHTMLSource

std::string CastOutputNumberForJS(int64_t n) {
  return base::NumberToString(n);
}

// The handler for listening to communications from the JS
class PrivacyReportHandler final : public content::WebUIMessageHandler {
 public:
  explicit PrivacyReportHandler(content::WebUI* web_ui);

  PrivacyReportHandler(const PrivacyReportHandler&) = delete;
  PrivacyReportHandler& operator=(const PrivacyReportHandler&) = delete;

  ~PrivacyReportHandler() override;

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;
  void HandleIsAdBlockEnabled(const base::Value::List& args);
  void HandleIsTrackerBlockEnabled(const base::Value::List& args);
  void HandleGetBlockingData(const base::Value::List& args);
  void OpenLinkInNewTab(const base::Value::List& args);
  void OnStatsDataLoaded(std::string callback_id,
                         std::unique_ptr<adblock_filter::StatsData> data);
  void CloseActivityFromJS(const base::Value::List& args);
  void CloseActivity(content::WebContents* webContents);

 private:
  Profile* _profile;

  base::WeakPtrFactory<PrivacyReportHandler> weak_factory_{this};
};  // PrivacyReportHandler

PrivacyReportHandler::~PrivacyReportHandler() {}

PrivacyReportHandler::PrivacyReportHandler(content::WebUI* web_ui)
    : _profile(Profile::FromWebUI(web_ui)) {}

// All messages called by the JS from privacy_report.js which are to be
// handled by the c++
void PrivacyReportHandler::RegisterMessages() {
  // Unretained should be OK here since this class is bound to the lifetime of
  // the WebUI.
  web_ui()->RegisterMessageCallback(
      "isAdBlockEnabled",
      base::BindRepeating(&PrivacyReportHandler::HandleIsAdBlockEnabled,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isTrackerBlockEnabled",
      base::BindRepeating(&PrivacyReportHandler::HandleIsTrackerBlockEnabled,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getBlockingData",
      base::BindRepeating(&PrivacyReportHandler::HandleGetBlockingData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openLinkInNewTab",
      base::BindRepeating(&PrivacyReportHandler::OpenLinkInNewTab,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "closeActivityFromJS",
      base::BindRepeating(&PrivacyReportHandler::CloseActivityFromJS,
                          base::Unretained(this)));
}

// Send down boolean for if Adblocker is enabled.
void PrivacyReportHandler::HandleIsAdBlockEnabled(
    const base::Value::List& args) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const base::Value& callback_id = args[0];
  bool adBlockEnabled =
      (adblock_filter::RuleServiceFactory::GetForBrowserContext(_profile)
           ->GetRuleManager()
           ->GetActiveExceptionList(
               adblock_filter::RuleGroup::kAdBlockingRules) ==
       adblock_filter::RuleManager::kExemptList);
  ResolveJavascriptCallback(callback_id, adBlockEnabled);
}

// Send down boolean for if Tracker Blocker is enabled.
void PrivacyReportHandler::HandleIsTrackerBlockEnabled(
    const base::Value::List& args) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const base::Value& callback_id = args[0];
  bool trackerBlockEnabled =
      (adblock_filter::RuleServiceFactory::GetForBrowserContext(_profile)
           ->GetRuleManager()
           ->GetActiveExceptionList(
               adblock_filter::RuleGroup::kTrackingRules) ==
       adblock_filter::RuleManager::kExemptList);
  ResolveJavascriptCallback(callback_id, trackerBlockEnabled);
}

// Gets all the trackers that have been blocked and returns it as a list
// of tracker names and how many times it has been blocked.
void PrivacyReportHandler::HandleGetBlockingData(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::string* callback_id = args[0].GetIfString();
  int interval;
  base::StringToInt(args[1].GetString(), &interval);
  CHECK(adblock_filter::RuleServiceFactory::GetForBrowserContext(_profile)
            ->IsLoaded());
  auto callback = base::BindOnce(&PrivacyReportHandler::OnStatsDataLoaded,
                                 weak_factory_.GetWeakPtr(), *callback_id);

  const auto* stats_store =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(_profile)
          ->GetStatsStore();

  base::Time interval_time;
  switch (interval) {
    case 1:
      interval_time = base::Time::Now() - base::Days(7);
      break;
    case 2:
      interval_time = base::Time::Now() - base::Days(30);
      break;
    case 3:
      interval_time = base::Time::UnixEpoch();
      break;
    default:
      interval_time = base::Time::UnixEpoch();
  }
  stats_store->GetStatsData(interval_time, base::Time::Now(),
                            std::move(callback));
}

void PrivacyReportHandler::OpenLinkInNewTab(const base::Value::List& args) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string url = args[1].GetString();
#if BUILDFLAG(IS_ANDROID)
  webUINativeCalls::openNewTab(url);
  PrivacyReportHandler::CloseActivity(web_ui()->GetWebContents());
#endif
}
void PrivacyReportHandler::CloseActivityFromJS(const base::Value::List& args) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrivacyReportHandler::CloseActivity(web_ui()->GetWebContents());
}
void PrivacyReportHandler::OnJavascriptAllowed() {}
void PrivacyReportHandler::OnJavascriptDisallowed() {}

base::Value::List ToVivaldiBlockedCounter(
    const adblock_filter::StatsData::Entries& entries) {
  base::Value::List res;
  for (const auto& entry : entries) {
    base::Value::List inner_list;
    inner_list.Append(entry.host);
    inner_list.Append(CastOutputNumberForJS(entry.ad_count));
    inner_list.Append(CastOutputNumberForJS(entry.tracker_count));
    res.Append(std::move(inner_list));
  }
  return res;
}
void PrivacyReportHandler::OnStatsDataLoaded(
    std::string callback_id,
    const std::unique_ptr<adblock_filter::StatsData> data) {
  AllowJavascript();
  const adblock_filter::StatsData::Entries& website_entries =
      data->WebsiteEntries();
  const adblock_filter::StatsData::Entries& tracker_entries =
      data->TrackerEntries();
  const base::Time reporting_start = data->ReportingStart();

  base::Value::List results;
  results.Append(
      CastOutputNumberForJS(reporting_start.InMillisecondsFSinceUnixEpoch()));
  results.Append(CastOutputNumberForJS(data->TotalAdsBlocked()));
  results.Append(CastOutputNumberForJS(data->TotalTrackersBlocked()));
  results.Append(ToVivaldiBlockedCounter(tracker_entries));
  results.Append(ToVivaldiBlockedCounter(website_entries));

  ResolveJavascriptCallback(callback_id, results);
}

void PrivacyReportHandler::CloseActivity(content::WebContents* webContents) {
#if BUILDFLAG(IS_ANDROID)
  webUINativeCalls::closeActivity(webContents);
#endif
}

}  // namespace

namespace vivaldi {

const char kVivaldiPrivacyReportHost[] = "privacy-report";

PrivacyReportUI::PrivacyReportUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Add the Handler for communication between JS and c++
  web_ui->AddMessageHandler(std::make_unique<PrivacyReportHandler>(web_ui));
  // Setup the actual web_ui source
  CreateAndAddPrivacyReportUIHTMLSource(Profile::FromWebUI(web_ui));
}

PrivacyReportUI::~PrivacyReportUI() {}

}  // namespace vivaldi
