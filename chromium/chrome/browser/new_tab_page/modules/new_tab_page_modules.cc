// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_clusters/core/features.h"
#include "components/page_image_service/features.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"

namespace ntp {

const std::vector<std::pair<const std::string, int>> MakeModuleIdNames(
    bool drive_module_enabled,
    bool is_managed_profile) {
  std::vector<std::pair<const std::string, int>> details;

  if (base::FeatureList::IsEnabled(ntp_features::kNtpCalendarModule) &&
      (is_managed_profile || (!base::GetFieldTrialParamValueByFeature(
                                   ntp_features::kNtpCalendarModule,
                                   ntp_features::kNtpCalendarModuleDataParam)
                                   .empty() &&
                              base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kSignedOutNtpModulesSwitch)))) {
    details.emplace_back("google_calendar",
                         IDS_NTP_MODULES_GOOGLE_CALENDAR_TITLE);
  }

  if (is_managed_profile &&
      base::FeatureList::IsEnabled(ntp_features::kNtpOutlookCalendarModule)) {
    details.emplace_back("outlook_calendar",
                         IDS_NTP_MODULES_OUTLOOK_CALENDAR_TITLE);
  }

  if (drive_module_enabled) {
    details.emplace_back("drive", IDS_NTP_MODULES_DRIVE_SENTENCE);
  }

  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpMostRelevantTabResumptionModule)) {
    details.emplace_back("tab_resumption",
                         IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE);
  } else if (base::FeatureList::IsEnabled(
                 ntp_features::kNtpTabResumptionModule)) {
    details.emplace_back("tab_resumption", IDS_NTP_TAB_RESUMPTION_TITLE);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFeedModule)) {
    details.emplace_back("feed", IDS_NTP_MODULES_FEED_TITLE);
  }

#if !defined(OFFICIAL_BUILD)
  if (base::FeatureList::IsEnabled(ntp_features::kNtpDummyModules)) {
    details.emplace_back("dummy", IDS_NTP_MODULES_DUMMY_TITLE);
  }
#endif

  return details;
}

bool HasModulesEnabled(
    std::vector<std::pair<const std::string, int>> module_id_names,
    signin::IdentityManager* identity_manager) {
  return !module_id_names.empty() &&
         !base::FeatureList::IsEnabled(ntp_features::kNtpModulesLoad) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSignedOutNtpModulesSwitch) ||
          (/* Can be null if Chrome signin is disabled. */ identity_manager &&
           identity_manager->GetAccountsInCookieJar()
                   .signed_in_accounts.size() > 0));
}

}  // namespace ntp
