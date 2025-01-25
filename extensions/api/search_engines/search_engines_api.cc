// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/search_engines/search_engines_api.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/search_engines/search_engines_manager.h"
#include "components/search_engines/search_engines_managers_factory.h"
#include "components/search_engines/search_engines_prompt_manager.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/vivaldi_pref_names.h"
#include "extensions/schema/search_engines.h"
#include "extensions/tools/vivaldi_tools.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace extensions {

namespace {
const char kSearchTermsParameterFull[] = "{searchTerms}";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";
const char kTemplateServiceNotAvailable[] =
    "TemplateURLService not available for profile.";
// Display value for kSearchTermsParameter.
const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
const char kDisplayUnescapedSearchTerms[] = "%S";

std::string ToDisplay(const std::string& turl_param) {
  std::string result(turl_param);
  base::ReplaceSubstringsAfterOffset(&result, 0, kSearchTermsParameterFull,
                                     kDisplaySearchTerms);
  base::ReplaceSubstringsAfterOffset(&result, 0,
                                     kGoogleUnescapedSearchTermsParameterFull,
                                     kDisplayUnescapedSearchTerms);
  return result;
}

std::string FromDisplay(const std::string& display_string) {
  std::string result(display_string);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplaySearchTerms,
                                     kSearchTermsParameterFull);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplayUnescapedSearchTerms,
                                     kGoogleUnescapedSearchTermsParameterFull);
  return result;
}

vivaldi::search_engines::TemplateURL TemplateURLToJSType(
    const TemplateURL* template_url,
    bool force_read_only = false) {
  vivaldi::search_engines::TemplateURL result_turl;
  result_turl.read_only =
      force_read_only ||
      template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION;
  if (template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    result_turl.extension_id = template_url->GetExtensionId();
  }
  result_turl.guid = template_url->sync_guid();
  result_turl.name = base::UTF16ToUTF8(template_url->short_name());
  result_turl.keyword = base::UTF16ToUTF8(template_url->keyword());
  result_turl.favicon_url = template_url->favicon_url().spec();
  result_turl.url = ToDisplay(template_url->url());
  result_turl.post_params = ToDisplay(template_url->search_url_post_params());
  result_turl.suggest_url = ToDisplay(template_url->suggestions_url());
  result_turl.suggest_post_params =
      ToDisplay(template_url->suggestions_url_post_params());
  result_turl.image_url = ToDisplay(template_url->image_url());
  result_turl.image_post_params =
      ToDisplay(template_url->image_url_post_params());
  result_turl.prepopulate_id = template_url->prepopulate_id();

  return result_turl;
}

void AddTemplateURLToResult(
    const TemplateURL* template_url,
    bool force_read_only,
    std::vector<vivaldi::search_engines::TemplateURL>& result) {
  // We currently don't support these at all. Pretend they don't exist.
  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
    return;

  vivaldi::search_engines::TemplateURL result_turl =
      TemplateURLToJSType(template_url, force_read_only);

  result.push_back(std::move(result_turl));
}

vivaldi::search_engines::SearchRequest BuildSearchRequest(
    const TemplateURLRef& template_url_ref,
    const SearchTermsData& search_terms_data,
    std::string search_terms) {
  TemplateURLRef::SearchTermsArgs search_terms_args(
      base::UTF8ToUTF16(search_terms));
  TemplateURLRef::PostContent post_content;

  vivaldi::search_engines::SearchRequest result;

  result.url = template_url_ref.ReplaceSearchTerms(
      search_terms_args, search_terms_data, &post_content);
  result.content_type = post_content.first;
  result.post_params = post_content.second;

  return result;
}

bool IsCreatedByExtension(const TemplateURL* template_url) {
  return template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
}
}  // namespace

SearchEnginesAPI::SearchEnginesAPI(content::BrowserContext* context)
    : browser_context_(context),
      service_(TemplateURLServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_))) {
  if (service_) {
    service_->AddObserver(this);
  }
}

