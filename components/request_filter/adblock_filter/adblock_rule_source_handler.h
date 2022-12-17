// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SOURCE_HANDLER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SOURCE_HANDLER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace adblock_filter {

class RuleSourceHandler {
 public:
  using OnUpdateCallback = base::RepeatingCallback<void(RuleSourceHandler*)>;
  using OnTrackerInfosUpdateCallback =
      base::RepeatingCallback<void(const RuleSource&, base::Value::Dict)>;

  RuleSourceHandler(
      content::BrowserContext* context,
      RuleSource rule_source,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      OnUpdateCallback on_update_callback,
      OnTrackerInfosUpdateCallback on_tracker_infos_update_callback);
  ~RuleSourceHandler();
  RuleSourceHandler(const RuleSourceHandler&) = delete;
  RuleSourceHandler& operator=(const RuleSourceHandler&) = delete;

  const RuleSource& rule_source() const { return rule_source_; }

  void FetchNow();

  // Remove the rules list file associated with this data source.
  void Clear();

 private:
  struct RulesReadResult;
  class RulesReader;

  void StartUpdateTimer();

  void DoFetch();
  void DownloadRules();
  void OnRulesDownloaded(base::FilePath file);
  void ReadRulesFromFile(const base::FilePath& file, bool delete_after_read);
  void OnRulesRead(std::unique_ptr<RulesReadResult> result);
  void OnTrackerInfosLoaded(absl::optional<base::Value::Dict> tracker_infos);

  content::BrowserContext* context_;

  OnUpdateCallback on_update_callback_;
  OnTrackerInfosUpdateCallback on_tracker_infos_update_callback_;
  RuleSource rule_source_;
  base::FilePath rules_list_path_;
  base::FilePath tracker_infos_path_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OneShotTimer update_timer_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<RuleSourceHandler> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SOURCE_HANDLER_H_
