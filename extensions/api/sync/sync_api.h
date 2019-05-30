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
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

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
class SyncEventRouter : public syncer::SyncServiceObserver {
 public:
  explicit SyncEventRouter(Profile* profile);
  ~SyncEventRouter() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  Profile* profile_;

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

  void StartSyncSetup(syncer::SyncService* sync);
  void SyncSetupComplete();

 private:
  friend class BrowserContextKeyedAPIFactory<SyncAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SyncAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<SyncEventRouter> sync_event_router_;

  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_setup_handle_;
};

class SyncStartFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.start", SYNC_START)
  SyncStartFunction() = default;

 private:
  ~SyncStartFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncStartFunction);
};

class SyncStopFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.stop", SYNC_STOP)
  SyncStopFunction() = default;

 private:
  ~SyncStopFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncStopFunction);
};

class SyncSetEncryptionPasswordFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setEncryptionPassword",
                             SYNC_SET_ENCRYPTION_PASSWORD)
  SyncSetEncryptionPasswordFunction() = default;

 private:
  ~SyncSetEncryptionPasswordFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetEncryptionPasswordFunction);
};

class SyncGetDefaultSessionNameFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.getDefaultSessionName",
                             SYNC_GET_DEFAULT_SESSION_NAME)
  SyncGetDefaultSessionNameFunction();

 private:
  ~SyncGetDefaultSessionNameFunction() override;
  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetDefaultSessionName(const std::string& session_name);

  DISALLOW_COPY_AND_ASSIGN(SyncGetDefaultSessionNameFunction);
};

class SyncSetTypesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setTypes", SYNC_SET_TYPES)
  SyncSetTypesFunction() = default;

 private:
  ~SyncSetTypesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetTypesFunction);
};

class SyncGetEngineStateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.getEngineState", SYNC_GET_ENGINE_STATE)
  SyncGetEngineStateFunction() = default;

 private:
  ~SyncGetEngineStateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncGetEngineStateFunction);
};

class SyncGetLastCycleStateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.getLastCycleState",
                             SYNC_GET_LAST_CYCLE_STATE)
  SyncGetLastCycleStateFunction() = default;

 private:
  ~SyncGetLastCycleStateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncGetLastCycleStateFunction);
};

class SyncSetupCompleteFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.setupComplete", SYNC_SETUP_COMPLETE)
  SyncSetupCompleteFunction() = default;

 private:
  ~SyncSetupCompleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupCompleteFunction);
};

class SyncClearDataFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.clearData", SYNC_CLEAR_DATA)
  SyncClearDataFunction() = default;

 private:
  ~SyncClearDataFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncClearDataFunction);
};

class SyncUpdateNotificationClientStatusFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.updateNotificationClientStatus",
                             SYNC_UPDATE_NOTIFICATION_CLIENT_STATUS)
  SyncUpdateNotificationClientStatusFunction() = default;

 private:
  ~SyncUpdateNotificationClientStatusFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncUpdateNotificationClientStatusFunction);
};

class SyncNotificationReceivedFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.notificationReceived",
                             SYNC_NOTIFICATION_RECEIVED)
  SyncNotificationReceivedFunction() = default;

 private:
  ~SyncNotificationReceivedFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SyncNotificationReceivedFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SYNC_SYNC_API_H_
