#include "components/omnibox/browser/autocomplete_controller.h"

#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/search_engines/template_url_service.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"


#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
bool AutocompleteController::VivaldiShouldRunProviderForDesktop(
    AutocompleteProvider* provider) const {
  if (input_.from_search_field) {
    switch (provider->type()) {
      case AutocompleteProvider::TYPE_SEARCH:
        return true;
      case AutocompleteProvider::TYPE_DIRECT_MATCH:
        return provider_client_.get()->GetPrefs()->GetBoolean(
            vivaldiprefs::kAddressBarSearchDirectMatchEnabled);
      case AutocompleteProvider::TYPE_RECENT_TYPED_HISTORY:
        return provider_client_.get()->GetPrefs()->GetBoolean(
            vivaldiprefs::kAddressBarOmniboxShowTypedHistory);
      default:
        return false;
    }
  }

  // When typing a search engine keyword, only show search and
  // search suggestions from this search engine.
  if (!input_.search_engine_guid.empty()) {
    switch (provider->type()) {
      case AutocompleteProvider::TYPE_SEARCH:
        return true;
      default:
        return false;
    }
  }

  switch (provider->type()) {
    // Allowed Chrome providers ------------------------------------------------
    case AutocompleteProvider::TYPE_SEARCH:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarInlineSearchEnabled);
    case AutocompleteProvider::TYPE_OPEN_TAB:
      return is_cros_launcher_;
    case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
      return provider_client_.get()->GetPrefs()->GetBoolean(
                 vivaldiprefs::kAddressBarOmniboxShowBrowserHistory) &&
             history_embeddings::GetFeatureParameters().omnibox_unscoped;
    case AutocompleteProvider::TYPE_HISTORY_QUICK:
    case AutocompleteProvider::TYPE_HISTORY_FUZZY:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowBrowserHistory);
    case AutocompleteProvider::TYPE_BOOKMARK:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxBookmarks);
    // Always keep TYPE_HISTORY_URL enable: it is suggesting url-what-you-typed
    // which is mandatory to have. (see VB-114310)
    case AutocompleteProvider::TYPE_HISTORY_URL:
    case AutocompleteProvider::TYPE_BUILTIN:
    case AutocompleteProvider::TYPE_SHORTCUTS:
    case AutocompleteProvider::TYPE_UNSCOPED_EXTENSION:
    case AutocompleteProvider::TYPE_CALCULATOR:
    case AutocompleteProvider::TYPE_KEYWORD:
      return true;
    // Vivaldi providers -------------------------------------------------------
    case AutocompleteProvider::TYPE_BOOKMARK_NICKNAME:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowNicknames);
    case AutocompleteProvider::TYPE_DIRECT_MATCH:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarSearchDirectMatchEnabled);
    case AutocompleteProvider::TYPE_RECENT_TYPED_HISTORY:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowTypedHistory);
    default:
      return false;
  }
}
#else
bool AutocompleteController::VivaldiShouldRunProviderForMobile(
    AutocompleteProvider* provider) const {
#if BUILDFLAG(IS_ANDROID)  // Vivaldi ref. VAB-10952
  if (input_.IsZeroSuggest() &&
      provider->type() == AutocompleteProvider::TYPE_VERBATIM_MATCH)
    return true;
#endif
  if (provider_client_.get()
          ->GetTemplateURLService()
          ->VivaldiIsDefaultOverridden()) {
    switch (provider->type()) {
      case AutocompleteProvider::TYPE_SEARCH:
        return true;
      default:
        return false;
    }
  }

  // When typing a search engine keyword, only show search and
  // search suggestions from this search engine.
  if (!input_.search_engine_guid.empty()) {
    switch (provider->type()) {
      case AutocompleteProvider::TYPE_SEARCH:
        return true;
      default:
        return false;
    }
  }

  switch (provider->type()) {
    // Allowed Chrome providers ------------------------------------------------
    case AutocompleteProvider::TYPE_OPEN_TAB:
#if BUILDFLAG(IS_ANDROID)
      // VAB-11216
      return omnibox::IsAndroidHub(input_.current_page_classification());
#else
      return is_cros_launcher_;
#endif
#if !BUILDFLAG(IS_IOS)
    case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
      return provider_client_.get()->GetPrefs()->GetBoolean(
                 vivaldiprefs::kAddressBarOmniboxShowBrowserHistory) &&
             history_embeddings::GetFeatureParameters().omnibox_unscoped;
#endif
    case AutocompleteProvider::TYPE_HISTORY_QUICK:
    case AutocompleteProvider::TYPE_HISTORY_FUZZY:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowBrowserHistory);
    case AutocompleteProvider::TYPE_BOOKMARK:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxBookmarks);
    // Always keep TYPE_HISTORY_URL enable: it is suggesting url-what-you-typed
    // which is mandatory to have. (see VB-114310)
    case AutocompleteProvider::TYPE_HISTORY_URL:
    case AutocompleteProvider::TYPE_SEARCH:
    case AutocompleteProvider::TYPE_BUILTIN:
    case AutocompleteProvider::TYPE_SHORTCUTS:
    case AutocompleteProvider::TYPE_UNSCOPED_EXTENSION:
    case AutocompleteProvider::TYPE_CALCULATOR:
    case AutocompleteProvider::TYPE_MOST_VISITED_SITES:
    case AutocompleteProvider::TYPE_RECENTLY_CLOSED_TABS:
    case AutocompleteProvider::TYPE_CLIPBOARD:
      return true;
    // Vivaldi providers -------------------------------------------------------
    case AutocompleteProvider::TYPE_BOOKMARK_NICKNAME:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowNicknames);
    case AutocompleteProvider::TYPE_DIRECT_MATCH:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarSearchDirectMatchEnabled);
    case AutocompleteProvider::TYPE_RECENT_TYPED_HISTORY:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowTypedHistory);
    default:
      return false;
  }
}
#endif  //! BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
