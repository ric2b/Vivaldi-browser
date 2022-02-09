// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/resource_reader.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#endif

namespace {

constexpr char kResourceUrlPrefix[] = "/resources/";

}  // namespace

ResourceReader::ResourceReader() = default;
ResourceReader::~ResourceReader() = default;

/* static */
bool ResourceReader::IsResourceURL(base::StringPiece url,
                                   std::string* resource_path) {
  base::StringPiece prefix(kResourceUrlPrefix);
  if (!base::StartsWith(url, prefix))
    return false;
  if (resource_path) {
    // Just strip the first slash.
    static_assert(kResourceUrlPrefix[0] == '/', "starts with a slash");
    resource_path->assign(url.data() + 1, url.size() - 1);
  }
  return true;
}

/* static */
absl::optional<base::Value> ResourceReader::ReadJSON(
    base::StringPiece resource_directory,
    base::StringPiece resource_name,
    std::string* error) {
  DCHECK(!resource_directory.empty());
  DCHECK(resource_directory.front() != '/');
  DCHECK(resource_directory.back() != '/');
  DCHECK(!resource_name.empty());
  DCHECK(resource_name.find('/') == std::string::npos);
  std::string resource_path;
  resource_path.append(resource_directory.data(), resource_directory.size());
  resource_path += '/';
  resource_path.append(resource_name.data(), resource_name.size());

  ResourceReader reader;
  reader.Open(std::move(resource_path));
  absl::optional<base::Value> json = reader.ParseJSON();
  if (!reader.error_.empty()) {
    if (error) {
      *error = std::move(reader.error_);
    } else {
      LOG(ERROR) << reader.error_;
    }
  }
  return json;
}

bool ResourceReader::Open(std::string resource_path) {
  DCHECK(!resource_path.empty());
  DCHECK(resource_path.front() != '/');
  DCHECK(error_.empty());
  DCHECK(!mapped_file_.IsValid());
  resource_path_ = std::move(resource_path);
#if defined(OS_ANDROID)
  base::MemoryMappedFile::Region region;
  int json_fd =
      base::android::OpenApkAsset("assets/" + resource_path_, &region);
  if (json_fd < 0) {
    error_ = resource_path_ + ": failed to open Android asset";
    return false;
  }
  if (!mapped_file_.Initialize(base::File(json_fd), region)) {
    error_ = resource_path_ + ": failed to initialize the memory mapping";
    return false;
  }
#else
  auto get_asset_dir = []() -> base::FilePath {
    base::FilePath asset_dir;
    base::PathService::Get(base::DIR_ASSETS, &asset_dir);
#if !defined(OS_MAC)
    asset_dir = asset_dir.Append(FILE_PATH_LITERAL("resources"));
#endif
    asset_dir = asset_dir.Append(FILE_PATH_LITERAL("vivaldi"));
    return asset_dir;
  };
  static base::NoDestructor<base::FilePath> asset_dir(get_asset_dir());
  base::FilePath path =
      asset_dir->Append(base::FilePath::FromUTF8Unsafe(resource_path_));
  if (!mapped_file_.Initialize(path, base::MemoryMappedFile::READ_ONLY)) {
    error_ = resource_path_ + ": failed to open as a memory-mapped file";
    return false;
  }
#endif
  return true;
}

absl::optional<base::Value> ResourceReader::ParseJSON() {
  if (!error_.empty())
    return absl::nullopt;
  DCHECK(mapped_file_.IsValid());
  base::JSONReader::ValueWithError v =
      base::JSONReader::ReadAndReturnValueWithError(as_string_view());
  if (!v.value) {
    error_ = base::StringPrintf("%s:%d:%d: JSON error - %s",
                                resource_path_.c_str(), v.error_line,
                                v.error_column, v.error_message.c_str());
  }
  return std::move(v.value);
}
