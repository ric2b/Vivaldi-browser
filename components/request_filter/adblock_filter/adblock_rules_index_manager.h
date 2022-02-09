// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"

namespace content {
class BrowserContext;
}

namespace adblock_filter {
namespace flat {
struct RulesList;
}
class RulesIndex;

class RuleBufferHolder {
 public:
  RuleBufferHolder(std::unique_ptr<std::string> rule_buffer,
                   const std::string& checksum);
  ~RuleBufferHolder();

  const flat::RulesList* rules_list() const { return rules_list_; }
  const std::string& checksum() const { return checksum_; }

 private:
  std::string rule_buffer_;
  std::string checksum_;
  const flat::RulesList* const rules_list_;

  DISALLOW_COPY_AND_ASSIGN(RuleBufferHolder);
};

class RulesIndexManager : public RuleService::Observer {
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

  base::WeakPtr<RulesIndexManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::string index_checksum() const { return index_checksum_; }

  RulesIndex* rules_index() const { return rules_index_.get(); }

  RuleGroup group() const { return group_; }

 private:
  void OnRulesSourceUpdated(const RuleSource& rule_source) override;
  void OnRuleSourceDeleted(uint32_t source_id, RuleGroup group) override;

  void ReadRules(const RuleSource& rule_source);
  void OnRulesRead(uint32_t source_id,
                   const std::string& checksum,
                   std::unique_ptr<std::string> rules_buffer);

  void RebuildIndex();
  void ReadIndex(const std::string& checksum);
  void OnIndexRead(std::unique_ptr<std::string> index_buffer);

  RuleGroup group_;
  bool reload_in_progress_;

  std::map<uint32_t, RuleSource> rule_sources_;
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

  DISALLOW_COPY_AND_ASSIGN(RulesIndexManager);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_MANAGER_H_
