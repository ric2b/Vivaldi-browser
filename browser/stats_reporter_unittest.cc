// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/stats_reporter_impl.h"

#include "base/json/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vivaldi {
namespace {
#ifdef NDEBUG
constexpr char kRequestUrlStart[] =
    "https://update.vivaldi.com/rep/rep?ping_version=2&installation_status=";
#else
constexpr char kRequestUrlStart[] =
    "https://update.vivaldi.com/rep/"
    "rep?ping_version=2&debug&installation_status=";
#endif

constexpr char kVivaldiMockVersion[] = "99.9.999";
constexpr char kMockUserAgent[] = "My fake user agent";
constexpr char kMockClientHints[] = "{}";
constexpr char kMockArchitecture[] = "x64";
constexpr gfx::Size kMockScreenSize(1920, 1200);

// idsite is different in debug builds.
#ifdef NDEBUG
constexpr char kBodyStart[] =
    "rec=1&idsite=36&ua=My+fake+user+agent&uadata=%7B%7D&res=1920x1200&_cvar={"
    "\"1\":[\"cpu\",\"x64\"],\"2\":[\"v\",\"99.9.999\"]}&architecture=x64&"
    "version=99.9.999&screen_width=1920&screen_height=1200&";
#else
constexpr char kBodyStart[] =
    "rec=1&idsite=13&ua=My+fake+user+agent&uadata=%7B%7D&res=1920x1200&_cvar={"
    "\"1\":[\"cpu\",\"x64\"],\"2\":[\"v\",\"99.9.999\"]}&architecture=x64&"
    "version=99.9.999&screen_width=1920&screen_height=1200&";
#endif

void ExpectReportingDataMatch(
    const StatsReporterImpl::ReportingData& local_reporting_data,
    const base::Value& os_reporting_data) {
  const std::string* uid =
      os_reporting_data.GetDict().FindString("unique_user_id");
  EXPECT_TRUE(uid && *uid == local_reporting_data.user_id);
  std::optional<int> pings_since_last_month =
      os_reporting_data.GetDict().FindInt("pings_since_last_month");
  EXPECT_TRUE(pings_since_last_month &&
              *pings_since_last_month ==
                  local_reporting_data.pings_since_last_month);
  std::optional<base::Time> next_daily_ping =
      base::ValueToTime(os_reporting_data.GetDict().Find("next_daily_ping"));
  EXPECT_TRUE(next_daily_ping &&
              *next_daily_ping == local_reporting_data.next_pings.daily);
  std::optional<base::Time> next_weekly_ping =
      base::ValueToTime(os_reporting_data.GetDict().Find("next_weekly_ping"));
  EXPECT_TRUE(next_weekly_ping &&
              *next_weekly_ping == local_reporting_data.next_pings.weekly);
  std::optional<base::Time> next_monthly_ping =
      base::ValueToTime(os_reporting_data.GetDict().Find("next_monthly_ping"));
  EXPECT_TRUE(next_monthly_ping &&
              *next_monthly_ping == local_reporting_data.next_pings.monthly);
  std::optional<base::Time> next_trimestrial_ping = base::ValueToTime(
      os_reporting_data.GetDict().Find("next_trimestrial_ping"));
  EXPECT_TRUE(next_trimestrial_ping &&
              *next_trimestrial_ping ==
                  local_reporting_data.next_pings.trimestrial);
  std::optional<base::Time> next_semestrial_ping = base::ValueToTime(
      os_reporting_data.GetDict().Find("next_semestrial_ping"));
  EXPECT_TRUE(next_semestrial_ping &&
              *next_semestrial_ping ==
                  local_reporting_data.next_pings.semestrial);
  std::optional<base::Time> next_yearly_ping =
      base::ValueToTime(os_reporting_data.GetDict().Find("next_yearly_ping"));
  EXPECT_TRUE(next_yearly_ping &&
              *next_yearly_ping == local_reporting_data.next_pings.yearly);
}
}  // namespace

TEST(StatsReporterTest, FirstRunNormal) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 1;
  time_exploded.day_of_month = 1;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";
  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time;
  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  EXPECT_TRUE(
      StatsReporterImpl::IsValidUserId(local_state_reporting_data.user_id));
  std::string uid = local_state_reporting_data.user_id;
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) +
                "new_user&weekly&monthly&trimestrial&semestrial&yearly");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=FirstRun&url=http://localhost/FirstRun&"
                      "installation_year=2020&installation_week=53&earliest_"
                      "installation_year=2020&earliest_installation_week=53&"
                      "pings_since_last_month=0");
  EXPECT_EQ(next_reporting_time_interval, base::Minutes(10));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&extra_ping_number=1&extra_ping_delay=10");
  EXPECT_EQ(
      body,
      std::string(kBodyStart) + "uid=" + uid +
          "&action_name=FirstRun_10_min&url=http://localhost/FirstRun_10_min&"
          "installation_year=2020&installation_week=53&earliest_"
          "installation_year=2020&earliest_installation_week=53");
  EXPECT_EQ(next_reporting_time_interval, base::Minutes(50));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&extra_ping_number=2&extra_ping_delay=60");
  EXPECT_EQ(
      body,
      std::string(kBodyStart) + "uid=" + uid +
          "&action_name=FirstRun_60_min&url=http://localhost/FirstRun_60_min&"
          "installation_year=2020&installation_week=53&earliest_"
          "installation_year=2020&earliest_installation_week=53");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1) - base::Minutes(60));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=53&earliest_"
                      "installation_year=2020&earliest_installation_week=53");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
}

