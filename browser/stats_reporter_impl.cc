#include "browser/stats_reporter_impl.h"

#include <inttypes.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/vivaldi_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
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
constexpr base::TimeDelta kLockDelay = base::Minutes(2);
constexpr int kMatomoId = 36;        // Browser Usage - New implementation.
constexpr int kScheduleJitter = 15;  // minutes
#else
constexpr base::TimeDelta kLockDelay = base::Seconds(1);
constexpr int kMatomoId = 13;       // Blackhole
constexpr int kScheduleJitter = 1;  // minutes
#endif

constexpr int kExtraPingCount = 2;
constexpr int kExtraPingDelays[kExtraPingCount] = {10, 50};  // minutes

constexpr base::FilePath::CharType kReportingDataFileName[] =
    FILE_PATH_LITERAL(".vivaldi_reporting_data");

constexpr char kUniqueUserIdKey[] = "unique_user_id";
constexpr char kNextDailyPingKey[] = "next_daily_ping";
constexpr char kNextWeeklyPingKey[] = "next_weekly_ping";
constexpr char kNextMonthlyPingKey[] = "next_monthly_ping";
constexpr char kNextTrimestrialPingKey[] = "next_trimestrial_ping";
constexpr char kNextSemestrialPingKey[] = "next_semestrial_ping";
constexpr char kNextYearlyPingKey[] = "next_yearly_ping";
constexpr char kInstallationTimeKey[] = "installation_time";
constexpr char kPingsSinceLastMonthKey[] = "pings_since_last_month";
constexpr char kDescriptionKey[] = "description";
constexpr char kDescriptionText[] =
    "This file contains data used for counting users. If you are worried about "
    "privacy implications, please see "
    "https://help.vivaldi.com/article/how-we-count-our-users/";

constexpr char kPingUrl[] = "https://update.vivaldi.com/rep/rep";

constexpr char kPingUrlParams[] =
    "?"
    // Ping version can be increased to help the server-side to deal with format
    // changes.
    "ping_version=2"
#ifndef NDEBUG
    "&debug"
#endif
    "&installation_status=";
constexpr char kNewUser[] = "new_user";
constexpr char kNewInstallation[] = "new_installation";
constexpr char kMovedStandalone[] = "moved_standalone";
constexpr char kNormal[] = "normal";
constexpr char kWeekly[] = "&weekly";
constexpr char kMonthly[] = "&monthly";
constexpr char kTrimestrial[] = "&trimestrial";
constexpr char kSemestrial[] = "&semestrial";
constexpr char kYearly[] = "&yearly";
constexpr char kDelayDays[] = "&delay_days=";
constexpr char kExtraPingNumber[] = "&extra_ping_number=";
constexpr char kExtraPingDelay[] = "&extra_ping_delay=";
constexpr char kActionUrl[] = "http://localhost/";

// These intervals are used to determine if a delay should be reset for being
// higher than should be possible. Allow an extra day of wiggle room, to make
// sure we only reset for good reasons.
constexpr base::TimeDelta kMaxDailyPingDelay = base::Days(2);
constexpr base::TimeDelta kMaxWeeklyPingDelay = base::Days(8);
constexpr base::TimeDelta kMaxMonthlyPingDelay = base::Days(32);
// Any period of three months will have at most two months with 31 days and
// one months with 30, which makes 92 days.
constexpr base::TimeDelta kMaxTrimestrialPingDelay = base::Days(93);
// Any period of 6 months will have at most four months with 31 days and two
// months with 30, which makes 184 days.
constexpr base::TimeDelta kMaxSemestrialPingDelay = base::Days(185);
constexpr base::TimeDelta kMaxYearlyPingDelay = base::Days(367);

