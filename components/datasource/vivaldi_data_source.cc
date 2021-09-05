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
#include "base/message_loop/message_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
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

namespace {
#if defined(OS_WIN)
const char kDesktopImageType[] = "desktop-image";
#endif
const char kCSSModsType[] = "css-mods";
const char kCSSModsData[] = "css";
const char kCSSModsExtenssion[] = ".css";

}

VivaldiDataSource::VivaldiDataSource(Profile* profile) {
  profile = profile->GetOriginalProfile();

  std::vector<std::pair<std::string, std::unique_ptr<VivaldiDataClassHandler>>>
      handlers;
#if defined(OS_WIN)
  handlers.emplace_back(
      kDesktopImageType,
      std::make_unique<DesktopWallpaperDataClassHandlerWin>());
#endif  // defined(OS_WIN)
  handlers.emplace_back(
      VIVALDI_DATA_URL_PATH_MAPPING_DIR,
      std::make_unique<LocalImageDataClassHandler>(
          profile, extensions::VivaldiDataSourcesAPI::PATH_MAPPING_URL));
  handlers.emplace_back(
      vivaldi::kVivaldiDataUrlThumbnailDir,
      std::make_unique<LocalImageDataClassHandler>(
          profile, extensions::VivaldiDataSourcesAPI::THUMBNAIL_URL));
  handlers.emplace_back(
      VIVALDI_DATA_URL_NOTES_ATTACHMENT,
      std::make_unique<NotesAttachmentDataClassHandler>(profile));
  handlers.emplace_back(
      kCSSModsType, std::make_unique<CSSModsDataClassHandler>(profile));

  data_class_handlers_ =
      base::flat_map<std::string, std::unique_ptr<VivaldiDataClassHandler>>(
          std::move(handlers));
}

VivaldiDataSource::~VivaldiDataSource() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

std::string VivaldiDataSource::GetSource() {
  return vivaldi::kVivaldiUIDataHost;
}

bool VivaldiDataSource::IsCSSRequest(const std::string& path) const {
  base::StringPiece type;
  base::StringPiece data;

  ExtractRequestTypeAndData(path, type, data);

  return type == kCSSModsType &&
      (data == kCSSModsData || data.ends_with(kCSSModsExtenssion));
}

void VivaldiDataSource::StartDataRequest(
    const GURL& path,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  base::StringPiece type;
  base::StringPiece data;

  ExtractRequestTypeAndData(path.path_piece(), type, data);

  auto it = data_class_handlers_.find(type);
  if (it != data_class_handlers_.end()) {
    it->second->GetData(data.as_string(), std::move(callback));
    return;
  }
  std::move(callback).Run(nullptr);
}

// In a url such as chrome://vivaldi-data/desktop-image/0 type is
// "desktop-image" and data is "0".
void VivaldiDataSource::ExtractRequestTypeAndData(
    base::StringPiece path,
    base::StringPiece& type,
    base::StringPiece& data) const {
  if (!path.empty() && path[0] == '/') {
    path = path.substr(1);
  }
  size_t pos = path.find("bookmark_thumbnail");
  if (pos != std::string::npos) {
    // This is a shortcut path to handle old-style
    // bookmark thumbnail links.
    pos = path.find('/', pos);
    if (pos != std::string::npos) {
      data = path.substr(pos + 1);
      type = vivaldi::kVivaldiDataUrlThumbnailDir;

      // Strip all after ? for links such as
      // chrome://thumb/http://bookmark_thumbnail/610?1535393294240
      pos = data.find('?');
      if (pos != std::string::npos) {
        data = data.substr(0, pos);
      }
      return;
    }
  }
  // Default path for new links.
  pos = path.find('/');
  if (pos != std::string::npos) {
    type = path.substr(0, pos);
    data = path.substr(pos+1);
  } else {
    type = path;
    data = base::StringPiece();
  }
  // Strip all after ?
  pos = data.find('?');
  if (pos != std::string::npos) {
    data = data.substr(0, pos);
  }
}

std::string VivaldiDataSource::GetMimeType(const std::string& path) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  if (IsCSSRequest(path)) {
    return "text/css";
  }
  return "image/png";
}

bool VivaldiDataSource::AllowCaching() {
  return false;
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
