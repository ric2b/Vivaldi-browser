// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_INFOBARS_INFOBARS_API_H_
#define EXTENSIONS_API_INFOBARS_INFOBARS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

class InfobarsSendButtonActionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("infobars.sendButtonAction",
                             INFOBARS_SENDBUTTONACTION);

  InfobarsSendButtonActionFunction();

 protected:
  ~InfobarsSendButtonActionFunction() override;

private:
  // BookmarksFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(InfobarsSendButtonActionFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_INFOBARS_INFOBARS_API_H_