constexpr char kReportRequest[] =
    "rec=1&"
    "idsite=%u&"
    "ua=%s&"
    "uadata=%s&"
    "res=%ux%u&"
    "_cvar={\"1\":[\"cpu\",\"%s\"],\"2\":[\"v\",\"%s\"]}&"
    // The four following items are redundant with _cvar and res. The goal is to
    // get rid of _cvar and res when We switch to the new stats reporting
    // solution completely
    "architecture=%s&"
    "version=%s&"
    "screen_width=%u&"
    "screen_height=%u&"
    "uid=%s&"
    "action_name=%s&"
    "url=%s%s&"
    "installation_year=%u&"
    "installation_week=%u&"
    "earliest_installation_year=%u&"
    "earliest_installation_week=%u";

constexpr char kPingsSinceLastMonth[] = "&pings_since_last_month=%u";

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
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader->AttachStringForUpload(body, "application/x-www-form-urlencoded");

  url_loader->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  return url_loader;
}

// Implemented based on
// https://en.wikipedia.org/wiki/ISO_week_date#Weeks_per_year
int weeks_in_year(int year) {
  auto p = [](int y) { return (y + (y / 4) - (y / 100) + (y / 400)) % 7; };

  if (p(year) == 4 || p(year - 1) == 3)
    return 53;
  return 52;
}

// Implemented based on
// https://en.wikipedia.org/wiki/ISO_week_date#Calculating_the_week_number_from_a_month_and_day_of_the_month_or_ordinal_date
std::pair<int, int> GetYearAndISOWeekNumber(base::Time time) {
  base::Time::Exploded time_exploded;
  time.LocalExplode(&time_exploded);

  if (!time_exploded.HasValidValues())
    return std::make_pair(0, 0);

  // 4th of January must be in the first week of the year, by definition.
  base::Time::Exploded first_january_exploded = {0};
  first_january_exploded.year = time_exploded.year;
  first_january_exploded.month = 1;
  first_january_exploded.day_of_month = 1;
  base::Time first_january;
  if (!base::Time::FromLocalExploded(first_january_exploded, &first_january))
    NOTREACHED();

  int year = time_exploded.year;
  int day_of_year = (time - first_january).InDays();
  DCHECK(day_of_year >= 0);
  int day_of_week = time_exploded.day_of_week;
  if (day_of_week == 0)
    day_of_week = 7;
  int week_of_year = (11 + day_of_year - day_of_week) / 7;

  if (week_of_year == 0)
    return std::make_pair(year - 1, weeks_in_year(year - 1));

  if (week_of_year == 53 && weeks_in_year(year) == 52)
    return std::make_pair(year + 1, 1);

  return std::make_pair(year, week_of_year);
}

base::Time ValidateTime(std::optional<base::Time> time,
                        base::Time now,
                        base::TimeDelta delay) {
  if (!time || *time > now + delay) {
    return base::Time();
  } else {
    return *time;
  }
}

std::optional<StatsReporterImpl::FileAndContent> LockAndReadFile(
    base::FilePath path) {
  // This should only return a fail state (nullopt) if we fail to get a lock
  // as that indicates there is another lock holder, and a ping is probably
  // being sent from another vivaldi instance. In all other cases, we prefer to
  // proceed with only the LocalState prefs rather than risking being stuck and
  // unable to send pings while waiting on an issue that may not solve itself.
  StatsReporterImpl::FileAndContent result;
  if (base::CreateDirectoryAndGetError(path.DirName(), nullptr)) {
    result.file.Initialize(path, base::File::FLAG_OPEN_ALWAYS |
                                     base::File::FLAG_READ |
                                     base::File::FLAG_WRITE);
  }

  if (!result.file.IsValid()) {
    LOG(ERROR) << "Failed to open " << path;
    return result;
  }
  if (result.file.Lock(base::File::LockMode::kExclusive) != base::File::FILE_OK)
    return std::nullopt;

  int64_t length = result.file.GetLength();
  result.content.resize(length);
  if (result.file.Read(0, &(result.content[0]), length) != length) {
    LOG(ERROR) << "Failed reading content of " << path;
    result.content.clear();
  }

  return result;
}
}  // namespace

