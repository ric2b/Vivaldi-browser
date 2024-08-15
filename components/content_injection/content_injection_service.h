// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_H_
#define COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_H_

#include "components/content_injection/content_injection_provider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace content_injection {
class Service : public KeyedService {
 public:
  Service();
  ~Service() override;

  virtual std::optional<int> RegisterWorldForJSInjection(
      mojom::JavascriptWorldInfoPtr world_info) = 0;

  virtual bool AddProvider(Provider* provider) = 0;
  virtual void OnStaticContentChanged() = 0;
  virtual bool RemoveProvider(Provider* provider) = 0;
};
}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_H_
