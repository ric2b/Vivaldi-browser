// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source.h"

#include <stddef.h>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_restrictions.h" // VivaldiScopedAllowBlocking
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#include "app/vivaldi_constants.h"
#include "components/datasource/synced_file_data_source.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/datasource/css_mods_data_source.h"
#include "components/datasource/local_image_data_source.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "components/datasource/desktop_data_source_win.h"
#endif  // BUILDFLAG(IS_WIN)

VivaldiDataSource::VivaldiDataSource(Profile* profile)
    : profile_(profile->GetOriginalProfile()) {
  std::vector<std::pair<PathType, std::unique_ptr<VivaldiDataClassHandler>>>
      handlers;
#if BUILDFLAG(IS_WIN)
  handlers.emplace_back(
      PathType::kDesktopWallpaper,
      std::make_unique<DesktopWallpaperDataClassHandlerWin>());
#endif  // BUILDFLAG(IS_WIN)
#if !BUILDFLAG(IS_ANDROID)
  handlers.emplace_back(PathType::kLocalPath,
                        std::make_unique<LocalImageDataClassHandler>(
                            VivaldiImageStore::kPathMappingUrl));
  handlers.emplace_back(PathType::kImage,
                        std::make_unique<LocalImageDataClassHandler>(
                            VivaldiImageStore::kImageUrl));
  handlers.emplace_back(PathType::kCSSMod,
                        std::make_unique<CSSModsDataClassHandler>());
  handlers.emplace_back(PathType::kDirectMatch,
                        std::make_unique<LocalImageDataClassHandler>(
                            VivaldiImageStore::kDirectMatchImageUrl));
#endif  // !BUILDFLAG(IS_ANDROID)
  handlers.emplace_back(PathType::kSyncedStore,
                        std::make_unique<SyncedFileDataClassHandler>());

  data_class_handlers_ =
      base::flat_map<PathType, std::unique_ptr<VivaldiDataClassHandler>>(
          std::move(handlers));
}

VivaldiDataSource::~VivaldiDataSource() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

std::string VivaldiDataSource::GetSource() {
  return vivaldi::kVivaldiUIDataHost;
}

void VivaldiDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string data;
  std::optional<PathType> type =
      vivaldi_data_url_utils::ParsePath(url.path_piece(), &data);
  if (type) {
    auto it = data_class_handlers_.find(*type);
    if (it != data_class_handlers_.end()) {
      it->second->GetData(profile_, data, std::move(callback));
      return;
    }
  }
  std::move(callback).Run(nullptr);
}

std::string VivaldiDataSource::GetMimeType(const GURL& url) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  // The |path| received here had its first '/' stripped

#if BUILDFLAG(IS_LINUX)
  // Determining mime-type triggers file access on Linux. We must keep this
  // synchronous. TODO: Can we avoid it? Original issue VB-109734
  base::VivaldiScopedAllowBlocking allow_blocking;
#endif

  std::string data;
  std::optional<PathType> type =
      vivaldi_data_url_utils::ParsePath(url.path_piece(), &data);
  if (type) {
    auto it = data_class_handlers_.find(*type);
    if (it != data_class_handlers_.end()) {
      return it->second->GetMimetype(profile_, data);
    }
  }

  return vivaldi_data_url_utils::kMimeTypePNG;
}

bool VivaldiDataSource::AllowCaching(const GURL& url) {
  std::optional<PathType> type = vivaldi_data_url_utils::ParsePath(url.path());
  return type == PathType::kLocalPath || type == PathType::kImage ||
         type == PathType::kDirectMatch;
}

/*
 * Code to handle the chrome://thumb/ protocol.
 */

VivaldiThumbDataSource::VivaldiThumbDataSource(Profile* profile)
    : VivaldiDataSource(profile) {}

VivaldiThumbDataSource::~VivaldiThumbDataSource() {}

std::string VivaldiThumbDataSource::GetSource() {
  return vivaldi::kVivaldiThumbDataHost;
}
