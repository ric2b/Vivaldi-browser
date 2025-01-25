// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <set>

#include "app/vivaldi_apptools.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chromium/base/strings/strcat.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_type.h"

#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_helper.h"
#include "components/search_engines/search_engines_manager.h"
#include "components/signature/vivaldi_signature.h"

#if BUILDFLAG(IS_IOS)
#include "ios/chrome/browser/shared/model/paths/paths.h"
#endif

namespace TemplateURLPrepopulateData {
PrepopulatedEngine google;
PrepopulatedEngine::PrepopulatedEngine() {}
}  // namespace TemplateURLPrepopulateData

namespace {

#include "components/search_engines/search_engines_default.inc"

static const char* kSearchEnginesJson = "search_engines.json";
static const char* kSearchEnginesJsonUpdated = "search_engines.json.update";
static const char* kGoogle = "google";
static const char* kBing = "bing";
static const char* kDefault = "default";

using namespace TemplateURLPrepopulateData;

struct EngineValueSetter {
  virtual ~EngineValueSetter() {}
  virtual void Set(const std::string&,
                   PrepopulatedEngine& engine,
                   SearchEnginesManager::Pool&) const = 0;
};

template <const char* PrepopulatedEngine::*item>
struct EngineValue : public EngineValueSetter {
  void Set(const std::string& s,
           PrepopulatedEngine& engine,
           SearchEnginesManager::Pool& pool) const override {
    engine.*item = pool.AllocString(s)->c_str();
  }
};

template <const char16_t* PrepopulatedEngine::*item>
struct EngineWcharValue : public EngineValueSetter {
  void Set(const std::string& s,
           PrepopulatedEngine& engine,
           SearchEnginesManager::Pool& pool) const override {
    engine.*item = pool.AllocU16String(base::UTF8ToUTF16(s))->c_str();
  }
};

std::string RemoveSpecialCharacters(const std::string& key,
                                    bool& is_private,
                                    bool& is_default) {
  is_private = is_default = false;
  size_t i = 0;
  for (; i < key.size(); ++i) {
    switch (key[i]) {
      case '!':
        is_private = true;
        continue;
      case '*':
        is_default = true;
        continue;
    }
    break;
  }
  return key.substr(i, key.size() - i);
}

template <typename Map1, typename Map2, typename ErrFn>
bool ValueIsKey(Map1& m1, Map2& m2, ErrFn err_fn) {
  for (auto it : m1) {
    if (m2.find(it.second) == m2.end()) {
      err_fn(base::StrCat({"missing key for ", it.second}));
      return false;
    }
  }
  return true;
}

bool StipCountryDefaultMark(const std::string& code,
                            std::string& result,
                            bool& is_default) {
  if (code.size() == 2) {
    is_default = false;
    result = code;
    return true;
  }

  if (code.size() != 3) {
    return false;
  }

  if (code[2] == '*') {
    result = code.substr(0, 2);
  } else if (code[0] == '*') {
    result = code.substr(1, 2);
  } else {
    return false;
  }

  is_default = true;
  return true;
}

using EngineSetters =
    std::unordered_map<std::string, std::unique_ptr<EngineValueSetter>>;

EngineSetters GetEngineSetters() {
  using Pop = PrepopulatedEngine;

  // Keep this not static to save some memory.
  std::unordered_map<std::string, std::unique_ptr<EngineValueSetter>> setters;
  setters["favicon_url"] = std::make_unique<EngineValue<&Pop::favicon_url>>();
  setters["search_url"] = std::make_unique<EngineValue<&Pop::search_url>>();
  setters["encoding"] = std::make_unique<EngineValue<&Pop::encoding>>();
  setters["suggest_url"] = std::make_unique<EngineValue<&Pop::suggest_url>>();
  setters["image_url"] = std::make_unique<EngineValue<&Pop::image_url>>();
  setters["image_translate_url"] =
      std::make_unique<EngineValue<&Pop::image_translate_url>>();
  setters["new_tab_url"] = std::make_unique<EngineValue<&Pop::new_tab_url>>();
  setters["contextual_search_url"] =
      std::make_unique<EngineValue<&Pop::contextual_search_url>>();
  setters["logo_url"] = std::make_unique<EngineValue<&Pop::logo_url>>();
  setters["doodle_url"] = std::make_unique<EngineValue<&Pop::doodle_url>>();
  setters["search_url_post_params"] =
      std::make_unique<EngineValue<&Pop::search_url_post_params>>();
  setters["suggest_url_post_params"] =
      std::make_unique<EngineValue<&Pop::suggest_url_post_params>>();
  setters["image_url_post_params"] =
      std::make_unique<EngineValue<&Pop::image_url_post_params>>();
  setters["side_search_param"] =
      std::make_unique<EngineValue<&Pop::side_search_param>>();
  setters["side_image_search_param"] =
      std::make_unique<EngineValue<&Pop::side_image_search_param>>();
  setters["image_translate_source_language_param_key"] = std::make_unique<
      EngineValue<&Pop::image_translate_source_language_param_key>>();
  setters["image_translate_target_language_param_key"] = std::make_unique<
      EngineValue<&Pop::image_translate_target_language_param_key>>();
  setters["preconnect_to_search_url"] =
      std::make_unique<EngineValue<&Pop::preconnect_to_search_url>>();
  setters["prefetch_likely_navigations"] =
      std::make_unique<EngineValue<&Pop::prefetch_likely_navigations>>();
  setters["name"] = std::make_unique<EngineWcharValue<&Pop::name>>();
  setters["keyword"] = std::make_unique<EngineWcharValue<&Pop::keyword>>();
  setters["image_search_branding_label"] =
      std::make_unique<EngineWcharValue<&Pop::image_search_branding_label>>();

  return setters;
}

}  // namespace

