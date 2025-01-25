// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_
#define EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Observes the adblock rules service and then routes the notifications as
// events to the extension system.
class ContentBlockingEventRouter
    : public adblock_filter::RuleService::Observer,
      public adblock_filter::RuleManager::Observer,
      public adblock_filter::KnownRuleSourcesHandler::Observer,
      public adblock_filter::StateAndLogs::Observer {
 public:
  explicit ContentBlockingEventRouter(content::BrowserContext* browser_context);
  ~ContentBlockingEventRouter() override;

  void Shutdown();

  // adblock_filter::RuleService::Observer implementation.
  void OnRuleServiceStateLoaded(
      adblock_filter::RuleService* rule_service) override;
  void OnGroupStateChanged(adblock_filter::RuleGroup group) override;

  // adblock_filter::RuleManager::Observer implementation.
  void OnRuleSourceUpdated(
      adblock_filter::RuleGroup group,
      const adblock_filter::ActiveRuleSource& rule_source) override;
  void OnExceptionListStateChanged(adblock_filter::RuleGroup group) override;
  void OnExceptionListChanged(
      adblock_filter::RuleGroup group,
      adblock_filter::RuleManager::ExceptionsList list) override;

  // adblock_filter::KnownRuleSourcesHandler::Observer implementation.
  void OnKnownSourceAdded(
      adblock_filter::RuleGroup group,
      const adblock_filter::KnownRuleSource& rule_source) override;
  void OnKnownSourceRemoved(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;

  void OnKnownSourceEnabled(adblock_filter::RuleGroup group,
                            uint32_t source_id) override;
  void OnKnownSourceDisabled(adblock_filter::RuleGroup group,
                             uint32_t source_id) override;

  // adblock_filter::StateAndLogs::Observer implementation.
  void OnNewBlockedUrlsReported(
      adblock_filter::RuleGroup group,
      std::set<content::WebContents*> tabs_with_new_blocks) override;
  virtual void OnAllowAttributionChanged(
      content::WebContents* web_contents) override;
  virtual void OnNewAttributionTrackerAllowed(
      std::set<content::WebContents*> tabs_with_new_attribution_trackers)
      override;

 private:
  const raw_ptr<content::BrowserContext> browser_context_;
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

  const raw_ptr<content::BrowserContext> browser_context_;

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
};

class ContentBlockingSetKnownSourceSettingsFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.setKnownSourceSettings",
                             CONTENT_BLOCKING_SET_KNOWN_SOURCE_SETTINGS)
  ContentBlockingSetKnownSourceSettingsFunction() = default;

 private:
  ~ContentBlockingSetKnownSourceSettingsFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
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
};

class ContentBlockingGetBlockedCountersFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getBlockedCounters",
                             CONTENT_BLOCKING_GET_BLOCKED_COUNTERS)
  ContentBlockingGetBlockedCountersFunction() = default;

 private:
  ~ContentBlockingGetBlockedCountersFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
};

class ContentBlockingClearBlockedCountersFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.clearBlockedCounters",
                             CONTENT_BLOCKING_CLEAR_BLOCKED_COUNTERS)
  ContentBlockingClearBlockedCountersFunction() = default;

 private:
  ~ContentBlockingClearBlockedCountersFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
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
};

class ContentBlockingIsExemptByPartnerURLFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.isExemptByPartnerURL",
                             CONTENT_BLOCKING_IS_EXEMPT_BY_PARTNER_URL)
  ContentBlockingIsExemptByPartnerURLFunction() = default;

 private:
  ~ContentBlockingIsExemptByPartnerURLFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
};

class ContentBlockingGetAdAttributionDomainFunction : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentBlocking.getAdAttributionDomain",
                             CONTENT_BLOCKING_GET_AD_ATTRIBUTION_DOMAIN)
  ContentBlockingGetAdAttributionDomainFunction() = default;

 private:
  ~ContentBlockingGetAdAttributionDomainFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
};

class ContentBlockingGetAdAttributionAllowedTrackersFunction
    : public AdBlockFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "contentBlocking.getAdAttributionAllowedTrackers",
      CONTENT_BLOCKING_GET_AD_ATTRIBUTION_ALLOWED_TRACKERS)
  ContentBlockingGetAdAttributionAllowedTrackersFunction() = default;

 private:
  ~ContentBlockingGetAdAttributionAllowedTrackersFunction() override = default;
  // AdBlockFunction:
  ResponseValue RunWithService(
      adblock_filter::RuleService* rules_service) override;
};
}  // namespace extensions

#endif  // EXTENSIONS_API_CONTENT_BLOCKING_CONTENT_BLOCKING_API_H_
