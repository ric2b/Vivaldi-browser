// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_url_utils.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"

#include "app/vivaldi_constants.h"

namespace vivaldi_data_url_utils {

const char* const kTypeNames[PathTypeCount] = {
    "local-image", "thumbnail", "css-mods", "notes-attachment", "desktop-image",
};

namespace {

const char kCSSModsData[] = "css";
const char kCSSModsExtension[] = ".css";

const char kOldThumbnailFormatPrefix[] = "/http://bookmark_thumbnail/";

}  // namespace

base::Optional<PathType> ParsePath(base::StringPiece path,
                                   std::string* data) {
  if (path.length() < 2 || path[0] != '/')
    return base::nullopt;

  base::StringPiece type_piece;
  base::StringPiece data_piece;
  size_t pos = path.find('/', 1);
  if (pos != std::string::npos) {
    type_piece = path.substr(1, pos - 1);
    data_piece = path.substr(pos + 1);
  } else {
    type_piece = path.substr(1);
    data_piece = base::StringPiece();
  }

  PathType type;
  const char* const* i =
      std::find(std::cbegin(kTypeNames), std::cend(kTypeNames), type_piece);
  if (i != std::cend(kTypeNames)) {
    type = static_cast<PathType>(std::distance(std::cbegin(kTypeNames), i));
  } else {
    // Check for old-style bookmark thumbnail links where the path
    // was a full http: URL.
    base::StringPiece prefix = kOldThumbnailFormatPrefix;
    if (path.starts_with(prefix)) {
      type = PathType::kThumbnail;
      data_piece = path.substr(prefix.length());
    } else {
      return base::nullopt;
    }
  }

  // Strip the query part from the data. We do this even when !data as we need
  // to check data_piece below.
  pos = data_piece.find('?');
  if (pos != std::string::npos) {
    data_piece = data_piece.substr(0, pos);
  }
  if (data) {
    data->assign(data_piece.data(), data_piece.length());
  }

  // Remap old /local-image/small-number path to thumbnail
  if (type == PathType::kLocalPath && isOldFormatThumbnailId(data_piece)) {
    type = PathType::kThumbnail;
    if (data) {
      *data += ".png";
    }
  }
  return type;
}

// Parse the full url.
base::Optional<PathType> ParseUrl(base::StringPiece url, std::string* data) {
  if (url.empty())
    return base::nullopt;

  // Short-circuit relative resource URLs to avoid the warning below.
  if (url.starts_with("/resources/"))
    return base::nullopt;

  GURL gurl(url);
  if (!gurl.is_valid()) {
    LOG(WARNING) << "The url argument is not a valid URL - " << url;
    return base::nullopt;
  }

  if (!gurl.SchemeIs(VIVALDI_DATA_URL_SCHEME))
    return base::nullopt;

  // Treat the old format chrome://thumb/ as an alias to chrome://vivaldi-data/
  // as the path alone allows to uniquely parse it, see ParsePath().
  base::StringPiece host = gurl.host_piece();
  if (host != VIVALDI_DATA_URL_HOST && host != VIVALDI_THUMB_URL_HOST)
    return base::nullopt;

  return ParsePath(gurl.path_piece(), data);
}

std::string GetPathMimeType(base::StringPiece path) {
  std::string data;
  base::Optional<PathType> type = ParsePath(path, &data);
  if (type == PathType::kCSSMod) {
    if (data == kCSSModsData ||
        base::StringPiece(data).ends_with(kCSSModsExtension))
      return "text/css";
  }
  return "image/png";
}

bool isOldFormatThumbnailId(base::StringPiece id) {
  int64_t bookmark_id;
  return id.length() <= 20 && base::StringToInt64(id, &bookmark_id) &&
         bookmark_id > 0;
}

bool IsBookmarkCapureUrl(base::StringPiece url) {
  base::Optional<PathType> type = ParseUrl(url);
  return type == PathType::kThumbnail;
}

std::string MakeUrl(PathType type, base::StringPiece data) {
  std::string url;
  url += vivaldi::kVivaldiUIDataURL;
  url += top_dir(type);
  url += '/';
  url.append(data.data(), data.length());
  return url;
}

}  // vivaldi_data_url_utils