StatsReporterImpl::FileHolder::FileHolder(base::File file)
    : file_(std::move(file)) {}

StatsReporterImpl::FileHolder::~FileHolder() {
  if (IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce([](base::File file) { file.Close(); },
                       std::move(file_)));
  }
}

StatsReporterImpl::FileHolder::FileHolder(FileHolder&& other) = default;
StatsReporterImpl::FileHolder& StatsReporterImpl::FileHolder::operator=(
    FileHolder&& other) = default;

base::File StatsReporterImpl::FileHolder::release() {
  return std::move(file_);
}

bool StatsReporterImpl::FileHolder::IsValid() {
  return file_.IsValid();
}

StatsReporterImpl::StatsReporterImpl() : report_backoff_(&kBackoffPolicy) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetUserIdFromLegacyStorage),
      base::BindOnce(&StatsReporterImpl::OnLegacyUserIdGot,
                     weak_factory_.GetWeakPtr()));
}

StatsReporterImpl::~StatsReporterImpl() = default;

void StatsReporterImpl::OnLegacyUserIdGot(const std::string& legacy_user_id) {
  legacy_user_id_ = IsValidUserId(legacy_user_id) ? legacy_user_id : "";
  StartReporting();
}

void StatsReporterImpl::StartReporting() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::LOWEST},
      base::BindOnce(&LockAndReadFile,
                     GetReportingDataFileDir().Append(kReportingDataFileName)),
      base::BindOnce(&StatsReporterImpl::OnOSStatFileRead,
                     weak_factory_.GetWeakPtr()));
}

/*static*/
bool StatsReporterImpl::IsValidUserId(const std::string& user_id) {
  uint64_t value;
  return !user_id.empty() && base::HexStringToUInt64(user_id, &value) &&
         value > 0;
}

