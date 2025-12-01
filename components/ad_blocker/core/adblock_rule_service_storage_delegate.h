// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_DELEGATE_H_
#define COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_DELEGATE_H_

#include <array>
#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
#include "components/ad_blocker/public/core/adblock_types.h"

namespace adblock_filter {
class KnownRuleSourcesHandler;

class RuleServiceStorageDelegate {
 public:
  struct LoadResult {
    using CounterGroup =
        std::array<std::map<std::string, int>, kRuleGroupCount>;

    LoadResult();
    ~LoadResult();
    LoadResult(LoadResult&& load_result);
    LoadResult& operator=(LoadResult&& load_result);

    std::array<bool, kRuleGroupCount> groups_enabled = {true, true};
    std::array<ActiveRuleSources, kRuleGroupCount> rule_sources;
    std::array<std::vector<KnownRuleSource>, kRuleGroupCount> known_sources;
    std::array<std::set<base::Uuid>, kRuleGroupCount> deleted_presets;
    std::array<RuleManager::ExceptionsList, kRuleGroupCount>
        active_exceptions_lists = {RuleManager::kProcessList,
                                   RuleManager::kProcessList};
    std::array<
        std::array<std::set<std::string>, RuleManager::kExceptionListCount>,
        kRuleGroupCount>
        exceptions;
    std::array<std::string, kRuleGroupCount> index_checksums;

    // Keep for migration
    base::Time blocked_reporting_start;
    CounterGroup blocked_domains_counters;
    CounterGroup blocked_for_origin_counters;

    int storage_version = 0;
  };

  virtual void OnStorageDoneLoading(LoadResult load_results) = 0;

  virtual ~RuleServiceStorageDelegate() = 0;

  virtual RuleManager* GetRuleManager() = 0;
  virtual KnownRuleSourcesHandler* GetKnownSourcesHandler() = 0;

  // Gets the checksum of the index used for fast-finding of the rules.
  // This will be an empty string until an index gets built for the first
  // time. If it remains empty or becomes empty later on, this means saving
  // the index to disk is failing. On iOS, this gives the checksum for the
  // organized rules instead, which are just the rules from all lists put
  // together in a way that overcomes some of the limitations of WebKit
  virtual std::string GetRulesIndexChecksum(RuleGroup group) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_DELEGATE_H_
