// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGERS_FACTORY_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGERS_FACTORY_H_

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/memory/singleton.h"

class SearchEnginesManager;
class SearchEnginesPromptManager;

class SearchEnginesManagersFactory {
 public:
  friend struct base::DefaultSingletonTraits<SearchEnginesManagersFactory>;

  static SearchEnginesManagersFactory* GetInstance();

  SearchEnginesManager* GetSearchEnginesManager() const;
  SearchEnginesPromptManager* GetSearchEnginesPromptManager() const;

  // SearchEnginesUpdater needs to know where to store the update.
  static std::optional<base::FilePath> GetSearchEnginesJsonUpdatePath();
  static std::optional<base::FilePath> GetSearchEnginesPromptJsonUpdatePath();

 private:
  SearchEnginesManagersFactory();
  ~SearchEnginesManagersFactory();

  SearchEnginesManagersFactory(const SearchEnginesManagersFactory&) = delete;
  SearchEnginesManagersFactory& operator=(const SearchEnginesManagersFactory&) =
      delete;

  static std::optional<base::FilePath> GetJsonPath(const std::string& filename);

  std::unique_ptr<SearchEnginesManager> search_engines_manager_;
  std::unique_ptr<SearchEnginesPromptManager> search_engines_prompt_manager_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGERS_FACTORY_H_