SearchEnginesManager::Pool::Pool() {}
SearchEnginesManager::Pool::~Pool() {}

std::string* SearchEnginesManager::Pool::AllocString(const std::string& s) {
  auto ptr = std::make_unique<std::string>();
  auto* rv = ptr.get();
  strings_.push_back(std::move(ptr));
  *rv = s;
  return rv;
}

std::u16string* SearchEnginesManager::Pool::AllocU16String(
    const std::u16string& s) {
  auto ptr = std::make_unique<std::u16string>();
  auto* rv = ptr.get();
  u16strings_.push_back(std::move(ptr));
  *rv = s;
  return rv;
}

std::vector<const char*>* SearchEnginesManager::Pool::AllocVector() {
  auto ptr = std::make_unique<std::vector<const char*>>();
  auto* rv = ptr.get();
  vectors_.push_back(std::move(ptr));
  return rv;
}

PrepopulatedEngine* SearchEnginesManager::Pool::AllocEngine() {
  auto ptr = std::make_unique<PrepopulatedEngine>();
  auto* rv = ptr.get();
  engines_.push_back(std::move(ptr));
  return rv;
}

void SearchEnginesManager::Pool::Clear() {
  strings_.clear();
  u16strings_.clear();
  vectors_.clear();
  engines_.clear();
}

SearchEnginesManager::SearchEnginesManager() {
  bool loaded = Load();
  DCHECK(loaded);
  if (loaded) {
    google = *GetGoogleEngine();
  }
}

SearchEnginesManager::~SearchEnginesManager() {}

const char** SearchEnginesManager::ReadStringList(
    const base::Value::List* strings,
    size_t& size,
    bool& err) {
  err = false;
  size = 0;

  if (!strings) {
    return nullptr;
  }

  if (strings->size() == 0) {
    return nullptr;
  }

  auto* new_vector = pool_.AllocVector();

  for (auto& val : *strings) {
    const std::string* s = val.GetIfString();
    if (!s) {
      err = true;
      return nullptr;
    }

    new_vector->push_back(pool_.AllocString(*s)->c_str());
    size++;
  }

  return new_vector->data();
}

