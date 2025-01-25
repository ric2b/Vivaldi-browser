// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/omnibox/omnibox_private_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/omnibox_service.h"
#include "components/omnibox/omnibox_service_factory.h"
#include "extensions/api/history/history_private_api.h"
#include "extensions/schema/omnibox_private.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

using vivaldi_omnibox::OmniboxService;
using vivaldi_omnibox::OmniboxServiceFactory;

namespace {

namespace OnOmniboxResultChanged =
    vivaldi::omnibox_private::OnOmniboxResultChanged;

using vivaldi::omnibox_private::OmniboxItem;
}  // namespace

OmniboxPrivateAPI::OmniboxPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnOmniboxResultChanged::kEventName);
}

void OmniboxPrivateAPI::Shutdown() {
  omnibox_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<OmniboxPrivateAPI>>::
    DestructorAtExit g_factory_omnibox_private = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<OmniboxPrivateAPI>*
OmniboxPrivateAPI::GetFactoryInstance() {
  return g_factory_omnibox_private.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    OmniboxPrivateAPI>::DeclareFactoryDependencies() {
  DependsOn(OmniboxServiceFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

void OmniboxPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  omnibox_event_router_.reset(new OmniboxEventRouter(
      profile, OmniboxServiceFactory::GetForProfile(profile)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

OmniboxEventRouter::OmniboxEventRouter(Profile* profile,
                                       OmniboxService* omnibox_service)
    : profile_(profile), omnibox_service_observer_(this) {
  DCHECK(profile);
  omnibox_service_observer_.Observe(omnibox_service);
}

OmniboxEventRouter::~OmniboxEventRouter() {}

// Helper to actually dispatch an event to extension listeners.
void OmniboxEventRouter::DispatchEvent(Profile* profile,
                                       const std::string& event_name,
                                       base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter::Get(profile)->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

vivaldi::omnibox_private::OmniboxItemCategory GetProviderCategory(
    std::string type) {
  if (type == "history-url" || type == "history-title" ||
      type == "history-body" || type == "history-keyword" ||
      type == "history-cluster" || type == "history-embeddings" ||
      type == "history-embeddings-answer") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kHistory;
  }
  if (type == "search-what-you-typed" || type == "search-history" ||
      type == "search-other-engine") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kSearch;
  }
  if (type == "search-suggest" || type == "search-suggest-entity" ||
      type == "search-suggest-infinite" ||
      type == "search-suggest-personalized" ||
      type == "search-suggest-profile" || type == "query-tiles") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kSearchSuggestion;
  }
  if (type == "bookmark-title") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kBookmark;
  }
  if (type == "open-tab") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kOpenTab;
  }
  if (type == "url-from-clipboard" || type == "text-from-clipboard" ||
      type == "image-from-clipboard") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kClipboard;
  }
  if (type == "search-calculator-answer") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kCalculator;
  }
  if (type == "navsuggest" || type == "navsuggest-personalized" ||
      type == "navsuggest-tiles") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kUrlSuggestion;
  }
  if (type == "null-result-message") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kInternalMessage;
  }
  if (type == "most-visited-site-tile") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kTopSites;
  }
  // Vivaldi provider types
  if (type == "bookmark-nickname") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kNickname;
  }
  if (type == "direct-match") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kDirectMatch;
  }
  // "url-what-you-typed" is included in kOther.
  // It correspond to a fully typed url and shouldn't be in a category.
  return vivaldi::omnibox_private::OmniboxItemCategory::kOther;
}

