// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_
#define EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"
#include "vivaldi_account/vivaldi_account_password_handler.h"

class VivaldiSyncModel;

namespace content {
class BrowserContext;
}

namespace extensions {

namespace vivaldi {
namespace vivaldi_account {
struct PendingRegistration;
}  // namespace vivaldi_account
}  // namespace vivaldi

class VivaldiAccountEventRouter
    : public ::vivaldi::VivaldiAccountManager::Observer,
      public ::vivaldi::VivaldiAccountPasswordHandler::Observer {
 public:
  explicit VivaldiAccountEventRouter(Profile* profile);
  ~VivaldiAccountEventRouter() override;

  // VivaldiAccountManager::Observer implementation.
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnTokenFetchFailed() override;
  void OnVivaldiAccountShutdown() override;

  // VivaldiAccountPasswordHandler::Observer implementation.
  void OnAccountPasswordStateChanged() override;

 private:
  const raw_ptr<Profile> profile_;
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

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VivaldiAccountAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<VivaldiAccountEventRouter> vivaldi_account_event_router_;
};

class VivaldiAccountLoginFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.login", VIVALDI_ACCOUNT_LOGIN)
  VivaldiAccountLoginFunction() = default;

 private:
  ~VivaldiAccountLoginFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class VivaldiAccountLogoutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.logout", VIVALDI_ACCOUNT_LOGOUT)
  VivaldiAccountLogoutFunction() = default;

 private:
  ~VivaldiAccountLogoutFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class VivaldiAccountGetStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.getState",
                             VIVALDI_ACCOUNT_GET_STATE)
  VivaldiAccountGetStateFunction() = default;

 private:
  ~VivaldiAccountGetStateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class VivaldiAccountSetPendingRegistrationFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.setPendingRegistration",
                             VIVALDI_ACCOUNT_SET_PENDING_REGISTRATION)
  VivaldiAccountSetPendingRegistrationFunction() = default;

 private:
  ~VivaldiAccountSetPendingRegistrationFunction() override = default;
  void OnEncryptDone(
      std::optional<vivaldi::vivaldi_account::PendingRegistration>
          pending_registration,
      std::unique_ptr<std::string> encrypted_password,
      bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class VivaldiAccountGetPendingRegistrationFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("vivaldiAccount.getPendingRegistration",
                             VIVALDI_ACCOUNT_GET_PENDING_REGISTRATION)
  VivaldiAccountGetPendingRegistrationFunction() = default;

 private:
  ~VivaldiAccountGetPendingRegistrationFunction() override = default;
  void OnDecryptDone(
      std::optional<vivaldi::vivaldi_account::PendingRegistration>
          pending_registration,
      bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_H_
