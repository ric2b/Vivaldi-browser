// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef BROWSER_STATS_REPORTER_IMPL_H_
#define BROWSER_STATS_REPORTER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "browser/stats_reporter.h"
#include "net/base/backoff_entry.h"

namespace network {
class SimpleURLLoader;
}

namespace vivaldi {

class StatsReporterImpl : public StatsReporter {
 public:
  StatsReporterImpl();
  ~StatsReporterImpl() override;

 private:
  struct NextPingTimes {
    base::Time daily;
    base::Time weekly;
    base::Time monthly;
  };

  struct ReportingData {
    std::string user_id;
    NextPingTimes next_pings;
    base::Time installation_time;
  };

  class Worker {
   public:
    static void Start(const base::FilePath& os_profile_reporting_data_path,
                      const std::string& legacy_user_id,
                      const ReportingData& local_state_reporting_data,
                      base::WeakPtr<StatsReporterImpl> stats_reporter);

   private:
    friend std::unique_ptr<Worker>::deleter_type;
    Worker(base::WeakPtr<StatsReporterImpl> stats_reporter);
    ~Worker();

    void Run(const base::FilePath& os_profile_reporting_data_path,
             const std::string& legacy_user_id,
             const ReportingData& local_state_reporting_data);
    void LoadUrlOnUIThread();
    void OnURLLoadComplete(std::unique_ptr<std::string> response_body);
    void Finish();

    base::File os_profile_reporting_data_file_;

    base::Value os_profile_reporting_data_json_;
    std::string user_id_;
    NextPingTimes new_next_pings_;

    std::unique_ptr<network::SimpleURLLoader> url_loader_;

    base::WeakPtr<StatsReporterImpl> stats_reporter_;
    scoped_refptr<base::SequencedTaskRunner> original_task_runner_;
  };

  static std::string GetUserIdFromLegacyStorage();
  static base::FilePath GetReportingDataFileDir();

  void Init();
  void OnLegacyUserIdGot(const std::string& legacy_user_id);
  void StartReporting();

  void OnWorkerDone(const std::string& user_id, NextPingTimes next_pings);
  void OnWorkerFailed(base::TimeDelta next_try_delay);

  std::string legacy_user_id_;

  net::BackoffEntry report_backoff_;
  base::OneShotTimer next_report_timer_;

  base::WeakPtrFactory<StatsReporterImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(StatsReporterImpl);
};

}  // namespace vivaldi

#endif  // BROWSER_STATS_REPORTER_IMPL_H_