// builds locales_for_country_, default_locale_for_language_
bool SearchEnginesManager::BuildEnginesInCountryMap(
    const base::Value::Dict& json) {
  const base::Value::List* engines_by_country =
      json.FindList("engines_by_country");
  if (!engines_by_country)
    return false;

  std::set<std::string> default_language_set;

  for (const auto& it : *engines_by_country) {
    const base::Value::List* country_ptr = it.GetIfList();
    if (!country_ptr) {
      SetError("engines_by_country is invalid");
      return false;
    }

    const base::Value::List& country = *country_ptr;

    if (country.size() != 2) {
      SetError("invalid entry in engines_by_country (array length)");
      return false;
    }

    const std::string* country_code = country[1].GetIfString();
    if (!country_code) {
      SetError("invalid entry in engines_by_country (country_code)");
      return false;
    }
    if (country_code->size() != 2)
      return false;

    const std::string* raw_language = country[0].GetIfString();
    if (!raw_language) {
      SetError("invalid entry in engines_by_country (language)");
      return false;
    }

    std::string lang;
    bool is_default = false;
    if (!StipCountryDefaultMark(*raw_language, lang, is_default)) {
      SetError("invalid entry in engines_by_country (invalid language)");
      return false;
    }

    const std::string locale = lang + "_" + *country_code;

    if (default_language_set.find(lang) == default_language_set.end()) {
      if (is_default) {
        default_locale_for_language_[lang] = locale;

        // Remember we have the default coutry for this language,
        default_language_set.insert(lang);
      } else {
        default_locale_for_language_[lang] = locale;
      }
    }

    int country_code_int = country_codes::CountryCharsToCountryID(
        (*country_code)[0], (*country_code)[1]);
    locales_for_country_[country_code_int].push_back(
        std::make_pair(lang, locale));
  }

  return true;
}

bool SearchEnginesManager::BuildEnginesMap(const base::Value::Dict& json) {
  const base::Value::Dict* engine_keys = json.FindDict("engines");
  if (!engine_keys) {
    SetError("'engines' key is missing");
    return false;
  }

  for (auto engines : *engine_keys) {
    std::vector<std::string> keys;
    const base::Value::List* engines_list = engines.second.GetIfList();
    if (!engines_list) {
      SetError("engines must be defined by a list of strings");
      return false;
    }
    for (auto& key : *engines_list) {
      if (!key.GetIfString()) {
        SetError("engines must be defined by a list of strings");
        return false;
      }
      keys.push_back(*key.GetIfString());
    }

    engines_for_locale_[engines.first] = keys;
  }

  return true;
}

