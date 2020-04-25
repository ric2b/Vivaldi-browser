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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "components/datasource/desktop_data_source_win.h"
#endif  // defined(OS_WIN)

namespace {
const char kDesktopImageType[] = "desktop-image";
const char kLocalImageType[] = "local-image";
const char kCSSModsType[] = "css-mods";
const char kCSSModsData[] = "css";
const char kDefaultFallbackImageBase64[] = "R0lGODlhAQABAAAAACwAAAAAAQABAAA=";

}

VivaldiDataSource::VivaldiDataSource(Profile* profile) {
  profile = profile->GetOriginalProfile();

  // Initialize data_class_handlers_ early as it is accessed from UI and IO
  // threads in StartDataRequest.
#if defined(OS_WIN)
  data_class_handlers_.insert(
      std::make_pair(kDesktopImageType,
                     std::make_unique<DesktopWallpaperDataClassHandlerWin>()));
#endif  // defined(OS_WIN)
  data_class_handlers_.insert(std::make_pair(
      kLocalImageType, std::make_unique<LocalImageDataClassHandler>(profile)));
  data_class_handlers_.insert(std::make_pair(
      kCSSModsType, std::make_unique<CSSModsDataClassHandler>(profile)));

  std::string data;
  base::Base64Decode(kDefaultFallbackImageBase64, &data);
  fallback_image_data_ =
    new base::RefCountedBytes(
      reinterpret_cast<const unsigned char *>
        (data.c_str()), (size_t)data.length());
}

VivaldiDataSource::~VivaldiDataSource() {
  // URLDatamanager in Chromium ensures that all URLDataSource instances
  // are deleted on the UI thread after all StartDataRequest completed. So we
  // cannot race here against any StartDataRequest calls and it is
  // safe to destruct any data the instance owns including all
  // VivaldiDataClassHandler implementations.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

std::string VivaldiDataSource::GetSource() {
  return vivaldi::kVivaldiUIDataHost;
}

bool VivaldiDataSource::IsCSSRequest(const std::string& path) const {
  std::string type;
  std::string data;

  ExtractRequestTypeAndData(path, type, data);

  size_t css_offset = data.find(".css");
  if (type == kCSSModsType &&
      (data == kCSSModsData || css_offset == data.length() - 4)) {
    return true;
  }
  return false;
}

scoped_refptr<base::SingleThreadTaskRunner>
VivaldiDataSource::TaskRunnerForRequestPath(const std::string& path) {
  if (IsCSSRequest(path)) {
    // Use UI thread to load CSS since its construction touches non-thread-safe
    // gfx::Font names in ui::ResourceBundle.
    return base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::UI});
  }
  // nullptr means the IO thread.
  return nullptr;
}

void VivaldiDataSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string type;
  std::string data;

  ExtractRequestTypeAndData(path, type, data);

  bool success = false;
  auto it = data_class_handlers_.find(type);
  if (it != data_class_handlers_.end()) {
    success = it->second->GetData(data, callback);
  } else {
    NOTIMPLEMENTED();
  }
  if (!success) {
    callback.Run(fallback_image_data_);
    return;
  }
}

// In a url such as chrome://vivaldi-data/desktop-image/0 type is
// "desktop-image" and data is "0".
void VivaldiDataSource::ExtractRequestTypeAndData(const std::string& path,
                                                  std::string& type,
                                                  std::string& data) const {
  size_t pos = path.find("bookmark_thumbnail");
  if (pos != std::string::npos) {
    // This is a shortcut path to handle old-style
    // bookmark thumbnail links.
    pos = path.find('/', pos);
    data = path.substr(pos + 1);
    type = "local-image";

    // Strip all after ? for links such as
    // chrome://thumb/http://bookmark_thumbnail/610?1535393294240
    pos = data.find('?');
    if (pos != std::string::npos) {
      data = data.substr(0, pos);
    }
    return;
  }
  // Default path for new links.
  pos = path.find('/');
  if (pos != std::string::npos) {
    type = path.substr(0, pos);
    data = path.substr(pos+1);
  } else {
    type = path;
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

bool VivaldiDataSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
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
