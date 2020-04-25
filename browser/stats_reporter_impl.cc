#include "browser/stats_reporter_impl.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chromium/chrome/browser/chrome_content_browser_client.h"
#include "chromium/ui/display/display.h"
#include "chromium/ui/display/screen.h"
#include "chromium/ui/gfx/geometry/size.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info_values.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/backoff_entry.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace vivaldi {

namespace {

enum class InstallationStatus {
  NEW_USER = 0,
  NEW_INSTALLATION,
  MOVED_STANDALONE,
  NORMAL
};

#ifdef NDEBUG
// Note: As long as the JS reporting code exist, we want this to execute after
// it on startup, to pick up its user id.
constexpr int kInitDelay = 10;       // minutes
constexpr int kLockDelay = 2;        // minutes
constexpr int kMatomoId = 36;        // Browser Usage - New implementation.
constexpr int kScheduleJitter = 15;  // minutes
#else
constexpr int kInitDelay = 2;       // minute
constexpr int kLockDelay = 0;       // minutes
constexpr int kMatomoId = 13;       // Blackhole
constexpr int kScheduleJitter = 1;  // minutes
#endif

constexpr base::FilePath::CharType kReportingDataFileName[] =
    FILE_PATH_LITERAL(".vivaldi_reporting_data");

constexpr char kUniqueUserIdKey[] = "unique_user_id";
constexpr char kNextDailyPingKey[] = "next_daily_ping";
constexpr char kNextWeeklyPingKey[] = "next_weekly_ping";
constexpr char kNextMonthlyPingKey[] = "next_monthly_ping";
constexpr char kInstallationTimeKey[] = "installation_time";

constexpr char kMatomoUrl[] =
    "https://update.vivaldi.com/rep/rep?installation_status=";
constexpr char kNewUser[] = "new_user";
constexpr char kNewInstallation[] = "new_installation";
constexpr char kMovedStandalone[] = "moved_standalone";
constexpr char kNormal[] = "normal";
constexpr char kWeekly[] = "&weekly";
constexpr char kMonthly[] = "&monthly";
constexpr char kDelayDays[] = "&delay_days=";
constexpr char kActionUrl[] = "http://localhost/";

constexpr char kReportRequest[] =
    "_cvar={\"1\":[\"cpu\",\"%s\"],\"2\":[\"v\",\"%s\"]}&"
    "action_name=%s&"
    "idsite=%u&"
    "rec=1&"
    "res=%ux%u&"
    "uid=%s&"
    "url=%s%s&"
    "installation_year=%u&"
    "installation_week=%u&"
    "earliest_installation_year=%u&"
    "earliest_installation_week=%u&"
    "ua=%s";

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    5000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 15,  // 15 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
    const GURL& url,
    const std::string& body) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("stats_reporter",
                                          R"(
        semantics {
          sender: "Vivaldi user count reporting"
          description:
            "This request is used by the Vivaldi stats reporter to assist with "
            "user counting. It includes a few useful information about the "
            "user's setup, which we collect statistics about"
          trigger:
            "This request is sent 10 minutes after the browser is started for "
            "a new user. It is then sent approximately every 24 hours "
            "afterwards, as long as the browser is running. If the browser is "
            "not running when the time for the next request comes, it will "
            "then be sent 10 minutes after the next browser start."
          data:
            "User counting metrics and some hardware data and software "
            "versions."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = "POST";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader->AttachStringForUpload(body, "application/x-www-form-urlencoded");

  url_loader->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  return url_loader;
}

std::pair<int, int> GetYearAndISOWeekNumber(base::Time time) {
  base::Time::Exploded time_exploded;
  time.LocalExplode(&time_exploded);

  // 4th of January must be in the first week of the year, by definition.
  base::Time::Exploded fourth_january_exploded = {0};
  fourth_january_exploded.year = time_exploded.year;
  fourth_january_exploded.month = 1;
  fourth_january_exploded.day_of_week = 0;
  fourth_january_exploded.day_of_month = 4;
  fourth_january_exploded.hour = 0;
  fourth_january_exploded.minute = 0;
  fourth_january_exploded.second = 0;
  fourth_january_exploded.millisecond = 0;

  base::Time fourth_january;
  if (!base::Time::FromLocalExploded(fourth_january_exploded, &fourth_january))
    return std::make_pair(0, 0);

  // Re-explode to get the actual day_of_week;
  base::Time::Exploded first_monday_of_year_exploded;
  fourth_january.LocalExplode(&first_monday_of_year_exploded);
  first_monday_of_year_exploded.hour = 0;
  first_monday_of_year_exploded.minute = 0;
  first_monday_of_year_exploded.second = 0;
  first_monday_of_year_exploded.day_of_month =
      first_monday_of_year_exploded.day_of_month -
      ((first_monday_of_year_exploded.day_of_week + 6) % 7);
  first_monday_of_year_exploded.day_of_week = 0;

  if (first_monday_of_year_exploded.day_of_month < 1) {
    // Monday of the first week of the year was previous year
    first_monday_of_year_exploded.day_of_month =
        31 + first_monday_of_year_exploded.day_of_month;
    first_monday_of_year_exploded.month = 12;
    first_monday_of_year_exploded.year -= 1;
  }

  base::Time first_monday_of_year;
  if (!base::Time::FromLocalExploded(first_monday_of_year_exploded,
                                     &first_monday_of_year))
    return std::make_pair(0, 0);

  return std::make_pair(time_exploded.year,
                        (time - first_monday_of_year).InDaysFloored() / 7 + 1);
}

base::Time ValidateTime(base::Time time, base::TimeDelta delay) {
  if (time > base::Time::Now() + delay) {
    return base::Time();
  } else {
    return time;
  }
}
}  // namespace

