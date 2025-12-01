// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_API_DIRECT_MATCH_DIRECT_MATCH_API_H_
#define EXTENSIONS_API_DIRECT_MATCH_DIRECT_MATCH_API_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/direct_match/direct_match_favicon_installer.h"
#include "components/direct_match/direct_match_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class DirectMatchAPI : public BrowserContextKeyedAPI,
                       public extensions::ExtensionRegistryObserver,
                       public direct_match::DirectMatchService::Observer {
 public:
  explicit DirectMatchAPI(content::BrowserContext* context);
  ~DirectMatchAPI() = default;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<DirectMatchAPI>* GetFactoryInstance();

  // DirectMatchService::Observer
  void OnFinishedDownloadingDirectMatchUnits() override {}
  void OnFinishedDownloadingDirectMatchUnitsIcon() override;

 private:
  friend class BrowserContextKeyedAPIFactory<DirectMatchAPI>;
  const raw_ptr<content::BrowserContext> browser_context_;
  // Piggyback code. Included here as the installer depends on code from the
  // chrome module. It can not be added directly to the direct match service
  // which is in components. Code in components can not have that dependecy
  // (will not link).
  std::unique_ptr<direct_match::DirectMatchFaviconInstaller> favicon_installer_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DirectMatchAPI"; }

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // ExtensionRegistryObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
};

class DirectMatchHideFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("directMatch.hide", DIRECT_MATCH_HIDE)
  DirectMatchHideFunction() = default;

 private:
  ~DirectMatchHideFunction() override = default;
  ResponseAction Run() override;
};

class DirectMatchGetPopularSitesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("directMatch.getPopularSites",
                             DIRECT_MATCH_GET_POPULAR_SITES)
  DirectMatchGetPopularSitesFunction() = default;

 private:
  ~DirectMatchGetPopularSitesFunction() override = default;
  ResponseAction Run() override;
};

class DirectMatchGetForCategoryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("directMatch.getForCategory",
                             DIRECT_MATCH_GET_FOR_CATEGORY)
  DirectMatchGetForCategoryFunction() = default;

 private:
  ~DirectMatchGetForCategoryFunction() override = default;
  ResponseAction Run() override;
};

class DirectMatchResetHiddenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("directMatch.resetHidden",
                             DIRECT_MATCH_RESET_HIDDEN)
  DirectMatchResetHiddenFunction() = default;

 private:
  ~DirectMatchResetHiddenFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_DIRECT_MATCH_DIRECT_MATCH_API_H_
