// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "parsed_search_engines.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_helper.h"

namespace {
constexpr char kIntVariables[] = "int_variables";
constexpr char kMaxPrepopulatedEngineID[] = "kMaxPrepopulatedEngineID";
constexpr char kCurrentDataVersion[] = "kCurrentDataVersion";

constexpr char kElements[] = "elements";
constexpr char kName[] = "name";
constexpr char kKeyword[] = "keyword";
constexpr char kFaviconUrl[] = "favicon_url";
constexpr char kSearchUrl[] = "search_url";
constexpr char kEncoding[] = "encoding";
constexpr char kSuggestUrl[] = "suggest_url";
constexpr char kImageUrl[] = "image_url";
constexpr char kImageTranslateUrl[] = "image_translate_url";
constexpr char kNewTabUrl[] = "new_tab_url";
constexpr char kContextualSearchUrl[] = "contextual_search_url";
constexpr char kLogoUrl[] = "logo_url";
constexpr char kDoodleUrl[] = "doodle_url";
constexpr char kSearchUrlPostParams[] = "search_url_post_params";
constexpr char kSuggestUrlPostParams[] = "suggest_url_post_params";
constexpr char kImageUrlPostParams[] = "image_url_post_params";
constexpr char kSideSearchParam[] = "side_search_param";
constexpr char kSideImageSearchParam[] = "side_image_search_param";
constexpr char kImageTranslateSourceLanguageParamKey[] =
    "image_translate_source_language_param_key";
constexpr char kImageTranslateTargetLanguageParamKey[] =
    "image_translate_target_language_param_key";
constexpr char kImageSearchBrandingLabel[] = "image_search_branding_label";
constexpr char kSearchIntentParams[] = "search_intent_params";
constexpr char kAlternateUrls[] = "alternate_urls";
constexpr char kType[] = "type";
constexpr char kPreconnectToSearchUrl[] = "preconnect_to_search_url";
constexpr char kPrefetchLikelyNavigations[] = "prefetch_likely_navigations";
constexpr char kId[] = "id";
constexpr char kRegulatoryExtensions[] = "regulatory_extensions";

constexpr char kVariant[] = "variant";
constexpr char kSearchParams[] = "search_params";
constexpr char kSuggestParams[] = "suggest_params";

constexpr char kEngines[] = "engines";
constexpr char kDefault[] = "default";
constexpr char kUnittests[] = "unittests";
constexpr char kEnginesByCountry[] = "engines_by_country";

#if defined(OEM_POLESTAR_BUILD)
constexpr char kGoogle[] = "google";
#endif  // defined(OEM_LYNKCO_BUILD)

#if defined(OEM_LYNKCO_BUILD)
constexpr char kEcosia[] = "ecosia";
#endif  // defined(OEM_LYNKCO_BUILD)

struct LocaleMaps {
  ParsedSearchEngines::EnginesForLocale engines_for_locale;
  std::unordered_map<std::string, int> default_country_for_language;
};

std::unique_ptr<std::string> ToStringUniquePtr(std::string* s) {
  if (s) {
    return std::make_unique<std::string>(std::move(*s));
  }

  return nullptr;
}

std::unique_ptr<std::u16string> ToStringUniquePtr16(std::string* s) {
  if (s) {
    return std::make_unique<std::u16string>(base::UTF8ToUTF16(*s));
  }

  return nullptr;
}

class RegulatoryExtensionStorage {
 public:
  static std::optional<RegulatoryExtensionStorage> FromDict(
      base::Value::Dict& dict,
      std::string& error) {
    return Build(dict.FindString(kVariant), dict.FindString(kSearchParams),
                 dict.FindString(kSuggestParams), error);
  }

  ~RegulatoryExtensionStorage() = default;
  RegulatoryExtensionStorage(RegulatoryExtensionStorage&&) = default;
  RegulatoryExtensionStorage& operator=(RegulatoryExtensionStorage&&) = default;

