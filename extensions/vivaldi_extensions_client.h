// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_

#include "chrome/common/extensions/chrome_extensions_client.h"

namespace extensions {

class VivaldiExtensionsClient : public ChromeExtensionsClient {
 public:
  VivaldiExtensionsClient();
  ~VivaldiExtensionsClient() override;
  VivaldiExtensionsClient(const VivaldiExtensionsClient&) = delete;
  VivaldiExtensionsClient& operator=(const VivaldiExtensionsClient&) = delete;
};

}  // namespace extensions

#endif  // EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_
