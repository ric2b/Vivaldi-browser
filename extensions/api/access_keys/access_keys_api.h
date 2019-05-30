// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_ACCESS_KEYS_H_
#define EXTENSIONS_API_ACCESS_KEYS_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "renderer/vivaldi_render_messages.h"

#include <string>

namespace extensions {

class AccessKeysGetAccessKeysForPageFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.getAccessKeysForPage",
                             ACCESSKEYS_GETACCESSKEYSFORPAGE)

  AccessKeysGetAccessKeysForPageFunction();
  void AccessKeysReceived(std::vector<VivaldiViewMsg_AccessKeyDefinition>);

 protected:
  ~AccessKeysGetAccessKeysForPageFunction() override;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(AccessKeysGetAccessKeysForPageFunction);
};

class AccessKeysActionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("accessKeys.action",
                             ACCESSKEYS_ACTION)

  AccessKeysActionFunction();

 protected:
  ~AccessKeysActionFunction() override;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(AccessKeysActionFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_ACCESS_KEYS_H_
