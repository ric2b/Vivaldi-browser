#include "components/omnibox/browser/autocomplete_controller.h"

#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

bool AutocompleteController::VivaldiShouldRunProvider(
    AutocompleteProvider* provider) const {
#if BUILDFLAG(IS_ANDROID)  // Vivaldi ref. VAB-10952
  if (input_.IsZeroSuggest() &&
      provider->type() == AutocompleteProvider::TYPE_VERBATIM_MATCH)
    return true;
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
#else
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
#endif

  if (input_.InKeywordMode()) {
    AutocompleteInput keyword_input = input_;
    const TemplateURL* keyword_turl =
        AutocompleteInput::GetSubstitutingTemplateURLForInput(
            template_url_service_, &keyword_input);
    if (!keyword_turl)
      return false;
    switch (provider->type()) {
      case AutocompleteProvider::TYPE_SEARCH:
        return true;
      // @history keyword
      case AutocompleteProvider::TYPE_HISTORY_QUICK:
      case AutocompleteProvider::TYPE_HISTORY_URL:
      case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
        return (keyword_turl->starter_pack_id() ==
                template_url_starter_pack_data::kHistory);
      // @bookmark keyword
      case AutocompleteProvider::TYPE_BOOKMARK:
      case AutocompleteProvider::TYPE_BOOKMARK_NICKNAME:
        return (keyword_turl->starter_pack_id() ==
                template_url_starter_pack_data::kBookmarks);
      // @tabs keyword
      case AutocompleteProvider::TYPE_OPEN_TAB:
        return (keyword_turl->starter_pack_id() ==
                template_url_starter_pack_data::kTabs);
      default:
        return false;
    }
  }

  switch (provider->type()) {
    // Allowed Chrome providers ------------------------------------------------
    case AutocompleteProvider::TYPE_SEARCH:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      return true;
#else
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarInlineSearchEnabled);
#endif
    case AutocompleteProvider::TYPE_OPEN_TAB:
#if BUILDFLAG(IS_ANDROID)
      // VAB-11216
      return omnibox::IsAndroidHub(input_.current_page_classification());
#else
      return is_cros_launcher_;
#endif
    case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
#if !BUILDFLAG(IS_IOS)
      return provider_client_.get()->GetPrefs()->GetBoolean(
                 vivaldiprefs::kAddressBarOmniboxShowBrowserHistory) &&
             history_embeddings::GetFeatureParameters().omnibox_unscoped;
#else
      return false;
#endif
    case AutocompleteProvider::TYPE_HISTORY_QUICK:
    case AutocompleteProvider::TYPE_HISTORY_FUZZY:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowBrowserHistory);
    case AutocompleteProvider::TYPE_BOOKMARK:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxBookmarks);
    // Don't run shortcuts provider when history is disabled.
    case AutocompleteProvider::TYPE_SHORTCUTS:
      return provider_client_.get()->GetPrefs()->GetBoolean(
          vivaldiprefs::kAddressBarOmniboxShowBrowserHistory);
    case AutocompleteProvider::TYPE_FEATURED_SEARCH:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      return false;
#else
      return true;
#endif
    // Always keep TYPE_HISTORY_URL enable: it is suggesting url-what-you-typed
    // which is mandatory to have. (see VB-114310)
    case AutocompleteProvider::TYPE_HISTORY_URL:
    case AutocompleteProvider::TYPE_BUILTIN:
    case AutocompleteProvider::TYPE_UNSCOPED_EXTENSION:
    case AutocompleteProvider::TYPE_CALCULATOR:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case AutocompleteProvider::TYPE_MOST_VISITED_SITES:
    case AutocompleteProvider::TYPE_RECENTLY_CLOSED_TABS:
    case AutocompleteProvider::TYPE_CLIPBOARD:
#endif
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
                 vivaldiprefs::kAddressBarOmniboxShowTypedHistory) &&
             input_.IsZeroSuggest();
    default:
      return false;
  }
}