bool IsValidUserId(const std::string& user_id) {
  uint64_t value;
  return !user_id.empty() && base::HexStringToUInt64(user_id, &value) &&
         value > 0;
}

StatsReporterImpl::StatsReporterImpl()
    : report_backoff_(&kBackoffPolicy), weak_factory_(this) {
  next_report_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kInitDelay),
      base::BindOnce(&StatsReporterImpl::Init, base::Unretained(this)));
}

StatsReporterImpl::~StatsReporterImpl() = default;

void StatsReporterImpl::Init() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetUserIdFromLegacyStorage),
      base::BindOnce(&StatsReporterImpl::OnLegacyUserIdGot,
                     weak_factory_.GetWeakPtr()));
}

void StatsReporterImpl::OnLegacyUserIdGot(const std::string& legacy_user_id) {
  legacy_user_id_ = IsValidUserId(legacy_user_id) ? legacy_user_id : "";
  StartReporting();
}

void StatsReporterImpl::StartReporting() {
  PrefService* prefs = g_browser_process->local_state();
  ReportingData local_reporting_data;
  local_reporting_data.user_id =
      prefs->GetString(vivaldiprefs::kVivaldiUniqueUserId);
  if (!IsValidUserId(local_reporting_data.user_id))
    local_reporting_data.user_id.clear();

  // Allow an extra day of wiggle room, to make sure we only reset for good
  // reasons.
  local_reporting_data.next_pings.daily =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextDailyPing),
                   base::TimeDelta::FromDays(2));
  local_reporting_data.next_pings.weekly =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextWeeklyPing),
                   base::TimeDelta::FromDays(8));
  local_reporting_data.next_pings.monthly =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextMonthlyPing),
                   base::TimeDelta::FromDays(32));

  local_reporting_data.installation_time =
      base::Time::FromTimeT(prefs->GetInt64(metrics::prefs::kInstallDate));

  Worker::Start(GetReportingDataFileDir().Append(kReportingDataFileName),
                legacy_user_id_, local_reporting_data,
                weak_factory_.GetWeakPtr());
}

void StatsReporterImpl::OnWorkerFailed(base::TimeDelta delay) {
  if (delay.is_zero()) {
    report_backoff_.InformOfRequest(false);
    delay = report_backoff_.GetTimeUntilRelease();
  }

  next_report_timer_.Start(FROM_HERE, delay,
                           base::BindOnce(&StatsReporterImpl::StartReporting,
                                          base::Unretained(this)));
}

void StatsReporterImpl::OnWorkerDone(const std::string& user_id,
                                     NextPingTimes next_pings) {
  // We just reset it here, because we won't be sending anything new before
  // another day.
  report_backoff_.Reset();

  PrefService* prefs = g_browser_process->local_state();

  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextDailyPing, next_pings.daily);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextWeeklyPing, next_pings.weekly);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextMonthlyPing,
                 next_pings.monthly);

  if (!IsValidUserId(prefs->GetString(vivaldiprefs::kVivaldiUniqueUserId)))
    prefs->SetString(vivaldiprefs::kVivaldiUniqueUserId, user_id);

  next_report_timer_.Start(
      FROM_HERE,
      (next_pings.daily - base::Time::Now()) +
          base::TimeDelta::FromMicroseconds(
              base::RandDouble() *
              (kScheduleJitter * base::Time::kMicrosecondsPerMinute)),
      base::BindOnce(&StatsReporterImpl::StartReporting,
                     base::Unretained(this)));
}