  TemplateURLPrepopulateData::RegulatoryExtension MakeRegulatoryExtension()
      const {
    return TemplateURLPrepopulateData::RegulatoryExtension{
        .variant = variant_,
        .search_params = search_params_->c_str(),
        .suggest_params = suggest_params_->c_str()};
  }

 private:
  static std::optional<RegulatoryExtensionStorage> Build(
      std::string* variant,
      std::string* search_params,
      std::string* suggest_params,
      std::string& error) {
    if (!variant) {
      error =
          base::StrCat({"Regulatory extension property missing: ", kVariant});
      return std::nullopt;
    }

    auto regulatory_type =
        TemplateURLPrepopulateData::StringToRegulatoryExtensionType(*variant);

    if (!regulatory_type) {
      error = base::StrCat(
          {"Invalid value for regulatory extension property : ", kVariant});
      return std::nullopt;
    }

    if (!search_params) {
      error = base::StrCat(
          {"Regulatory extension property missing: ", kSearchParams});
      return std::nullopt;
    }
    if (!suggest_params) {
      error = base::StrCat(
          {"Regulatory extension property missing: ", kSuggestParams});
      return std::nullopt;
    }

    return RegulatoryExtensionStorage(*regulatory_type,
                                      ToStringUniquePtr(search_params),
                                      ToStringUniquePtr(suggest_params));
  }

  RegulatoryExtensionStorage(RegulatoryExtensionType variant,
                             std::unique_ptr<std::string> search_params,
                             std::unique_ptr<std::string> suggest_params)
      : variant_(variant),
        search_params_(std::move(search_params)),
        suggest_params_(std::move(suggest_params)) {}