TEST(StatsReporterTest, RejectEarlyAttempt) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(5);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 2;
  local_state_reporting_data.next_pings.daily = time - base::Hours(2);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time + base::Days(23);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(84);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(176);
  local_state_reporting_data.next_pings.yearly = time + base::Days(360);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 2);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time",
      base::TimeToValue(local_state_reporting_data.installation_time));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.daily));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.monthly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=8&earliest_"
                      "installation_year=2021&earliest_installation_week=8");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Hours(7);
  EXPECT_FALSE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));
}

TEST(StatsReporterTest, NewInstallation) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time;

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 2);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time", base::TimeToValue(time - base::Days(5)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping", base::TimeToValue(time + base::Hours(2)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping", base::TimeToValue(time + base::Days(2)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping", base::TimeToValue(time + base::Days(23)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping", base::TimeToValue(time + base::Days(84)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping", base::TimeToValue(time + base::Days(176)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping", base::TimeToValue(time + base::Days(360)));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_FALSE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  EXPECT_EQ(next_reporting_time_interval, base::Hours(2));

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "new_installation");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=9&earliest_"
                      "installation_year=2021&earliest_installation_week=8");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=9&earliest_"
                      "installation_year=2021&earliest_installation_week=8");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
}

TEST(StatsReporterTest, MovedStandalone) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  std::string other_uid = "1234567890ab";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));
  DCHECK(StatsReporterImpl::IsValidUserId(other_uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(5);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 2;
  local_state_reporting_data.next_pings.daily = time - base::Hours(2);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time + base::Days(23);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(84);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(176);
  local_state_reporting_data.next_pings.yearly = time + base::Days(360);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", other_uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 6);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time", base::TimeToValue(time - base::Days(365)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping", base::TimeToValue(time + base::Hours(7)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping", base::TimeToValue(time + base::Days(5)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping", base::TimeToValue(time + base::Days(30)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping", base::TimeToValue(time + base::Days(90)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping", base::TimeToValue(time + base::Days(180)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping", base::TimeToValue(time + base::Days(360)));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  EXPECT_FALSE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "moved_standalone");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=8&earliest_"
                      "installation_year=0&earliest_installation_week=0");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
}

TEST(StatsReporterTest, UseLegacyUserId) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 1;
  time_exploded.day_of_month = 1;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "abc123def456";
  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time;
  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  EXPECT_EQ(local_state_reporting_data.user_id, legacy_user_id);
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(
      request_url,
      std::string(kRequestUrlStart) +
          "new_installation&weekly&monthly&trimestrial&semestrial&yearly");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + legacy_user_id +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=53&earliest_"
                      "installation_year=2020&earliest_installation_week=53&"
                      "pings_since_last_month=0");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));
  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + legacy_user_id +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=53&earliest_"
                      "installation_year=2020&earliest_installation_week=53");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
}

TEST(StatsReporterTest, IgnoreLegacyUserId) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "abc123def456";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(5);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 2;
  local_state_reporting_data.next_pings.daily = time - base::Hours(2);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time + base::Days(23);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(84);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(176);
  local_state_reporting_data.next_pings.yearly = time + base::Days(360);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 2);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time",
      base::TimeToValue(local_state_reporting_data.installation_time));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.daily));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.monthly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=8&earliest_"
                      "installation_year=2021&earliest_installation_week=8");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
}

TEST(StatsReporterTest, DifferentPingsSinceLastMonth) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(5);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 6;
  local_state_reporting_data.next_pings.daily = time - base::Hours(2);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time + base::Days(23);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(84);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(176);
  local_state_reporting_data.next_pings.yearly = time + base::Days(360);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 7);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time", base::TimeToValue(time - base::Days(365)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.daily));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.monthly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=8&earliest_"
                      "installation_year=2020&earliest_installation_week=9");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
  EXPECT_EQ(local_state_reporting_data.pings_since_last_month, 8);
}

TEST(StatsReporterTest, OsMonthlyPingAhead) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 3;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 10;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(5);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 10;
  local_state_reporting_data.next_pings.daily = time - base::Days(1);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time - base::Days(1);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(84);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(176);
  local_state_reporting_data.next_pings.yearly = time + base::Days(360);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 0);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time", base::TimeToValue(time - base::Days(365)));
  os_profile_reporting_data_json->GetDict().Set("next_daily_ping",
                                                base::TimeToValue(time));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping", base::TimeToValue(time + base::Days(30)));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2021&installation_week=8&earliest_"
                      "installation_year=2020&earliest_installation_week=9");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
  EXPECT_EQ(local_state_reporting_data.pings_since_last_month, 1);
}

