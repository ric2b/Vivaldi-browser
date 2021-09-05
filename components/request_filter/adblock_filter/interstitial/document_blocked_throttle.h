// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_THROTTLE_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_THROTTLE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace adblock_filter {
// Observes navigations and shows an interstitial if the navigation in a frame
// was rejected by the ad blocker.
class DocumentBlockedThrottle : public content::NavigationThrottle {
 public:
  explicit DocumentBlockedThrottle(content::NavigationHandle* handle);
  ~DocumentBlockedThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;

 private:
  base::WeakPtrFactory<DocumentBlockedThrottle> weak_factory_{this};
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_INTERSTITIAL_DOCUMENT_BLOCKED_THROTTLE_H_
