// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_types.h"

namespace content {
class BrowserContext;
}

namespace adblock_filter {
namespace flat {
struct RulesList;
}
class RulesIndex;
class RuleService;

class RuleBufferHolder {
 public:
  RuleBufferHolder(std::string rule_buffer, const std::string& checksum);
  ~RuleBufferHolder();
  RuleBufferHolder(const RuleBufferHolder&) = delete;
  RuleBufferHolder& operator=(const RuleBufferHolder&) = delete;

  const flat::RulesList* rules_list() const { return rules_list_; }
  const std::string& checksum() const { return checksum_; }

 private:
  std::string rule_buffer_;
  std::string checksum_;
  const raw_ptr<const flat::RulesList> rules_list_;
};

class RulesIndexManager : public RuleManager::Observer {
 public:
  using RulesIndexChangedCallback = base::RepeatingClosure;
  using RulesIndexLoadedCallback = base::RepeatingClosure;
  using RulesBufferReadFailCallback =
      base::RepeatingCallback<void(RuleGroup rule_group, uint32_t source_id)>;

  RulesIndexManager(content::BrowserContext* context,
                    RuleService* rule_service,
                    RuleGroup group,
                    const std::string& index_checksum,
                    RulesIndexChangedCallback rules_index_change_callback,
                    RulesIndexLoadedCallback rules_index_loaded_callback,
                    RulesBufferReadFailCallback rule_buffer_read_fail_callback,
                    scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~RulesIndexManager() override;
  RulesIndexManager(const RulesIndexManager&) = delete;
  RulesIndexManager& operator=(const RulesIndexManager&) = delete;

  base::WeakPtr<RulesIndexManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::string index_checksum() const { return index_checksum_; }

  RulesIndex* rules_index() const { return rules_index_.get(); }

  RuleGroup group() const { return group_; }

 private:
  // Implementing RuleManager::Observer
  void OnRuleSourceUpdated(RuleGroup group,
                           const ActiveRuleSource& rule_source) override;
  void OnRuleSourceDeleted(uint32_t source_id, RuleGroup group) override;
  void OnExceptionListStateChanged(RuleGroup group) override;
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

  void ReadRules(const ActiveRuleSource& rule_source);
  void OnRulesRead(uint32_t source_id,
                   const std::string& checksum,
                   std::string rules_buffer);

  void RebuildIndex();
  void ReadIndex(const std::string& checksum);
  void OnIndexRead(std::string index_buffer);

  RuleGroup group_;
  bool reload_in_progress_;

  std::map<uint32_t, ActiveRuleSource> rule_sources_;
  base::FilePath rules_list_folder_;

  std::map<uint32_t, std::unique_ptr<RuleBufferHolder>> rules_buffers_;
  std::vector<std::unique_ptr<RuleBufferHolder>> old_rules_buffers_;

  std::string index_checksum_;
  std::unique_ptr<RulesIndex> rules_index_;
  int index_read_fail_count_ = 0;

  RulesIndexChangedCallback rules_index_change_callback_;
  RulesIndexLoadedCallback rules_index_loaded_callback_;
  RulesBufferReadFailCallback rule_buffer_read_fail_callback_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<RulesIndexManager> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_