  RegulatoryExtensionType variant_;
  std::unique_ptr<const std::string> search_params_;
  std::unique_ptr<const std::string> suggest_params_;
};

std::vector<TemplateURLPrepopulateData::RegulatoryExtension>
MakeRegulatoryExtensionVector(const std::vector<RegulatoryExtensionStorage>&
                                  regulatory_extension_storages) {
  std::vector<TemplateURLPrepopulateData::RegulatoryExtension> result;

  for (const RegulatoryExtensionStorage& storage :
       regulatory_extension_storages) {
    result.push_back(storage.MakeRegulatoryExtension());
  }

  return result;
}

std::vector<const char*> MakeStringPtrVector(
    const std::vector<std::unique_ptr<const std::string>>& strings) {
  std::vector<const char*> result;
  for (auto& s : strings) {
    result.emplace_back(s->c_str());
  }
  return result;
}

bool SplitCodeAndDefaultMark(const std::string& code_and_default_mark,
                             std::string& code,
                             bool& is_default) {
  code = code_and_default_mark;
  if (code.size() == 2) {
    is_default = false;
    return true;
  }

  if (code.size() != 3 || code.back() != '*') {
    return false;
  }

  code.pop_back();
  is_default = true;

  return true;
}

std::optional<ParsedSearchEngines::EnginesListWithDefaults>
GetEnginesListWithDefaultsForLocale(
    std::string locale,
    const ParsedSearchEngines::EnginesMap& engines_map,
    base::Value::Dict& locales_dict,
    std::string& error) {
  base::Value::List* engines_list = locales_dict.FindList(locale);
  if (!engines_list) {
    error =
        base::StrCat({"Locale ", locale, " not found in ", kEngines, " list"});
    return std::nullopt;
  }

  ParsedSearchEngines::EnginesListWithDefaults result;
  std::optional<size_t> default_index;
  std::optional<size_t> private_default_index;

  for (size_t i = 0; i < engines_list->size(); i++) {
    auto& engine_name = (*engines_list)[i];
    if (!engine_name.is_string()) {
      error = base::StrCat({"Expected string type for items in ", locale,
                            " list in dictionary ", kEngines});
      return std::nullopt;
    }

    std::string_view engine_name_view(engine_name.GetString());

    bool is_default = false;
    bool is_private_default = false;
    bool reading_special_markers = true;
    while(reading_special_markers) {
      if (engine_name_view.front() == '*') {
        engine_name_view.remove_prefix(1);
        is_default = true;
      } else if (engine_name_view.front() == '!') {
        engine_name_view.remove_prefix(1);
        is_private_default = true;
      } else {
        reading_special_markers = false;
      }
    }

    auto prepopulate_engine = engines_map.find(std::string(engine_name_view));
    if (prepopulate_engine == engines_map.end()) {
      error = base::StrCat({"Search engines ", engine_name_view, " for locale ",
                            locale, " not found."});
      return std::nullopt;
    }
    result.list.push_back(prepopulate_engine->second);

#if defined(OEM_POLESTAR_BUILD)
    is_default = engine_name_view == kGoogle;
#elif defined(OEM_LYNKCO_BUILD)
    is_default = engine_name_view == kEcosia;
#endif

    if (is_default) {
      if (default_index) {
        error = base::StrCat(
            {"Found multiple default search engines list for locale ", locale,
             "."});
      }
      default_index = i;
    }

    if (is_private_default) {
      if (private_default_index) {
        error = base::StrCat(
            {"Found multiple private default search engines list for locale ",
             locale, "."});
      }
      private_default_index = i;
    }

    if (prepopulate_engine->second->image_url != nullptr) {
      if (is_default || !result.default_image_search_index) {
        result.default_image_search_index = i;
      }
    }
  }

  if (!default_index) {
    error = base::StrCat(
        {"Default search engine mark in search engines list for locale ",
         locale, " not found."});
    return std::nullopt;
  }

  if (!private_default_index) {
    error = base::StrCat(
        {"Default search engine mark in search engines list for locale ",
         locale, " not found."});
    return std::nullopt;
  }

  result.default_index = *default_index;
  result.private_default_index = *private_default_index;

  return result;
}

std::optional<LocaleMaps> BuildLocaleMaps(
    const ParsedSearchEngines::EnginesMap& engines_map,
    base::Value::Dict& locales_dict,
    base::Value::List& country_list,
    std::string& error) {
  LocaleMaps results;
  std::set<std::string> explicit_default_language_set;

  for (auto& country_list_entry : country_list) {
    if (!country_list_entry.is_list()) {
      error = base::StrCat(
          {"Expected type list for entry in list ", kEnginesByCountry});
      return std::nullopt;
    }

    base::Value::List& country_and_language = country_list_entry.GetList();

    if (country_and_language.size() != 2) {
      error =
          base::StrCat({"Expected 2 items in ", kEnginesByCountry, " entry"});
      return std::nullopt;
    }

    const std::string* language_code_and_default_mark =
        country_and_language[0].GetIfString();
    if (!language_code_and_default_mark) {
      error = base::StrCat(
          {"Expected string for second item in ", kEnginesByCountry, " entry"});
      return std::nullopt;
    }

    std::string language_code;
    bool is_default = false;
    if (!SplitCodeAndDefaultMark(*language_code_and_default_mark, language_code,
                                 is_default)) {
      std::string(
          "Expected 2 letter language code, optionally followed by '*' for "
          "first item in ") +
          kEnginesByCountry + " entry";
      return std::nullopt;
    }

    const std::string* country_code = country_and_language[1].GetIfString();
    if (!country_code) {
      error = base::StrCat(
          {"Expected string for second item in ", kEnginesByCountry, " entry"});
      return std::nullopt;
    }

    if (country_code->size() != 2) {
      error =
          base::StrCat({"Expected 2 letter country code for second item in ",
                        kEnginesByCountry, " entry"});
      return std::nullopt;
    }

    int country_id = country_codes::CountryCharsToCountryID((*country_code)[0],
                                                            (*country_code)[1]);

    if (!explicit_default_language_set.contains(language_code)) {
      if (is_default) {
        results.default_country_for_language[language_code] = country_id;

        // Remember we have the default coutry for this language,
        explicit_default_language_set.insert(language_code);
      } else {
        results.default_country_for_language[language_code] = country_id;
      }
    }

    auto prepopulated_engines_for_locales = GetEnginesListWithDefaultsForLocale(
        base::StrCat({language_code, "_", *country_code}), engines_map,
        locales_dict, error);
    if (!prepopulated_engines_for_locales) {
      return std::nullopt;
    }
    results.engines_for_locale[country_id].push_back(std::make_pair(
        language_code, std::move(*prepopulated_engines_for_locales)));
  }

  return results;
}
}  // namespace