bool SearchEnginesManager::ParseEnginesFile(const std::string& jsonString) {
  std::optional<base::Value> json = base::JSONReader::Read(
      jsonString, base::JSON_PARSE_RFC | base::JSON_ALLOW_COMMENTS);
  if (!json) {
    SetError("invalid json");
    return false;
  }

  const base::Value::Dict* elements = json->GetDict().FindDict("elements");
  const base::Value::Dict* int_variables =
      json->GetDict().FindDict("int_variables");

  if (!elements) {
    SetError("missing 'elements' key");
    return false;
  }

  if (!int_variables) {
    SetError("missing 'int_variables' key");
    return false;
  }

  {
    std::optional<int> i;
    i = int_variables->FindInt("kMaxPrepopulatedEngineID");
    if (!i) {
      SetError("missing 'kMaxPrepopulatedEngineID' key");
      return false;
    }
    max_prepopulated_engine_id_ = *i;

    i = int_variables->FindInt("kCurrentDataVersion");
    if (!i) {
      SetError("missing 'kCurrentDataVersion' key");
      return false;
    }
    current_data_version_ = *i;
  }

  EngineSetters setters = GetEngineSetters();

  for (auto element : *elements) {
    auto* dict = element.second.GetIfDict();
    if (!dict) {
      SetError("all the 'elements' items must be dicts");
      return false;
    }

    PrepopulatedEngine engine;

    for (auto item : *dict) {
      auto it = setters.find(item.first);
      if (it != setters.end()) {
        const std::string* value = item.second.GetIfString();
        if (value) {
          it->second->Set(*value, engine, pool_);
        }

        continue;
      }
    }

    // const SearchEngineType type;
    {
      const std::string* type = dict->FindString("type");
      if (!type) {
        LOG(INFO) << "unknown engine 'type', engine: " << element.first;
        engine.type = SEARCH_ENGINE_UNKNOWN;
      } else {
        engine.type = TemplateURLPrepopulateData::StringToSearchEngine(*type);
      }
    }

    // const int id;
    {
      std::optional<int> i;
      i = dict->FindInt("id");
      if (!i) {
        SetError(
            base::StrCat({"unknown engine 'id', engine: ", element.first}));
        return false;
      }
      engine.id = *i;
    }
    bool err = false;

    engine.search_intent_params =
        ReadStringList(dict->FindList("search_intent_params"),
                       engine.search_intent_params_size, err);
    if (err) {
      SetError("invalid engine value: 'search_intent_params'");
      return false;
    }

    engine.alternate_urls = ReadStringList(dict->FindList("alternate_urls"),
                                           engine.alternate_urls_size, err);
    if (err) {
      SetError("invalid engine value: 'alternate_urls'");
      return false;
    }

    if (!engine.name) {
      SetError("missing name for search engine.");
    }

    auto* engine_ptr = pool_.AllocEngine();
    *engine_ptr = engine;

    engines_.emplace_back(engine_ptr);
    engines_map_[element.first] = engine_ptr;
  }

  if (!BuildEnginesMap(json->GetDict())) {
    SetError("search_engines.json is not a dict");
    return false;
  }

  if (!BuildEnginesInCountryMap(json->GetDict()))
    return false;

  if (engines_for_locale_.find(kDefault) == engines_for_locale_.end()) {
    SetError("'default' engine is missing");
    return false;
  }

  if (!CheckConsistency())
    return false;

  return true;
}

bool SearchEnginesManager::LoadFromString(const std::string& json,
                                          bool check_sha) {
  DCHECK(!initialized_);
  if (check_sha && !vivaldi::VerifyJsonSignature(json)) {
    if (vivaldi::IsDebuggingSearchEngines()) {
      VLOG(1) << "Ignoring invalid signature due to debug mode.";
    } else {
      SetError("invalid signature");
      return false;
    }
  }

  if (ParseEnginesFile(json)) {
    initialized_ = true;
    return true;
  }

  LOG(ERROR) << "parsing search engines config failed: " << GetErrorMessage();
  return false;
}

bool SearchEnginesManager::LoadDefaults() {
  std::string json(kDefaultSearchEnginesJson);
  bool loaded = LoadFromString(json, false);
  if (loaded) {
    LOG(INFO) << "search engines loaded from hard-coded string";
    return true;
  }
  LOG(FATAL) << "loading search engines from hard-coded string failed";
}

std::optional<base::FilePath> SearchEnginesManager::GetSearchEnginesJsonPath(
    const std::string& filename) const {
  base::FilePath user_data_dir;
  // Determine the directory key based on the platform.
#if !BUILDFLAG(IS_IOS)
  int dir_key = chrome::DIR_USER_DATA;
#else
  int dir_key = ios::DIR_USER_DATA;
#endif

  // Use the determined directory key to get the user data directory.
  if (!base::PathService::Get(dir_key, &user_data_dir)) {
    LOG(INFO) << "unknown user data directory";
    return std::optional<base::FilePath>();
  }

  return user_data_dir.AppendASCII(filename);
}

