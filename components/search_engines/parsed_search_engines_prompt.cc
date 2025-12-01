// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "parsed_search_engines_prompt.h"

#include <optional>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_helper.h"

namespace {
constexpr char kIntVariables[] = "int_variables";
constexpr char kPromptIfDomain[] = "prompt_if_domain";
constexpr char kPromptIfType[] = "prompt_if_type";

constexpr char kExcludes[] = "exclude_list";
constexpr char kExcludeDomain[] = "domain";
constexpr char kExcludeType[] = "type";

constexpr char kSearchEnginesDataVersionRequired[] =
    "kSearchEnginesDataVersionRequired";
constexpr char kVersion[] = "kVersion";

}  // namespace

/* static */
std::unique_ptr<ParsedSearchEnginesPrompt>
ParsedSearchEnginesPrompt::FromJsonString(std::string_view json_string,
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

  base::Value::Dict* int_variables = root.FindDict(kIntVariables);

  if (!int_variables) {
    error = base::StrCat({"Missing key: ", kIntVariables});
    return nullptr;
  }

  std::optional<int> version = int_variables->FindInt(kVersion);
  if (!version) {
    error = base::StrCat({"Missing key: ", kVersion});
    return nullptr;
  }

  std::optional<int> search_engines_data_version_required =
      int_variables->FindInt(kSearchEnginesDataVersionRequired);
  if (!search_engines_data_version_required) {
    error = base::StrCat({"Missing key: ", kSearchEnginesDataVersionRequired});
    return nullptr;
  }

  auto get_domains =
      [](const base::Value::Dict& parent,
         std::string_view key) -> std::optional<std::vector<std::string>> {
    std::vector<std::string> domains;

    const base::Value::List* domain_list = parent.FindList(key);
    if (!domain_list) {
      return std::nullopt;
    }

    for (const auto& value : *domain_list) {
      if (const auto str = value.GetIfString()) {
        domains.push_back(*str);
      } else {
        // Error is printed only when the response is nullptr,
        // but here we just silently ignore the unexpected value.
        LOG(ERROR) << "Unexpected value type for " << kPromptIfDomain
                   << " - expected string.";
      }
    }
    return domains;
  };

  auto get_type =
      [](const base::Value::Dict& parent,
         std::string_view key) -> std::optional<std::set<SearchEngineType>> {
    std::set<SearchEngineType> types;

    const base::Value::List* type_list = parent.FindList(key);
    if (!type_list) {
      return std::nullopt;
    }
    for (const auto& value : *type_list) {
      if (const auto str = value.GetIfString()) {
        const SearchEngineType type =
            TemplateURLPrepopulateData::StringToSearchEngine(*str);
        if (type != SearchEngineType::SEARCH_ENGINE_UNKNOWN) {
          types.emplace(type);
        }
      } else {
        // Error is printed only when the response is nullptr,
        // but here we just silently ignore the unexpected value.
        LOG(ERROR) << "Unexpected value type for " << key
                   << " - expected string.";
      }
    }
    return types;
  };

  const auto maybe_prompt_if_domain = get_domains(root, kPromptIfDomain);
  if (!maybe_prompt_if_domain) {
    error = base::StrCat({"Missing key: ", kPromptIfDomain});
    return nullptr;
  }

  const auto maybe_prompt_if_type = get_type(root, kPromptIfType);
  if (!maybe_prompt_if_type) {
    error = base::StrCat({"Missing key: ", kPromptIfType});
    return nullptr;
  }

  std::vector<std::string> exclude_if_domain;
  std::set<SearchEngineType> exclude_if_type;

  base::Value::Dict* excludes = root.FindDict(kExcludes);
  // If the exclude list is missing, we assume it is empty.
  if (excludes) {
    exclude_if_domain = get_domains(*excludes, kExcludeDomain)
                            .value_or(std::vector<std::string>());
    exclude_if_type = get_type(*excludes, kExcludeType)
                          .value_or(std::set<SearchEngineType>());
  }

  return std::unique_ptr<ParsedSearchEnginesPrompt>(
      new ParsedSearchEnginesPrompt(
          *maybe_prompt_if_domain, *maybe_prompt_if_type, exclude_if_domain,
          exclude_if_type, *version, *search_engines_data_version_required));
}

ParsedSearchEnginesPrompt::ParsedSearchEnginesPrompt(
    std::vector<std::string> prompt_if_domain,
    std::set<SearchEngineType> prompt_if_type,
    std::vector<std::string> exclude_if_domain,
    std::set<SearchEngineType> exclude_if_type,
    int version,
    int search_engines_data_version_required)
    : prompt_if_domain_(std::move(prompt_if_domain)),
      prompt_if_type_(std::move(prompt_if_type)),
      exclude_if_domain_(std::move(exclude_if_domain)),
      exclude_if_type_(std::move(exclude_if_type)),
      version_(version),
      search_engines_data_version_required_(
          search_engines_data_version_required) {}

ParsedSearchEnginesPrompt::~ParsedSearchEnginesPrompt() = default;
