// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_resources.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/escape.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#else
#include "base/json/json_file_value_serializer.h"
#endif

namespace adblock_filter {
namespace {
#if defined(OS_ANDROID)
const base::FilePath::CharType kResourcesFilePath[] =
    FILE_PATH_LITERAL("assets/ublock_resources/resources.json");
#else
const base::FilePath::CharType kResourcesFilePath[] =
    FILE_PATH_LITERAL("vivaldi/ublock_resources/resources.json");
#endif

const std::map<base::StringPiece, base::StringPiece> kAliasMap{
    // Aliases used by ublock rules
    {"1x1-transparent.gif", "1x1.gif"},
    {"2x2-transparent.png", "2x2.png"},
    {"3x2-transparent.png", "3x2.png"},
    {"32x32-transparent.png", "32x32.png"},
    {"addthis.com/addthis_widget.js", "addthis_widget.js"},
    {"amazon-adsystem.com/aax2/amzn_ads.js", "amazon_ads.js"},
    {"ampproject.org/v0.js", "ampproject_v0.js"},
    {"static.chartbeat.com/chartbeat.js", "chartbeat.js"},
    {"doubleclick.net/instream/ad_status.js",
     "doubleclick_instream_ad_status.js"},
    {"google-analytics.com/analytics.js", "google-analytics_analytics.js"},
    {"google-analytics.com/cx/api.js", "google-analytics_cx_api.js"},
    {"google-analytics.com/ga.js", "google-analytics_ga.js"},
    {"google-analytics.com/inpage_linkid.js",
     "google-analytics_inpage_linkid.js"},
    {"googlesyndication.com/adsbygoogle.js",
     "googlesyndication_adsbygoogle.js"},
    {"googletagmanager.com/gtm.js", "googletagmanager_gtm.js"},
    {"googletagservices.com/gpt.js", "googletagservices_gpt.js"},
    {"ligatus.com/*/angular-tag.js", "ligatus_angular-tag.js"},
    {"d3pkae9owd2lcf.cloudfront.net/mb105.js", "monkeybroker.js"},
    {"silent-noeval.js", "noeval-silent.js"},
    {"bab-defuser.js", "nobab.js"},
    {"fuckadblock.js-3.2.0", "nofab.js"},
    {"noopmp3-0.1s", "noop-0.1s.mp3"},
    {"noopmp4-1s", "noop-1s.mp4"},
    {"noopjs", "noop.js"},
    {"nooptext", "noop.txt"},
    {"widgets.outbrain.com/outbrain.js", "outbrain-widget.js"},
    {"popads.net.js", "popads.js"},
    {"scorecardresearch.com/beacon.js", "scorecardresearch_beacon.js"},
    {"nowoif.js", "window.open-defuser.js"},

    // Aliases used to support adblock rewrite rules
    {"blank-text", "noop.txt"},
    {"blank-css", "noop.css"},
    {"blank-js", "noop.js"},
    {"blank-html", "noop.html"},
    {"blank-mp3", "noopmp3-0.1s"},
    {"blank-mp4", "noopmp4-1s"},
    {"1x1-transparent-gif", "1x1.gif"},
    {"2x2-transparent-png", "2x2.png"},
    {"3x2-transparent-png", "3x2.png"},
    {"32x32-transparent-png", "32x32.png"},

    // Surrogate names used by the DDG list
    {"ga.js", "google-analytics_ga.js"},
    {"analytics.js", "google-analytics_analytics.js"},
    {"inpage_linkid.js", "google-analytics_inpage_linkid.js"},
    {"api.js", "google-analytics_cx_api.js"},
    {"gpt.js", "googletagservices_gpt.js"},
    {"gtm.js", "googletagmanager_gtm.js"},
    {"adsbygoogle.js", "googlesyndication_adsbygoogle.js"},
    {"ad_status.js", "doubleclick_instream_ad_status.js"},
    {"beacon.js", "scorecardresearch_beacon.js"},
    {"outbrain.js", "outbrain-widget.js"},
    {"amzn_ads.js", "amazon_ads.js"},
};

const std::map<flat::ResourceType, base::StringPiece> mimetype_for_empty = {
    {flat::ResourceType_SUBDOCUMENT, "text/html,"},
    {flat::ResourceType_OTHER, "text/plain,"},
    {flat::ResourceType_STYLESHEET, "text/css,"},
    {flat::ResourceType_SCRIPT, "application/javascript,"},
    {flat::ResourceType_XMLHTTPREQUEST, "text/plain,"},
};

const std::map<base::StringPiece, base::StringPiece> mimetype_for_extension = {
    {".gif", "image/gif;base64,"},
    {".html", "text/html,"},
    {".js", "application/javascript,"},
    {".mp3", "audio/mp3;base64,"},
    {".mp4", "video/mp4;base64,"},
    {".png", "image/png;base64,"},
    {".txt", "text/plain,"},
    {".css", "text/css,"},
};

void LoadResources(
    base::OnceCallback<void(std::unique_ptr<base::Value>)> callback) {
#if defined(OS_ANDROID)
  base::MemoryMappedFile::Region region;
  base::MemoryMappedFile mapped_file;
  int json_fd = base::android::OpenApkAsset(kResourcesFilePath, &region);
  if (json_fd < 0) {
    LOG(ERROR) << "Adblock resources not found in APK assest.";
    std::move(callback).Run(nullptr);
  } else {
    if (!mapped_file.Initialize(base::File(json_fd), region)) {
      LOG(ERROR) << "failed to initialize memory mapping for "
                 << kResourcesFilePath;
      std::move(callback).Run(nullptr);
    }
    base::StringPiece json_text(reinterpret_cast<char*>(mapped_file.data()),
                                mapped_file.length());
    JSONStringValueDeserializer deserializer(json_text);
    std::move(callback).Run(deserializer.Deserialize(nullptr, nullptr));
  }
#else
  base::FilePath path;
  base::PathService::Get(chrome::DIR_RESOURCES, &path);
  path = path.Append(kResourcesFilePath);
  JSONFileValueDeserializer deserializer(path);
  std::move(callback).Run(deserializer.Deserialize(nullptr, nullptr));
#endif
}
}  // namespace

Resources::Resources(scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner), weak_factory_(this) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LoadResources,
                                base::BindOnce(&Resources::OnLoadFinished,
                                               weak_factory_.GetWeakPtr())));
}
Resources::~Resources() = default;

