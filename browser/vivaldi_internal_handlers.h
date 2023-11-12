// Copyright (c) 2021 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALID_DOWNLOADS_UTIL_H_
#define BROWSER_VIVALID_DOWNLOADS_UTIL_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace download {
class DownloadItem;
}

namespace vivaldi {
using DownloadHandler =
    base::RepeatingCallback<bool(Profile* profile, download::DownloadItem*)>;
using ProtocolHandler =
    base::RepeatingCallback<bool(Profile* profile, GURL url)>;
void RegisterDownloadHandler(base::FilePath::StringType extension,
                             DownloadHandler handler);
void RegisterProtocolHandler(std::string protocol, ProtocolHandler handler);
bool HandleDownload(Profile* profile, download::DownloadItem* download);
bool HandleProtocol(Profile* profile, GURL url);
}  // namespace vivaldi

#endif  // BROWSER_VIVALID_DOWNLOADS_UTIL_H_