std::optional<base::FilePath>
SearchEnginesManager::GetSearchEnginesJsonUpdatePath() const {
  return GetSearchEnginesJsonPath(kSearchEnginesJsonUpdated);
}

bool SearchEnginesManager::Load() {
  if (initialized_)
    return true;

  if (!vivaldi::IsVivaldiRunning()) {
    return LoadDefaults();
  }

  auto filename_update = GetSearchEnginesJsonUpdatePath();
  auto filename_regular = GetSearchEnginesJsonPath(kSearchEnginesJson);

  if (!filename_update || !filename_regular) {
    return LoadDefaults();
  }

  if (base::PathExists(*filename_update)) {
    // The updated engines file is there.
    if (LoadFromFile(*filename_update)) {
      // Make it a regular search_engines.json file.
      base::Move(*filename_update, *filename_regular);
      LOG(INFO) << "search_egines.json sucessfully updated.";
      return true;
    } else {
      LOG(INFO) << "attempt to use search_egines.json update failed from: "
                << *filename_update << " reason=" << GetErrorMessage();

      // Get rid of the broken json file.
      base::DeleteFile(*filename_update);
    }
    Reset();
  }

  if (LoadFromFile(*filename_regular)) {
    LOG(INFO) << "search_engines.json loaded from: " << *filename_regular;
    return true;
  } else {
    LOG(INFO) << "attempt to load search_egines.json failed; reason="
              << GetErrorMessage();
  }
  Reset();

  // Take the hard-coded defaults.
  return LoadDefaults();
}

bool SearchEnginesManager::LoadFromFile(base::FilePath& path) {
  DCHECK(!initialized_);

  std::string file_contents;
  bool read_success = ::base::ReadFileToString(path, &file_contents);
  if (!read_success) {
    SetError(base::StrCat({"can't read file: ", path.MaybeAsASCII()}));
    return false;
  }

  return LoadFromString(file_contents);
}

SearchEnginesManager* SearchEnginesManager::GetInstance() {
  return base::Singleton<SearchEnginesManager>::get();
}

void SearchEnginesManager::Reset() {
  engines_for_locale_.clear();
  engines_map_.clear();
  locales_for_country_.clear();
  default_locale_for_language_.clear();
  engines_.clear();
  initialized_ = false;
  max_prepopulated_engine_id_ = 0;
  current_data_version_ = 0;
  pool_.Clear();
  err_ = std::optional<std::string>();
}

std::vector<const PrepopulatedEngine*>
SearchEnginesManager::GetEnginesByEngineKey(const std::string& engines_key,
                                            EnginesInfo* info) const {
  std::vector<const PrepopulatedEngine*> res;
  auto it = engines_for_locale_.find(engines_key);
  CHECK(it != engines_for_locale_.end());

  for (const std::string& key : it->second) {
    bool is_private;
    bool is_default;

    auto iit =
        engines_map_.find(RemoveSpecialCharacters(key, is_private, is_default));
    CHECK(iit != engines_map_.end());
    res.push_back(iit->second);

    if (!info)
      continue;

    if (is_private)
      info->private_engine_index = int(res.size()) - 1;
    if (is_default)
      info->default_engine_index = int(res.size()) - 1;
  }

  return res;
}

