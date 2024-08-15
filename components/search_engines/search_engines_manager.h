// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/values.h"

#include "components/search_engines/prepopulated_engines.h"

class SearchEnginesManager {
 public:
  friend struct base::DefaultSingletonTraits<SearchEnginesManager>;
  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;

  // Very simple pool to allocate PrepopulatedEngine structures.
  struct Pool {
    Pool();
    ~Pool();

    std::string* AllocString(const std::string&);
    std::u16string* AllocU16String(const std::u16string&);
    std::vector<const char*>* AllocVector();
    PrepopulatedEngine* AllocEngine();
    void Clear();

   private:
    std::vector<std::unique_ptr<std::string>> strings_;
    std::vector<std::unique_ptr<std::u16string>> u16strings_;
    std::vector<std::unique_ptr<std::vector<const char*>>> vectors_;
    std::vector<std::unique_ptr<PrepopulatedEngine>> engines_;
  };

  struct EnginesInfo {
    int private_engine_index = -1;
    int default_engine_index = -1;
  };

  static SearchEnginesManager* GetInstance();

  bool Load();

  std::vector<const PrepopulatedEngine*> GetEnginesByCountryId(
      int country_id,
      const std::string& lang,
      EnginesInfo* info) const;

  std::vector<const PrepopulatedEngine*> GetEnginesByEngineKey(
      const std::string& engine_key,
      EnginesInfo* info) const;

  bool IsInitialized() const;
  const std::vector<const PrepopulatedEngine*>& GetAllEngines() const;
  const PrepopulatedEngine* GetEngine(const std::string& name) const;

  // We enforce google and bing engines presence.
  // It's guaranteed that those functions never return nullptr.
  const PrepopulatedEngine* GetGoogleEngine() const;
  const PrepopulatedEngine* GetBingEngine() const;

  int GetCurrentDataVersion() const;
  std::string GetErrorMessage() const;
  bool HasError() const;
  void Reset();

  // SearchEnginesUpdater needs to know where to store the update.
  std::optional<base::FilePath> GetSearchEnginesJsonUpdatePath() const;

  int GetMaxPrepopulatedEngineID() const;

 private:
  SearchEnginesManager();
  ~SearchEnginesManager();

  std::optional<base::FilePath> GetSearchEnginesJsonPath(
      const std::string& filename) const;

  bool LoadFromFile(base::FilePath& path);
  bool LoadFromString(const std::string& json, bool check_sha = true);
  bool LoadDefaults();

  const char** ReadStringList(const base::Value::List* strings,
                              size_t& size,
                              bool& err);
  bool BuildEnginesInCountryMap(const base::Value::Dict& json);
  bool BuildEnginesMap(const base::Value::Dict& json);
  bool ParseEnginesFile(const std::string& jsonString);
  void SetError(const std::string& errorMessage);
  bool CheckConsistency();

  // "be_BY" -> [ "yandex_by", "startpage_com", ... ]
  std::unordered_map<std::string, std::vector<std::string>> engines_for_locale_;

  // "google" -> PrepopulatedEngine*
  std::unordered_map<std::string, const PrepopulatedEngine*> engines_map_;

  // CharsToCountryID("NO") -> [ ("nb", "nb_NO"), ("nn", "nn_NO") ]
  std::unordered_map<int, std::vector<std::pair<std::string, std::string>>>
      locales_for_country_;

  // "en"-> "en_US"
  std::unordered_map<std::string, std::string> default_locale_for_language_;

  // All the search engines in the vector engines.
  std::vector<const PrepopulatedEngine*> engines_;

  bool initialized_ = false;
  int max_prepopulated_engine_id_ = 0;
  int current_data_version_ = 0;

  Pool pool_;
  std::optional<std::string> err_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
