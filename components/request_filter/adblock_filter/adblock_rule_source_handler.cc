// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_source_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/request_filter/adblock_filter/adblock_ruleset_file_parser.h"
#include "components/request_filter/adblock_filter/ddg_rules_parser.h"
#include "components/request_filter/adblock_filter/parse_result.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

using FlatStringOffset = FlatOffset<flatbuffers::String>;
using FlatStringListOffset = FlatVectorOffset<flatbuffers::String>;

struct OffsetVectorCompare {
  bool operator()(const std::vector<FlatStringOffset>& a,
                  const std::vector<FlatStringOffset>& b) const {
    auto compare = [](const FlatStringOffset a_offset,
                      const FlatStringOffset b_offset) {
      DCHECK(!a_offset.IsNull());
      DCHECK(!b_offset.IsNull());
      return a_offset.o < b_offset.o;
    };
    // |lexicographical_compare| is how vector::operator< is implemented.
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                        compare);
  }
};
using FlatDomainMap = std::map<std::vector<FlatStringOffset>,
                               FlatStringListOffset,
                               OffsetVectorCompare>;

namespace adblock_filter {

namespace {
constexpr int kMinTimeBetweenUpdates = 6;   // hours
constexpr int kMaxTimeBetweenUpdates = 14;  // days
constexpr int kUpdateTimeJitter = 30;       // minutes
constexpr int kInitialUpdateDelay = 1;      // minutes

constexpr char kTrackerInfoFileSuffix[] = "_tracker_infos.json";

base::Time CalculateNextUpdateTime(const RuleSource& source) {
  return source.last_update +
         std::min(std::max(source.unsafe_adblock_metadata.expires,
                           base::TimeDelta::FromHours(kMinTimeBetweenUpdates)),
                  base::TimeDelta::FromDays(kMaxTimeBetweenUpdates)) +
         base::TimeDelta::FromMinutes(base::RandDouble() * kUpdateTimeJitter);
}

base::Time GetNextUpdateTimeAfterFailUpdate(base::Time last_update_time) {
  return last_update_time + base::TimeDelta::FromHours(kMinTimeBetweenUpdates) +
         base::TimeDelta::FromMinutes(base::RandDouble() * kUpdateTimeJitter);
}

template <typename T>
FlatStringListOffset SerializeDomainList(
    flatbuffers::FlatBufferBuilder* builder,
    const T& container,
    FlatDomainMap* domain_map) {
  if (container.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> domains;
  domains.reserve(container.size());
  for (const std::string& str : container)
    domains.push_back(builder->CreateSharedString(str));

  auto precedes = [&builder](FlatStringOffset lhs, FlatStringOffset rhs) {
    return CompareDomains(
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, lhs)),
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, rhs))) <
           0;
  };
  if (domains.empty())
    return FlatStringListOffset();
  std::sort(domains.begin(), domains.end(), precedes);

  // Share domain lists if we've already serialized an exact duplicate. Note
  // that this can share excluded and included domain lists.
  DCHECK(domain_map);
  auto it = domain_map->find(domains);
  if (it == domain_map->end()) {
    auto offset = builder->CreateVector(domains);
    (*domain_map)[domains] = offset;
    return offset;
  }
  return it->second;
}

uint8_t OptionsFromFilterRule(const FilterRule& filter_rule) {
  uint8_t options = 0;
  if (filter_rule.party.test(FilterRule::kFirstParty))
    options |= flat::OptionFlag_FIRST_PARTY;
  if (filter_rule.party.test(FilterRule::kThirdParty))
    options |= flat::OptionFlag_THIRD_PARTY;
  if (filter_rule.is_allow_rule)
    options |= flat::OptionFlag_IS_ALLOW_RULE;
  if (filter_rule.is_case_sensitive)
    options |= flat::OptionFlag_IS_CASE_SENSITIVE;
  if (filter_rule.is_csp_rule)
    options |= flat::OptionFlag_IS_CSP_RULE;
  return options;
}

