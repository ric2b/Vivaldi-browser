// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_PROVIDER_H_
#define COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_PROVIDER_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/content_injection/content_injection_types.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace content_injection {
class Service;

class Provider {
 public:
  Provider();
  virtual ~Provider();
  Provider(const Provider&) = delete;
  Provider& operator=(const Provider&) = delete;

  virtual mojom::InjectionsForFramePtr GetInjectionsForFrame(
      const GURL& url,
      content::RenderFrameHost* frame) = 0;

  virtual const std::map<std::string, StaticInjectionItem>&
  GetStaticContent() = 0;

 private:
  friend class Service;

  void OnAddedToService(base::WeakPtr<Service> service);
  void OnRemovedFromService();
  base::WeakPtr<Service> service_;
};
}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_PROVIDER_H_
