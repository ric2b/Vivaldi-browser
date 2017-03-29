//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef CHROME_BROWSER_EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/thumbnails.h"

namespace extensions {

class ThumbnailsIsThumbnailAvailableFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.isThumbnailAvailable",
                             THUMBNAILS_ISTHUMBNAILAVAILABLE)
  ThumbnailsIsThumbnailAvailableFunction();

 protected:
  ~ThumbnailsIsThumbnailAvailableFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
