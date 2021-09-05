// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_
#define EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/request_filter/adblock_filter/adblock_known_sources_handler.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Observes the adblock rules service and then routes the notifications as
// events to the extension system.
class ContentBlockingEventRouter
    : public adblock_filter::RuleService::Observer,
      public adblock_filter::KnownRuleSourcesHandler::Observer,
      public adblock_filter::BlockedUrlsReporter::Observer {
 public:
  explicit ContentBlockingEventRouter(content::BrowserContext* browser_context);
  ~ContentBlockingEventRouter() override;

  // adblock_filter::RuleService::Observer implementation.
  void OnRuleServiceStateLoaded(
      adblock_filter::RuleService* rule_service) override;
  void OnRulesSourceUpdated(
      const adblock_filter::RuleSource& rule_source) override;
  void OnGroupStateChanged(adblock_filter::RuleGroup group) override;
  void OnExceptionListChanged(
      adblock_filter::RuleGroup group,
      adblock_filter::RuleService::ExceptionsList list) override;

  // adblock_filter::KnownRuleSourcesHandler::Observer implementation.
  void OnKnownSourceAdded(
      const adblock_filter::KnownRuleSource& rule_source) override;
  void OnKnownSourceRemoved(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;

  void OnKnownSourceEnabled(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;
  void OnKnownSourceDisabled(adblock_filter::RuleGroup group,
                             uint32_t source_id) override;

  // adblock_filter::BlockedUrlsReporter::Observer implementation.
  void OnNewBlockedUrlsReported(
      adblock_filter::RuleGroup group,
      std::set<content::WebContents*> tabs_with_new_blocks) override;

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingEventRouter);
};

class ContentBlockingAPI : public BrowserContextKeyedAPI,
                           public EventRouter::Observer {
 public:
  explicit ContentBlockingAPI(content::BrowserContext* context);
  ~ContentBlockingAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ContentBlockingAPI>*
  GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ContentBlockingAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "contentBlockingAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ContentBlockingEventRouter> content_blocking_event_router_;
};

class AdBlockFunction : public ExtensionFunction,
                        public adblock_filter::RuleService::Observer {
 public:
  AdBlockFunction() = default;

  // adblock_filter::RuleService::Observer implementation.
  void OnRuleServiceStateLoaded(
      adblock_filter::RuleService* rule_service) override;

 protected:
  ~AdBlockFunction() override = default;
  virtual ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) = 0;
  static ResponseValue ValidationFailure(AdBlockFunction* function);

 private:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AdBlockFunction);
};

class ContentBlockingSetRuleGroupEnabledFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.setRuleGroupEnabled",
                             CONTENT_BLOCKING_SET_RULE_GROUP_ENABLED)
  ContentBlockingSetRuleGroupEnabledFunction() = default;

 private:
  ~ContentBlockingSetRuleGroupEnabledFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingSetRuleGroupEnabledFunction);
};

class ContentBlockingIsRuleGroupEnabledFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.isRuleGroupEnabled",
                             CONTENT_BLOCKING_IS_RULE_GROUP_ENABLED)
  ContentBlockingIsRuleGroupEnabledFunction() = default;

 private:
  ~ContentBlockingIsRuleGroupEnabledFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingIsRuleGroupEnabledFunction);
};

class ContentBlockingAddKnownSourceFromURLFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.addKnownSourceFromURL",
                             CONTENT_BLOCKING_ADD_KNOWN_SOURCE_FROM_URL)
  ContentBlockingAddKnownSourceFromURLFunction() = default;

 private:
  ~ContentBlockingAddKnownSourceFromURLFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingAddKnownSourceFromURLFunction);
};

class ContentBlockingAddKnownSourceFromFileFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.addKnownSourceFromFile",
                             CONTENT_BLOCKING_ADD_KNOWN_SOURCE_FROM_FILE)
  ContentBlockingAddKnownSourceFromFileFunction() = default;

 private:
  ~ContentBlockingAddKnownSourceFromFileFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingAddKnownSourceFromFileFunction);
};

class ContentBlockingEnableSourceFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.enableSource",
                             CONTENT_BLOCKING_ENABLE_SOURCE)
  ContentBlockingEnableSourceFunction() = default;

 private:
  ~ContentBlockingEnableSourceFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingEnableSourceFunction);
};

class ContentBlockingDisableSourceFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.disableSource",
                             CONTENT_BLOCKING_DISABLE_SOURCE)
  ContentBlockingDisableSourceFunction() = default;

 private:
  ~ContentBlockingDisableSourceFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingDisableSourceFunction);
};

class ContentBlockingFetchSourceNowFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.fetchSourceNow",
                             CONTENT_BLOCKING_FETCH_SOURCE_NOW)
  ContentBlockingFetchSourceNowFunction() = default;

 private:
  ~ContentBlockingFetchSourceNowFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingFetchSourceNowFunction);
};

class ContentBlockingDeleteKnownSourceFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.deleteKnownSource",
                             CONTENT_BLOCKING_DELETE_KNOWN_SOURCE)
  ContentBlockingDeleteKnownSourceFunction() = default;

 private:
  ~ContentBlockingDeleteKnownSourceFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingDeleteKnownSourceFunction);
};

class ContentBlockingResetPresetSourcesFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.resetPresetSources",
                             CONTENT_BLOCKING_RESET_PRESET_SOURCES)
  ContentBlockingResetPresetSourcesFunction() = default;

 private:
  ~ContentBlockingResetPresetSourcesFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingResetPresetSourcesFunction);
};

class ContentBlockingGetRuleSourceFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getRuleSource",
                             CONTENT_BLOCKING_GET_RULE_SOURCE)
  ContentBlockingGetRuleSourceFunction() = default;

 private:
  ~ContentBlockingGetRuleSourceFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetRuleSourceFunction);
};

class ContentBlockingGetRuleSourcesFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getRuleSources",
                             CONTENT_BLOCKING_GET_RULE_SOURCES)
  ContentBlockingGetRuleSourcesFunction() = default;

 private:
  ~ContentBlockingGetRuleSourcesFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetRuleSourcesFunction);
};

class ContentBlockingSetActiveExceptionsListFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.setActiveExceptionsList",
                             CONTENT_BLOCKING_SET_ACTIVE_EXCEPTION_LIST)
  ContentBlockingSetActiveExceptionsListFunction() = default;

 private:
  ~ContentBlockingSetActiveExceptionsListFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingSetActiveExceptionsListFunction);
};

class ContentBlockingGetActiveExceptionsListFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getActiveExceptionsList",
                             CONTENT_BLOCKING_GET_ACTIVE_EXCEPTION_LIST)
  ContentBlockingGetActiveExceptionsListFunction() = default;

 private:
  ~ContentBlockingGetActiveExceptionsListFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetActiveExceptionsListFunction);
};

class ContentBlockingAddExceptionForDomainFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.addExceptionForDomain",
                             CONTENT_BLOCKING_ADD_EXCEPTION_FOR_DOMAIN)
  ContentBlockingAddExceptionForDomainFunction() = default;

 private:
  ~ContentBlockingAddExceptionForDomainFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingAddExceptionForDomainFunction);
};

class ContentBlockingRemoveExceptionForDomainFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.removeExceptionForDomain",
                             CONTENT_BLOCKING_REMOVE_EXCEPTION_FOR_DOMAIN)
  ContentBlockingRemoveExceptionForDomainFunction() = default;

 private:
  ~ContentBlockingRemoveExceptionForDomainFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingRemoveExceptionForDomainFunction);
};

class ContentBlockingRemoveAllExceptionsFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.removeAllExceptions",
                             CONTENT_BLOCKING_REMOVE_ALL_EXCEPTIONS)
  ContentBlockingRemoveAllExceptionsFunction() = default;

 private:
  ~ContentBlockingRemoveAllExceptionsFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingRemoveAllExceptionsFunction);
};

class ContentBlockingGetExceptionsFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getExceptions",
                             CONTENT_BLOCKING_GET_EXCEPTIONS)
  ContentBlockingGetExceptionsFunction() = default;

 private:
  ~ContentBlockingGetExceptionsFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetExceptionsFunction);
};

class ContentBlockingGetAllExceptionListsFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getAllExceptionLists",
                             CONTENT_BLOCKING_GET_ALL_EXCEPTIONS_LISTS)
  ContentBlockingGetAllExceptionListsFunction() = default;

 private:
  ~ContentBlockingGetAllExceptionListsFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetAllExceptionListsFunction);
};

class ContentBlockingGetBlockedUrlsInfoFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getBlockedUrlsInfo",
                             CONTENT_BLOCKING_GET_BLOCKED_URLS_INFO)
  ContentBlockingGetBlockedUrlsInfoFunction() = default;

 private:
  ~ContentBlockingGetBlockedUrlsInfoFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetBlockedUrlsInfoFunction);
};

class ContentBlockingGetBlockedDomainCountersFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getBlockedDomainCounters",
                             CONTENT_BLOCKING_GET_BLOCKED_DOMAIN_COUNTERS)
  ContentBlockingGetBlockedDomainCountersFunction() = default;

 private:
  ~ContentBlockingGetBlockedDomainCountersFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingGetBlockedDomainCountersFunction);
};

class ContentBlockingClearBlockedDomainCountersFunction
    : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.clearBlockedDomainCounters",
                             CONTENT_BLOCKING_CLEAR_BLOCKED_DOMAIN_COUNTERS)
  ContentBlockingClearBlockedDomainCountersFunction() = default;

 private:
  ~ContentBlockingClearBlockedDomainCountersFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingClearBlockedDomainCountersFunction);
};

class ContentBlockingIsExemptOfFilteringFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.isExemptOfFiltering",
                             CONTENT_BLOCKING_IS_EXEMPT_OF_FILTERING)
  ContentBlockingIsExemptOfFilteringFunction() = default;

 private:
  ~ContentBlockingIsExemptOfFilteringFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;

  DISALLOW_COPY_AND_ASSIGN(ContentBlockingIsExemptOfFilteringFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_
