// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_source_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/ad_blocker/adblock_ruleset_file_parser.h"
#include "components/ad_blocker/ddg_rules_parser.h"
#include "components/ad_blocker/parse_result.h"
#include "components/ad_blocker/utils.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace adblock_filter {

namespace {
constexpr int kMinTimeBetweenUpdates = 1;   // hours
constexpr int kMaxTimeBetweenUpdates = 14;  // days
constexpr int kUpdateTimeJitter = 30;       // minutes
constexpr int kInitialUpdateDelay = 1;      // minutes

constexpr char kTrackerInfoFileSuffix[] = "_tracker_infos.json";

base::Time CalculateNextUpdateTime(const ActiveRuleSource& source) {
  return source.last_update +
         std::min(std::max(source.unsafe_adblock_metadata.expires,
                           base::Hours(kMinTimeBetweenUpdates)),
                  base::Days(kMaxTimeBetweenUpdates)) +
         base::Minutes(base::RandDouble() * kUpdateTimeJitter);
}

base::Time GetNextUpdateTimeAfterFailUpdate(base::Time last_update_time) {
  return last_update_time + base::Hours(kMinTimeBetweenUpdates) +
         base::Minutes(base::RandDouble() * kUpdateTimeJitter);
}

void ParseContent(const std::string& file_contents,
                  RuleSourceSettings source_settings,
                  ParseResult* parse_result) {
  JSONStringValueDeserializer serializer(file_contents);
  std::unique_ptr<base::Value> root(serializer.Deserialize(nullptr, nullptr));
  if (root.get()) {
    DuckDuckGoRulesParser(parse_result).Parse(*root);
    return;
  }

  RulesetFileParser(parse_result, source_settings).Parse(file_contents);
  return;
}

void LoadTrackerInfos(
    const base::FilePath& tracker_infos_path,
    base::OnceCallback<void(std::optional<base::Value::Dict>)> callback) {
  JSONFileValueDeserializer serializer(tracker_infos_path);
  std::unique_ptr<base::Value> tracker_infos(
      serializer.Deserialize(nullptr, nullptr));
  if (!tracker_infos || !tracker_infos->is_dict())
    std::move(callback).Run(std::nullopt);
  else
    std::move(callback).Run(std::move(tracker_infos->GetDict()));
}

}  // namespace

struct RuleSourceHandler::RulesReadResult {
 public:
  RulesReadResult() = default;
  ~RulesReadResult() = default;
  RulesReadResult(RulesReadResult&&) = default;
  RulesReadResult& operator=(RulesReadResult&&) = default;

  AdBlockMetadata metadata;
  FetchResult fetch_result = FetchResult::kSuccess;
  RulesInfo rules_info;
  std::string checksum;
  std::optional<base::Value::Dict> tracker_infos;
};

/*static*/
RuleSourceHandler::RulesReadResult RuleSourceHandler::ReadRules(
    const base::FilePath& source_path,
    const base::FilePath& output_path,
    const base::FilePath& tracker_info_output_path,
    RulesCompiler rules_compiler,
    RuleSourceSettings source_settings,
    bool delete_after_read) {
  RulesReadResult read_result;
  if (!base::PathExists(source_path)) {
    read_result.fetch_result = FetchResult::kFileNotFound;
    return read_result;
  }

  std::string file_contents;
  if (!base::ReadFileToString(source_path, &file_contents)) {
    read_result.fetch_result = FetchResult::kFileReadError;
    return read_result;
  }
  ParseResult parse_result;
  ParseContent(file_contents, source_settings, &parse_result);
  read_result.fetch_result = parse_result.fetch_result;
  read_result.metadata = parse_result.metadata;
  read_result.rules_info = parse_result.rules_info;
  if (parse_result.tracker_infos) {
    JSONFileValueSerializer serializer(tracker_info_output_path);
    // Missing the tracker infos isn't critical. If we fail at saving it,
    // just act as if we didn't get it.
    if (serializer.Serialize(*parse_result.tracker_infos))
      read_result.tracker_infos = std::move(parse_result.tracker_infos);
  }

  if (read_result.fetch_result == FetchResult::kFileUnsupported) {
    // If the file used to have supported rules in a previous version, our
    // compiled copy of it is now obsolete, remove it.
    base::DeleteFile(output_path);
    // We want to return an empty checksum and expect it hasn't been set to
    // anything by this point.
    CHECK(read_result.checksum.empty());
  }

  if (delete_after_read) {
    base::DeleteFile(source_path);
  }

  if (read_result.fetch_result != FetchResult::kSuccess)
    return read_result;

  if (!rules_compiler.Run(parse_result, output_path, read_result.checksum))
    read_result.fetch_result = FetchResult::kFailedSavingParsedRules;

  return read_result;
}

