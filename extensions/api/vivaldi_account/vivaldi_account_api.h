// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_
#define EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

class VivaldiSyncModel;

namespace content {
class BrowserContext;
}

namespace extensions {

class VivaldiAccountEventRouter
    : public ::vivaldi::VivaldiAccountManager::Observer {
 public:
  explicit VivaldiAccountEventRouter(Profile* profile);
  ~VivaldiAccountEventRouter() override;

  // VivaldiAccountManager::Observer implementation.
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnTokenFetchFailed() override;
  void OnAccountInfoFetchFailed() override;
  void OnVivaldiAccountShutdown() override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountEventRouter);
};

class VivaldiAccountAPI : public BrowserContextKeyedAPI,
                          public EventRouter::Observer {
 public:
  explicit VivaldiAccountAPI(content::BrowserContext* context);
  ~VivaldiAccountAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiAccountAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiAccountAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VivaldiAccountAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<VivaldiAccountEventRouter> vivaldi_account_event_router_;
};

class VivaldiAccountLoginFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.login", VIVALDI_ACCOUNT_LOGIN)
  VivaldiAccountLoginFunction() = default;

 private:
  ~VivaldiAccountLoginFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountLoginFunction);
};

class VivaldiAccountLogoutFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.logout", VIVALDI_ACCOUNT_LOGOUT)
  VivaldiAccountLogoutFunction() = default;

 private:
  ~VivaldiAccountLogoutFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountLogoutFunction);
};

class VivaldiAccountGetStateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.getState",
                             VIVALDI_ACCOUNT_GET_STATE)
  VivaldiAccountGetStateFunction() = default;

 private:
  ~VivaldiAccountGetStateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountGetStateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_