TEST(StatsReporterTest, LongRun) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2020;
  time_exploded.month = 12;
  time_exploded.day_of_month = 30;
  time_exploded.hour = 17;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(365);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 20;
  local_state_reporting_data.next_pings.daily = time - base::Minutes(1);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time + base::Days(5);
  local_state_reporting_data.next_pings.trimestrial = time + base::Days(5);
  local_state_reporting_data.next_pings.semestrial = time + base::Days(5);
  local_state_reporting_data.next_pings.yearly = time + base::Days(5);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 20);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time",
      base::TimeToValue(local_state_reporting_data.installation_time));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.daily));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.monthly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(3);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) + "normal&weekly&delay_days=2");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(2);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) +
                "normal&monthly&trimestrial&semestrial&yearly&delay_days=1");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=22");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1) + base::Hours(23);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(4) - base::Hours(6);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) + "normal&weekly&delay_days=2");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(25);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) + "normal&weekly&delay_days=24");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal&monthly");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=3");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(58);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&weekly&monthly&delay_days=57");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=0");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal&trimestrial");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(90);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&weekly&monthly&delay_days=89");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url,
            std::string(kRequestUrlStart) + "normal&trimestrial&semestrial");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(91);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&weekly&monthly&delay_days=90");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) + "normal&trimestrial");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(91);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&weekly&monthly&delay_days=90");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1&"
                      "pings_since_last_month=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);

  time += base::Days(1) + base::Hours(2);

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&trimestrial&semestrial&yearly");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=1&earliest_"
                      "installation_year=2020&earliest_installation_week=1");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
}

TEST(StatsReporterTest, CorrectMonthSemestrialPing) {
  base::Time time;
  base::Time::Exploded time_exploded = {0};
  time_exploded.year = 2021;
  time_exploded.month = 6;
  time_exploded.day_of_month = 1;
  time_exploded.hour = 15;
  if (!base::Time::FromLocalExploded(time_exploded, &time))
    NOTREACHED();
  std::string legacy_user_id = "";

  std::string uid = "1a2b3c4d5e6f7";
  DCHECK(StatsReporterImpl::IsValidUserId(uid));

  StatsReporterImpl::ReportingData local_state_reporting_data;
  local_state_reporting_data.installation_time = time - base::Days(500);
  local_state_reporting_data.user_id = uid;
  local_state_reporting_data.pings_since_last_month = 10;
  local_state_reporting_data.next_pings.daily = time - base::Hours(1);
  local_state_reporting_data.next_pings.weekly = time + base::Days(2);
  local_state_reporting_data.next_pings.monthly = time - base::Hours(1);
  local_state_reporting_data.next_pings.trimestrial = time - base::Hours(1);
  local_state_reporting_data.next_pings.semestrial = time - base::Hours(1);
  local_state_reporting_data.next_pings.yearly = time + base::Days(50);

  std::optional<base::Value> os_profile_reporting_data_json(
      base::Value::Type::DICT);
  os_profile_reporting_data_json->GetDict().Set("unique_user_id", uid);
  os_profile_reporting_data_json->GetDict().Set("pings_since_last_month", 10);
  os_profile_reporting_data_json->GetDict().Set(
      "installation_time",
      base::TimeToValue(local_state_reporting_data.installation_time));
  os_profile_reporting_data_json->GetDict().Set(
      "next_daily_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.daily));
  os_profile_reporting_data_json->GetDict().Set(
      "next_weekly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.weekly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_monthly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.monthly));
  os_profile_reporting_data_json->GetDict().Set(
      "next_trimestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.trimestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_semestrial_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.semestrial));
  os_profile_reporting_data_json->GetDict().Set(
      "next_yearly_ping",
      base::TimeToValue(local_state_reporting_data.next_pings.yearly));

  std::string request_url;
  std::string body;
  base::TimeDelta next_reporting_time_interval;

  ASSERT_TRUE(StatsReporterImpl::GeneratePingRequest(
      time, legacy_user_id, kMockScreenSize, kMockArchitecture,
      kVivaldiMockVersion, kMockUserAgent, kMockClientHints,
      local_state_reporting_data, os_profile_reporting_data_json, request_url,
      body, next_reporting_time_interval));

  ASSERT_TRUE(os_profile_reporting_data_json);

  EXPECT_EQ(request_url, std::string(kRequestUrlStart) +
                             "normal&monthly&trimestrial&semestrial");
  EXPECT_EQ(body, std::string(kBodyStart) + "uid=" + uid +
                      "&action_name=Ping&url=http://localhost/Ping&"
                      "installation_year=2020&installation_week=3&earliest_"
                      "installation_year=2020&earliest_installation_week=3&"
                      "pings_since_last_month=10");
  EXPECT_EQ(next_reporting_time_interval, base::Days(1));
  ExpectReportingDataMatch(local_state_reporting_data,
                           *os_profile_reporting_data_json);
  base::Time::Exploded exploded;
  local_state_reporting_data.next_pings.trimestrial.LocalExplode(&exploded);
  EXPECT_EQ(exploded.month, 9);
  local_state_reporting_data.next_pings.semestrial.LocalExplode(&exploded);
  EXPECT_EQ(exploded.month, 12);
}

}  // namespace vivaldi