// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_CONTROLLER_CLIENT_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_CONTROLLER_CLIENT_H_

#include <memory>

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace adblock_filter {
class DocumentBlockedControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  DocumentBlockedControllerClient(content::WebContents* web_contents,
                                  const GURL& request_url);

  ~DocumentBlockedControllerClient() override;
  DocumentBlockedControllerClient(const DocumentBlockedControllerClient&) =
      delete;
  DocumentBlockedControllerClient& operator=(
      const DocumentBlockedControllerClient&) = delete;

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;
  void Proceed() override;

 private:
  const GURL request_url_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_CONTROLLER_CLIENT_H_
