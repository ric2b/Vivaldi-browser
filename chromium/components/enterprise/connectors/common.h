// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_COMMON_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_COMMON_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace download {
class DownloadItem;
}  // namespace download

namespace enterprise_connectors {

// Alias to reduce verbosity when using TriggeredRule::Actions.
using TriggeredRule = ContentAnalysisResponse::Result::TriggeredRule;

// Pair to specify the source and destination.
using SourceDestinationStringPair = std::pair<std::string, std::string>;

// Keys used to read a connector's policy values.
constexpr char kKeyServiceProvider[] = "service_provider";
constexpr char kKeyLinuxVerification[] = "verification.linux";
constexpr char kKeyMacVerification[] = "verification.mac";
constexpr char kKeyWindowsVerification[] = "verification.windows";
constexpr char kKeyEnable[] = "enable";
constexpr char kKeyDisable[] = "disable";
constexpr char kKeyUrlList[] = "url_list";
constexpr char kKeySourceDestinationList[] = "source_destination_list";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
constexpr char kKeyBlockLargeFiles[] = "block_large_files";
constexpr char kKeyMinimumDataSize[] = "minimum_data_size";
constexpr char kKeyEnabledEventNames[] = "enabled_event_names";
constexpr char kKeyCustomMessages[] = "custom_messages";
constexpr char kKeyRequireJustificationTags[] = "require_justification_tags";
constexpr char kKeyCustomMessagesTag[] = "tag";
constexpr char kKeyCustomMessagesMessage[] = "message";
constexpr char kKeyCustomMessagesLearnMoreUrl[] = "learn_more_url";
constexpr char kKeyMimeTypes[] = "mime_types";
constexpr char kKeyEnterpriseId[] = "enterprise_id";
constexpr char kKeyDefaultAction[] = "default_action";
constexpr char kKeyDomain[] = "domain";
constexpr char kKeyEnabledOptInEvents[] = "enabled_opt_in_events";
constexpr char kKeyOptInEventName[] = "name";
constexpr char kKeyOptInEventUrlPatterns[] = "url_patterns";

// Available tags.
constexpr char kDlpTag[] = "dlp";
constexpr char kMalwareTag[] = "malware";

// A MIME type string that matches all MIME types.
constexpr char kWildcardMimeType[] = "*";

// The reporting connector subdirectory in User_Data_Directory
constexpr base::FilePath::CharType RC_BASE_DIR[] =
    FILE_PATH_LITERAL("Enterprise/ReportingConnector/");

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Struct holding the necessary data to tweak the behavior of the reporting
// Connector.
struct ReportingSettings {
  ReportingSettings();
  ReportingSettings(GURL url, const std::string& dm_token, bool per_profile);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings(const ReportingSettings&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  GURL reporting_url;
  std::set<std::string> enabled_event_names;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  std::string dm_token;

  // Indicates if the report should be made for the profile, or the browser if
  // false.
  bool per_profile = false;
};

// Returns the pref path corresponding to a connector.
const char* ConnectorPref(AnalysisConnector connector);
const char* ConnectorPref(ReportingConnector connector);
const char* ConnectorScopePref(AnalysisConnector connector);
const char* ConnectorScopePref(ReportingConnector connector);

// Returns the highest precedence action in the given parameters. Writes the tag
// field of the result containing the highest precedence action into |tag|.
TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag);
TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2);
ContentAnalysisAcknowledgement::FinalAction GetHighestPrecedenceAction(
    const ContentAnalysisAcknowledgement::FinalAction& action_1,
    const ContentAnalysisAcknowledgement::FinalAction& action_2);

// Struct used to persist metadata about a file in base::SupportsUserData
// through ScanResult.
struct FileMetadata {
  FileMetadata(
      const std::string& filename,
      const std::string& sha256,
      const std::string& mime_type,
      int64_t size,
      const ContentAnalysisResponse& scan_response = ContentAnalysisResponse());
  FileMetadata(FileMetadata&&);
  FileMetadata(const FileMetadata&);
  FileMetadata& operator=(const FileMetadata&);
  ~FileMetadata();

  std::string filename;
  std::string sha256;
  std::string mime_type;
  int64_t size;
  ContentAnalysisResponse scan_response;
};

// User data class to persist scanning results for multiple files corresponding
// to a single base::SupportsUserData object.
struct ScanResult : public base::SupportsUserData::Data {
  ScanResult();
  explicit ScanResult(FileMetadata metadata);
  ~ScanResult() override;
  static const char kKey[];

  std::vector<FileMetadata> file_metadata;
  std::optional<std::u16string> user_justification;
};

// Enum to identify which message to show once scanning is complete. Ordered
// by precedence for when multiple files have conflicting results.
enum class FinalContentAnalysisResult {
  // Show that an issue was found and that the upload is blocked.
  FAILURE = 0,

  // Show that the scan failed and that the upload is blocked.
  FAIL_CLOSED = 1,

  // Show that files were not uploaded since they were too large.
  LARGE_FILES = 2,

  // Show that files were not uploaded since they were encrypted.
  ENCRYPTED_FILES = 3,

  // Show that DLP checks failed, but that the user can proceed if they want.
  WARNING = 4,

  // Show that no issue was found and that the user may proceed.
  SUCCESS = 5,
};

// Result for a single request of the RequestHandler classes.
struct RequestHandlerResult {
  RequestHandlerResult();
  ~RequestHandlerResult();
  RequestHandlerResult(RequestHandlerResult&&);
  RequestHandlerResult& operator=(RequestHandlerResult&&);
  RequestHandlerResult(const RequestHandlerResult&);
  RequestHandlerResult& operator=(const RequestHandlerResult&);

  bool complies;
  FinalContentAnalysisResult final_result;
  std::string tag;
  std::string request_token;
  ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
      custom_rule_message;
};

// Calculates the ContentAnalysisAcknowledgement::FinalAction for an action
// based on the response it got from scanning.
ContentAnalysisAcknowledgement::FinalAction GetAckFinalAction(
    const ContentAnalysisResponse& response);

// Extracts the message string from the custom rule message field in the content
// analysis response.
std::u16string GetCustomRuleString(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message);

// Extracts the ranges and their corresponding links from the custom rule
// message field in the content analysis response. Used to style the custom rule
// message in the content analysis dialog. `offset` corresponds to its start
// index as we are inserting it in another message.
std::vector<std::pair<gfx::Range, GURL>> GetCustomRuleStyles(
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message,
    size_t offset);

// Simple custom rule message for tests, with one message segment containing the
// text and associated url.
ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
CreateSampleCustomRuleMessage(const std::u16string& msg,
                              const std::string& url);

// Extracts the custom rule message from `download_item`. The rule for that
// message needs to have an action (WARN, BLOCK) corresponding to `danger_type`.
std::optional<ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage>
GetDownloadsCustomRuleMessage(const download::DownloadItem* download_item,
                              download::DownloadDangerType danger_type);

// Checks if |response| contains a negative malware verdict.
bool ContainsMalwareVerdict(const ContentAnalysisResponse& response);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_COMMON_H_
