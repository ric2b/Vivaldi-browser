// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/interstitial/document_blocked_interstitial.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace adblock_filter {
namespace {
void DoAllowDomain(adblock_filter::RuleService* service,
                   adblock_filter::RuleGroup group,
                   const std::string& domain) {
  service->RemoveExceptionForDomain(
      group, adblock_filter::RuleService::kProcessList, domain);
  if (service->GetActiveExceptionList(group) ==
      adblock_filter::RuleService::kExemptList) {
    service->AddExceptionForDomain(
        group, adblock_filter::RuleService::kExemptList, domain);
  }
}
}  // namespace
// static
const security_interstitials::SecurityInterstitialPage::TypeID
    DocumentBlockedInterstitial::kTypeForTesting =
        &DocumentBlockedInterstitial::kTypeForTesting;

DocumentBlockedInterstitial::DocumentBlockedInterstitial(
    content::WebContents* web_contents,
    const GURL& request_url,
    RuleGroup blocking_group,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      blocking_group_(blocking_group) {}

DocumentBlockedInterstitial::~DocumentBlockedInterstitial() = default;

security_interstitials::SecurityInterstitialPage::TypeID
DocumentBlockedInterstitial::GetTypeForTesting() {
  return DocumentBlockedInterstitial::kTypeForTesting;
}

bool DocumentBlockedInterstitial::ShouldDisplayURL() const {
  return false;
}

void DocumentBlockedInterstitial::OnInterstitialClosing() {}

void DocumentBlockedInterstitial::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);

  base::string16 blocker_name =
      blocking_group_ == RuleGroup::kTrackingRules
          ? l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_TRACKER_BLOCKER)
          : l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_AD_BLOCKER);
  base::string16 block_type =
      blocking_group_ == RuleGroup::kTrackingRules
          ? l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_TRACKERS)
          : l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_ADS);

  load_time_data->SetString(
      "tabTitle",
      l10n_util::GetStringFUTF16(IDS_DOCUMENT_BLOCKED_TAB_TITLE, blocker_name));
  load_time_data->SetString(
      "heading",
      l10n_util::GetStringFUTF16(IDS_DOCUMENT_BLOCKED_HEADING, blocker_name));
  load_time_data->SetString(
      "primaryParagraph", l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_INFO));
  load_time_data->SetString(
      "proceedButtonText",
      l10n_util::GetStringFUTF16(IDS_DOCUMENT_BLOCKED_ALLOW_DOMAIN, block_type,
                                 GetFormattedHostName()));
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_DOCUMENT_BLOCKED_GO_BACK));
}

// This handles the commands sent from the interstitial JavaScript.
void DocumentBlockedInterstitial::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  switch (cmd) {
    case security_interstitials::CMD_DONT_PROCEED:
      controller()->GoBack();
      break;
    case security_interstitials::CMD_PROCEED: {
      auto* service = RuleServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
      std::string domain = request_url().host();
      DoAllowDomain(service, adblock_filter::RuleGroup::kAdBlockingRules,
                    domain);
      if (blocking_group_ == adblock_filter::RuleGroup::kTrackingRules) {
        DoAllowDomain(service, adblock_filter::RuleGroup::kTrackingRules,
                      domain);
      }
      controller()->Proceed();
    } break;
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_SHOW_MORE_SECTION:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_OPEN_HELP_CENTER:
    case security_interstitials::CMD_RELOAD:
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_LOGIN:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Not supported by the lookalike URL warning page.
      NOTREACHED() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

int DocumentBlockedInterstitial::GetHTMLTemplateId() {
  return IDR_DOCUMENT_BLOCKED_INTERSTITIAL_HTML;
}

}  // namespace adblock_filter
