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

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "sync/vivaldi_sync_model_observer.h"

class VivaldiSyncModel;

namespace content {
class BrowserContext;
}

namespace vivaldi {
class VivaldiSyncManager;
}

using vivaldi::VivaldiSyncModelObserver;
using vivaldi::VivaldiSyncManager;

namespace extensions {

// Observes SyncModel and then routes the notifications as events to
// the extension system.
class SyncEventRouter : public VivaldiSyncModelObserver {
 public:
  explicit SyncEventRouter(Profile* profile);
  ~SyncEventRouter() override;

  // VivaldiSyncModelObserver:
  void SyncOnMessage(const std::string& param1,
                     const std::string& param2) override;
  void SyncModelBeingDeleted(VivaldiSyncModel* model) override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;
  VivaldiSyncModel* model_;

  VivaldiSyncManager* client_;

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

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<SyncEventRouter> sync_event_router_;
};

class SyncSendFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sync.send", SYNC_SEND)
  SyncSendFunction();

 private:
  ~SyncSendFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SyncSendFunction);
};
}  // namespace extensions

#endif  // EXTENSIONS_API_SYNC_SYNC_API_H_
