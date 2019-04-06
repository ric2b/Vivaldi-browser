// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
#define EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/schema/window_private.h"

namespace extensions {

/*
This API will listen window events and do the appropriate actions
based on that.
*/
class VivaldiWindowsAPI : public BrowserListObserver,
                          public content::NotificationObserver,
                          public app_modal::AppModalDialogObserver,
                          public BrowserContextKeyedAPI {
 public:
  explicit VivaldiWindowsAPI(content::BrowserContext* context);
  ~VivaldiWindowsAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiWindowsAPI>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // AppModalDialogObserver implementation.
  void Notify(app_modal::JavaScriptAppModalDialog* dialog) override;

 protected:
  // chrome::BrowserListObserver implementation
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserAdded(Browser* browser) override;

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiWindowsAPI>;

  content::BrowserContext* browser_context_;

  content::NotificationRegistrar registrar_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "WindowsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class WindowPrivateCreateFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.create", WINDOW_PRIVATE_CREATE);

  WindowPrivateCreateFunction() = default;

 protected:
  ~WindowPrivateCreateFunction() override = default;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(WindowPrivateCreateFunction);
};

class WindowPrivateGetCurrentIdFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.getCurrentId",
                             WINDOW_PRIVATE_GET_CURRENT_ID);

  WindowPrivateGetCurrentIdFunction() = default;

 protected:
  ~WindowPrivateGetCurrentIdFunction() override = default;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(WindowPrivateGetCurrentIdFunction);
};

class WindowPrivateSetStateFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windowPrivate.setState",
                             WINDOW_PRIVATE_SET_STATE);

  WindowPrivateSetStateFunction() = default;

 protected:
  ~WindowPrivateSetStateFunction() override = default;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(WindowPrivateSetStateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_WINDOW_WINDOW_PRIVATE_API_H_
