//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/schema/vivaldi_utilities.h"

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

class UtilitiesIsUrlValidFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isUrlValid", UTILITIES_ISURLVALID)
  UtilitiesIsUrlValidFunction();

 protected:
  ~UtilitiesIsUrlValidFunction() override;

  // ExtensionFunction:
  bool RunSync() override;

 private:
};

class UtilitiesClearAllRecentlyClosedSessionsFunction
    : public ChromeAsyncExtensionFunction {
 protected:
  ~UtilitiesClearAllRecentlyClosedSessionsFunction() override {}
  bool RunAsync() override;
  DECLARE_EXTENSION_FUNCTION("utilities.clearAllRecentlyClosedSessions",
                             UTILITIES_CLEARALLRECENTLYCLOSEDSESSIONS)
 private:
};

class UtilitiesGetAvailablePageEncodingsFunction
    : public ChromeSyncExtensionFunction {
 protected:
  ~UtilitiesGetAvailablePageEncodingsFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("utilities.getAvailablePageEncodings",
                             UTILITIES_GETAVAILABLEPAGEENCODINGS)
 private:
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