uint16_t ResourceTypesFromFilterRule(const FilterRule& filter_rule) {
  uint16_t resource_types = 0;
  if (filter_rule.resource_types.test(FilterRule::kStylesheet))
    resource_types |= flat::ResourceType_STYLESHEET;
  if (filter_rule.resource_types.test(FilterRule::kImage))
    resource_types |= flat::ResourceType_IMAGE;
  if (filter_rule.resource_types.test(FilterRule::kObject))
    resource_types |= flat::ResourceType_OBJECT;
  if (filter_rule.resource_types.test(FilterRule::kScript))
    resource_types |= flat::ResourceType_SCRIPT;
  if (filter_rule.resource_types.test(FilterRule::kXmlHttpRequest))
    resource_types |= flat::ResourceType_XMLHTTPREQUEST;
  if (filter_rule.resource_types.test(FilterRule::kSubDocument))
    resource_types |= flat::ResourceType_SUBDOCUMENT;
  if (filter_rule.resource_types.test(FilterRule::kFont))
    resource_types |= flat::ResourceType_FONT;
  if (filter_rule.resource_types.test(FilterRule::kMedia))
    resource_types |= flat::ResourceType_MEDIA;
  if (filter_rule.resource_types.test(FilterRule::kWebSocket))
    resource_types |= flat::ResourceType_WEBSOCKET;
  if (filter_rule.resource_types.test(FilterRule::kWebRTC))
    resource_types |= flat::ResourceType_WEBRTC;
  if (filter_rule.resource_types.test(FilterRule::kPing))
    resource_types |= flat::ResourceType_PING;
  if (filter_rule.resource_types.test(FilterRule::kOther))
    resource_types |= flat::ResourceType_OTHER;
  return resource_types;
}

uint8_t ActivationTypesFromFilterRule(const FilterRule& filter_rule) {
  uint8_t activation_types = 0;
  if (filter_rule.activation_types.test(FilterRule::kPopup))
    activation_types |= flat::ActivationType_POPUP;
  if (filter_rule.activation_types.test(FilterRule::kDocument))
    activation_types |= flat::ActivationType_DOCUMENT;
  if (filter_rule.activation_types.test(FilterRule::kElementHide))
    activation_types |= flat::ActivationType_ELEMENT_HIDE;
  if (filter_rule.activation_types.test(FilterRule::kGenericHide))
    activation_types |= flat::ActivationType_GENERIC_HIDE;
  if (filter_rule.activation_types.test(FilterRule::kGenericBlock))
    activation_types |= flat::ActivationType_GENERIC_BLOCK;
  return activation_types;
}

flat::PatternType PatternTypeFromFilterRule(const FilterRule& filter_rule) {
  switch (filter_rule.pattern_type) {
    case FilterRule::kPlain:
      return flat::PatternType_PLAIN;
    case FilterRule::kWildcarded:
      return flat::PatternType_WILDCARDED;
    case FilterRule::kRegex:
      return flat::PatternType_REGEXP;
  }
}

uint8_t AnchorTypeFromFilterRule(const FilterRule& filter_rule) {
  uint8_t anchor_type = 0;
  if (filter_rule.anchor_type.test(FilterRule::kAnchorStart))
    anchor_type |= flat::AnchorType_START;
  if (filter_rule.anchor_type.test(FilterRule::kAnchorEnd))
    anchor_type |= flat::AnchorType_END;
  if (filter_rule.anchor_type.test(FilterRule::kAnchorHost))
    anchor_type |= flat::AnchorType_HOST;
  return anchor_type;
}

