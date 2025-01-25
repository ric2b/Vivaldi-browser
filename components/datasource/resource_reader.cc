// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/resource_reader.h"

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#include "app/vivaldi_apptools.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "chrome/common/chrome_paths.h"
#endif

namespace {}  // namespace

#if !BUILDFLAG(IS_ANDROID)

namespace {

base::FilePath GetResourceDirectoryImpl() {
#if !defined(OFFICIAL_BUILD) && !BUILDFLAG(IS_IOS)
  // Allow to edit resources without recompiling the browser.
  if (vivaldi::IsVivaldiRunning()) {
    // Duplicate the definition from apps/switches.h to avoid dependency on
    // the apps component.
    const char kLoadAndLaunchApp[] = "load-and-launch-app";
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kLoadAndLaunchApp)) {
      base::FilePath app_path = base::MakeAbsoluteFilePath(
          command_line->GetSwitchValuePath(kLoadAndLaunchApp));
      if (app_path.BaseName().value() == FILE_PATH_LITERAL("src") &&
          app_path.DirName().BaseName().value() ==
              FILE_PATH_LITERAL("vivapp")) {
        return app_path;
      }
    }
  }
#endif
  base::FilePath dir;
#if BUILDFLAG(IS_IOS)
  base::PathService::Get(base::DIR_ASSETS, &dir);
  return dir.Append(FILE_PATH_LITERAL("res"));
#else
  base::PathService::Get(chrome::DIR_RESOURCES, &dir);
  return dir.Append(FILE_PATH_LITERAL("vivaldi"));
#endif
}

}  // namespace

/* static */
const base::FilePath& ResourceReader::GetResourceDirectory() {
  static base::NoDestructor<base::FilePath> dir(GetResourceDirectoryImpl());
  return *dir;
}

#endif  // !IS_ANDROID

/* static */
std::optional<base::Value> ResourceReader::ReadJSON(
    std::string_view resource_directory,
    std::string_view resource_name) {
  DCHECK(resource_directory.empty() || resource_directory.front() != '/');
  DCHECK(resource_directory.empty() || resource_directory.back() != '/');
  DCHECK(!resource_name.empty());
  DCHECK(resource_name.find('/') == std::string::npos);
  std::string resource_path;
  if (!resource_directory.empty()) {
    resource_path.append(resource_directory.data(), resource_directory.size());
    resource_path += '/';
  }
  resource_path.append(resource_name.data(), resource_name.size());

  ResourceReader reader(std::move(resource_path));
  std::optional<base::Value> json = reader.ParseJSON();
  if (!json) {
    LOG(ERROR) << reader.GetError();
  }
  return json;
}

/* static */
gfx::Image ResourceReader::ReadPngImage(std::string_view resource_url) {
  std::string resource_path;
  if (!vivaldi_data_url_utils::IsResourceURL(resource_url, &resource_path)) {
    LOG(ERROR) << "resource_url does not start with "
               << vivaldi_data_url_utils::kResourceUrlPrefix
               << " prefix: " << resource_url;
    return gfx::Image();
  }
#if BUILDFLAG(IS_ANDROID)
  // On Android we need to strip the prefix resources folder from the path.
  else {
    resource_path.erase(
        0, std::string(vivaldi_data_url_utils::kResourceUrlPrefix).size() - 1);
  }
#endif  // IS_ANDROID
  ResourceReader reader(std::move(resource_path));
  if (!reader.IsValid()) {
    LOG(ERROR) << reader.GetError();
    return gfx::Image();
  }
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(reader.bytes());
  if (image.IsEmpty()) {
    LOG(ERROR) << "Failed to read " << resource_url << " as PNG image";
    return gfx::Image();
  }
  return image;
}

ResourceReader::ResourceReader(std::string resource_path)
    : resource_path_(std::move(resource_path)) {
  DCHECK(!resource_path_.empty());
  DCHECK(resource_path_.front() != '/');
#if BUILDFLAG(IS_ANDROID)
  base::MemoryMappedFile::Region region;
  int json_fd =
      base::android::OpenApkAsset("assets/" + resource_path_, &region);
  if (json_fd < 0) {
    not_found_error_ = true;
    return;
  }
  if (!mapped_file_.Initialize(base::File(json_fd), region)) {
    error_ = resource_path_ + ": failed to initialize the memory mapping";
    return;
  }
#else
  base::FilePath path = GetResourceDirectory().Append(
      base::FilePath::FromUTF8Unsafe(resource_path_));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      not_found_error_ = true;
    } else {
      error_ = resource_path_ + ": failed to open for reading,  error_code=" +
               std::to_string(file.error_details());
    }
    return;
  }
  if (!mapped_file_.Initialize(std::move(file))) {
    error_ = resource_path_ + ": failed to initialize the memory mapping";
    return;
  }
#endif
}

ResourceReader::~ResourceReader() = default;

std::string ResourceReader::GetError() const {
  if (not_found_error_)
    return resource_path_ + ": resource was not found";
  return error_;
}

std::optional<base::Value> ResourceReader::ParseJSON() {
  if (!IsValid())
    return std::nullopt;
  DCHECK(mapped_file_.IsValid());
  auto v = base::JSONReader::ReadAndReturnValueWithError(as_string_view());
  if (!v.has_value()) {
    error_ = base::StringPrintf("%s:%d:%d: JSON error - %s",
                                resource_path_.c_str(), v.error().line,
                                v.error().column, v.error().message.c_str());
  }
  return std::move(v.value());
}
