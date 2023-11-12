// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_IMPL_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {
class RuleManagerImpl : public RuleManager {
 public:
  explicit RuleManagerImpl(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& profile_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::array<RuleSources, kRuleGroupCount> rule_sources,
      ActiveExceptionsLists active_exceptions_lists,
      Exceptions exceptions,
      base::RepeatingClosure schedule_save,
      RuleSourceHandler::RulesCompiler rules_compiler,
      RuleSourceHandler::OnTrackerInfosUpdateCallback
          on_tracker_infos_update_callback);
  ~RuleManagerImpl() override;
  RuleManagerImpl(const RuleManagerImpl&) = delete;
  RuleManagerImpl& operator=(const RuleManagerImpl&) = delete;

  // Implementing RuleManager
  bool AddRulesSource(const KnownRuleSource& known_source) override;
  void DeleteRuleSource(const KnownRuleSource& known_source) override;
  absl::optional<RuleSource> GetRuleSource(RuleGroup group,
                                           uint32_t source_id) override;
  std::map<uint32_t, RuleSource> GetRuleSources(RuleGroup group) const override;
  bool FetchRuleSourceNow(RuleGroup group, uint32_t source_id) override;
  void SetActiveExceptionList(RuleGroup group, ExceptionsList list) override;
  ExceptionsList GetActiveExceptionList(RuleGroup group) const override;
  void AddExceptionForDomain(RuleGroup group,
                             ExceptionsList list,
                             const std::string& domain) override;
  void RemoveExceptionForDomain(RuleGroup group,
                                ExceptionsList list,
                                const std::string& domain) override;
  void RemoveAllExceptions(RuleGroup group, ExceptionsList list) override;
  const std::set<std::string>& GetExceptions(
      RuleGroup group,
      ExceptionsList list) const override;
  bool IsExemptOfFiltering(RuleGroup group, url::Origin origin) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void OnCompiledRulesReadFailCallback(RuleGroup rule_group,
                                       uint32_t source_id) override;

 private:
  void OnSourceUpdated(RuleSourceHandler* rule_source_handler);

  std::map<int64_t, std::unique_ptr<RuleSourceHandler>>& GetSourceMap(
      RuleGroup group);

  const std::map<int64_t, std::unique_ptr<RuleSourceHandler>>& GetSourceMap(
      RuleGroup group) const;

  void AddRequestFilter(RuleGroup group);

  std::array<std::map<int64_t, std::unique_ptr<RuleSourceHandler>>,
             kRuleGroupCount>
      rule_sources_;
  ActiveExceptionsLists active_exceptions_lists_ = {kProcessList, kProcessList};
  Exceptions exceptions_;
  base::RepeatingClosure schedule_save_;

  base::FilePath profile_path_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  RuleSourceHandler::RulesCompiler rules_compiler_;
  RuleSourceHandler::OnTrackerInfosUpdateCallback
      on_tracker_infos_update_callback_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<Observer> observers_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_MANAGER_IMPL_H_
