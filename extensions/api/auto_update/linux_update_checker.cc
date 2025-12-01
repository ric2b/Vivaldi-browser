// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/linux_update_checker.h"

#include "app/vivaldi_version_info.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "base/vivaldi_switches.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// See
// https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("vivaldi_update_check", R"(
      semantics {
        sender: "Vivaldi Update Checker"
        description:
          "Checks for available updates to the Vivaldi browser by downloading "
          "and parsing the update feed from Vivaldi's servers."
        trigger:
          "Triggered when user opens the about page.
        data: "No user data is sent. Only the request for the update feed."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled."
      })");

}  // namespace

LinuxUpdateChecker::LinuxUpdateChecker() = default;
LinuxUpdateChecker::~LinuxUpdateChecker() = default;

std::string LinuxUpdateChecker::GetUpdateURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kVivaldiUpdateURL)) {
    // Ref. VB-7983: If the --vuu switch is specified,
    // show the update url in stdout
    std::string url_string =
        command_line.GetSwitchValueASCII(switches::kVivaldiUpdateURL);
    if (!url_string.empty()) {
      GURL command_line_url(url_string);
      if (command_line_url.is_valid()) {
        LOG(INFO) << "Vivaldi Update URL: " << command_line_url.spec();
        return command_line_url.spec();
      }
    }
  }
#if defined(OFFICIAL_BUILD) && \
    (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
  // Public channel URLs for official builds
#if defined(ARCH_CPU_ARM64)
  return "https://update.vivaldi.com/update/1.0/public/linux/appcast.arm64.xml";
#else
  return "https://update.vivaldi.com/update/1.0/public/linux/appcast.x64.xml";
#endif
#else
  // Snapshot channel URLs for non-official builds
#if defined(ARCH_CPU_ARM64)
  return "https://update.vivaldi.com/update/1.0/snapshot/linux/"
         "appcast.arm64.xml";
#else
  return "https://update.vivaldi.com/update/1.0/snapshot/linux/appcast.x64.xml";
#endif
#endif
}

void LinuxUpdateChecker::CheckForUpdates(
    network::mojom::URLLoaderFactory* url_loader_factory,
    UpdateCallback callback) {
  callback_ = std::move(callback);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(GetUpdateURL());
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader_->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToString(
      url_loader_factory,
      base::BindOnce(&LinuxUpdateChecker::OnURLLoadComplete,
                     weak_factory_.GetWeakPtr()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void LinuxUpdateChecker::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int net_error = url_loader_->NetError();
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  // Check for network errors or HTTP errors.
  if (!response_body || net_error != net::OK || response_code < 200 ||
      response_code >= 300) {
    LOG(WARNING) << "Update check failed - NetError: " << net_error
                 << ", HTTP: " << response_code;
    UpdateResult result;
    result.status = AutoUpdateStatus::kError;
    result.version = std::string{::vivaldi::GetVivaldiVersionString()};
    result.release_notes_url = "";
    std::move(callback_).Run(result);
    return;
  }

  ParseUpdateXML(*response_body);
}

void LinuxUpdateChecker::ParseUpdateXML(const std::string& xml_content) {
  // Use the safe XML parser to parse the Sparkle XML
  data_decoder::DataDecoder::ParseXmlIsolated(
      xml_content, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&LinuxUpdateChecker::OnXMLParsed,
                     weak_factory_.GetWeakPtr()));
}

void LinuxUpdateChecker::OnXMLParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  std::string current_version{::vivaldi::GetVivaldiVersionString()};

  if (!result.has_value()) {
    UpdateResult update_result;
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  std::vector<std::string> all_versions;
  std::string release_notes_url;

  // Navigate the parsed XML structure to find version information.
  const base::Value& root = result.value();
  if (!root.is_dict()) {
    LOG(WARNING) << "Root XML element is not a dictionary";
    UpdateResult update_result;
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  // Step 1: Get root children.
  const auto* root_children = root.GetDict().FindList("children");
  if (!root_children || root_children->empty()) {
    LOG(WARNING) << "No root children found";
    UpdateResult update_result;
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  // Step 2: Get channel element (first child).
  if (!(*root_children)[0].is_dict()) {
    LOG(WARNING) << "Channel element is not a dictionary";
    UpdateResult update_result;
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }
  const auto& channel = (*root_children)[0].GetDict();

  // Step 3: Get channel children.
  const auto* channel_children = channel.FindList("children");
  if (!channel_children) {
    LOG(WARNING) << "No channel children found";
    UpdateResult update_result;
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  // Step 4: Process each item
  for (const auto& child : *channel_children) {
    if (!child.is_dict())
      continue;

    const auto& child_dict = child.GetDict();
    const auto* tag = child_dict.FindString("tag");
    if (!tag || *tag != "item")
      continue;

    // Step 5: Process item children
    const auto* item_children = child_dict.FindList("children");
    if (!item_children)
      continue;

    for (const auto& item_child : *item_children) {
      if (!item_child.is_dict())
        continue;

      const auto& item_child_dict = item_child.GetDict();
      const auto* item_tag = item_child_dict.FindString("tag");
      if (!item_tag)
        continue;

      if (*item_tag == "enclosure") {
        const auto* attrs = item_child_dict.FindDict("attributes");
        if (!attrs)
          continue;

        const auto* sparkle_version = attrs->FindString("sparkle:version");
        if (sparkle_version) {
          LOG(INFO) << "Linux update checker: Found version: "
                    << *sparkle_version;
          all_versions.push_back(*sparkle_version);
        }
      } else if (*item_tag == "sparkle:releaseNotesLink" &&
                 release_notes_url.empty()) {
        const auto* text = item_child_dict.FindString("text");
        if (text) {
          release_notes_url = *text;
          LOG(INFO) << "Linux update checker: Found release notes: " << *text;
        }
      }
    }
  }

  // Find the newest version from all collected versions
  std::string latest_version;
  base::Version newest_version;
  for (const auto& version_string : all_versions) {
    base::Version version(version_string);
    if (version.IsValid() &&
        (!newest_version.IsValid() || version > newest_version)) {
      newest_version = version;
      latest_version = version_string;
    }
  }

  UpdateResult update_result;
  if (latest_version.empty()) {
    LOG(WARNING) << "No valid version found in update feed";
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  // Compare with current version
  base::Version current_ver(current_version);
  base::Version latest_ver(latest_version);

  if (!current_ver.IsValid() || !latest_ver.IsValid()) {
    update_result.status = AutoUpdateStatus::kError;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    std::move(callback_).Run(update_result);
    return;
  }

  // Return status based on version comparison
  if (latest_ver > current_ver) {
    update_result.status = AutoUpdateStatus::kDidFindValidUpdate;
    update_result.version = latest_version;
    update_result.release_notes_url = release_notes_url;
    LOG(INFO) << "Vivaldi update available: " << current_version << " -> "
              << latest_version;
  } else {
    update_result.status = AutoUpdateStatus::kNoUpdate;
    update_result.version = current_version;
    update_result.release_notes_url = "";
    LOG(INFO) << "Vivaldi up to date: current version " << current_version;
  }

  std::move(callback_).Run(update_result);
}

}  // namespace extensions