// static
BrowserContextKeyedAPIFactory<SearchEnginesAPI>*
SearchEnginesAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<SearchEnginesAPI>>
      instance;
  return instance.get();
}

SearchEnginesAPI::~SearchEnginesAPI() {}

// KeyedService implementation.
void SearchEnginesAPI::Shutdown() {
  if (service_) {
    service_->RemoveObserver(this);
  }
}

void SearchEnginesAPI::OnTemplateURLServiceChanged() {
  ::vivaldi::BroadcastEvent(
      vivaldi::search_engines::OnTemplateUrlsChanged::kEventName,
      vivaldi::search_engines::OnTemplateUrlsChanged::Create(),
      browser_context_);
}

void SearchEnginesAPI::OnTemplateURLServiceShuttingDown() {
  if (service_) {
    service_->RemoveObserver(this);
  }
  service_ = nullptr;
}

ExtensionFunction::ResponseAction SearchEnginesGetKeywordForUrlFunction::Run() {
  std::optional<vivaldi::search_engines::GetKeywordForUrl::Params> params(
      vivaldi::search_engines::GetKeywordForUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetKeywordForUrl::Results::Create(
          base::UTF16ToUTF8(TemplateURL::GenerateKeyword(GURL(params->url))))));
}

ExtensionFunction::ResponseAction SearchEnginesGetTemplateUrlsFunction::Run() {
  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }
  vivaldi::search_engines::AllTemplateURLs result;
  if (service->loaded()) {
    TemplateURLService::TemplateURLVector template_urls =
        service->GetTemplateURLs();

    // Stable sort preserves the order of search engines added by extensions,
    // which have no position.
    std::stable_sort(template_urls.begin(), template_urls.end(),
                     [](const auto template_url1, const auto template_url2) {
                       // Important to have the false-returning test first, so
                       // that if both are invalid, they'll be evaluated as
                       // equal.
                       if (!template_url1->data().vivaldi_position.IsValid())
                         return false;
                       if (!template_url2->data().vivaldi_position.IsValid())
                         return true;
                       return template_url1->data().vivaldi_position.LessThan(
                           template_url2->data().vivaldi_position);
                     });
    for (const TemplateURL* template_url : template_urls) {
      // We abuse is_active to hide "removed" prepopulated searches.
      if (template_url->is_active() != TemplateURLData::ActiveStatus::kFalse)
        AddTemplateURLToResult(template_url, false, result.template_urls);
    }
  }

  const TemplateURL* default_search;
  default_search =
      service->GetDefaultSearchProvider(TemplateURLService::kDefaultSearchMain);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_private = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchField);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search_field = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchFieldPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search_field_private = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchSpeeddials);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_speeddials = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchSpeeddialsPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_speeddials_private = default_search->sync_guid();
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchImage);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_image = default_search->sync_guid();
  }

  const auto checkSystemDefaultSearchEngine = [&]() {
    const auto index_pref_path = vivaldiprefs::kSystemSearchEngineDefaultIndex;
    const auto last_change_pref_path =
        vivaldiprefs::kSystemSearchEngineDefaultLastChange;

    if (!service->loaded()) {
      return false;
    }

    auto search_engines_manager =
        SearchEnginesManagersFactory::GetInstance()->GetSearchEnginesManager();

    auto* prefs = Profile::FromBrowserContext(browser_context())->GetPrefs();
    const auto app_locale = prefs->GetString(prefs::kLanguageAtInstall);
    // GetEnginesByCountryId expect only the first 2 chars of locale
    std::string lang(app_locale.begin(),
                     std::find(app_locale.begin(), app_locale.end(), '-'));

    const auto version = search_engines_manager->GetCurrentDataVersion();
    const auto engines = search_engines_manager->GetEnginesByCountryId(
        country_codes::GetCurrentCountryID(), lang);
    const auto default_search_engine_id =
        engines.list[engines.default_index]->id;

    const bool engine_has_changed = [&]() {
      const int index = prefs->GetInteger(index_pref_path);
      const int last_change = prefs->GetInteger(last_change_pref_path);

      // Search engine has changed if both prefs were valid and the last seen
      // version is lower than a new one and the index of the default search
      // engine changed.
      return last_change > 0 && index > 0 && last_change < version &&
             index != default_search_engine_id;
    }();

    prefs->SetInteger(index_pref_path, default_search_engine_id);
    prefs->SetInteger(last_change_pref_path, version);

    // If the new engine is the same as the curent default, notify about the
    // change.
    return engine_has_changed &&
           service->GetDefaultSearchProvider(
                      TemplateURLService::kDefaultSearchMain)
                   ->prepopulate_id() == default_search_engine_id;
  };
  result.system_default_search_changed = checkSystemDefaultSearchEngine();

  return RespondNow(ArgumentList(
      vivaldi::search_engines::GetTemplateUrls::Results::Create(result)));
}

