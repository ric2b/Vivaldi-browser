// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_H_
#define COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "components/ad_blocker/core/adblock_rule_service_storage_delegate.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {

class RuleServiceStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  RuleServiceStorage(
      const base::FilePath& profile_path,
      RuleServiceStorageDelegate* delegate,
      scoped_refptr<base::SequencedTaskRunner> file_io_task_runner);
  ~RuleServiceStorage() override;
  RuleServiceStorage(const RuleServiceStorage&) = delete;
  RuleServiceStorage& operator=(const RuleServiceStorage&) = delete;

  // Save the state of the service's rule sources at the earliest opportunity.
  void ScheduleSave();

  // The rules service is going down. Handle any pending save.
  void OnRuleServiceShutdown();

  void Load();

 private:
  // ImportantFileWriter::DataSerializer implementation.
  std::optional<std::string> SerializeData() override;

  // Callback from backend after obtaining the sources from file.
  void OnLoadFinished(RuleServiceStorageDelegate::LoadResult load_result);

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> file_io_task_runner_;

  raw_ptr<RuleServiceStorageDelegate> delegate_;

  // Helper to write rule sources safely.
  base::ImportantFileWriter writer_;

  base::WeakPtrFactory<RuleServiceStorage> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_RULE_SERVICE_STORAGE_H_
