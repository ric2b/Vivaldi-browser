// Copyright 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef VIVALDI_EXTENSION_HOST_H_
#define VIVALDI_EXTENSION_HOST_H_

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)

#include <string>

#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class ExtensionHostDelegate;
}  // namespace extensions

namespace vivaldi {

class VivaldiExtensionHost
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  VivaldiExtensionHost(content::BrowserContext* browser_context,
                       const GURL& url,
                       extensions::mojom::ViewType host_type,
                       content::WebContents* webcontents);

  ~VivaldiExtensionHost() override;
  VivaldiExtensionHost(const VivaldiExtensionHost&) = delete;
  VivaldiExtensionHost& operator=(const VivaldiExtensionHost&) = delete;

 private:
  std::unique_ptr<extensions::ExtensionHostDelegate> delegate_;
};

}  // namespace vivaldi

#endif  // Extensions

#endif  // VIVALDI_EXTENSION_HOST_H_
