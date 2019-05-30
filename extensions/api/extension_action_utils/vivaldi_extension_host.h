// Copyright 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef VIVALDI_EXTENSION_HOST_H_
#define VIVALDI_EXTENSION_HOST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/view_type.h"

namespace content {
  class WebContents;
} // namespace content

namespace extensions {
class ExtensionHostDelegate;
}  // namespace extensions

namespace vivaldi {

class VivaldiExtensionHost
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  VivaldiExtensionHost(content::BrowserContext* browser_context,
                       extensions::ViewType host_type,
                       content::WebContents* webcontents);

  ~VivaldiExtensionHost() override;

 private:
  std::unique_ptr<extensions::ExtensionHostDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiExtensionHost);
};

}  // namespace vivaldi

#endif  // VIVALDI_EXTENSION_HOST_H_
