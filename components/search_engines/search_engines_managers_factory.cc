// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/search_engines/search_engines_managers_factory.h"
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

#if BUILDFLAG(IS_IOS)
#include "ios/chrome/browser/shared/model/paths/paths.h"
#endif

#include "app/vivaldi_apptools.h"
#include "components/search_engines/parsed_search_engines.h"
#include "components/search_engines/parsed_search_engines_prompt.h"
#include "components/search_engines/search_engines_manager.h"
#include "components/search_engines/search_engines_prompt_manager.h"
#include "components/signature/vivaldi_signature.h"

namespace {

#include "components/search_engines/search_engines_default.inc"

constexpr char kSearchEnginesJson[] = "search_engines.json";
constexpr char kSearchEnginesJsonUpdated[] = "search_engines.json.update";

#include "components/search_engines/search_engines_prompt_default.inc"

constexpr char kSearchEnginesPromptJson[] = "search_engines_prompt.json";
constexpr char kSearchEnginesPromptJsonUpdated[] =
    "search_engines_prompt.json.update";

template <typename T>
std::unique_ptr<T> LoadFromString(const std::string& json,
                                  bool check_sha = true) {
  if (check_sha && !vivaldi::VerifyJsonSignature(json)) {
    if (vivaldi::IsDebuggingSearchEngines()) {
      VLOG(1) << "Ignoring invalid signature due to debug mode.";
    } else {
      LOG(ERROR) << "Parsing config failed: "
                 << "invalid signature";
      return nullptr;
    }
  }

  std::string error;
  std::unique_ptr<T> prompt = T::FromJsonString(json, error);

  if (!prompt) {
    LOG(ERROR) << "Parsing config failed: " << error;
    return nullptr;
  }

  return prompt;
}

template <typename T>
std::unique_ptr<T> LoadDefaults(const std::string& defaults_string) {
  std::unique_ptr<T> t = LoadFromString<T>(defaults_string, false);
  if (!t) {
    LOG(FATAL) << "loading search engines file from hard-coded string failed";
  }
  LOG(INFO) << "search engines file loaded from hard-coded string";
  return t;
}

template <typename T>
std::unique_ptr<T> LoadFromFile(const base::FilePath& path) {
  std::string file_contents;
  bool read_success = ::base::ReadFileToString(path, &file_contents);
  if (!read_success) {
    LOG(ERROR) << "File can't be read: " << path.MaybeAsASCII();
    return nullptr;
  }

  return LoadFromString<T>(file_contents);
}

template <typename T>
std::unique_ptr<T> UpdateJsonFileAndParse(
    std::optional<base::FilePath> update_file,
    std::optional<base::FilePath> regular_file,
    const std::string& defaults_string) {
  if (!update_file || !regular_file || !vivaldi::IsVivaldiRunning()) {
    return LoadDefaults<T>(defaults_string);
  }

  if (base::PathExists(*update_file)) {
    // The updated prompt file is there.
    std::unique_ptr<T> t = LoadFromFile<T>(*update_file);
    if (t) {
      // Make it a regular file.
      base::Move(*update_file, *regular_file);
      LOG(INFO) << regular_file->BaseName() << " sucessfully updated.";
      return t;
    } else {
      LOG(INFO) << "Update failed from: " << *update_file
                << "Attempting to use " << regular_file->BaseName()
                << " instead.";

      // Get rid of the broken json file.
      base::DeleteFile(*update_file);
    }
  }

  std::unique_ptr<T> t = LoadFromFile<T>(*regular_file);
  if (!t) {
    LOG(INFO) << "Attempting to load " << regular_file->BaseName()
              << " failed.";
    return LoadDefaults<T>(defaults_string);
  }
  LOG(INFO) << regular_file->BaseName() << " loaded from: " << *regular_file;
  return t;
}

}  // namespace

SearchEnginesManagersFactory::SearchEnginesManagersFactory() {
  std::unique_ptr<ParsedSearchEngines> parsed_search_engines =
      UpdateJsonFileAndParse<ParsedSearchEngines>(
          GetSearchEnginesJsonUpdatePath(), GetJsonPath(kSearchEnginesJson),
          std::string(kDefaultSearchEnginesJson));
  CHECK(parsed_search_engines);
  search_engines_manager_ =
      std::make_unique<SearchEnginesManager>(std::move(parsed_search_engines));

  std::unique_ptr<ParsedSearchEnginesPrompt> parsed_search_engines_prompt =
      UpdateJsonFileAndParse<ParsedSearchEnginesPrompt>(
          GetSearchEnginesPromptJsonUpdatePath(),
          GetJsonPath(kSearchEnginesPromptJson),
          std::string(kDefaultSearchEnginesPromptJson));
  CHECK(parsed_search_engines_prompt);
  search_engines_prompt_manager_ = std::make_unique<SearchEnginesPromptManager>(
      std::move(parsed_search_engines_prompt));
}

SearchEnginesManagersFactory::~SearchEnginesManagersFactory() = default;

SearchEnginesManagersFactory* SearchEnginesManagersFactory::GetInstance() {
  return base::Singleton<SearchEnginesManagersFactory>::get();
}

std::optional<base::FilePath> SearchEnginesManagersFactory::GetJsonPath(
    const std::string& filename) {
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
SearchEnginesManagersFactory::GetSearchEnginesJsonUpdatePath() {
  return GetJsonPath(kSearchEnginesJsonUpdated);
}

std::optional<base::FilePath>
SearchEnginesManagersFactory::GetSearchEnginesPromptJsonUpdatePath() {
  return GetJsonPath(kSearchEnginesPromptJsonUpdated);
}

SearchEnginesManager* SearchEnginesManagersFactory::GetSearchEnginesManager()
    const {
  return search_engines_manager_.get();
}

SearchEnginesPromptManager*
SearchEnginesManagersFactory::GetSearchEnginesPromptManager() const {
  return search_engines_prompt_manager_.get();
}
