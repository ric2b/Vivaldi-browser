// Copyright (c) 2021 Vivaldi. All rights reserved.

#include "browser/vivaldi_internal_handlers.h"

#include <map>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/download/public/common/download_item.h"
namespace vivaldi {

using DownloadHandlerMap =
    std::map<base::FilePath::StringType, DownloadHandler>;
using ProtocolHandlerMap = std::map<std::string, ProtocolHandler>;

DownloadHandlerMap& GetDownloadHandlerMap() {
  static base::NoDestructor<DownloadHandlerMap> handler_map;
  return *handler_map;
}

ProtocolHandlerMap& GetProtocolHandlerMap() {
  static base::NoDestructor<ProtocolHandlerMap> handler_map;
  return *handler_map;
}

void RegisterDownloadHandler(base::FilePath::StringType extension,
                             DownloadHandler handler) {
  DCHECK(extension.length() > 2 && extension[0] == '.');
  DCHECK(GetDownloadHandlerMap().count(extension) == 0);
  GetDownloadHandlerMap()[extension] = std::move(handler);
}

void RegisterProtocolHandler(std::string protocol, ProtocolHandler handler) {
  DCHECK(GetProtocolHandlerMap().count(protocol) == 0);
  GetProtocolHandlerMap()[protocol] = std::move(handler);
}

bool HandleDownload(Profile* profile, download::DownloadItem* download) {
  const auto handler =
      GetDownloadHandlerMap().find(download->GetTargetFilePath().Extension());
  if (handler == GetDownloadHandlerMap().end())
    return false;
  return handler->second.Run(profile, download);
}

bool HandleProtocol(Profile* profile, GURL url) {
  const auto handler = GetProtocolHandlerMap().find(url.scheme());
  if (handler == GetProtocolHandlerMap().end())
    return false;
  return handler->second.Run(profile, url);
}

}  // namespace vivaldi