// static
void StatsReporterImpl::Worker::Start(
    const base::FilePath& os_profile_reporting_data_path,
    const std::string& legacy_user_id,
    const ReportingData& local_state_reporting_data,
    base::WeakPtr<StatsReporterImpl> stats_reporter) {
  auto* worker = new Worker(stats_reporter);
  // The worker is in charge of cleaning itself up when done. If for any reason
  // the worker does not complete, due to a task not firing or other issues,
  // it will leak (small leak), but no new worker will be started.
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::LOWEST},
      base::BindOnce(&StatsReporterImpl::Worker::Run, base::Unretained(worker),
                     os_profile_reporting_data_path, legacy_user_id,
                     local_state_reporting_data));
}

StatsReporterImpl::Worker::Worker(
    base::WeakPtr<StatsReporterImpl> stats_reporter)
    : os_profile_reporting_data_json_(base::DictionaryValue()),
      stats_reporter_(stats_reporter),
      original_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

StatsReporterImpl::Worker::~Worker() = default;

void StatsReporterImpl::Worker::Run(
    const base::FilePath& os_profile_reporting_data_path,
    const std::string& legacy_user_id,
    const ReportingData& local_state_reporting_data) {
  std::unique_ptr<StatsReporterImpl::Worker> self_deleter(this);

  if (base::CreateDirectoryAndGetError(os_profile_reporting_data_path.DirName(),
                                       nullptr)) {
    os_profile_reporting_data_file_.Initialize(os_profile_reporting_data_path,
                                               base::File::FLAG_OPEN_ALWAYS |
                                                   base::File::FLAG_READ |
                                                   base::File::FLAG_WRITE);
  }

  std::string os_profile_reporting_data_file_contents;
  if (os_profile_reporting_data_file_.IsValid()) {
    base::File::Error lock_result = os_profile_reporting_data_file_.Lock();
    if (lock_result != base::File::FILE_OK) {
      original_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&StatsReporterImpl::OnWorkerFailed, stats_reporter_,
                         base::TimeDelta::FromMinutes(kLockDelay)));
      return;
    }

    int64_t length = os_profile_reporting_data_file_.GetLength();
    os_profile_reporting_data_file_contents.resize(length);
    os_profile_reporting_data_file_.Read(
        0, &os_profile_reporting_data_file_contents[0], length);
  }

  base::DictionaryValue* os_profile_reporting_data_dict = nullptr;
  ReportingData os_profile_reporting_data;
  if (!os_profile_reporting_data_file_contents.empty()) {
    auto os_profile_reporting_data_json =
        base::JSONReader::Read(os_profile_reporting_data_file_contents);
    if (os_profile_reporting_data_json &&
        os_profile_reporting_data_json->is_dict())
      os_profile_reporting_data_json_ =
          std::move(*os_profile_reporting_data_json);
  }

  if (!os_profile_reporting_data_json_.GetAsDictionary(
          &os_profile_reporting_data_dict))
    NOTREACHED();

  const std::string* unique_user_id =
      os_profile_reporting_data_dict->FindStringKey(kUniqueUserIdKey);
  if (unique_user_id && IsValidUserId(*unique_user_id))
    os_profile_reporting_data.user_id = *unique_user_id;

  const std::string* next_daily_ping_str =
      os_profile_reporting_data_dict->FindStringKey(kNextDailyPingKey);
  int64_t next_daily_ping;
  if (next_daily_ping_str &&
      base::StringToInt64(*next_daily_ping_str, &next_daily_ping))
    os_profile_reporting_data.next_pings.daily =
        ValidateTime(base::Time::FromDeltaSinceWindowsEpoch(
                         base::TimeDelta::FromMicroseconds(next_daily_ping)),
                     base::TimeDelta::FromDays(2));

  const std::string* next_weekly_ping_str =
      os_profile_reporting_data_dict->FindStringKey(kNextWeeklyPingKey);
  int64_t next_weekly_ping;
  if (next_weekly_ping_str &&
      base::StringToInt64(*next_weekly_ping_str, &next_weekly_ping))
    os_profile_reporting_data.next_pings.weekly =
        ValidateTime(base::Time::FromDeltaSinceWindowsEpoch(
                         base::TimeDelta::FromMicroseconds(next_weekly_ping)),
                     base::TimeDelta::FromDays(8));

  const std::string* next_monthly_ping_str =
      os_profile_reporting_data_dict->FindStringKey(kNextMonthlyPingKey);
  int64_t next_monthly_ping;
  if (next_monthly_ping_str &&
      base::StringToInt64(*next_monthly_ping_str, &next_monthly_ping))
    os_profile_reporting_data.next_pings.monthly =
        ValidateTime(base::Time::FromDeltaSinceWindowsEpoch(
                         base::TimeDelta::FromMicroseconds(next_monthly_ping)),
                     base::TimeDelta::FromDays(32));

  const std::string* installation_time_str =
      os_profile_reporting_data_dict->FindStringKey(kInstallationTimeKey);
  int64_t installation_time;
  if (installation_time_str &&
      base::StringToInt64(*installation_time_str, &installation_time))
    os_profile_reporting_data.installation_time =
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(installation_time));

  if (os_profile_reporting_data.user_id.empty() && !legacy_user_id.empty()) {
    os_profile_reporting_data.user_id = legacy_user_id;
    os_profile_reporting_data_dict->SetStringKey(kUniqueUserIdKey,
                                                 legacy_user_id);
  }

  InstallationStatus installation_status;

  if (local_state_reporting_data.user_id.empty()) {
    if (os_profile_reporting_data.user_id.empty()) {
      installation_status = InstallationStatus::NEW_USER;
    } else {
      installation_status = InstallationStatus::NEW_INSTALLATION;
    }
  } else if (os_profile_reporting_data.user_id !=
             local_state_reporting_data.user_id) {
    installation_status = InstallationStatus::MOVED_STANDALONE;
    // In this case, we definitely don't want to write to the file.
    os_profile_reporting_data_file_.Close();
  } else {
    installation_status = InstallationStatus::NORMAL;
  }

  NextPingTimes next_pings;

  switch (installation_status) {
    case InstallationStatus::NEW_USER:
      user_id_ = base::StringPrintf("%016" PRIX64, base::RandUint64());
      os_profile_reporting_data_dict->SetStringKey(kUniqueUserIdKey, user_id_);
      break;

    case InstallationStatus::MOVED_STANDALONE:
      user_id_ = local_state_reporting_data.user_id;

      // We ignore the user profile value in this case as they are potentially
      // for a different user.
      next_pings.daily = local_state_reporting_data.next_pings.daily;
      next_pings.weekly = local_state_reporting_data.next_pings.weekly;
      next_pings.monthly = local_state_reporting_data.next_pings.monthly;
      break;

    default:
      user_id_ = os_profile_reporting_data.user_id;

      // We keep track of the earliest installation time seen for this user
      if (!local_state_reporting_data.installation_time.is_null() &&
          (local_state_reporting_data.installation_time <
               os_profile_reporting_data.installation_time ||
           os_profile_reporting_data.installation_time.is_null())) {
        os_profile_reporting_data_dict->SetStringKey(
            kInstallationTimeKey,
            base::NumberToString(local_state_reporting_data.installation_time
                                     .ToDeltaSinceWindowsEpoch()
                                     .InMicroseconds()));
        os_profile_reporting_data.installation_time =
            local_state_reporting_data.installation_time;
      }

      next_pings.daily = std::max(local_state_reporting_data.next_pings.daily,
                                  os_profile_reporting_data.next_pings.daily);
      next_pings.weekly = std::max(local_state_reporting_data.next_pings.weekly,
                                   os_profile_reporting_data.next_pings.weekly);
      next_pings.monthly =
          std::max(local_state_reporting_data.next_pings.monthly,
                   os_profile_reporting_data.next_pings.monthly);
  }

  base::Time now = base::Time::Now();

  // Only report if we would report a daily ping. We want the weekly and monthly
  // pings to be done alongside with a daily ping.
  if (next_pings.daily > now) {
    original_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &StatsReporterImpl::OnWorkerFailed, stats_reporter_,
            (next_pings.daily - now) +
                base::TimeDelta::FromMicroseconds(
                    base::RandDouble() *
                    (kScheduleJitter * base::Time::kMicrosecondsPerMinute))));
    return;
  }

  int report_delay = 0;
  if (!next_pings.daily.is_null())
    report_delay = (now - local_state_reporting_data.next_pings.daily).InDays();

  std::string request_url = kMatomoUrl;
  switch (installation_status) {
    case InstallationStatus::NEW_USER:
      request_url += kNewUser;
      break;
    case InstallationStatus::NEW_INSTALLATION:
      request_url += kNewInstallation;
      break;
    case InstallationStatus::MOVED_STANDALONE:
      request_url += kMovedStandalone;
      break;
    case InstallationStatus::NORMAL:
      request_url += kNormal;
      break;
  }

  new_next_pings_ = next_pings;
  new_next_pings_.daily = now + base::TimeDelta::FromDays(1);

  if (next_pings.weekly <= now) {
    request_url += kWeekly;
    new_next_pings_.weekly = now + base::TimeDelta::FromDays(7);
  }
  if (next_pings.monthly <= now) {
    request_url += kMonthly;
    base::Time::Exploded time_exploded;
    now.LocalExplode(&time_exploded);
    time_exploded.month += 1;
    if (time_exploded.day_of_month > 28)
      time_exploded.day_of_month = 28;
    if (!base::Time::FromLocalExploded(time_exploded, &new_next_pings_.monthly))
      NOTREACHED();
  }
  if (report_delay > 1) {
    request_url += kDelayDays + base::NumberToString(report_delay);
  };

  std::string action_name =
      installation_status == InstallationStatus::NEW_USER ? "FirstRun" : "Ping";
  gfx::Size size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();

  auto installation_year_and_week =
      GetYearAndISOWeekNumber(local_state_reporting_data.installation_time);
  std::pair<int, int> earliest_installation_year_and_week(0, 0);
  if (installation_status != InstallationStatus::MOVED_STANDALONE)
    earliest_installation_year_and_week =
        GetYearAndISOWeekNumber(os_profile_reporting_data.installation_time);

  std::string body = base::StringPrintf(
      kReportRequest,
      net::EscapeUrlEncodedData(base::SysInfo::OperatingSystemArchitecture(),
                                true)
          .c_str(),
      net::EscapeUrlEncodedData(VIVALDI_UA_VERSION, true).c_str(),
      action_name.c_str(), kMatomoId, size.width(), size.height(),
      net::EscapeUrlEncodedData(user_id_, true).c_str(), kActionUrl,
      action_name.c_str(), installation_year_and_week.first,
      installation_year_and_week.second,
      earliest_installation_year_and_week.first,
      earliest_installation_year_and_week.second,
      net::EscapeUrlEncodedData(GetUserAgent(), true).c_str());

  url_loader_ = CreateURLLoader(GURL(request_url), body);

  original_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StatsReporterImpl::Worker::LoadUrlOnUIThread,
                                base::Unretained(this)));

  // Everything went fine. We want to stick around until the request is
  // complete.
  self_deleter.release();
}

