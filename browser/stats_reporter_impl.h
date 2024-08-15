// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef BROWSER_STATS_REPORTER_IMPL_H_
#define BROWSER_STATS_REPORTER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "browser/stats_reporter.h"
#include "net/base/backoff_entry.h"
#include "ui/display/screen.h"

class PrefService;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace vivaldi {

class StatsReporterImpl : public StatsReporter {
 public:
  struct NextPingTimes {
    base::Time daily;
    base::Time weekly;
    base::Time monthly;
    base::Time trimestrial;
    base::Time semestrial;
    base::Time yearly;
  };

  struct ReportingData {
    std::string user_id;
    NextPingTimes next_pings;
    base::Time installation_time;
    int next_extra_ping = 0;
    base::Time next_extra_ping_time;
    int pings_since_last_month = 0;
  };

  struct FileAndContent {
    base::File file;
    std::string content;
  };

  StatsReporterImpl();

  StatsReporterImpl(const StatsReporterImpl&) = delete;
  StatsReporterImpl& operator=(const StatsReporterImpl&) = delete;

  ~StatsReporterImpl() override;

  static bool GeneratePingRequest(
      base::Time now,
      const std::string& legacy_user_id,
      const gfx::Size& display_size,
      const std::string& architecture,
      const std::string& vivaldi_version,
      const std::string& user_agent,
      const std::string& client_hints,
      ReportingData& local_state_reporting_data,
      std::optional<base::Value>& os_profile_reporting_data_json,
      std::string& request_url,
      std::string& body,
      base::TimeDelta& next_reporting_time_interval);

  static bool IsValidUserId(const std::string& user_id);

 private:
  class FileHolder {
   public:
    explicit FileHolder(base::File file);
    ~FileHolder();
    FileHolder(FileHolder&& other);
    FileHolder& operator=(FileHolder&& other);

    base::File release();
    bool IsValid();

   private:
    base::File file_;
  };

  static std::string GetUserIdFromLegacyStorage();
  static base::FilePath GetReportingDataFileDir();
  static PrefService* GetLocalState();
  static scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();
  static std::string GetUserAgent();
  static std::string GetClientHints();

  void OnLegacyUserIdGot(const std::string& legacy_user_id);
  void StartReporting();

  void OnOSStatFileRead(std::optional<FileAndContent> file_and_content);
  void DoReporting(FileHolder os_profile_reporting_data_file,
                   std::string os_profile_reporting_data);
  void OnURLLoadComplete(
      FileHolder os_profile_reporting_data_file,
      ReportingData local_state_reporting_data,
      std::optional<base::Value> os_profile_reporting_data_json,
      base::TimeDelta next_reporting_time_interval_,
      std::unique_ptr<std::string> response_body);
  void ScheduleNextReporting(base::TimeDelta next_try_delay, bool add_jitter);

  std::string legacy_user_id_;

  net::BackoffEntry report_backoff_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OneShotTimer next_report_timer_;

  base::WeakPtrFactory<StatsReporterImpl> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // BROWSER_STATS_REPORTER_IMPL_H_
