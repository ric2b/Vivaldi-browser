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
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/omnibox_input.h"
#include "components/omnibox/omnibox_service.h"
#include "components/omnibox/omnibox_service_factory.h"
#include "extensions/api/history/history_private_api.h"
#include "extensions/schema/omnibox_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

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
    : profile_(profile),
      omnibox_service_observer_(this),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
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
  if (type == "recent-typed-history") {
    return vivaldi::omnibox_private::OmniboxItemCategory::kRecentTypedHistory;
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
  if (name == "RecentTypedHistory") {
    return vivaldi::omnibox_private::OmniboxProviderName::kRecentTypedHistory;
  }
  return vivaldi::omnibox_private::OmniboxProviderName::kUnknown;
}

OmniboxItem CreateOmniboxItem(AutocompleteMatch match,  TemplateURLService* template_url_service) {
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
  res.type = AutocompleteMatchType::ToString(match.type);
  if (res.category ==
      vivaldi::omnibox_private::OmniboxItemCategory::kDirectMatch) {
    res.favicon_url = base::UTF16ToUTF8(match.local_favicon_path);
    res.favicon_type = "url";
  } else {
    if (!match.keyword.empty() && template_url_service) {
      TemplateURL* template_url = template_url_service->GetTemplateURLForKeyword(match.keyword);
      res.favicon_url = template_url ? template_url->favicon_url().spec().c_str() : res.destination_url;
    } else {
      res.favicon_url = res.destination_url;
    }
    res.favicon_type = "favicon";
  }

  return res;
}

void OmniboxEventRouter::OnResultChanged(AutocompleteController* controller,
                                         bool default_match_changed) {
  std::vector<OmniboxItem> urls;
  OnOmniboxResultChanged::Results results;

  results.input_text = base::UTF16ToUTF8(controller->input().text());
  results.done = controller->done();

  for (const auto& result : controller->result()) {
    // TODO: Decide if we should use SEARCH_OTHER_ENGINE or continue to use
    // our implementation in the frontend.
    if (result.type == AutocompleteMatchType::SEARCH_OTHER_ENGINE &&
        result.destination_url.spec().size() == 0)
      continue;
    results.combined_results.push_back(CreateOmniboxItem(result, template_url_service_));
  }

  base::Value::List args = OnOmniboxResultChanged::Create(results);
  DispatchEvent(profile_, OnOmniboxResultChanged::kEventName, std::move(args));
}

metrics::OmniboxEventProto::PageClassification
            OmniboxPrivateStartOmniboxFunction::GetPageClassification(
                vivaldi::omnibox_private::PageClassification name) {
  if (name == vivaldi::omnibox_private::PageClassification::kNtp) {
    return metrics::OmniboxEventProto::NTP;
  } else if (name == vivaldi::omnibox_private::PageClassification::kBlank) {
    return metrics::OmniboxEventProto::BLANK;
  }
  return metrics::OmniboxEventProto::OTHER;
}

ExtensionFunction::ResponseAction OmniboxPrivateStartOmniboxFunction::Run() {
  std::optional<vivaldi::omnibox_private::StartOmnibox::Params> params(
      vivaldi::omnibox_private::StartOmnibox::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = GetFunctionCallerProfile(*this);
  OmniboxService* service = OmniboxServiceFactory::GetForProfile(profile);
  DCHECK(service);

  vivaldi_omnibox::OmniboxPrivateInput input;
  input.clear_state_before_searching =
      params->parameters.clear_state_before_searching;
  input.prevent_inline_autocomplete =
      params->parameters.prevent_inline_autocomplete;
  input.from_search_field = params->parameters.from_search_field;
  input.search_engine_guid = params->parameters.search_engine_guid;
  input.focus_type = params->parameters.focus_type ==
    extensions::vivaldi::omnibox_private::OmniboxFocusType::kInteractionFocus ?
      metrics::OmniboxFocusType::INTERACTION_FOCUS :
      metrics::OmniboxFocusType::INTERACTION_DEFAULT;


  service->StartSearch(
      base::UTF8ToUTF16(params->parameters.query),
      input,
      GetPageClassification(params->parameters.page_classification));
  return RespondNow(NoArguments());
}

}  // namespace extensions
