// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
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

  kLastType = kDesktopWallpaper,
};

constexpr size_t PathTypeCount = static_cast<size_t>(PathType::kLastType) + 1;

constexpr char kMimeTypePNG[] = "image/png";
constexpr char kResourceUrlPrefix[] = "/resources/";

// Parse the path component of chrome://vivaldi-data/ URLs. Typically it is
// /type/data, but there are few older formats that deviates from it.
absl::optional<PathType> ParsePath(base::StringPiece path,
                                   std::string* data = nullptr);

// Parse the full url.
absl::optional<PathType> ParseUrl(base::StringPiece url,
                                  std::string* data = nullptr);

// Check if the url points to internal Vivaldi resources. If |subpath| is not
// null on return |*subpath| holds the resource path.
bool IsResourceURL(base::StringPiece url,
                          std::string* subpath = nullptr);

// Check if path mapping id is really old-format thumbnail, not a path
// mapping.
bool isOldFormatThumbnailId(base::StringPiece id);

bool IsBookmarkCaptureUrl(base::StringPiece id);

bool IsLocalPathUrl(base::StringPiece id);

absl::optional<std::string> GetSyncedStoreChecksumForUrl(base::StringPiece url);

// Construct full vivaldi-data URL from the type and data.
std::string MakeUrl(PathType type, base::StringPiece data);

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

}  // namespace vivaldi_data_url_utils

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
