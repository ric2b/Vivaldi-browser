// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source.h"

#include <memory>
#include <stddef.h>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "components/datasource/css_mods_data_source.h"
#include "components/datasource/local_image_data_source.h"
#include "components/datasource/notes_attachment_data_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "components/datasource/desktop_data_source_win.h"
#endif  // defined(OS_WIN)

VivaldiDataSource::VivaldiDataSource(Profile* profile) :
profile_(profile->GetOriginalProfile()) {

  std::vector<std::pair<PathType, std::unique_ptr<VivaldiDataClassHandler>>>
      handlers;
#if defined(OS_WIN)
  handlers.emplace_back(
      PathType::kDesktopWallpaper,
      std::make_unique<DesktopWallpaperDataClassHandlerWin>());
#endif  // defined(OS_WIN)
  handlers.emplace_back(PathType::kLocalPath,
                        std::make_unique<LocalImageDataClassHandler>(
                            VivaldiDataSourcesAPI::PATH_MAPPING_URL));
  handlers.emplace_back(PathType::kThumbnail,
                        std::make_unique<LocalImageDataClassHandler>(
                            VivaldiDataSourcesAPI::THUMBNAIL_URL));
  handlers.emplace_back(
      PathType::kNotesAttachment,
      std::make_unique<NotesAttachmentDataClassHandler>());
  handlers.emplace_back(PathType::kCSSMod,
                        std::make_unique<CSSModsDataClassHandler>());

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
  base::Optional<PathType> type =
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

std::string VivaldiDataSource::GetMimeType(const std::string& path) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return vivaldi_data_url_utils::GetPathMimeType(path);
}

bool VivaldiDataSource::AllowCaching(const std::string& path) {
  base::Optional<PathType> type = vivaldi_data_url_utils::ParsePath(path);
  return type == PathType::kLocalPath || type == PathType::kThumbnail;
}

/*
 * Code to handle the chrome://thumb/ protocol.
 */

VivaldiThumbDataSource::VivaldiThumbDataSource(Profile* profile)
  : VivaldiDataSource(profile) {
}

VivaldiThumbDataSource::~VivaldiThumbDataSource() {
}

std::string VivaldiThumbDataSource::GetSource() {
  return vivaldi::kVivaldiThumbDataHost;
}
