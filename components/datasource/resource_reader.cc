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
#include "chrome/common/chrome_paths.h"

#include "app/vivaldi_apptools.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#endif

namespace {

constexpr char kResourceUrlPrefix[] = "/resources/";

}  // namespace

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

#if !BUILDFLAG(IS_ANDROID)

namespace {

base::FilePath GetResourceDirectoryImpl() {
#if !defined(OFFICIAL_BUILD)
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
  base::PathService::Get(chrome::DIR_RESOURCES, &dir);
  return dir.Append(FILE_PATH_LITERAL("vivaldi"));
}

}  // namespace

/* static */
const base::FilePath& ResourceReader::GetResourceDirectory() {
  static base::NoDestructor<base::FilePath> dir(GetResourceDirectoryImpl());
  return *dir;
}

#endif  // !IS_ANDROID

/* static */
absl::optional<base::Value> ResourceReader::ReadJSON(
    base::StringPiece resource_directory,
    base::StringPiece resource_name) {
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
  absl::optional<base::Value> json = reader.ParseJSON();
  if (!json) {
    LOG(ERROR) << reader.GetError();
  }
  return json;
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

absl::optional<base::Value> ResourceReader::ParseJSON() {
  if (!IsValid())
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
