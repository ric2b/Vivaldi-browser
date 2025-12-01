// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/direct_match/direct_match_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "extensions/schema/direct_match.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

using direct_match::DirectMatchServiceFactory;

DirectMatchAPI::DirectMatchAPI(content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(context));
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context_);
  if (service) {
    service->AddObserver(this);
  }
}

void DirectMatchAPI::Shutdown() {
  extension_registry_observation_.Reset();
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context_);
  if (service) {
    service->RemoveObserver(this);
  }
}

void DirectMatchAPI::OnExtensionReady(content::BrowserContext* browser_context,
                                      const extensions::Extension* extension) {
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context);
  if (!service) {
    LOG(ERROR) << "No Direct Match instance";
    return;
  }

  service->OnExtensionReady(extension->id());
}

void DirectMatchAPI::OnFinishedDownloadingDirectMatchUnitsIcon() {
  ::vivaldi::BroadcastEvent(
      vivaldi::direct_match::OnPopularSitesReady::kEventName,
      vivaldi::direct_match::OnPopularSitesReady::Create(),
      browser_context_);
}

void DirectMatchAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context);
  if (!service) {
    LOG(ERROR) << "No Direct Match instance";
    return;
  }
  service->OnExtensionUnloaded(extension->id());
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<DirectMatchAPI>>::
    DestructorAtExit g_factory_direct_match = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<DirectMatchAPI>*
DirectMatchAPI::GetFactoryInstance() {
  return g_factory_direct_match.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    DirectMatchAPI>::DeclareFactoryDependencies() {}

ExtensionFunction::ResponseAction DirectMatchGetFunction::Run() {
  using vivaldi::direct_match::Get::Params;
  namespace Results = vivaldi::direct_match::Get::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto isWhitespaceOnly = [](const std::string& str) {
    return std::all_of(str.begin(), str.end(),
                       [](unsigned char ch) { return std::isspace(ch); });
  };

  if (isWhitespaceOnly(params->query)) {
    return RespondNow(NoArguments());
  }

  auto* service =
      DirectMatchServiceFactory::GetForBrowserContext(browser_context());
  auto [unit_found, allowed_to_be_default_match] =
      service->GetDirectMatch(params->query);
  if (unit_found) {
    vivaldi::direct_match::Item item;
    item.name = unit_found->name;
    item.title = unit_found->title;
    item.image_url = unit_found->image_url;
    item.image_path = unit_found->image_path;
    item.category = unit_found->category;
    item.display_location_address_bar =
        unit_found->display_locations.address_bar;
    item.display_location_sd_dialog = unit_found->display_locations.sd_dialog;
    item.redirect_url = unit_found->redirect_url;
    item.allowed_to_be_default_match = allowed_to_be_default_match;
    return RespondNow(ArgumentList(Results::Create(item)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction DirectMatchHideFunction::Run() {
  using vivaldi::direct_match::Hide::Params;
  namespace Results = vivaldi::direct_match::Hide::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  if (!service) {
    return RespondNow(Error("No Direct Match instance"));
  }
  bool result = service->HideDirectMatchFromOmnibox(params->url);
  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction DirectMatchGetPopularSitesFunction::Run() {
  namespace Results = vivaldi::direct_match::GetPopularSites::Results;
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  const auto& units = service->GetPopularSites();
  std::vector<vivaldi::direct_match::Item> items;
  if (units.size() > 0) {
    for (const auto& unit : units) {
      vivaldi::direct_match::Item item;
      item.name = unit->name;
      item.title = unit->title;
      item.image_url = unit->image_url;
      item.image_path = unit->image_path;
      item.category = unit->category;
      item.display_location_address_bar = unit->display_locations.address_bar;
      item.display_location_sd_dialog = unit->display_locations.sd_dialog;
      item.redirect_url = unit->redirect_url;
      items.push_back(std::move(item));
    }
  }
  return RespondNow(ArgumentList(Results::Create(items)));
}

ExtensionFunction::ResponseAction DirectMatchGetForCategoryFunction::Run() {
  using vivaldi::direct_match::GetForCategory::Params;
  namespace Results = vivaldi::direct_match::GetForCategory::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  const auto& units = service->GetDirectMatchesForCategory(params->category_id);
  std::vector<vivaldi::direct_match::Item> items;
  if (units.size() > 0) {
    for (const auto& unit : units) {
      vivaldi::direct_match::Item item;
      item.name = unit->name;
      item.title = unit->title;
      item.image_url = unit->image_url;
      item.image_path = unit->image_path;
      item.category = unit->category;
      item.display_location_address_bar = unit->display_locations.address_bar;
      item.display_location_sd_dialog = unit->display_locations.sd_dialog;
      item.redirect_url = unit->redirect_url;
      items.push_back(std::move(item));
    }
  }
  return RespondNow(ArgumentList(Results::Create(items)));
}

ExtensionFunction::ResponseAction DirectMatchResetHiddenFunction::Run() {
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());

  if (!service) {
    return RespondNow(Error("No Direct Match instance"));
  }

  service->ResetHiddenDirectMatch();
  return RespondNow(NoArguments());
}

}  // namespace extensions
