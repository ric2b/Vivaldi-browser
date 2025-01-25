// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_STORAGE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_STORAGE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {

class RuleServiceStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  struct LoadResult {
    LoadResult();
    ~LoadResult();
    LoadResult(LoadResult&& load_result);
    LoadResult& operator=(LoadResult&& load_result);

    std::array<bool, kRuleGroupCount> groups_enabled = {true, true};
    std::array<ActiveRuleSources, kRuleGroupCount> rule_sources;
    std::array<std::vector<KnownRuleSource>, kRuleGroupCount> known_sources;
    std::array<std::set<std::string>, kRuleGroupCount> deleted_presets;
    std::array<RuleManager::ExceptionsList, kRuleGroupCount>
        active_exceptions_lists = {RuleManager::kProcessList,
                                   RuleManager::kProcessList};
    std::array<
        std::array<std::set<std::string>, RuleManager::kExceptionListCount>,
        kRuleGroupCount>
        exceptions;
    std::array<std::string, kRuleGroupCount> index_checksums;
    base::Time blocked_reporting_start;
    StateAndLogs::CounterGroup blocked_domains_counters;
    StateAndLogs::CounterGroup blocked_for_origin_counters;

    int storage_version = 0;
  };
  using LoadingDoneCallback = base::OnceCallback<void(LoadResult)>;

  RuleServiceStorage(
      const base::FilePath& profile_path,
      RuleService* rule_service,
      scoped_refptr<base::SequencedTaskRunner> file_io_task_runner);
  ~RuleServiceStorage() override;
  RuleServiceStorage(const RuleServiceStorage&) = delete;
  RuleServiceStorage& operator=(const RuleServiceStorage&) = delete;

  // Save the state of the service's rule sources at the earliest opportunity.
  void ScheduleSave();

  // The rules service is going down. Handle any pending save.
  void OnRuleServiceShutdown();

  void Load(LoadingDoneCallback loading_done_callback);

 private:
  // ImportantFileWriter::DataSerializer implementation.
  std::optional<std::string> SerializeData() override;

  // Callback from backend after obtaining the sources from file.
  void OnLoadFinished(LoadResult load_result);

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> file_io_task_runner_;

  LoadingDoneCallback loading_done_callback_;

  const raw_ptr<RuleService> rule_service_;

  // Helper to write rule sources safely.
  base::ImportantFileWriter writer_;

  base::WeakPtrFactory<RuleServiceStorage> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_STORAGE_H_
