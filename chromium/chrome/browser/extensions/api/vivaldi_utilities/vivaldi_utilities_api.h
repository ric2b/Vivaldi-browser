//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef CHROME_BROWSER_EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/vivaldi_utilities.h"

namespace extensions {

class UtilitiesIsTabInLastSessionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isTabInLastSession",
                             UTILITIES_ISTABINLASTSESSION)
  UtilitiesIsTabInLastSessionFunction();

 protected:
  ~UtilitiesIsTabInLastSessionFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