ExtensionFunction::ResponseAction SearchEnginesAddTemplateUrlFunction::Run() {
  std::optional<vivaldi::search_engines::AddTemplateUrl::Params> params(
      vivaldi::search_engines::AddTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  if (params->template_url.keyword.empty() || params->template_url.url.empty())
    return RespondNow(ArgumentList(
        vivaldi::search_engines::AddTemplateUrl::Results::Create("")));

  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(params->template_url.name));
  data.SetKeyword(base::UTF8ToUTF16(params->template_url.keyword));
  data.SetURL(FromDisplay(params->template_url.url));
  data.suggestions_url = FromDisplay(params->template_url.suggest_url);
  data.image_url = FromDisplay(params->template_url.image_url);
  data.search_url_post_params = FromDisplay(params->template_url.post_params);
  data.suggestions_url_post_params =
      FromDisplay(params->template_url.suggest_post_params);
  data.image_url_post_params =
      FromDisplay(params->template_url.image_post_params);
  data.favicon_url = GURL(params->template_url.favicon_url);
  data.safe_for_autoreplace = false;

  TemplateURL* template_url = service->Add(std::make_unique<TemplateURL>(data));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::AddTemplateUrl::Results::Create(
          template_url ? template_url->sync_guid() : "")));
}

ExtensionFunction::ResponseAction
SearchEnginesRemoveTemplateUrlFunction::Run() {
  std::optional<vivaldi::search_engines::RemoveTemplateUrl::Params> params(
      vivaldi::search_engines::RemoveTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* turl_to_remove = service->GetTemplateURLForGUID(params->guid);

  if (!turl_to_remove)
    return RespondNow(ArgumentList(
        vivaldi::search_engines::RemoveTemplateUrl::Results::Create(false)));

  for (int i = 0; i < TemplateURLService::kDefaultSearchTypeCount; i++) {
    const TemplateURL* default_turl = service->GetDefaultSearchProvider(
        TemplateURLService::DefaultSearchType(i));
    if (default_turl && turl_to_remove->id() == default_turl->id())
      switch (i) {
        case TemplateURLService::kDefaultSearchMain:
        case TemplateURLService::kDefaultSearchPrivate:
        case TemplateURLService::kDefaultSearchImage:
          return RespondNow(ArgumentList(
              vivaldi::search_engines::RemoveTemplateUrl::Results::Create(
                  false)));
        case TemplateURLService::kDefaultSearchField:
        case TemplateURLService::kDefaultSearchSpeeddials:
          service->SetUserSelectedDefaultSearchProvider(
              const_cast<TemplateURL*>(service->GetDefaultSearchProvider(
                  TemplateURLService::kDefaultSearchMain)),
              TemplateURLService::DefaultSearchType(i));
          break;
        case TemplateURLService::kDefaultSearchFieldPrivate:
        case TemplateURLService::kDefaultSearchSpeeddialsPrivate:
          service->SetUserSelectedDefaultSearchProvider(
              const_cast<TemplateURL*>(service->GetDefaultSearchProvider(
                  TemplateURLService::kDefaultSearchPrivate)),
              TemplateURLService::DefaultSearchType(i));
          break;
      }
  }

  if (turl_to_remove->prepopulate_id() != 0) {
    // Instead of removing prepopulated turls and then needing to add support to
    // chromium for keeping track of which one was removed and preventing it to
    // be readded either by sync or when an update to prepopulate data comes up,
    // we mis-use the is_active flag (which we don't really use otherwise) to
    // signify that they shouldn't show up.
    service->SetIsActiveTemplateURL(turl_to_remove, false);
  } else {
    // This will CHECK if we are not allowed to remove the search engine.
    service->Remove(turl_to_remove);
  }

  return RespondNow(ArgumentList(
      vivaldi::search_engines::RemoveTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction
SearchEnginesUpdateTemplateUrlFunction::Run() {
  std::optional<vivaldi::search_engines::UpdateTemplateUrl::Params> params(
      vivaldi::search_engines::UpdateTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* turl_to_update =
      service->GetTemplateURLForGUID(params->template_url.guid);

  if (!turl_to_update || IsCreatedByExtension(turl_to_update) ||
      params->template_url.keyword.empty() || params->template_url.url.empty())
    return RespondNow(ArgumentList(
        vivaldi::search_engines::UpdateTemplateUrl::Results::Create(false)));

  service->ResetTemplateURL(
      turl_to_update, base::UTF8ToUTF16(params->template_url.name),
      base::UTF8ToUTF16(params->template_url.keyword),
      FromDisplay(params->template_url.url),
      FromDisplay(params->template_url.post_params),
      FromDisplay(params->template_url.suggest_url),
      FromDisplay(params->template_url.suggest_post_params),
      FromDisplay(params->template_url.image_url),
      FromDisplay(params->template_url.image_post_params),
      GURL(params->template_url.favicon_url));
  return RespondNow(ArgumentList(
      vivaldi::search_engines::UpdateTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesMoveTemplateUrlFunction::Run() {
  std::optional<vivaldi::search_engines::MoveTemplateUrl::Params> params(
      vivaldi::search_engines::MoveTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* turl_to_move = service->GetTemplateURLForGUID(params->guid);

  if (!turl_to_move || IsCreatedByExtension(turl_to_move))
    return RespondNow(ArgumentList(
        vivaldi::search_engines::MoveTemplateUrl::Results::Create(false)));

  TemplateURL* successor = nullptr;
  if (params->successor_guid) {
    successor = service->GetTemplateURLForGUID(*params->successor_guid);
    if (successor == turl_to_move) {
      return RespondNow(ArgumentList(
          vivaldi::search_engines::MoveTemplateUrl::Results::Create(false)));
    }
  }

  service->VivaldiMoveTemplateURL(turl_to_move, successor);

  return RespondNow(ArgumentList(
      vivaldi::search_engines::MoveTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesSetDefaultFunction::Run() {
  std::optional<vivaldi::search_engines::SetDefault::Params> params(
      vivaldi::search_engines::SetDefault::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* new_default = service->GetTemplateURLForGUID(params->guid);

  if (!new_default || IsCreatedByExtension(new_default))
    return RespondNow(ArgumentList(
        vivaldi::search_engines::SetDefault::Results::Create(false)));

  auto default_type = [](vivaldi::search_engines::DefaultType default_type)
      -> std::optional<TemplateURLService::DefaultSearchType> {
    switch (default_type) {
      case vivaldi::search_engines::DefaultType::kDefaultSearch:
        return TemplateURLService::kDefaultSearchMain;
      case vivaldi::search_engines::DefaultType::kDefaultPrivate:
        return TemplateURLService::kDefaultSearchPrivate;
      case vivaldi::search_engines::DefaultType::kDefaultSearchField:
        return TemplateURLService::kDefaultSearchField;
      case vivaldi::search_engines::DefaultType::kDefaultSearchFieldPrivate:
        return TemplateURLService::kDefaultSearchFieldPrivate;
      case vivaldi::search_engines::DefaultType::kDefaultSpeeddials:
        return TemplateURLService::kDefaultSearchSpeeddials;
      case vivaldi::search_engines::DefaultType::kDefaultSpeeddialsPrivate:
        return TemplateURLService::kDefaultSearchSpeeddialsPrivate;
      case vivaldi::search_engines::DefaultType::kDefaultImage:
        return TemplateURLService::kDefaultSearchImage;
      default:
        NOTREACHED();
    }
  }(params->default_type);

  service->SetUserSelectedDefaultSearchProvider(new_default,
                                                default_type.value());
  return RespondNow(
      ArgumentList(vivaldi::search_engines::SetDefault::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesGetSearchRequestFunction::Run() {
  std::optional<vivaldi::search_engines::GetSearchRequest::Params> params(
      vivaldi::search_engines::GetSearchRequest::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* template_url = service->GetTemplateURLForGUID(params->guid);

  if (!template_url)
    return RespondNow(Error("No search engine with this id"));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetSearchRequest::Results::Create(
          BuildSearchRequest(template_url->url_ref(),
                             service->search_terms_data(),
                             params->search_terms))));
}

ExtensionFunction::ResponseAction
SearchEnginesGetSuggestRequestFunction::Run() {
  std::optional<vivaldi::search_engines::GetSuggestRequest::Params> params(
      vivaldi::search_engines::GetSuggestRequest::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURL* template_url = service->GetTemplateURLForGUID(params->guid);

  if (!template_url)
    return RespondNow(Error("No search engine with this id"));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetSuggestRequest::Results::Create(
          BuildSearchRequest(template_url->suggestions_url_ref(),
                             service->search_terms_data(),
                             params->search_terms))));
}

ExtensionFunction::ResponseAction
SearchEnginesRepairPrepopulatedTemplateUrlsFunction::Run() {
  std::optional<vivaldi::search_engines::RepairPrepopulatedTemplateUrls::Params>
      params(vivaldi::search_engines::RepairPrepopulatedTemplateUrls::Params::
                 Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }
  service->RepairPrepopulatedSearchEngines();

  if (!params->only_keep_prepopulated)
    return RespondNow(NoArguments());

  // After repair, all defaults have been set back to prepopulated engines, so
  // it's safe to remove anything that's not prepopulated;
  TemplateURLService::TemplateURLVector template_urls =
      service->GetTemplateURLs();
  for (const auto template_url : template_urls) {
    if (template_url->prepopulate_id() == 0)
      service->Remove(template_url);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SearchEnginesGetSwitchPromptDataFunction::Run() {
  vivaldi::search_engines::SwitchPromptData data;

  auto* template_url_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          browser_context());
  auto* prefs = Profile::FromBrowserContext(browser_context())->GetPrefs();
  if (!prefs || !template_url_service || !template_url_service->loaded() ||
      !rules_service) {
    return RespondNow(Error("Services not available for profile."));
  }

  const TemplateURL* current_search;
  current_search = template_url_service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchMain);

  const auto maybe_default_search =
      SearchEnginesManagersFactory::GetInstance()
          ->GetSearchEnginesPromptManager()
          ->GetDefaultSearchEngineToPrompt(prefs, template_url_service,
                                           rules_service);

  data.should_prompt = maybe_default_search != nullptr;
  if (data.should_prompt) {
    data.current_search_engine = TemplateURLToJSType(current_search);
    data.partner_search_engine = TemplateURLToJSType(maybe_default_search);
  }
  return RespondNow(ArgumentList(
      vivaldi::search_engines::GetSwitchPromptData::Results::Create(data)));
}

ExtensionFunction::ResponseAction
SearchEnginesMarkSwitchPromptAsSeenFunction::Run() {
  auto* prefs = Profile::FromBrowserContext(browser_context())->GetPrefs();
  if (!prefs) {
    return RespondNow(Error("PrefService is not valid for profile."));
  }

  SearchEnginesManagersFactory::GetInstance()
      ->GetSearchEnginesPromptManager()
      ->MarkCurrentPromptAsSeen(prefs);

  return RespondNow(NoArguments());
}

}  // namespace extensions