// This function reflect AutocompleteProvider::TypeToString but convert it
// into an enum that can be used by js side.
vivaldi::omnibox_private::OmniboxProviderName providerNameToVivaldiProviderName(
    std::string name) {
  if (name == "Bookmark") {
    return vivaldi::omnibox_private::OmniboxProviderName::kBookmark;
  }
  if (name == "Builtin") {
    return vivaldi::omnibox_private::OmniboxProviderName::kBuiltin;
  }
  if (name == "Clipboard") {
    return vivaldi::omnibox_private::OmniboxProviderName::kClipboard;
  }
  if (name == "Document") {
    return vivaldi::omnibox_private::OmniboxProviderName::kDocument;
  }
  if (name == "HistoryQuick") {
    return vivaldi::omnibox_private::OmniboxProviderName::kHistoryQuick;
  }
  if (name == "HistoryURL") {
    return vivaldi::omnibox_private::OmniboxProviderName::kHistoryUrl;
  }
  if (name == "Keyword") {
    return vivaldi::omnibox_private::OmniboxProviderName::kKeyword;
  }
  if (name == "OnDeviceHead") {
    return vivaldi::omnibox_private::OmniboxProviderName::kOnDeviceHead;
  }
  if (name == "Search") {
    return vivaldi::omnibox_private::OmniboxProviderName::kSearch;
  }
  if (name == "Shortcuts") {
    return vivaldi::omnibox_private::OmniboxProviderName::kShortcuts;
  }
  if (name == "ZeroSuggest") {
    return vivaldi::omnibox_private::OmniboxProviderName::kZeroSuggest;
  }
  if (name == "LocalHistoryZeroSuggest") {
    return vivaldi::omnibox_private::OmniboxProviderName::
        kLocalHistoryZeroSuggest;
  }
  if (name == "QueryTile") {
    return vivaldi::omnibox_private::OmniboxProviderName::kQueryTile;
  }
  if (name == "MostVisitedSites") {
    return vivaldi::omnibox_private::OmniboxProviderName::kMostVisitedSites;
  }
  if (name == "VerbatimMatch") {
    return vivaldi::omnibox_private::OmniboxProviderName::kVerbatimMatch;
  }
  if (name == "VoiceSuggest") {
    return vivaldi::omnibox_private::OmniboxProviderName::kVoiceSuggest;
  }
  if (name == "HistoryFuzzy") {
    return vivaldi::omnibox_private::OmniboxProviderName::kHistoryFuzzy;
  }
  if (name == "OpenTab") {
    return vivaldi::omnibox_private::OmniboxProviderName::kOpenTab;
  }
  if (name == "HistoryCluster") {
    return vivaldi::omnibox_private::OmniboxProviderName::kHistoryCluster;
  }
  if (name == "Calculator") {
    return vivaldi::omnibox_private::OmniboxProviderName::kCalculator;
  }
  if (name == "FeaturedSearch") {
    return vivaldi::omnibox_private::OmniboxProviderName::kFeaturedSearch;
  }
  if (name == "HistoryEmbeddings") {
    return vivaldi::omnibox_private::OmniboxProviderName::kHistoryEmbeddings;
  }
  // Vivaldi providers
  if (name == "BookmarkNickname") {
    return vivaldi::omnibox_private::OmniboxProviderName::kBookmarkNickname;
  }
  if (name == "DirectMatch") {
    return vivaldi::omnibox_private::OmniboxProviderName::kDirectMatch;
  }
  return vivaldi::omnibox_private::OmniboxProviderName::kUnknown;
}

OmniboxItem CreateOmniboxItem(AutocompleteMatch match) {
  OmniboxItem res;

  res.allowed_to_be_default_match = match.allowed_to_be_default_match;
  res.contents = base::UTF16ToUTF8(match.contents);
  res.destination_url = match.destination_url.spec().c_str();
  res.fill_into_edit = base::UTF16ToUTF8(match.fill_into_edit);
  res.has_tab_match = match.has_tab_match.value_or(false);
  res.relevance = match.relevance;
  res.provider_name =
      providerNameToVivaldiProviderName(match.provider->GetName());
  res.transition = HistoryPrivateAPI::UiTransitionToPrivateHistoryTransition(
      match.transition);
  res.description = base::UTF16ToUTF8(match.description);
  res.inline_autocompletion = base::UTF16ToUTF8(match.inline_autocompletion);
  res.category =
      GetProviderCategory(AutocompleteMatchType::ToString(match.type));
  res.deletable = match.deletable;

  return res;
}

void OmniboxEventRouter::OnResultChanged(AutocompleteController* controller,
                                         bool default_match_changed) {
  std::vector<OmniboxItem> urls;
  OnOmniboxResultChanged::Results results;

  if (controller->done()) {
    results.cursor_position = controller->input().cursor_position();
    results.input_text = base::UTF16ToUTF8(controller->input().text());

    for (const auto& result : controller->result()) {
      results.combined_results.push_back(CreateOmniboxItem(result));
    }

    base::Value::List args = OnOmniboxResultChanged::Create(results);
    DispatchEvent(profile_, OnOmniboxResultChanged::kEventName,
                  std::move(args));
  }
}

ExtensionFunction::ResponseAction OmniboxPrivateStartOmniboxFunction::Run() {
  std::optional<vivaldi::omnibox_private::StartOmnibox::Params> params(
      vivaldi::omnibox_private::StartOmnibox::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = GetFunctionCallerProfile(*this);
  OmniboxService* service = OmniboxServiceFactory::GetForProfile(profile);
  DCHECK(service);
  service->StartSearch(base::UTF8ToUTF16(params->query));
  return RespondNow(NoArguments());
}

}  // namespace extensions
