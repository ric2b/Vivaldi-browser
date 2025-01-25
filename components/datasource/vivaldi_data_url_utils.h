// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace vivaldi_data_url_utils {

enum class PathType {
  kLocalPath,
  kImage,
  kCSSMod,
  kSyncedStore,

  // Windows-specific
  kDesktopWallpaper,

  kDirectMatch,
  kLastType = kDirectMatch,
};

constexpr size_t PathTypeCount = static_cast<size_t>(PathType::kLastType) + 1;

constexpr char kMimeTypePNG[] = "image/png";
constexpr char kResourceUrlPrefix[] = "/resources/";

// Parse the path component of chrome://vivaldi-data/ URLs. Typically it is
// /type/data, but there are few older formats that deviates from it.
std::optional<PathType> ParsePath(std::string_view path,
                                   std::string* data = nullptr);

// Parse the full url.
std::optional<PathType> ParseUrl(std::string_view url,
                                  std::string* data = nullptr);

// Check if the url points to internal Vivaldi resources. If |subpath| is not
// null on return |*subpath| holds the resource path.
bool IsResourceURL(std::string_view url, std::string* subpath = nullptr);

// Check if path mapping id is really old-format thumbnail, not a path
// mapping.
bool isOldFormatThumbnailId(std::string_view id);

bool IsBookmarkCaptureUrl(std::string_view id);

bool IsLocalPathUrl(std::string_view id);

std::optional<std::string> GetSyncedStoreChecksumForUrl(std::string_view url);

// Construct full vivaldi-data URL from the type and data.
std::string MakeUrl(PathType type, std::string_view data);

extern const char* const kTypeNames[PathTypeCount];

constexpr const char* top_dir(PathType type) {
  return kTypeNames[static_cast<size_t>(type)];
}

// Limit the read size by `ReadFileOnBlockingThread()` to a big but sane limit
// as the function should not be used to read DVD.iso files.
constexpr int kMaxAllowedRead = 512 * 1024 * 1024;

// Read the given file. Return null when file does not exit or on errors. The
// errors are logged. If `log_not_found` and `file_path` is not found, treat
// that as an error and log as well. If the size to read exceds
// `kMaxAllowedRead`, this will be logged an an error and null is returned.
scoped_refptr<base::RefCountedMemory> ReadFileOnBlockingThread(
    const base::FilePath& file_path,
    bool log_not_found = true);

// Same as above, but passing the results by the vector reference.
// Returns true on success.
bool ReadFileToVectorOnBlockingThread(const base::FilePath& file_path,
                                      std::vector<unsigned char>& buffer,
                                      bool log_not_found = true);
}  // namespace vivaldi_data_url_utils

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
