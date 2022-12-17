// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SOURCE_HANDLER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SOURCE_HANDLER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "components/ad_blocker/parse_result.h"

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace adblock_filter {

class RuleSourceHandler {
 public:
  using OnUpdateCallback = base::RepeatingCallback<void(RuleSourceHandler*)>;
  using OnTrackerInfosUpdateCallback =
      base::RepeatingCallback<void(const RuleSource&, base::Value::Dict)>;
  using RulesCompiler =
      base::RepeatingCallback<bool(const ParseResult& parse_result,
                                   const base::FilePath& output_path,
                                   std::string& checksum)>;

  RuleSourceHandler(
      RuleSource rule_source,
      const base::FilePath& profile_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      RulesCompiler rules_compiler,
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

  void StartUpdateTimer();

  void DoFetch();
  void DownloadRules();
  void OnRulesDownloaded(base::FilePath file);
  void ReadRulesFromFile(const base::FilePath& file, bool delete_after_read);
  void OnRulesRead(RulesReadResult result);
  void OnTrackerInfosLoaded(absl::optional<base::Value::Dict> tracker_infos);

  static RulesReadResult ReadRules(
      const base::FilePath& source_path,
      const base::FilePath& output_path,
      const base::FilePath& tracker_info_output_path,
      RulesCompiler rules_compiler,
      bool allow_abp_snippets,
      bool delete_after_read);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  RulesCompiler rules_compiler_;
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

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_SOURCE_HANDLER_H_
