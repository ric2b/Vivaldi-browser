// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTOUPDATE_API_H_
#define EXTENSIONS_API_AUTOUPDATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/schema/autoupdate.h"

namespace extensions {

class AutoUpdateCheckForUpdatesFunction : public ChromeAsyncExtensionFunction {
public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.checkForUpdates",
                             AUTOUPDATE_CHECKFORUPDATES)
  AutoUpdateCheckForUpdatesFunction();

protected:
  ~AutoUpdateCheckForUpdatesFunction() override;
  bool RunAsync() override;
  DISALLOW_COPY_AND_ASSIGN(AutoUpdateCheckForUpdatesFunction);
};

}  // namespace extensions
#endif  // EXTENSIONS_API_AUTOUPDATE_API_H_