class ParsedSearchEngines::PrepopulatedEngineStorage {
 public:
  static std::optional<PrepopulatedEngineStorage> FromDict(
      base::Value::Dict& dict,
      std::string& error) {
    return Build(
        dict.FindString(kName), dict.FindString(kKeyword),
        dict.FindString(kFaviconUrl), dict.FindString(kSearchUrl),
        dict.FindString(kEncoding), dict.FindString(kSuggestUrl),
        dict.FindString(kImageUrl), dict.FindString(kImageTranslateUrl),
        dict.FindString(kNewTabUrl), dict.FindString(kContextualSearchUrl),
        dict.FindString(kLogoUrl), dict.FindString(kDoodleUrl),
        dict.FindString(kSearchUrlPostParams),
        dict.FindString(kSuggestUrlPostParams),
        dict.FindString(kImageUrlPostParams), dict.FindString(kSideSearchParam),
        dict.FindString(kSideImageSearchParam),
        dict.FindString(kImageTranslateSourceLanguageParamKey),
        dict.FindString(kImageTranslateTargetLanguageParamKey),
        dict.FindString(kImageSearchBrandingLabel),
        dict.FindList(kSearchIntentParams), dict.FindList(kAlternateUrls),
        dict.FindString(kType), dict.FindString(kPreconnectToSearchUrl),
        dict.FindString(kPrefetchLikelyNavigations), dict.FindInt(kId),
        dict.FindList(kRegulatoryExtensions), error);
  }

  ~PrepopulatedEngineStorage() = default;

  PrepopulatedEngineStorage(PrepopulatedEngineStorage&&) = default;
  PrepopulatedEngineStorage& operator=(PrepopulatedEngineStorage&&) = default;

  const TemplateURLPrepopulateData::PrepopulatedEngine MakePrepopulateEngine() {
    return TemplateURLPrepopulateData::PrepopulatedEngine{
        {.name = name_ ? name_->c_str() : nullptr,
         .keyword = keyword_ ? keyword_->c_str() : nullptr,
         .favicon_url = favicon_url_ ? favicon_url_->c_str() : nullptr,
         .search_url = search_url_ ? search_url_->c_str() : nullptr,
         .encoding = encoding_ ? encoding_->c_str() : nullptr,
         .suggest_url = suggest_url_ ? suggest_url_->c_str() : nullptr,
         .image_url = image_url_ ? image_url_->c_str() : nullptr,
         .image_translate_url =
             image_translate_url_ ? image_translate_url_->c_str() : nullptr,
         .new_tab_url = new_tab_url_ ? new_tab_url_->c_str() : nullptr,
         .contextual_search_url =
             contextual_search_url_ ? contextual_search_url_->c_str() : nullptr,
         .logo_url = logo_url_ ? logo_url_->c_str() : nullptr,
         .doodle_url = doodle_url_ ? doodle_url_->c_str() : nullptr,
         .search_url_post_params = search_url_post_params_
                                       ? search_url_post_params_->c_str()
                                       : nullptr,
         .suggest_url_post_params = suggest_url_post_params_
                                        ? suggest_url_post_params_->c_str()
                                        : nullptr,
         .image_url_post_params =
             image_url_post_params_ ? image_url_post_params_->c_str() : nullptr,
         .side_search_param =
             side_search_param_ ? side_search_param_->c_str() : nullptr,
         .side_image_search_param = side_image_search_param_
                                        ? side_image_search_param_->c_str()
                                        : nullptr,
         .image_translate_source_language_param_key =
             image_translate_source_language_param_key_
                 ? image_translate_source_language_param_key_->c_str()
                 : nullptr,
         .image_translate_target_language_param_key =
             image_translate_target_language_param_key_
                 ? image_translate_target_language_param_key_->c_str()
                 : nullptr,
         .image_search_branding_label =
             image_search_branding_label_
                 ? image_search_branding_label_->c_str()
                 : nullptr,
         .search_intent_params = search_intent_params_ptr_,
         .alternate_urls = alternate_urls_ptr_,
         .type = type_,
         .preconnect_to_search_url = preconnect_to_search_url_
                                         ? preconnect_to_search_url_->c_str()
                                         : nullptr,
         .prefetch_likely_navigations =
             prefetch_likely_navigations_
                 ? prefetch_likely_navigations_->c_str()
                 : nullptr,
         .id = id_,
         .regulatory_extensions = regulatory_extensions_}};
  }