RuleSourceHandler::RuleSourceHandler(
    RuleGroup group,
    ActiveRuleSource rule_source,
    const base::FilePath& profile_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    RulesCompiler rules_compiler,
    OnUpdateCallback on_update_callback,
    OnTrackerInfosUpdateCallback on_tracker_infos_update_callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      rules_compiler_(rules_compiler),
      on_update_callback_(on_update_callback),
      on_tracker_infos_update_callback_(on_tracker_infos_update_callback),
      rule_source_(rule_source),
      group_(group),
      rules_list_path_(
          profile_path.Append(GetRulesFolderName())
              .Append(GetGroupFolderName(group))
              .AppendASCII(base::NumberToString(rule_source_.core.id()))),
      tracker_infos_path_(
          profile_path.Append(GetRulesFolderName())
              .Append(GetGroupFolderName(group))
              .AppendASCII(base::NumberToString(rule_source_.core.id()) +
                           kTrackerInfoFileSuffix)),
      file_task_runner_(file_task_runner),
      weak_factory_(this) {
  if (rule_source_.next_fetch == base::Time()) {
    rule_source_.next_fetch = CalculateNextUpdateTime(rule_source_);
  }

  if (rule_source_.has_tracker_infos) {
    file_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&LoadTrackerInfos, tracker_infos_path_,
                       base::BindOnce(&RuleSourceHandler::OnTrackerInfosLoaded,
                                      weak_factory_.GetWeakPtr())));
  }

  StartUpdateTimer();
}

RuleSourceHandler::~RuleSourceHandler() = default;

void RuleSourceHandler::OnTrackerInfosLoaded(
    std::optional<base::Value::Dict> tracker_infos) {
  if (tracker_infos) {
    on_tracker_infos_update_callback_.Run(group_, rule_source_,
                                          std::move(*tracker_infos));
  }
}

void RuleSourceHandler::FetchNow() {
  // We already have an update in progress.
  if (!update_timer_.IsRunning())
    return;

  update_timer_.FireNow();
}

void RuleSourceHandler::Clear() {
  // We'll probably get deleted soon at this point, but it's worth
  // making sure the file won't get re-created by then.
  update_timer_.Stop();

  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::GetDeleteFileCallback(rules_list_path_)));
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::GetDeleteFileCallback(tracker_infos_path_)));
}

void RuleSourceHandler::StartUpdateTimer() {
  update_timer_.Start(
      FROM_HERE,
      rule_source_.next_fetch > base::Time::Now()
          ? rule_source_.next_fetch - base::Time::Now()
          : base::Minutes(kInitialUpdateDelay) +
                base::Minutes(base::RandDouble() * kUpdateTimeJitter),
      base::BindOnce(&RuleSourceHandler::DoFetch, base::Unretained(this)));
}

void RuleSourceHandler::DoFetch() {
  rule_source_.is_fetching = true;
  on_update_callback_.Run(this);

  if (rule_source_.core.is_from_url())
    DownloadRules();
  else
    ReadRulesFromFile(rule_source_.core.source_file(), false);
}

void RuleSourceHandler::DownloadRules() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = rule_source_.core.source_url();
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // See
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_adblock_rules", R"(
        semantics {
          sender: "Vivaldi Adblock Rules"
          description: "Download new or updated rules in the EasyList/AdBlock format."
          trigger: "Triggered when a new list is added or when an existing list is about to be out of date."
          data: "Adblock filter list in one of the Adblock format variants"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the ad blcoker settings."
          chrome_policy {
            }
          }
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToTempFile(
      url_loader_factory_.get(),
      base::BindOnce(&RuleSourceHandler::OnRulesDownloaded,
                     base::Unretained(this)));
}

void RuleSourceHandler::OnRulesDownloaded(base::FilePath file) {
  std::unique_ptr<network::SimpleURLLoader> url_loader;
  url_loader_.swap(url_loader);

  if (file.empty()) {
    LOG(WARNING) << "Downloading rule source:" << rule_source_.core.source_url()
                 << " failed with error " << url_loader->NetError();

    rule_source_.is_fetching = false;
    rule_source_.last_fetch_result = FetchResult::kDownloadFailed;
    rule_source_.next_fetch =
        GetNextUpdateTimeAfterFailUpdate(base::Time::Now());
    StartUpdateTimer();
    on_update_callback_.Run(this);
    return;
  }

  ReadRulesFromFile(file, true);
}

void RuleSourceHandler::ReadRulesFromFile(const base::FilePath& file,
                                          bool delete_after_read) {
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RuleSourceHandler::ReadRules, file, rules_list_path_,
                     tracker_infos_path_, rules_compiler_,
                     rule_source_.core.settings(), delete_after_read),
      base::BindOnce(&RuleSourceHandler::OnRulesRead,
                     weak_factory_.GetWeakPtr()));
}

void RuleSourceHandler::OnRulesRead(RulesReadResult result) {
  rule_source_.last_fetch_result = result.fetch_result;
  rule_source_.is_fetching = false;
  if (rule_source_.last_fetch_result == FetchResult::kSuccess ||
      rule_source_.last_fetch_result == FetchResult::kFileUnsupported) {
    rule_source_.unsafe_adblock_metadata = result.metadata;
    rule_source_.rules_info = result.rules_info;
    rule_source_.rules_list_checksum = result.checksum;
    rule_source_.last_update = base::Time::Now();

    rule_source_.next_fetch = CalculateNextUpdateTime(rule_source_);

    if (result.tracker_infos) {
      rule_source_.has_tracker_infos = true;
      on_tracker_infos_update_callback_.Run(group_, rule_source_,
                                            std::move(*result.tracker_infos));
    }
  } else {
    rule_source_.next_fetch =
        GetNextUpdateTimeAfterFailUpdate(base::Time::Now());
  }

  StartUpdateTimer();
  on_update_callback_.Run(this);
}
}  // namespace adblock_filter
