// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SEARCH_ENGINES_SEARCH_ENGINES_API_H_
#define EXTENSIONS_API_SEARCH_ENGINES_SEARCH_ENGINES_API_H_

#include <memory>

#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class TemplateURLService;

namespace extensions {
class SearchEnginesAPI : public BrowserContextKeyedAPI,
                         public TemplateURLServiceObserver {
 public:
  explicit SearchEnginesAPI(content::BrowserContext* context);
  SearchEnginesAPI(const SearchEnginesAPI& other) = delete;
  SearchEnginesAPI& operator=(const SearchEnginesAPI& other) = delete;
  ~SearchEnginesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SearchEnginesAPI>* GetFactoryInstance();

  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  friend class BrowserContextKeyedAPIFactory<SearchEnginesAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<TemplateURLService> service_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "searchEnginesAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = false;
};

class SearchEnginesGetKeywordForUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.getKeywordForUrl",
                             SEARCH_ENGINES_GET_KEYWORD_FOR_TEMPLATE_URL)
  SearchEnginesGetKeywordForUrlFunction() = default;

 private:
  ~SearchEnginesGetKeywordForUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesGetTemplateUrlsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.getTemplateUrls",
                             SEARCH_ENGINES_GET_TEMPLATE_URLS)
  SearchEnginesGetTemplateUrlsFunction() = default;

 private:
  ~SearchEnginesGetTemplateUrlsFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesAddTemplateUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.addTemplateUrl",
                             SEARCH_ENGINES_ADD_TEMPLATE_URL)
  SearchEnginesAddTemplateUrlFunction() = default;

 private:
  ~SearchEnginesAddTemplateUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesRemoveTemplateUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.removeTemplateUrl",
                             SEARCH_ENGINES_REMOVE_TEMPLATE_URL)
  SearchEnginesRemoveTemplateUrlFunction() = default;

 private:
  ~SearchEnginesRemoveTemplateUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesUpdateTemplateUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.updateTemplateUrl",
                             SEARCH_ENGINES_UPDATE_TEMPLATE_URL)
  SearchEnginesUpdateTemplateUrlFunction() = default;

 private:
  ~SearchEnginesUpdateTemplateUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesMoveTemplateUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.moveTemplateUrl",
                             SEARCH_ENGINES_MOVE_TEMPLATE_URL)
  SearchEnginesMoveTemplateUrlFunction() = default;

 private:
  ~SearchEnginesMoveTemplateUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesSetDefaultFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.setDefault",
                             SEARCH_ENGINES_SET_DEFAULT)
  SearchEnginesSetDefaultFunction() = default;

 private:
  ~SearchEnginesSetDefaultFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesGetSearchRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.getSearchRequest",
                             SEARCH_ENGINES_GET_SEARCH_REQUEST)
  SearchEnginesGetSearchRequestFunction() = default;

 private:
  ~SearchEnginesGetSearchRequestFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesGetSuggestRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.getSuggestRequest",
                             SEARCH_ENGINES_GET_SUGGEST_REQUEST)
  SearchEnginesGetSuggestRequestFunction() = default;

 private:
  ~SearchEnginesGetSuggestRequestFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesRepairPrepopulatedTemplateUrlsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.repairPrepopulatedTemplateUrls",
                             SEARCH_ENGINES_REPAIR_PREPOPULATED_TEMPLATE_URLS)
  SearchEnginesRepairPrepopulatedTemplateUrlsFunction() = default;

 private:
  ~SearchEnginesRepairPrepopulatedTemplateUrlsFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesGetSwitchPromptDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.getSwitchPromptData",
                             SEARCH_ENGINES_GET_SWITCH_PROMPT_DATA)
  SearchEnginesGetSwitchPromptDataFunction() = default;

 private:
  ~SearchEnginesGetSwitchPromptDataFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class SearchEnginesMarkSwitchPromptAsSeenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("searchEngines.markSwitchPromptAsSeen",
                             SEARCH_ENGINES_MARK_SWITCH_PROMPT_AS_SEEN)
  SearchEnginesMarkSwitchPromptAsSeenFunction() = default;

 private:
  ~SearchEnginesMarkSwitchPromptAsSeenFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};
}  // namespace extensions

#endif  // EXTENSIONS_API_SEARCH_ENGINES_SEARCH_ENGINES_API_H_
