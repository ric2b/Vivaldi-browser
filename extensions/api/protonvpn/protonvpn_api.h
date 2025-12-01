// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_PROTONVPN_API_H
#define EXTENSIONS_API_PROTONVPN_API_H

#include "extensions/browser/extension_function.h"

namespace extensions {

class ProtonvpnGetStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("protonvpn.getStatus", PROTONVPN_PRIVATE_GET_STATUS)

 protected:
  ~ProtonvpnGetStatusFunction() override = default;
  ResponseAction Run() override;
};

} // namespace extensions

#endif /* EXTENSIONS_API_PROTONVPN_API_H */
