// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_INFOBARS_INFOBARS_API_H_
#define EXTENSIONS_API_INFOBARS_INFOBARS_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class InfobarsSendButtonActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("infobars.sendButtonAction",
                             INFOBARS_SENDBUTTONACTION)

  InfobarsSendButtonActionFunction() = default;

 private:
  ~InfobarsSendButtonActionFunction() override = default;

  // BookmarksFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_INFOBARS_INFOBARS_API_H_