 private:
  static std::optional<PrepopulatedEngineStorage> Build(
      std::string* name,
      std::string* keyword,
      std::string* favicon_url,
      std::string* search_url,
      std::string* encoding,
      std::string* suggest_url,
      std::string* image_url,
      std::string* image_translate_url,
      std::string* new_tab_url,
      std::string* contextual_search_url,
      std::string* logo_url,
      std::string* doodle_url,
      std::string* search_url_post_params,
      std::string* suggest_url_post_params,
      std::string* image_url_post_params,
      std::string* side_search_param,
      std::string* side_image_search_param,
      std::string* image_translate_source_language_param_key,
      std::string* image_translate_target_language_param_key,
      std::string* image_search_branding_label,
      base::Value::List* search_intent_params_list,
      base::Value::List* alternate_urls_list,
      std::string* type,
      std::string* preconnect_to_search_url,
      std::string* prefetch_likely_navigations,
      std::optional<int> id,
      base::Value::List* regulatory_extensions_list,
      std::string& error) {
    if (!name) {
      error = base::StrCat({"Search engine property missing: ", kName});
      return std::nullopt;
    }

    if (!id) {
      error = base::StrCat({"Search engine property missing: ", kId});
      return std::nullopt;
    }

    if (!type) {
      error = base::StrCat({"Search engine property missing: ", kType});
      return std::nullopt;
    }

    std::vector<std::unique_ptr<const std::string>> search_intent_params;
    if (search_intent_params_list) {
      for (auto& search_intent_param : *search_intent_params_list) {
        if (!search_intent_param.is_string()) {
          error = base::StrCat(
              {"Expected type string for ", kSearchIntentParams, " item"});
          return std::nullopt;
        }
        search_intent_params.push_back(std::make_unique<std::string>(
            std::move(search_intent_param.GetString())));
      }
    }

    std::vector<std::unique_ptr<const std::string>> alternate_urls;
    if (alternate_urls_list) {
      for (auto& alternate_url : *alternate_urls_list) {
        if (!alternate_url.is_string()) {
          error = base::StrCat(
              {"Expected type string for ", kAlternateUrls, " item"});
          return std::nullopt;
        }
        alternate_urls.push_back(std::make_unique<std::string>(
            std::move(alternate_url.GetString())));
      }
    }

    std::vector<RegulatoryExtensionStorage> regulatory_extensions_storage;
    if (regulatory_extensions_list) {
      for (auto& regulatory_extension : *regulatory_extensions_list) {
        if (!regulatory_extension.is_dict()) {
          error = base::StrCat(
              {"Expected type dict for ", kRegulatoryExtensions, " item"});
          return std::nullopt;
        }
        std::optional<RegulatoryExtensionStorage> storage =
            RegulatoryExtensionStorage::FromDict(regulatory_extension.GetDict(),
                                                 error);
        if (!storage) {
          return std::nullopt;
        }
        regulatory_extensions_storage.push_back(std::move(*storage));
      }
    }

    return PrepopulatedEngineStorage(
        ToStringUniquePtr16(name), ToStringUniquePtr16(keyword),
        ToStringUniquePtr(favicon_url), ToStringUniquePtr(search_url),
        ToStringUniquePtr(encoding), ToStringUniquePtr(suggest_url),
        ToStringUniquePtr(image_url), ToStringUniquePtr(image_translate_url),
        ToStringUniquePtr(new_tab_url),
        ToStringUniquePtr(contextual_search_url), ToStringUniquePtr(logo_url),
        ToStringUniquePtr(doodle_url),
        ToStringUniquePtr(search_url_post_params),
        ToStringUniquePtr(suggest_url_post_params),
        ToStringUniquePtr(image_url_post_params),
        ToStringUniquePtr(side_search_param),
        ToStringUniquePtr(side_image_search_param),
        ToStringUniquePtr(image_translate_source_language_param_key),
        ToStringUniquePtr(image_translate_target_language_param_key),
        ToStringUniquePtr16(image_search_branding_label),
        std::move(search_intent_params), std::move(alternate_urls),
        TemplateURLPrepopulateData::StringToSearchEngine(*type),
        ToStringUniquePtr(preconnect_to_search_url),
        ToStringUniquePtr(prefetch_likely_navigations), *id,
        std::move(regulatory_extensions_storage));
  }

