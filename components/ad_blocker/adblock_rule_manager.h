// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {
class RuleManager {
 public:
  enum ExceptionsList {
    kProcessList = 0,
    kExemptList,
    kFirstExceptionList = kProcessList,
    kLastExceptionList = kExemptList,
  };
  static constexpr size_t kExceptionListCount = kLastExceptionList + 1;

  using ActiveExceptionsLists = std::array<ExceptionsList, kRuleGroupCount>;
  using Exceptions =
      std::array<std::array<std::set<std::string>, kExceptionListCount>,
                 kRuleGroupCount>;

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    // The properties of a rule source have been updated. Either because a
    // fetch started or completed.
    virtual void OnRulesSourceUpdated(const RuleSource& rule_source) {}

    virtual void OnRuleSourceDeleted(uint32_t source_id, RuleGroup group) {}

    // This is called when changing active exception list
    virtual void OnExceptionListStateChanged(RuleGroup group) {}

    virtual void OnExceptionListChanged(RuleGroup group, ExceptionsList list) {}
  };

  virtual ~RuleManager();

  virtual bool AddRulesSource(const KnownRuleSource& known_source) = 0;
  virtual void DeleteRuleSource(const KnownRuleSource& known_source) = 0;

  // Returns the rule source matching the given ID, if it is an existing ID.
  virtual absl::optional<RuleSource> GetRuleSource(RuleGroup group,
                                                   uint32_t source_id) = 0;
  virtual std::map<uint32_t, RuleSource> GetRuleSources(
      RuleGroup group) const = 0;

  // Triggers an immediate fetching of a rule source instead of waiting for its
  // next update time.
  virtual bool FetchRuleSourceNow(RuleGroup group, uint32_t source_id) = 0;

  virtual void SetActiveExceptionList(RuleGroup group, ExceptionsList list) = 0;
  virtual ExceptionsList GetActiveExceptionList(RuleGroup group) const = 0;

  virtual void AddExceptionForDomain(RuleGroup group,
                                     ExceptionsList list,
                                     const std::string& domain) = 0;
  virtual void RemoveExceptionForDomain(RuleGroup group,
                                        ExceptionsList list,
                                        const std::string& domain) = 0;
  virtual void RemoveAllExceptions(RuleGroup group, ExceptionsList list) = 0;
  virtual const std::set<std::string>& GetExceptions(
      RuleGroup group,
      ExceptionsList list) const = 0;

  // This returns whether a given origin will be subject to filtering in a given
  // group or not, based on the active exception list.
  virtual bool IsExemptOfFiltering(RuleGroup group,
                                   url::Origin origin) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void OnCompiledRulesReadFailCallback(RuleGroup rule_group,
                                               uint32_t source_id) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_H_
