// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_API_OMNIBOX_OMNIBOX_PRIVATE_API_H_
#define EXTENSIONS_API_OMNIBOX_OMNIBOX_PRIVATE_API_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/omnibox_service.h"
#include "components/omnibox/omnibox_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

using vivaldi_omnibox::OmniboxService;
using vivaldi_omnibox::OmniboxServiceObserver;

class OmniboxEventRouter : public OmniboxServiceObserver {
 public:
  OmniboxEventRouter(Profile* profile, OmniboxService* omnibox_service);

  ~OmniboxEventRouter() override;

 private:
  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     base::Value::List event_args);

  // OmniboxServiceObserver overrides:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;
  const raw_ptr<Profile> profile_;

  base::ScopedObservation<OmniboxService, OmniboxServiceObserver>
      omnibox_service_observer_;
};

class OmniboxPrivateAPI : public BrowserContextKeyedAPI,
                          public EventRouter::Observer {
 public:
  explicit OmniboxPrivateAPI(content::BrowserContext* context);
  ~OmniboxPrivateAPI() = default;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<OmniboxPrivateAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<OmniboxPrivateAPI>;
  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "OmniboxPrivateAPI"; }

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<OmniboxEventRouter> omnibox_event_router_;
};

class OmniboxPrivateStartOmniboxFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("omniboxPrivate.startOmnibox",
                             OMNIBOX_PRIVATE_START_OMNIBOX_QUERY)

 private:
  ~OmniboxPrivateStartOmniboxFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_OMNIBOX_OMNIBOX_PRIVATE_API_H_