  PrepopulatedEngineStorage(
      std::unique_ptr<std::u16string> name,
      std::unique_ptr<std::u16string> keyword,
      std::unique_ptr<std::string> favicon_url,
      std::unique_ptr<std::string> search_url,
      std::unique_ptr<std::string> encoding,
      std::unique_ptr<std::string> suggest_url,
      std::unique_ptr<std::string> image_url,
      std::unique_ptr<std::string> image_translate_url,
      std::unique_ptr<std::string> new_tab_url,
      std::unique_ptr<std::string> contextual_search_url,
      std::unique_ptr<std::string> logo_url,
      std::unique_ptr<std::string> doodle_url,
      std::unique_ptr<std::string> search_url_post_params,
      std::unique_ptr<std::string> suggest_url_post_params,
      std::unique_ptr<std::string> image_url_post_params,
      std::unique_ptr<std::string> side_search_param,
      std::unique_ptr<std::string> side_image_search_param,
      std::unique_ptr<std::string> image_translate_source_language_param_key,
      std::unique_ptr<std::string> image_translate_target_language_param_key,
      std::unique_ptr<std::u16string> image_search_branding_label,
      std::vector<std::unique_ptr<const std::string>> search_intent_params,
      std::vector<std::unique_ptr<const std::string>> alternate_urls,
      SearchEngineType type,
      std::unique_ptr<std::string> preconnect_to_search_url,
      std::unique_ptr<std::string> prefetch_likely_navigations,
      int id,
      std::vector<RegulatoryExtensionStorage> regulatory_extensions_storage)
      : name_(std::move(name)),
        keyword_(std::move(keyword)),
        favicon_url_(std::move(favicon_url)),
        search_url_(std::move(search_url)),
        encoding_(std::move(encoding)),
        suggest_url_(std::move(suggest_url)),
        image_url_(std::move(image_url)),
        image_translate_url_(std::move(image_translate_url)),
        new_tab_url_(std::move(new_tab_url)),
        contextual_search_url_(std::move(contextual_search_url)),
        logo_url_(std::move(logo_url)),
        doodle_url_(std::move(doodle_url)),
        search_url_post_params_(std::move(search_url_post_params)),
        suggest_url_post_params_(std::move(suggest_url_post_params)),
        image_url_post_params_(std::move(image_url_post_params)),
        side_search_param_(std::move(side_search_param)),
        side_image_search_param_(std::move(side_search_param)),
        image_translate_source_language_param_key_(
            std::move(image_translate_source_language_param_key)),
        image_translate_target_language_param_key_(
            std::move(image_translate_target_language_param_key)),
        image_search_branding_label_(std::move(image_search_branding_label)),
        search_intent_params_(std::move(search_intent_params)),
        alternate_urls_(std::move(alternate_urls)),
        type_(std::move(type)),
        preconnect_to_search_url_(std::move(preconnect_to_search_url)),
        prefetch_likely_navigations_(std::move(prefetch_likely_navigations)),
        id_(std::move(id)),
        regulatory_extension_storage_(std::move(regulatory_extensions_storage)),
        search_intent_params_ptr_(MakeStringPtrVector(search_intent_params_)),
        alternate_urls_ptr_(MakeStringPtrVector(alternate_urls_)),
        regulatory_extensions_(
            MakeRegulatoryExtensionVector(regulatory_extension_storage_)) {}

