// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_EXTENSIONS_INIT_H_
#define EXTENSIONS_VIVALDI_EXTENSIONS_INIT_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class VivaldiExtensionInit : public BrowserContextKeyedAPI {
 public:
  explicit VivaldiExtensionInit(content::BrowserContext* context);
  ~VivaldiExtensionInit() override;

  // Convenience method to get the instance for a profile.
  static VivaldiExtensionInit* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<VivaldiExtensionInit>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiExtensionInit>;

  static const char* service_name() { return "VivaldiExtensionInit"; }
  static const bool kServiceRedirectedInIncognito = true;

  DISALLOW_COPY_AND_ASSIGN(VivaldiExtensionInit);
};

}  // namespace extensions

#endif  // EXTENSIONS_VIVALDI_EXTENSIONS_INIT_H_
