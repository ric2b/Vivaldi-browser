// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace adblock_filter {
class RuleManager;
class KnownRuleSourcesHandler;
class BlockedUrlsReporter;
class CosmeticFilter;

class RuleService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnRuleServiceStateLoaded(RuleService* rule_service) {}

    // This is called when enabling/disabling groups
    virtual void OnGroupStateChanged(RuleGroup group) {}
  };

  ~RuleService() override;

  virtual bool IsLoaded() const = 0;

  virtual bool IsRuleGroupEnabled(RuleGroup group) const = 0;
  virtual void SetRuleGroupEnabled(RuleGroup group, bool enabled) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the checksum of the index used for fast-finding of the rules.
  // This will be an empty string until an index gets built for the first time.
  // If it remains empty or becomes empty later on, this means saving the index
  // to disk is failing.
  virtual std::string GetRulesIndexChecksum(RuleGroup group) = 0;

  virtual RuleManager* GetRuleManager() = 0;
  virtual KnownRuleSourcesHandler* GetKnownSourcesHandler() = 0;
  virtual BlockedUrlsReporter* GetBlockerUrlsReporter() = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_H_