  std::unique_ptr<const std::u16string> name_;
  std::unique_ptr<const std::u16string> keyword_;
  std::unique_ptr<const std::string> favicon_url_;
  std::unique_ptr<const std::string> search_url_;
  std::unique_ptr<const std::string> encoding_;
  std::unique_ptr<const std::string> suggest_url_;
  std::unique_ptr<const std::string> image_url_;
  std::unique_ptr<const std::string> image_translate_url_;
  std::unique_ptr<const std::string> new_tab_url_;
  std::unique_ptr<const std::string> contextual_search_url_;
  std::unique_ptr<const std::string> logo_url_;
  std::unique_ptr<const std::string> doodle_url_;
  std::unique_ptr<const std::string> search_url_post_params_;
  std::unique_ptr<const std::string> suggest_url_post_params_;
  std::unique_ptr<const std::string> image_url_post_params_;
  std::unique_ptr<const std::string> side_search_param_;
  std::unique_ptr<const std::string> side_image_search_param_;
  std::unique_ptr<const std::string> image_translate_source_language_param_key_;
  std::unique_ptr<const std::string> image_translate_target_language_param_key_;
  std::unique_ptr<const std::u16string> image_search_branding_label_;
  std::vector<std::unique_ptr<const std::string>> search_intent_params_;
  std::vector<std::unique_ptr<const std::string>> alternate_urls_;
  SearchEngineType type_;
  std::unique_ptr<const std::string> preconnect_to_search_url_;
  std::unique_ptr<const std::string> prefetch_likely_navigations_;
  int id_;
  std::vector<RegulatoryExtensionStorage> regulatory_extension_storage_;
  std::vector<const char*> search_intent_params_ptr_;
  std::vector<const char*> alternate_urls_ptr_;
  std::vector<TemplateURLPrepopulateData::RegulatoryExtension>
      regulatory_extensions_;
};

