// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_resources.h"

#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#else
#include "base/json/json_file_value_serializer.h"
#endif

namespace adblock_filter {
namespace {
#if BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kRedirectableResourcesFilePath[] =
    FILE_PATH_LITERAL("assets/adblocker_resources/redirectable_resources.json");
const base::FilePath::CharType kInjectableResourcesFilePath[] =
    FILE_PATH_LITERAL("assets/adblocker_resources/injectable_resources.json");
#elif BUILDFLAG(IS_IOS)
const auto kResourceDir = base::DIR_ASSETS;
const base::FilePath::CharType kRedirectableResourcesFilePath[] =
    FILE_PATH_LITERAL("res/adblocker_resources/redirectable_resources.json");
const base::FilePath::CharType kInjectableResourcesFilePath[] =
    FILE_PATH_LITERAL("res/adblocker_resources/injectable_resources.json");
#else
const auto kResourceDir = chrome::DIR_RESOURCES;
const base::FilePath::CharType kRedirectableResourcesFilePath[] =
    FILE_PATH_LITERAL(
        "vivaldi/adblocker_resources/redirectable_resources.json");
const base::FilePath::CharType kInjectableResourcesFilePath[] =
    FILE_PATH_LITERAL("vivaldi/adblocker_resources/injectable_resources.json");
#endif

#if !BUILDFLAG(IS_IOS)
constexpr auto kAliasMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
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
        {"noopvmap-1.0", "noop-vmap1.0.xml"},
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
    });

constexpr auto kMimetypeForEmpty =
    base::MakeFixedFlatMap<flat::ResourceType, std::string_view>({
        {flat::ResourceType_SUBDOCUMENT, "text/html,"},
        {flat::ResourceType_OTHER, "text/plain,"},
        {flat::ResourceType_STYLESHEET, "text/css,"},
        {flat::ResourceType_SCRIPT, "application/javascript,"},
        {flat::ResourceType_XMLHTTPREQUEST, "text/plain,"},
    });

constexpr auto kMimeTypeForExtension =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        {".gif", "image/gif;base64,"},
        {".html", "text/html,"},
        {".js", "application/javascript,"},
        {".mp3", "audio/mp3;base64,"},
        {".mp4", "video/mp4;base64,"},
        {".png", "image/png;base64,"},
        {".txt", "text/plain,"},
        {".css", "text/css,"},
        {".xml", "text/xml,"},
    });
#endif  // !IS_IOS

// uBlock technically allows to inject any of those scripts, even if it doesn't
// make sense for all of them.
constexpr auto kInjectableRedirectables =
    base::MakeFixedFlatSet<std::string_view>(
        {"amazon_ads.js", "doubleclick_instream_ad_status.js",
         "google-analytics_analytics.js", "google-analytics_cx_api.js",
         "google-analytics_ga.js", "googlesyndication_adsbygoogle.js",
         "googletagmanager_gtm.js", "googletagservices_gpt.js", "noeval.js",
         "noeval-silent.js", "nobab.js", "nofab.js", "noop.js", "popads.js",
         "popads-dummy.js", "window.open-defuser.js"});

std::unique_ptr<base::Value> LoadResources(
    const base::FilePath::CharType* resource_file) {
#if BUILDFLAG(IS_ANDROID)
  base::MemoryMappedFile::Region region;
  base::MemoryMappedFile mapped_file;
  int json_fd = base::android::OpenApkAsset(resource_file, &region);
  if (json_fd < 0) {
    LOG(ERROR) << "Adblock resources not found in APK assest.";
    return nullptr;
  } else {
    if (!mapped_file.Initialize(base::File(json_fd), region)) {
      LOG(ERROR) << "failed to initialize memory mapping for " << resource_file;
      return nullptr;
    }
    std::string_view json_text(reinterpret_cast<char*>(mapped_file.data()),
                               mapped_file.length());
    JSONStringValueDeserializer deserializer(json_text);
    return deserializer.Deserialize(nullptr, nullptr);
  }
#else
  base::FilePath path;
  base::PathService::Get(kResourceDir, &path);
  path = path.Append(resource_file);
  JSONFileValueDeserializer deserializer(path);
  return deserializer.Deserialize(nullptr, nullptr);
#endif
}

bool ShouldUseMainWorldForResource(std::string_view name) {
  return name == "abp-main.js";
}
}  // namespace

Resources::Resources(scoped_refptr<base::SequencedTaskRunner> task_runner)
    : weak_factory_(this) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadResources, kRedirectableResourcesFilePath),
      base::BindOnce(&Resources::OnLoadFinished, weak_factory_.GetWeakPtr(),
                     &redirectable_resources_));
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadResources, kInjectableResourcesFilePath),
      base::BindOnce(&Resources::OnLoadFinished, weak_factory_.GetWeakPtr(),
                     &injectable_resources_));
}
Resources::~Resources() = default;

void Resources::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Resources::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Resources::OnLoadFinished(base::Value* destination,
                               std::unique_ptr<base::Value> resources) {
  if (resources && resources->is_dict())
    *destination = std::move(*resources);

  if (loaded()) {
    for (Observer& observer : observers_)
      observer.OnResourcesLoaded();
  }
}

#if !BUILDFLAG(IS_IOS)
std::optional<std::string> Resources::GetRedirect(
    const std::string& name,
    flat::ResourceType resource_type) const {
  // If resources aren't yet loaded, then we'll just block the request.
  if (!redirectable_resources_.is_dict() ||
      resource_type == flat::ResourceType_WEBSOCKET ||
      resource_type == flat::ResourceType_WEBRTC ||
      resource_type == flat::ResourceType_PING)
    return std::nullopt;

  std::string_view actual_name(name);
  auto actual_name_it = kAliasMap.find(name);
  if (actual_name_it != kAliasMap.end())
    actual_name = actual_name_it->second;

  if (actual_name == "empty") {
    auto mimetype_it = kMimetypeForEmpty.find(resource_type);
    if (mimetype_it == kMimetypeForEmpty.end())
      return std::nullopt;
    return std::string("data:") + std::string(mimetype_it->second);
  }

  const std::string* resource =
      redirectable_resources_.GetDict().FindString(actual_name);
  if (!resource)
    return std::nullopt;

  size_t extension_separator_position = actual_name.find_last_of('.');
  if (extension_separator_position == std::string_view::npos)
    return std::nullopt;

  auto mimetype_it = kMimeTypeForExtension.find(
      actual_name.substr(extension_separator_position));
  if (mimetype_it == kMimeTypeForExtension.end())
    return std::nullopt;

  return std::string("data:") + std::string(mimetype_it->second) +
         base::EscapeUrlEncodedData(*resource, false);
}
#endif  // !IS_IOS

std::map<std::string, Resources::InjectableResource>
Resources::GetInjections() {
  DCHECK(loaded());

  std::map<std::string, InjectableResource> result;

  for (auto resource : injectable_resources_.GetDict()) {
    result.try_emplace(
        resource.first,
        InjectableResource{std::string_view(resource.second.GetString()),
                           ShouldUseMainWorldForResource(resource.first)});
  }

  for (auto resource : redirectable_resources_.GetDict()) {
    if (kInjectableRedirectables.count(resource.first)) {
      result.try_emplace(
          resource.first,
          InjectableResource{std::string_view(resource.second.GetString()),
                             false});
    }
  }

  return result;
}

}  // namespace adblock_filter