void ParseContent(const std::string& file_contents, ParseResult* parse_result) {
  JSONStringValueDeserializer serializer(file_contents);
  std::unique_ptr<base::Value> root(serializer.Deserialize(nullptr, nullptr));
  if (root.get()) {
    DuckDuckGoRulesParser(parse_result).Parse(*root);
    return;
  }

  RulesetFileParser(parse_result).Parse(file_contents);
  return;
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const FilterRule& filter_rule,
    std::vector<FlatOffset<flat::FilterRule>>* filter_rules_offsets,
    FlatDomainMap* domain_map) {
  FlatStringListOffset domains_included_offset =
      SerializeDomainList(builder, filter_rule.included_domains, domain_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeDomainList(builder, filter_rule.excluded_domains, domain_map);

  FlatStringOffset pattern_offset =
      builder->CreateSharedString(filter_rule.pattern);

  FlatStringOffset ngram_search_string_offset =
      builder->CreateSharedString(filter_rule.ngram_search_string);

  FlatStringOffset host_offset = builder->CreateSharedString(filter_rule.host);
  FlatStringOffset redirect_offset =
      builder->CreateSharedString(filter_rule.redirect);
  FlatStringOffset csp_offset = builder->CreateSharedString(filter_rule.csp);

  filter_rules_offsets->push_back(flat::CreateFilterRule(
      *builder, OptionsFromFilterRule(filter_rule),
      ResourceTypesFromFilterRule(filter_rule),
      ActivationTypesFromFilterRule(filter_rule),
      PatternTypeFromFilterRule(filter_rule),
      AnchorTypeFromFilterRule(filter_rule), host_offset,
      domains_included_offset, domains_excluded_offset, redirect_offset,
      csp_offset, pattern_offset, ngram_search_string_offset));
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const CosmeticRule& cosmetic_rule,
    std::vector<FlatOffset<flat::CosmeticRule>>* cosmetic_rules_offsets,
    FlatDomainMap* domain_map) {
  FlatStringListOffset domains_included_offset =
      SerializeDomainList(builder, cosmetic_rule.included_domains, domain_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeDomainList(builder, cosmetic_rule.excluded_domains, domain_map);
  FlatStringOffset selector_offset =
      builder->CreateSharedString(cosmetic_rule.selector);
  cosmetic_rules_offsets->push_back(flat::CreateCosmeticRule(
      *builder, cosmetic_rule.is_allow_rule, domains_included_offset,
      domains_excluded_offset, selector_offset));
}

bool SaveRulesList(const base::FilePath& output_path,
                   base::span<const uint8_t> data,
                   std::string* checksum) {
  if (!base::CreateDirectory(output_path.DirName()))
    return false;

  base::File output_file(
      output_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!output_file.IsValid())
    return false;

  // Write the version header.
  std::string version_header = GetRulesListVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (output_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return false;
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return false;
  int data_size = static_cast<int>(data.size());
  if (output_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                    data_size) != data_size) {
    return false;
  }

  *checksum = CalculateBufferChecksum(data);

  return true;
}

void LoadTrackerInfos(const base::FilePath& tracker_infos_path,
                      base::OnceCallback<void(base::Value)> callback) {
  JSONFileValueDeserializer serializer(tracker_infos_path);
  std::unique_ptr<base::Value> tracker_infos(
      serializer.Deserialize(nullptr, nullptr));
  if (!tracker_infos)
    std::move(callback).Run(base::Value());
  else
    std::move(callback).Run(std::move(*tracker_infos));
}

}  // namespace

struct RuleSourceHandler::RulesReadResult {
 public:
  RulesReadResult() = default;
  ~RulesReadResult() = default;

  AdBlockMetadata metadata;
  FetchResult fetch_result = FetchResult::kSuccess;
  RulesInfo rules_info;
  std::string checksum;
  base::Value tracker_infos;

  DISALLOW_COPY_AND_ASSIGN(RulesReadResult);
};

class RuleSourceHandler::RulesReader {
 public:
  static void Start(
      const base::FilePath& source_path,
      const base::FilePath& output_path,
      const base::FilePath& tracker_info_output_path,
      bool delete_after_read,
      base::OnceCallback<void(std::unique_ptr<RulesReadResult>)> callback);

  void Read(RulesReadResult* result);

 private:
  RulesReader(const base::FilePath& source_path,
              const base::FilePath& output_path,
              const base::FilePath& tracker_info_output_path,
              bool delete_after_read);
  ~RulesReader();

  base::FilePath source_path_;
  base::FilePath output_path_;
  base::FilePath tracker_info_output_path_;
  bool delete_after_read_;
  DISALLOW_COPY_AND_ASSIGN(RulesReader);
};

void RuleSourceHandler::RulesReader::Start(
    const base::FilePath& source_path,
    const base::FilePath& output_path,
    const base::FilePath& tracker_info_output_path,
    bool delete_after_read,
    base::OnceCallback<void(std::unique_ptr<RulesReadResult>)> callback) {
  std::unique_ptr<RulesReadResult> read_result =
      std::make_unique<RulesReadResult>();
  RulesReader(source_path, output_path, tracker_info_output_path,
              delete_after_read)
      .Read(read_result.get());

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(callback), std::move(read_result)));
}

