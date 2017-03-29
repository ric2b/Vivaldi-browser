// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef CHROME_BROWSER_EXTENSIONS_API_UIZOOM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_UIZOOM_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

  class UizoomSetVivaldiUIZoomFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("uizoom.setVivaldiUIZoom",
   UIZOOM_SET_VIVALDI_UI_ZOOM)
     UizoomSetVivaldiUIZoomFunction();

 protected:
   ~UizoomSetVivaldiUIZoomFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(UizoomSetVivaldiUIZoomFunction);
};

  class UizoomGetVivaldiUIZoomFunction :
  public ChromeAsyncExtensionFunction{
   public:
    DECLARE_EXTENSION_FUNCTION("uizoom.getVivaldiUIZoom",
    UIZOOM_GET_VIVALDI_UI_ZOOM)
      UizoomGetVivaldiUIZoomFunction();

   protected:
     ~UizoomGetVivaldiUIZoomFunction() override;
    // ExtensionFunction:
    bool RunAsync() override;

    DISALLOW_COPY_AND_ASSIGN(UizoomGetVivaldiUIZoomFunction);
  };

}

#endif  // CHROME_BROWSER_EXTENSIONS_API_UIZOOM_API_H_