void StatsReporterImpl::Worker::LoadUrlOnUIThread() {
  url_loader_->DownloadToString(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&StatsReporterImpl::Worker::OnURLLoadComplete,
                     base::Unretained(this)),
      1024);
}

void StatsReporterImpl::Worker::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::LOWEST},
                           base::BindOnce(&StatsReporterImpl::Worker::Finish,
                                          base::Unretained(this)));
}

void StatsReporterImpl::Worker::Finish() {
  std::unique_ptr<StatsReporterImpl::Worker> self_deleter(this);

  if (url_loader_->NetError() != net::OK) {
    original_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StatsReporterImpl::OnWorkerFailed,
                                  stats_reporter_, base::TimeDelta()));
    return;
  }

  if (os_profile_reporting_data_file_.IsValid()) {
    base::DictionaryValue* os_profile_reporting_data_dict;
    if (!os_profile_reporting_data_json_.GetAsDictionary(
            &os_profile_reporting_data_dict))
      NOTREACHED();

    os_profile_reporting_data_dict->SetStringKey(
        kNextDailyPingKey,
        base::NumberToString(
            new_next_pings_.daily.ToDeltaSinceWindowsEpoch().InMicroseconds()));

    os_profile_reporting_data_dict->SetStringKey(
        kNextWeeklyPingKey,
        base::NumberToString(new_next_pings_.weekly.ToDeltaSinceWindowsEpoch()
                                 .InMicroseconds()));

    os_profile_reporting_data_dict->SetStringKey(
        kNextMonthlyPingKey,
        base::NumberToString(new_next_pings_.monthly.ToDeltaSinceWindowsEpoch()
                                 .InMicroseconds()));

    std::string os_profile_reporting_data_file_contents;
    base::JSONWriter::Write(os_profile_reporting_data_json_,
                            &os_profile_reporting_data_file_contents);

    os_profile_reporting_data_file_.Write(
        0, os_profile_reporting_data_file_contents.c_str(),
        os_profile_reporting_data_file_contents.length());
  }

  original_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StatsReporterImpl::OnWorkerDone,
                                stats_reporter_, user_id_, new_next_pings_));
}

}  // namespace vivaldi