RuleSourceHandler::RulesReader::RulesReader(
    const base::FilePath& source_path,
    const base::FilePath& output_path,
    const base::FilePath& tracker_info_output_path,
    bool delete_after_read)
    : source_path_(source_path),
      output_path_(output_path),
      tracker_info_output_path_(tracker_info_output_path),
      delete_after_read_(delete_after_read) {}

RuleSourceHandler::RulesReader::~RulesReader() = default;

void RuleSourceHandler::RulesReader::Read(RulesReadResult* read_result) {
  if (!base::PathExists(source_path_)) {
    read_result->fetch_result = FetchResult::kFileNotFound;
    return;
  }

  std::string file_contents;
  if (!base::ReadFileToString(source_path_, &file_contents)) {
    read_result->fetch_result = FetchResult::kFileReadError;
    return;
  }
  ParseResult parse_result;
  ParseContent(file_contents, &parse_result);
  read_result->fetch_result = parse_result.fetch_result;
  read_result->metadata = parse_result.metadata;
  read_result->rules_info = parse_result.rules_info;
  if (parse_result.tracker_infos.is_dict()) {
    JSONFileValueSerializer serializer(tracker_info_output_path_);
    // Missing the tracker infos isn't critical. If we fail at saving it,
    // just act as if we didn't get it.
    if (serializer.Serialize(parse_result.tracker_infos))
      read_result->tracker_infos = std::move(parse_result.tracker_infos);
  }

  if (delete_after_read_) {
    base::DeleteFile(source_path_, false);
  }

  if (read_result->fetch_result != FetchResult::kSuccess)
    return;

  flatbuffers::FlatBufferBuilder builder;
  std::vector<FlatOffset<flat::FilterRule>> filter_rules_offsets;
  FlatDomainMap domain_map;
  for (const auto& filter_rule : parse_result.filter_rules) {
    AddRuleToBuffer(&builder, filter_rule, &filter_rules_offsets, &domain_map);
  }
  std::vector<FlatOffset<flat::CosmeticRule>> cosmetic_rules_offsets;
  for (const auto& cosmetic_rule : parse_result.cosmetic_rules) {
    AddRuleToBuffer(&builder, cosmetic_rule, &cosmetic_rules_offsets,
                    &domain_map);
  }

  FlatOffset<flat::RulesList> root_offset =
      flat::CreateRulesList(builder, builder.CreateVector(filter_rules_offsets),
                            builder.CreateVector(cosmetic_rules_offsets));

  flat::FinishRulesListBuffer(builder, root_offset);

  if (!SaveRulesList(
          output_path_,
          base::make_span(builder.GetBufferPointer(), builder.GetSize()),
          &(read_result->checksum))) {
    read_result->fetch_result = FetchResult::kFailedSavingParsedRules;
  }
}