void Resources::OnLoadFinished(std::unique_ptr<base::Value> resources) {
  resources_.swap(resources);
}

base::Optional<std::string> Resources::Get(
    const std::string& name,
    flat::ResourceType resource_type) const {
  // If resources aren't yet loaded, then we'll just block the request.
  if (!resources_ || resource_type == flat::ResourceType_WEBSOCKET ||
      resource_type == flat::ResourceType_WEBRTC ||
      resource_type == flat::ResourceType_PING)
    return base::nullopt;

  base::StringPiece actual_name(name);
  const auto& actual_name_it = kAliasMap.find(name);
  if (actual_name_it != kAliasMap.end())
    actual_name = actual_name_it->second;

  if (actual_name == "empty") {
    const auto& mimetype_it = mimetype_for_empty.find(resource_type);
    if (mimetype_it == mimetype_for_empty.end())
      return base::nullopt;
    return std::string("data:") + mimetype_it->second.as_string();
  }

  std::string* resource = resources_->FindStringKey(actual_name);
  if (!resource)
    return base::nullopt;

  size_t extension_separator_position = actual_name.find_last_of('.');
  if (extension_separator_position == base::StringPiece::npos)
    return base::nullopt;

  const auto& mimetype_it = mimetype_for_extension.find(
      actual_name.substr(extension_separator_position));
  if (mimetype_it == mimetype_for_extension.end())
    return base::nullopt;

  return std::string("data:") + mimetype_it->second.as_string() +
         net::EscapeUrlEncodedData(*resource, false);
}

}  // namespace adblock_filter