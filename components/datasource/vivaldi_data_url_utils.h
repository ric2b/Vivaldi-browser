// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_

#include "base/optional.h"
#include "base/strings/string_piece.h"

namespace vivaldi_data_url_utils {

enum class PathType {
  kLocalPath,
  kThumbnail,
  kCSSMod,
  kNotesAttachment,

  // Windows-specific
  kDesktopWallpaper,

  kLastType = kDesktopWallpaper,
};

constexpr size_t PathTypeCount = static_cast<size_t>(PathType::kLastType) + 1;

// Parse the path component of chrome://vivaldi-data/ URLs. Typically it is
// /type/data, but there are few older formats that deviates from it.
base::Optional<PathType> ParsePath(base::StringPiece path,
                                   std::string* data = nullptr);

// Parse the full url.
base::Optional<PathType> ParseUrl(base::StringPiece url,
                                  std::string* data = nullptr);

std::string GetPathMimeType(base::StringPiece path);

// Check if path mapping id is really old-format thumbanil, not a path
// mapping.
bool isOldFormatThumbnailId(base::StringPiece id);

bool IsBookmarkCapureUrl(base::StringPiece id);

// Construct full vivaldi-data URL from the type and data.
std::string MakeUrl(PathType type, base::StringPiece data);

extern const char* const kTypeNames[PathTypeCount];

constexpr const char* top_dir(PathType type) {
  return kTypeNames[static_cast<size_t>(type)];
}

}  // vivaldi_data_url_utils

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_URL_UTILS_H_