RuleSourceHandler::RuleSourceHandler(
    content::BrowserContext* context,
    RuleSource rule_source,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    OnUpdateCallback on_update_callback,
    OnTrackerInfosUpdateCallback on_tracker_infos_update_callback)
    : context_(context),
      on_update_callback_(on_update_callback),
      on_tracker_infos_update_callback_(on_tracker_infos_update_callback),
      rule_source_(rule_source),
      rules_list_path_(context_->GetPath()
                           .Append(GetRulesFolderName())
                           .Append(GetGroupFolderName(rule_source_.group))
                           .AppendASCII(base::NumberToString(rule_source_.id))),
      tracker_infos_path_(
          context_->GetPath()
              .Append(GetRulesFolderName())
              .Append(GetGroupFolderName(rule_source_.group))
              .AppendASCII(base::NumberToString(rule_source_.id) +
                           kTrackerInfoFileSuffix)),
      file_task_runner_(file_task_runner),
      weak_factory_(this) {
  DCHECK(rule_source_.is_from_url && !rule_source_.source_url.is_empty() &&
             rule_source_.source_url.is_valid() ||
         !rule_source_.is_from_url && !rule_source_.source_file.empty());
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

void RuleSourceHandler::OnTrackerInfosLoaded(base::Value tracker_infos) {
  if (tracker_infos.is_dict())
    on_tracker_infos_update_callback_.Run(rule_source_,
                                          std::move(tracker_infos));
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
      FROM_HERE,
      base::BindOnce(base::GetDeleteFileCallback(), rules_list_path_));
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::GetDeleteFileCallback(), tracker_infos_path_));
}

void RuleSourceHandler::StartUpdateTimer() {
  update_timer_.Start(
      FROM_HERE,
      rule_source_.next_fetch > base::Time::Now()
          ? rule_source_.next_fetch - base::Time::Now()
          : base::TimeDelta::FromMinutes(kInitialUpdateDelay) +
                base::TimeDelta::FromMinutes(base::RandDouble() *
                                             kUpdateTimeJitter),
      base::BindOnce(&RuleSourceHandler::DoFetch, base::Unretained(this)));
}

void RuleSourceHandler::DoFetch() {
  rule_source_.is_fetching = true;
  on_update_callback_.Run(this);

  if (rule_source_.is_from_url)
    DownloadRules();
  else
    ReadRulesFromFile(rule_source_.source_file, false);
}

void RuleSourceHandler::DownloadRules() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = rule_source_.source_url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_BYPASS_CACHE;

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

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetURLLoaderFactoryForBrowserProcess();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToTempFile(
      url_loader_factory.get(),
      base::BindOnce(&RuleSourceHandler::OnRulesDownloaded,
                     base::Unretained(this)));
}

void RuleSourceHandler::OnRulesDownloaded(base::FilePath file) {
  std::unique_ptr<network::SimpleURLLoader> url_loader;
  url_loader_.swap(url_loader);

  if (file.empty()) {
    LOG(WARNING) << "Downloading rule source:" << rule_source_.source_url
                 << " failed with error " << url_loader->NetError();

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
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RulesReader::Start, file, rules_list_path_,
                                tracker_infos_path_, delete_after_read,
                                base::BindOnce(&RuleSourceHandler::OnRulesRead,
                                               weak_factory_.GetWeakPtr())));
}

void RuleSourceHandler::OnRulesRead(std::unique_ptr<RulesReadResult> result) {
  rule_source_.last_fetch_result = result->fetch_result;
  rule_source_.is_fetching = false;
  if (rule_source_.last_fetch_result == FetchResult::kSuccess) {
    rule_source_.unsafe_adblock_metadata = result->metadata;
    rule_source_.rules_info = result->rules_info;
    rule_source_.rules_list_checksum = result->checksum;
    rule_source_.last_update = base::Time::Now();

    rule_source_.next_fetch = CalculateNextUpdateTime(rule_source_);

    if (result->tracker_infos.is_dict()) {
      rule_source_.has_tracker_infos = true;
      on_tracker_infos_update_callback_.Run(rule_source_,
                                            std::move(result->tracker_infos));
    }
  } else {
    rule_source_.next_fetch =
        GetNextUpdateTimeAfterFailUpdate(base::Time::Now());
  }

  StartUpdateTimer();
  on_update_callback_.Run(this);
}
}  // namespace adblock_filter