/*static*/
bool StatsReporterImpl::GeneratePingRequest(
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
    base::TimeDelta& next_reporting_time_interval) {
  ReportingData os_profile_reporting_data;
  {
    std::string* unique_user_id =
        os_profile_reporting_data_json->GetDict().FindString(kUniqueUserIdKey);
    if (unique_user_id && IsValidUserId(*unique_user_id))
      os_profile_reporting_data.user_id = *unique_user_id;

    os_profile_reporting_data.next_pings.daily = ValidateTime(
        base::ValueToTime(
            os_profile_reporting_data_json->GetDict().Find(kNextDailyPingKey)),
        now, kMaxDailyPingDelay);

    os_profile_reporting_data.next_pings.weekly = ValidateTime(
        base::ValueToTime(
            os_profile_reporting_data_json->GetDict().Find(kNextWeeklyPingKey)),
        now, kMaxWeeklyPingDelay);

    os_profile_reporting_data.next_pings.monthly = ValidateTime(
        base::ValueToTime(os_profile_reporting_data_json->GetDict().Find(
            kNextMonthlyPingKey)),
        now, kMaxMonthlyPingDelay);

    os_profile_reporting_data.next_pings.trimestrial = ValidateTime(
        base::ValueToTime(os_profile_reporting_data_json->GetDict().Find(
            kNextTrimestrialPingKey)),
        now, kMaxTrimestrialPingDelay);

    os_profile_reporting_data.next_pings.semestrial = ValidateTime(
        base::ValueToTime(os_profile_reporting_data_json->GetDict().Find(
            kNextSemestrialPingKey)),
        now, kMaxSemestrialPingDelay);

    os_profile_reporting_data.next_pings.yearly = ValidateTime(
        base::ValueToTime(
            os_profile_reporting_data_json->GetDict().Find(kNextYearlyPingKey)),
        now, kMaxYearlyPingDelay);

    std::optional<base::Time> installation_time = base::ValueToTime(
        os_profile_reporting_data_json->GetDict().Find(kInstallationTimeKey));
    if (installation_time)
      os_profile_reporting_data.installation_time = *installation_time;

    if (os_profile_reporting_data.user_id.empty() && !legacy_user_id.empty()) {
      os_profile_reporting_data.user_id = legacy_user_id;
      os_profile_reporting_data_json->GetDict().Set(kUniqueUserIdKey,
                                                    legacy_user_id);
    }

    std::optional<int> pings_since_last_month =
        os_profile_reporting_data_json->GetDict().FindInt(
            kPingsSinceLastMonthKey);
    if (pings_since_last_month)
      os_profile_reporting_data.pings_since_last_month =
          *pings_since_last_month;
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
    os_profile_reporting_data_json.reset();
  } else {
    installation_status = InstallationStatus::NORMAL;
  }

  NextPingTimes next_pings;
  std::string user_id;
  int pings_since_last_month = 0;

  switch (installation_status) {
    case InstallationStatus::NEW_USER:
      user_id = base::StringPrintf("%016" PRIX64, base::RandUint64());
      os_profile_reporting_data_json->GetDict().Set(kUniqueUserIdKey, user_id);
      os_profile_reporting_data_json->GetDict().Set(
          kInstallationTimeKey,
          base::TimeToValue(local_state_reporting_data.installation_time));
      os_profile_reporting_data.installation_time =
          local_state_reporting_data.installation_time;
      break;

    case InstallationStatus::MOVED_STANDALONE:
      user_id = local_state_reporting_data.user_id;

      // We ignore the user profile value in this case as they are potentially
      // for a different user.
      next_pings = local_state_reporting_data.next_pings;
      pings_since_last_month =
          local_state_reporting_data.pings_since_last_month;
      break;

    default:
      user_id = os_profile_reporting_data.user_id;

      // We keep track of the earliest installation time seen for this user
      if (!local_state_reporting_data.installation_time.is_null() &&
          (local_state_reporting_data.installation_time <
               os_profile_reporting_data.installation_time ||
           os_profile_reporting_data.installation_time.is_null())) {
        os_profile_reporting_data_json->GetDict().Set(
            kInstallationTimeKey,
            base::TimeToValue(local_state_reporting_data.installation_time));
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
      next_pings.trimestrial =
          std::max(local_state_reporting_data.next_pings.trimestrial,
                   os_profile_reporting_data.next_pings.trimestrial);
      next_pings.semestrial =
          std::max(local_state_reporting_data.next_pings.semestrial,
                   os_profile_reporting_data.next_pings.semestrial);
      next_pings.yearly = std::max(local_state_reporting_data.next_pings.yearly,
                                   os_profile_reporting_data.next_pings.yearly);

      if (local_state_reporting_data.next_pings.monthly >
          os_profile_reporting_data.next_pings.monthly) {
        pings_since_last_month =
            local_state_reporting_data.pings_since_last_month;
      } else if (local_state_reporting_data.next_pings.monthly <
                 os_profile_reporting_data.next_pings.monthly) {
        pings_since_last_month =
            os_profile_reporting_data.pings_since_last_month;
      } else {
        pings_since_last_month =
            std::max(local_state_reporting_data.pings_since_last_month,
                     os_profile_reporting_data.pings_since_last_month);
      }
  }

  // If it's time for the next daily ping, drop any pending extra pings.
  bool do_extra_ping = local_state_reporting_data.next_extra_ping != 0 &&
                       local_state_reporting_data.next_extra_ping_time <= now &&
                       next_pings.daily > now;

  int next_extra_ping = 0;
  if (installation_status == InstallationStatus::NEW_USER) {
    next_extra_ping = 1;
  } else if (do_extra_ping) {
    next_extra_ping = local_state_reporting_data.next_extra_ping + 1;
    if (next_extra_ping > kExtraPingCount)
      next_extra_ping = 0;
  }

  // Only report if we would report a daily ping. We want the weekly and
  // monthly pings to be done alongside with a daily ping.
  if (next_pings.daily > now && !do_extra_ping) {
    next_reporting_time_interval = next_pings.daily - now;
    return false;
  }

  int report_delay = 0;
  if (!next_pings.daily.is_null())
    report_delay = (now - next_pings.daily).InDays();

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (!version_info::IsOfficialBuild() &&
      cmd_line.HasSwitch(switches::kOverrideStatsReporterPingUrl))
    request_url =
        cmd_line.GetSwitchValueASCII(switches::kOverrideStatsReporterPingUrl);
  else
    request_url = kPingUrl;
  request_url.append(kPingUrlParams);
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

  NextPingTimes new_next_pings = next_pings;
  int new_pings_since_last_month = pings_since_last_month;
  if (!do_extra_ping) {
    new_next_pings.daily = now + base::Days(1);
    new_pings_since_last_month++;
  }

  if (next_pings.weekly <= now) {
    request_url += kWeekly;
    new_next_pings.weekly = now + base::Days(7);
  }
  if (next_pings.monthly <= now) {
    new_pings_since_last_month = 0;
    request_url += kMonthly;
    base::Time::Exploded time_exploded;
    now.LocalExplode(&time_exploded);
    if (time_exploded.month != 12) {
      time_exploded.month += 1;
    } else {
      time_exploded.month = 1;
      time_exploded.year += 1;
    }
    if (time_exploded.day_of_month > 28)
      time_exploded.day_of_month = 28;
    if (!base::Time::FromLocalExploded(time_exploded, &new_next_pings.monthly))
      NOTREACHED();
  }
  if (next_pings.trimestrial <= now) {
    request_url += kTrimestrial;
    base::Time::Exploded time_exploded;
    now.LocalExplode(&time_exploded);
    // Months are 1-based.
    int new_month = (time_exploded.month + 2) % 12 + 1;
    if (new_month < time_exploded.month)
      time_exploded.year += 1;
    time_exploded.month = new_month;
    if (time_exploded.day_of_month > 28)
      time_exploded.day_of_month = 28;
    if (!base::Time::FromLocalExploded(time_exploded,
                                       &new_next_pings.trimestrial))
      NOTREACHED();
  }
  if (next_pings.semestrial <= now) {
    request_url += kSemestrial;
    base::Time::Exploded time_exploded;
    now.LocalExplode(&time_exploded);
    // Months are 1-based.
    int new_month = (time_exploded.month + 5) % 12 + 1;
    if (new_month < time_exploded.month)
      time_exploded.year += 1;
    time_exploded.month = new_month;
    if (time_exploded.day_of_month > 28)
      time_exploded.day_of_month = 28;
    if (!base::Time::FromLocalExploded(time_exploded,
                                       &new_next_pings.semestrial))
      NOTREACHED();
  }
  if (next_pings.yearly <= now) {
    request_url += kYearly;
    base::Time::Exploded time_exploded;
    now.LocalExplode(&time_exploded);
    time_exploded.year += 1;
    if (time_exploded.day_of_month == 29 && time_exploded.month == 2)
      time_exploded.day_of_month = 28;
    if (!base::Time::FromLocalExploded(time_exploded, &new_next_pings.yearly))
      NOTREACHED();
  }
  if (report_delay > 0) {
    request_url += kDelayDays + base::NumberToString(report_delay);
  };

  std::string action_name;
  if (installation_status == InstallationStatus::NEW_USER) {
    action_name = "FirstRun";
  } else if (do_extra_ping) {
    int total_extra_ping_delay = 0;
    for (int i = 0; i < local_state_reporting_data.next_extra_ping; i++)
      total_extra_ping_delay += kExtraPingDelays[i];
    request_url +=
        kExtraPingNumber +
        base::NumberToString(local_state_reporting_data.next_extra_ping) +
        kExtraPingDelay + base::NumberToString(total_extra_ping_delay);
    action_name =
        "FirstRun_" + base::NumberToString(total_extra_ping_delay) + "_min";
  } else {
    action_name = "Ping";
  }

  auto installation_year_and_week =
      GetYearAndISOWeekNumber(local_state_reporting_data.installation_time);
  std::pair<int, int> earliest_installation_year_and_week(0, 0);
  if (installation_status != InstallationStatus::MOVED_STANDALONE)
    earliest_installation_year_and_week =
        GetYearAndISOWeekNumber(os_profile_reporting_data.installation_time);

  local_state_reporting_data.user_id = user_id;
  local_state_reporting_data.next_pings = new_next_pings;
  local_state_reporting_data.next_extra_ping = next_extra_ping;
  local_state_reporting_data.next_extra_ping_time =
      now + next_reporting_time_interval;
  local_state_reporting_data.pings_since_last_month =
      new_pings_since_last_month;

  if (os_profile_reporting_data_json) {
    os_profile_reporting_data_json->GetDict().Set(kDescriptionKey,
                                                  kDescriptionText);

    os_profile_reporting_data_json->GetDict().Set(
        kNextDailyPingKey, base::TimeToValue(new_next_pings.daily));
    os_profile_reporting_data_json->GetDict().Set(
        kNextWeeklyPingKey, base::TimeToValue(new_next_pings.weekly));
    os_profile_reporting_data_json->GetDict().Set(
        kNextMonthlyPingKey, base::TimeToValue(new_next_pings.monthly));
    os_profile_reporting_data_json->GetDict().Set(
        kNextTrimestrialPingKey, base::TimeToValue(new_next_pings.trimestrial));
    os_profile_reporting_data_json->GetDict().Set(
        kNextSemestrialPingKey, base::TimeToValue(new_next_pings.semestrial));
    os_profile_reporting_data_json->GetDict().Set(
        kNextYearlyPingKey, base::TimeToValue(new_next_pings.yearly));
    os_profile_reporting_data_json->GetDict().Set(kPingsSinceLastMonthKey,
                                                  new_pings_since_last_month);
  }

  body = base::StringPrintf(
      kReportRequest, kMatomoId,
      base::EscapeUrlEncodedData(user_agent, true).c_str(),
      base::EscapeUrlEncodedData(client_hints, true).c_str(),
      display_size.width(), display_size.height(),
      base::EscapeUrlEncodedData(architecture, true).c_str(),
      base::EscapeUrlEncodedData(vivaldi_version, true).c_str(),
      base::EscapeUrlEncodedData(architecture, true).c_str(),
      base::EscapeUrlEncodedData(vivaldi_version, true).c_str(),
      display_size.width(), display_size.height(),
      base::EscapeUrlEncodedData(user_id, true).c_str(), action_name.c_str(),
      kActionUrl, action_name.c_str(), installation_year_and_week.first,
      installation_year_and_week.second,
      earliest_installation_year_and_week.first,
      earliest_installation_year_and_week.second);

  if (next_pings.monthly <= now) {
    body += base::StringPrintf(kPingsSinceLastMonth, pings_since_last_month);
  }

  if (next_extra_ping) {
    next_reporting_time_interval =
        base::Minutes(kExtraPingDelays[next_extra_ping - 1]);
  } else {
    next_reporting_time_interval = new_next_pings.daily - now;
  }
  return true;
}

void StatsReporterImpl::OnOSStatFileRead(
    std::optional<FileAndContent> file_and_content) {
  if (!file_and_content) {
    ScheduleNextReporting(kLockDelay, false);
    return;
  }

  DoReporting(FileHolder(std::move(file_and_content->file)),
              std::move(file_and_content->content));
}

void StatsReporterImpl::DoReporting(FileHolder os_profile_reporting_data_file,
                                    std::string os_profile_reporting_data) {
  PrefService* prefs = GetLocalState();
  ReportingData local_state_reporting_data;
  local_state_reporting_data.user_id =
      prefs->GetString(vivaldiprefs::kVivaldiUniqueUserId);
  if (!IsValidUserId(local_state_reporting_data.user_id))
    local_state_reporting_data.user_id.clear();

  base::Time now = base::Time::Now();

  // Allow an extra day of wiggle room, to make sure we only reset for good
  // reasons.
  local_state_reporting_data.next_pings.daily =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextDailyPing),
                   now, kMaxDailyPingDelay);
  local_state_reporting_data.next_pings.weekly =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextWeeklyPing),
                   now, kMaxWeeklyPingDelay);
  local_state_reporting_data.next_pings.monthly =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextMonthlyPing),
                   now, kMaxMonthlyPingDelay);
  local_state_reporting_data.next_pings.trimestrial = ValidateTime(
      prefs->GetTime(vivaldiprefs::kVivaldiStatsNextTrimestrialPing), now,
      kMaxTrimestrialPingDelay);
  local_state_reporting_data.next_pings.semestrial = ValidateTime(
      prefs->GetTime(vivaldiprefs::kVivaldiStatsNextSemestrialPing), now,
      kMaxSemestrialPingDelay);
  local_state_reporting_data.next_pings.yearly =
      ValidateTime(prefs->GetTime(vivaldiprefs::kVivaldiStatsNextYearlyPing),
                   now, kMaxYearlyPingDelay);

  local_state_reporting_data.installation_time =
      base::Time::FromTimeT(prefs->GetInt64(metrics::prefs::kInstallDate));

  local_state_reporting_data.next_extra_ping =
      prefs->GetInteger(vivaldiprefs::kVivaldiStatsExtraPing);
  local_state_reporting_data.next_extra_ping_time =
      prefs->GetTime(vivaldiprefs::kVivaldiStatsExtraPingTime);
  local_state_reporting_data.pings_since_last_month =
      prefs->GetInteger(vivaldiprefs::kVivaldiStatsPingsSinceLastMonth);

  std::optional<base::Value> os_profile_reporting_data_json;
  if (!os_profile_reporting_data.empty()) {
    std::string_view fixed_os_profile_reporting_data(
        os_profile_reporting_data);
    os_profile_reporting_data_json =
        base::JSONReader::Read(fixed_os_profile_reporting_data);

    while (!os_profile_reporting_data_json) {
      fixed_os_profile_reporting_data.remove_suffix(1);
      // There was a bug where the file would not be truncated before overwrite,
      // causing some data to be potentially left over after the last brace.
      size_t last_brace = fixed_os_profile_reporting_data.rfind('}');
      if (last_brace == std::string_view::npos)
        break;

      fixed_os_profile_reporting_data =
          fixed_os_profile_reporting_data.substr(0, last_brace + 1);
      os_profile_reporting_data_json =
          base::JSONReader::Read(fixed_os_profile_reporting_data);
    }
  }
  if (!os_profile_reporting_data_json ||
      !os_profile_reporting_data_json->is_dict()) {
    os_profile_reporting_data_json.emplace(base::Value::Type::DICT);
  }

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  // Screen info should only be missing if we reach this too early in the
  // startup process.
  DCHECK(display::Screen::GetScreen());
  if (!GeneratePingRequest(
          now, legacy_user_id_,
          display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
          base::SysInfo::OperatingSystemArchitecture(), VIVALDI_UA_VERSION,
          GetUserAgent(), GetClientHints(), local_state_reporting_data,
          os_profile_reporting_data_json, request_url, body,
          next_reporting_time_interval)) {
    ScheduleNextReporting(next_reporting_time_interval, true);
    return;
  }

  url_loader_ = CreateURLLoader(GURL(request_url), body);

  // Unretained is safe because the callback is destroyed when the url_loader_
  // (which we own) is destroyed
  url_loader_->DownloadToString(
      GetUrlLoaderFactory().get(),
      base::BindOnce(&StatsReporterImpl::OnURLLoadComplete,
                     base::Unretained(this),
                     std::move(os_profile_reporting_data_file),
                     std::move(local_state_reporting_data),
                     std::move(os_profile_reporting_data_json),
                     next_reporting_time_interval),
      1024);
}

