// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/sync/sync_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/sync.h"
#include "sync/vivaldi_sync_model_factory.h"
#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

using vivaldi::VivaldiSyncManagerFactory;
using vivaldi::VivaldiSyncManager;

namespace extensions {

using vivaldi::sync::SyncParamItem;

typedef std::vector<vivaldi::sync::SyncParamItem> SyncParamItemVector;

SyncEventRouter::SyncEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(SyncModelFactory::GetForProfile(profile)),
      client_(VivaldiSyncManagerFactory::GetForProfileVivaldi(profile)) {
  model_->AddObserver(this);
}

SyncEventRouter::~SyncEventRouter() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

// Helper to actually dispatch an event to extension listeners.
void SyncEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

void SyncEventRouter::SyncOnMessage(const std::string& param1,
                                    const std::string& param2) {
  DispatchEvent(vivaldi::sync::OnChanged::kEventName,
                vivaldi::sync::OnChanged::Create(param1, param2));
}

void SyncEventRouter::SyncModelBeingDeleted(VivaldiSyncModel* model) {
  model_ = NULL;
}

SyncAPI::SyncAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, vivaldi::sync::OnChanged::kEventName);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SyncAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SyncAPI>* SyncAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

SyncAPI::~SyncAPI() {}

void SyncAPI::OnListenerAdded(const EventListenerInfo& details) {
  sync_event_router_.reset(
      new SyncEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// KeyedService implementation.
void SyncAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

SyncSendFunction::SyncSendFunction() {}

SyncSendFunction::~SyncSendFunction() {}

bool SyncSendFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::Send::Params> params(
      vivaldi::sync::Send::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string string_value = params->event_name;

  SyncParamItemVector& syncParms = params->event_arg_list;

  std::unique_ptr<base::DictionaryValue> syncParmsValues(
      new base::DictionaryValue);
  DCHECK(syncParmsValues.get());

  for (SyncParamItemVector::const_iterator it = syncParms.begin();
       it != syncParms.end(); it++) {
    CHECK(!it->key.empty());
    syncParmsValues->SetString(it->key, it->value);
  }

  std::string result_value = "Success";
  bool success = true;
  Profile* profile = GetProfile();

  VivaldiSyncManager* syncmanager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(profile);
  CHECK(syncmanager);

  if (string_value == "vivaldiCompleteLogin") {
    syncmanager->HandleLoggedInMessage(*syncParmsValues.get());
  } else if (string_value == "vivaldiRefreshToken") {
    syncmanager->HandleRefreshToken(*syncParmsValues.get());
  } else if (string_value == "vivaldiLogout") {
    syncmanager->HandleLogOutMessage(*syncParmsValues.get());
  } else if (string_value == "vivaldiPollSync") {
    syncmanager->HandlePollServerMessage(*syncParmsValues.get());
  } else if (string_value == "vivaldiConfigurePolling") {
    syncmanager->HandleConfigurePollingMessage(*syncParmsValues.get());
  } else {
    result_value = "Failed";
    success = false;
  }

  results_ = vivaldi::sync::Send::Results::Create(result_value);

  SendResponse(success);
  return true;
}

}  // namespace extensions
