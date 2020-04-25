// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_ACCESS_KEYS_H_
#define EXTENSIONS_API_ACCESS_KEYS_H_

#include "renderer/vivaldi_render_messages.h"
#include "extensions/browser/extension_function.h"

#include <string>

namespace extensions {

class AccessKeysGetAccessKeysForPageFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.getAccessKeysForPage",
                             ACCESSKEYS_GETACCESSKEYSFORPAGE)

  AccessKeysGetAccessKeysForPageFunction() = default;

 private:
  ~AccessKeysGetAccessKeysForPageFunction() override = default;

  ResponseAction Run() override;

  void AccessKeysReceived(std::vector<VivaldiViewMsg_AccessKeyDefinition>);

  DISALLOW_COPY_AND_ASSIGN(AccessKeysGetAccessKeysForPageFunction);
};

class AccessKeysActionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.action",
                             ACCESSKEYS_ACTION)

  AccessKeysActionFunction() = default;

 private:
  ~AccessKeysActionFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AccessKeysActionFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_ACCESS_KEYS_H_