void StatsReporterImpl::OnURLLoadComplete(
    FileHolder os_profile_reporting_data_file,
    ReportingData local_state_reporting_data,
    std::optional<base::Value> os_profile_reporting_data_json,
    base::TimeDelta next_reporting_time_interval_,
    std::unique_ptr<std::string> response_body) {
  bool success = url_loader_->NetError() == net::OK;
  url_loader_.reset();

  if (!success) {
    report_backoff_.InformOfRequest(false);
    ScheduleNextReporting(report_backoff_.GetTimeUntilRelease(), false);
    return;
  }

  if (os_profile_reporting_data_file.IsValid() &&
      os_profile_reporting_data_json) {
    std::string os_profile_reporting_data_file_contents;
    base::JSONWriter::Write(*os_profile_reporting_data_json,
                            &os_profile_reporting_data_file_contents);

    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(
            [](base::File file, std::string content) {
              // Clear existing content
              file.SetLength(0);
              file.Write(0, content.c_str(), content.length());
              file.Close();
            },
            os_profile_reporting_data_file.release(),
            std::move(os_profile_reporting_data_file_contents)));
  }

  DCHECK_GE(local_state_reporting_data.next_extra_ping, 0);
  DCHECK_LE(local_state_reporting_data.next_extra_ping, kExtraPingCount);
  // We just reset it here, because we won't be sending anything new before
  // another day.
  report_backoff_.Reset();

  PrefService* prefs = GetLocalState();

  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextDailyPing,
                 local_state_reporting_data.next_pings.daily);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextWeeklyPing,
                 local_state_reporting_data.next_pings.weekly);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextMonthlyPing,
                 local_state_reporting_data.next_pings.monthly);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextTrimestrialPing,
                 local_state_reporting_data.next_pings.trimestrial);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextSemestrialPing,
                 local_state_reporting_data.next_pings.semestrial);
  prefs->SetTime(vivaldiprefs::kVivaldiStatsNextYearlyPing,
                 local_state_reporting_data.next_pings.yearly);

  if (local_state_reporting_data.next_extra_ping) {
    prefs->SetInteger(vivaldiprefs::kVivaldiStatsExtraPing,
                      local_state_reporting_data.next_extra_ping);
    prefs->SetTime(vivaldiprefs::kVivaldiStatsExtraPingTime,
                   local_state_reporting_data.next_extra_ping_time);
  } else {
    prefs->ClearPref(vivaldiprefs::kVivaldiStatsExtraPing);
    prefs->ClearPref(vivaldiprefs::kVivaldiStatsExtraPingTime);
  }

  prefs->SetInteger(vivaldiprefs::kVivaldiStatsPingsSinceLastMonth,
                    local_state_reporting_data.pings_since_last_month);

  if (!IsValidUserId(prefs->GetString(vivaldiprefs::kVivaldiUniqueUserId)))
    prefs->SetString(vivaldiprefs::kVivaldiUniqueUserId,
                     local_state_reporting_data.user_id);

  ScheduleNextReporting(next_reporting_time_interval_, true);
  DCHECK(!os_profile_reporting_data_file.IsValid() ||
         !os_profile_reporting_data_json);
}

void StatsReporterImpl::ScheduleNextReporting(base::TimeDelta delay,
                                              bool add_jitter) {
  DCHECK(!delay.is_zero());

  if (add_jitter) {
    delay += base::Microseconds(
        base::RandDouble() *
        (kScheduleJitter * base::Time::kMicrosecondsPerMinute));
  }

  // Unretained is safe because the callback is destroyed when the
  // next_report_timer_ (which we own) is destroyed
  next_report_timer_.Start(FROM_HERE, delay,
                           base::BindOnce(&StatsReporterImpl::StartReporting,
                                          base::Unretained(this)));
}

}  // namespace vivaldi