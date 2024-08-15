// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_INTERSTITIAL_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_INTERSTITIAL_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

class GURL;

namespace adblock_filter {
// This class is responsible for showing/hiding the interstitial page that
// occurs when a document gets blocked by the ad/tracker blocker
class DocumentBlockedInterstitial
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  DocumentBlockedInterstitial(
      content::WebContents* web_contents,
      const GURL& request_url,
      RuleGroup blocking_group,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  ~DocumentBlockedInterstitial() override;
  DocumentBlockedInterstitial(const DocumentBlockedInterstitial&) = delete;
  DocumentBlockedInterstitial& operator=(const DocumentBlockedInterstitial&) =
      delete;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
  void OnInterstitialClosing() override;
  bool ShouldDisplayURL() const override;
  int GetHTMLTemplateId() override;

 private:
  friend class LookalikeUrlNavigationThrottleBrowserTest;

  RuleGroup blocking_group_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_INTERSTITIAL_H_
