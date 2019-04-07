// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_

#include "chrome/common/extensions/chrome_extensions_client.h"

namespace extensions {

class VivaldiExtensionsClient : public ChromeExtensionsClient {
 public:
  VivaldiExtensionsClient();
  ~VivaldiExtensionsClient() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_
