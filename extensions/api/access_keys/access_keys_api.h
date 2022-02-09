// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_ACCESS_KEYS_H_
#define EXTENSIONS_API_ACCESS_KEYS_H_

#include <string>

#include "extensions/browser/extension_function.h"

#include "renderer/mojo/vivaldi_frame_service.mojom.h"

namespace extensions {

class AccessKeysGetAccessKeysForPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.getAccessKeysForPage",
                             ACCESSKEYS_GETACCESSKEYSFORPAGE)

  AccessKeysGetAccessKeysForPageFunction() = default;

 private:
  ~AccessKeysGetAccessKeysForPageFunction() override = default;

  ResponseAction Run() override;

  void AccessKeysReceived(std::vector<::vivaldi::mojom::AccessKeyPtr>);
};

class AccessKeysActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.action", ACCESSKEYS_ACTION)

  AccessKeysActionFunction() = default;

 private:
  ~AccessKeysActionFunction() override = default;

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_ACCESS_KEYS_H_
