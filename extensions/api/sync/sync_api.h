// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// See
// http://www.chromium.org/developers/design-documents/extensions/proposed-changes/creating-new-apis

#ifndef EXTENSIONS_API_SYNC_SYNC_API_H_
#define EXTENSIONS_API_SYNC_SYNC_API_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "sync/vivaldi_sync_manager_observer.h"

class VivaldiSyncModel;

namespace content {
class BrowserContext;
}

namespace vivaldi {
class VivaldiSyncManager;
}

namespace extensions {

// Observes SyncModel and then routes the notifications as events to
// the extension system.
class SyncEventRouter : public ::vivaldi::VivaldiSyncManagerObserver {
 public:
  explicit SyncEventRouter(Profile* profile);
  ~SyncEventRouter() override;

  // VivaldiSyncModelObserver:

  void OnAccessTokenRequested() override;
  void OnEncryptionPasswordRequested() override;
  void OnEngineStarted() override;
  void OnEngineInitFailed() override;
  void OnBeginSyncing() override;
  void OnEndSyncing() override;
  void OnEngineStopped() override;
  void OnDeletingSyncManager() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;

  base::WeakPtr<::vivaldi::VivaldiSyncManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(SyncEventRouter);
};

class SyncAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit SyncAPI(content::BrowserContext* context);
  ~SyncAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SyncAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<SyncAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SyncAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<SyncEventRouter> sync_event_router_;
};

class SyncStartFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.start", SYNC_START)
  SyncStartFunction() = default;

 private:
  ~SyncStartFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncStartFunction);
};

class SyncRefreshTokenFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.refreshToken", SYNC_REFRESH_TOKEN)
  SyncRefreshTokenFunction() = default;

 private:
  ~SyncRefreshTokenFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncRefreshTokenFunction);
};

class SyncSetEncryptionPasswordFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setEncryptionPassword",
                             SYNC_SET_ENCRYPTION_PASSWORD)
  SyncSetEncryptionPasswordFunction() = default;

 private:
  ~SyncSetEncryptionPasswordFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetEncryptionPasswordFunction);
};

class SyncIsFirstSetupFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.isFirstSetup", SYNC_IS_FIRST_SETUP)
  SyncIsFirstSetupFunction() = default;

 private:
  ~SyncIsFirstSetupFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncIsFirstSetupFunction);
};

class SyncIsEncryptionPasswordSetUpFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.isEncryptionPasswordSetUp",
                             SYNC_IS_ENCRYPTION_PASSWORD_SET_UP)
  SyncIsEncryptionPasswordSetUpFunction() = default;

 private:
  ~SyncIsEncryptionPasswordSetUpFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncIsEncryptionPasswordSetUpFunction);
};

class SyncSetTypesFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setTypes", SYNC_SET_TYPES)
  SyncSetTypesFunction() = default;

 private:
  ~SyncSetTypesFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetTypesFunction);
};

class SyncGetTypesFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.getTypes", SYNC_GET_TYPES)
  SyncGetTypesFunction() = default;

 private:
  ~SyncGetTypesFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncGetTypesFunction);
};

class SyncGetStatusFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.getStatus", SYNC_GET_STATUS)
  SyncGetStatusFunction() = default;

 private:
  ~SyncGetStatusFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncGetStatusFunction);
};

class SyncSetupCompleteFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setupComplete", SYNC_SETUP_COMPLETE)
  SyncSetupCompleteFunction() = default;

 private:
  ~SyncSetupCompleteFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupCompleteFunction);
};

class SyncLogoutFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.logout", SYNC_LOGOUT)
  SyncLogoutFunction() = default;

 private:
  ~SyncLogoutFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncLogoutFunction);
};

class SyncClearDataFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.clearData", SYNC_CLEAR_DATA)
  SyncClearDataFunction() = default;

 private:
  ~SyncClearDataFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncClearDataFunction);
};

class SyncUpdateNotificationClientStatusFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.updateNotificationClientStatus",
                             SYNC_UPDATE_NOTIFICATION_CLIENT_STATUS)
  SyncUpdateNotificationClientStatusFunction() = default;

 private:
  ~SyncUpdateNotificationClientStatusFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncUpdateNotificationClientStatusFunction);
};

class SyncNotificationReceivedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.notificationReceived",
                             SYNC_NOTIFICATION_RECEIVED)
  SyncNotificationReceivedFunction() = default;

 private:
  ~SyncNotificationReceivedFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncNotificationReceivedFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SYNC_SYNC_API_H_