/* static */
std::unique_ptr<ParsedSearchEngines> ParsedSearchEngines::FromJsonString(
    std::string_view json_string,
    std::string& error) {
  error = "";
  std::unique_ptr<base::Value> json =
      JSONStringValueDeserializer(json_string).Deserialize(nullptr, nullptr);

  if (!json) {
    error = "Invalid JSON";
    return nullptr;
  }

  if (!json->is_dict()) {
    error = "Not a JSON Dict";
    return nullptr;
  }

  base::Value::Dict& root = json->GetDict();

  base::Value::Dict* elements = root.FindDict(kElements);

  if (!elements) {
    error = base::StrCat({"Missing key: ", kElements});
    return nullptr;
  }

  base::Value::Dict* int_variables = root.FindDict(kIntVariables);

  if (!int_variables) {
    error = base::StrCat({"Missing key: ", kIntVariables});
    return nullptr;
  }

  std::optional<int> max_prepopulated_engine_id(
      int_variables->FindInt(kMaxPrepopulatedEngineID));
  if (!max_prepopulated_engine_id) {
    error = base::StrCat({"Missing key: ", kMaxPrepopulatedEngineID});
    return nullptr;
  }

  std::optional<int> current_data_version =
      int_variables->FindInt(kCurrentDataVersion);
  if (!current_data_version) {
    error = base::StrCat({"Missing key: ", kCurrentDataVersion});
    return nullptr;
  }

  std::vector<PrepopulatedEngineStorage> prepopulated_engines_storage;
  std::vector<std::unique_ptr<TemplateURLPrepopulateData::PrepopulatedEngine>>
      all_engines;
  EnginesMap engines_map;

  for (auto [entry_name, element] : *elements) {
    if (!element.is_dict()) {
      error = "Search engine elements should be JSON Dict";
      return nullptr;
    }

    std::optional<PrepopulatedEngineStorage> storage =
        PrepopulatedEngineStorage::FromDict(element.GetDict(), error);

    if (!storage) {
      error.append(" for search engine ");
      error.append(entry_name);
      return nullptr;
    }

    prepopulated_engines_storage.push_back(std::move(*storage));
    all_engines.push_back(
        std::make_unique<TemplateURLPrepopulateData::PrepopulatedEngine>(
            prepopulated_engines_storage.back().MakePrepopulateEngine()));
    engines_map[entry_name] = all_engines.back().get();
  }

  base::Value::List* country_list = root.FindList(kEnginesByCountry);
  if (!country_list) {
    error = base::StrCat({"Missing key: ", kEnginesByCountry});
    return nullptr;
  }

  base::Value::Dict* locales_dict = root.FindDict(kEngines);
  if (!locales_dict) {
    error = base::StrCat({"Missing key: ", kEngines});
    return nullptr;
  }

  std::optional<LocaleMaps> locale_maps;
  std::optional<EnginesListWithDefaults> default_engines_list;

  if (vivaldi::IsVivaldiRunning()) {
    locale_maps =
        BuildLocaleMaps(engines_map, *locales_dict, *country_list, error);
    if (!locale_maps) {
      return nullptr;
    }

    default_engines_list = GetEnginesListWithDefaultsForLocale(
        kDefault, engines_map, *locales_dict, error);
    if (!default_engines_list) {
      return nullptr;
    }
  } else {
    locale_maps = LocaleMaps();

    default_engines_list = GetEnginesListWithDefaultsForLocale(
        kUnittests, engines_map, *locales_dict, error);
    if (!default_engines_list) {
      return nullptr;
    }
  }

  // Cannot use make_unique because of private destructor
  return std::unique_ptr<ParsedSearchEngines>(new ParsedSearchEngines(
      std::move(prepopulated_engines_storage), std::move(all_engines),
      std::move(*default_engines_list), std::move(engines_map),
      std::move(locale_maps->engines_for_locale),
      std::move(locale_maps->default_country_for_language),
      *max_prepopulated_engine_id, *current_data_version));
}

ParsedSearchEngines::ParsedSearchEngines(
    std::vector<PrepopulatedEngineStorage> storage,
    std::vector<std::unique_ptr<TemplateURLPrepopulateData::PrepopulatedEngine>>
        all_engines,
    EnginesListWithDefaults default_engines_list,
    EnginesMap engines_map,
    EnginesForLocale engines_for_locale,
    std::unordered_map<std::string, int> default_country_for_language,
    int max_prepopulated_engine_id,
    int current_data_version)
    : storage_(std::move(storage)),
      all_engines_(std::move(all_engines)),
      all_engines_ptr_([this]() {
        PrepopulateEnginesList all_engines_ptr;
        for (const auto& engine : all_engines_) {
          all_engines_ptr.push_back(engine.get());
        }
        return all_engines_ptr;
      }()),
      default_engines_list_(std::move(default_engines_list)),
      engines_map_(std::move(engines_map)),
      engines_for_locale_(std::move(engines_for_locale)),
      default_country_for_language_(std::move(default_country_for_language)),
      max_prepopulated_engine_id_(max_prepopulated_engine_id),
      current_data_version_(current_data_version) {}

ParsedSearchEngines::~ParsedSearchEngines() = default;