std::vector<const PrepopulatedEngine*>
SearchEnginesManager::GetEnginesByCountryId(int country_id,
                                            const std::string& lang,
                                            EnginesInfo* info) const {
  auto it = locales_for_country_.find(country_id);

  if (it == locales_for_country_.end()) {
    // We were unable to find the country_id in locales_for_country_
    // but we still have the language. We can try to choose the country by the
    // language.
    auto it_locale_by_language = default_locale_for_language_.find(lang);
    if (it_locale_by_language != default_locale_for_language_.end()) {
      // We found the language, now we need to update the country code.
      std::string code_str = it_locale_by_language->second;
      // Double check, the code is 2 chararacters long.
      if (it_locale_by_language->second.size() == 2) {
        // Update the country code.
        country_id =
            country_codes::CountryCharsToCountryID(code_str[0], code_str[1]);
        // And try to find another locale.
        it = locales_for_country_.find(country_id);
      }
    }
  }

  if (it == locales_for_country_.end()) {
    // No option left, return the default set of the search engines.
    return GetEnginesByEngineKey(kDefault, info);
  }
  DCHECK(!it->second.empty());
  if (it->second.empty()) {
    // This is probably a bug.
    return GetEnginesByEngineKey(kDefault, info);
  }
  // This is Norwegian extravurst since only Norway has >1 languages
  // per country => ["nb", "NO", "nb_NO"] and ["nn", "NO", "nn_NO"]
  for (auto& language_country_pair : it->second) {
    if (language_country_pair.first == lang) {
      return GetEnginesByEngineKey(language_country_pair.second, info);
    }
  }
  // Take the first.
  return GetEnginesByEngineKey(it->second[0].second, info);
}

std::string SearchEnginesManager::GetErrorMessage() const {
  if (err_) {
    return *err_;
  }

  return std::string();
}

bool SearchEnginesManager::HasError() const {
  if (err_) {
    return true;
  }
  return false;
}

bool SearchEnginesManager::CheckConsistency() {
  if (!ValueIsKey(
          default_locale_for_language_, engines_for_locale_,
          [this](const std::string& errorMsg) { return SetError(errorMsg); })) {
    return false;
  }

  // Check every engine in "engines" has its definition in "elements",
  for (auto it : engines_for_locale_) {
    bool has_private = false;
    bool has_default = false;
    for (auto engine_keys : it.second) {
      bool is_private, is_default;
      if (engines_map_.find(RemoveSpecialCharacters(
              engine_keys, is_private, is_default)) == engines_map_.end()) {
        SetError(base::StrCat({"missing ", engine_keys, " engine definition"}));
        return false;
      }
      has_private |= is_private;
      has_default |= is_default;
    }
    if (!has_private) {
      SetError(base::StrCat({it.first, " has no private search negine set"}));
      return false;
    }
    if (!has_default) {
      SetError(base::StrCat({it.first, " has no default search negine set"}));
      return false;
    }
  }

  // Google is mandatory, is it?
  if (!GetEngine(kGoogle)) {
    SetError("google engine is missing");
    return false;
  }

  // We like Bing as well.
  if (!GetEngine(kBing)) {
    SetError("bing engine is missing");
    return false;
  }

  return true;
}

const PrepopulatedEngine* SearchEnginesManager::GetEngine(
    const std::string& name) const {
  auto it = engines_map_.find(name);
  if (it == engines_map_.end()) {
    return nullptr;
  }
  return it->second;
}

void SearchEnginesManager::SetError(const std::string& errorMessage) {
  err_ = errorMessage;
}

const std::vector<const PrepopulatedEngine*>&
SearchEnginesManager::GetAllEngines() const {
  CHECK(IsInitialized());
  return engines_;
}

const PrepopulatedEngine* SearchEnginesManager::GetGoogleEngine() const {
  const PrepopulatedEngine* rv = GetEngine(kGoogle);
  DCHECK(rv);
  return rv;
}

const PrepopulatedEngine* SearchEnginesManager::GetBingEngine() const {
  const PrepopulatedEngine* rv = GetEngine(kBing);
  DCHECK(rv);
  return rv;
}

int SearchEnginesManager::GetCurrentDataVersion() const {
  CHECK(IsInitialized());
  return current_data_version_;
}

bool SearchEnginesManager::IsInitialized() const {
  if (initialized_)
    return true;

  if (HasError()) {
    LOG(INFO) << "searchEnginesManager not initialized, " << GetErrorMessage();
  }

  return false;
}

int SearchEnginesManager::GetMaxPrepopulatedEngineID() const {
  return max_prepopulated_engine_id_;
}
