// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_PANELS_FILTER_H_
#define COMPONENTS_REQUEST_FILTER_PANELS_FILTER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class VivaldiPanelsThrottle : public content::NavigationThrottle {
 public:
  explicit VivaldiPanelsThrottle(content::NavigationHandle* handle);
  ~VivaldiPanelsThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

  static bool IsRelevant(content::NavigationHandle* handle);

 private:
  base::WeakPtrFactory<VivaldiPanelsThrottle> weak_factory_{this};
};

#endif  // COMPONENTS_REQUEST_FILTER_PANELS_FILTER_H_
