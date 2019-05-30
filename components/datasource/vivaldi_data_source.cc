// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source.h"

#include <stddef.h>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#if defined(OS_WIN)
#include "components/datasource/desktop_data_source_win.h"
#endif  // defined(OS_WIN)
#include "components/datasource/local_image_data_source.h"
#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {
const char kDesktopImageType[] = "desktop-image";
const char kLocalImageType[] = "local-image";
const char kDefaultFallbackImageBase64[] = "R0lGODlhAQABAAAAACwAAAAAAQABAAA=";

}

VivaldiDataSource::VivaldiDataSource(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  std::string data = kDefaultFallbackImageBase64;
  base::Base64Decode(data, &data);
  fallback_image_data_ =
    new base::RefCountedBytes(
      reinterpret_cast<const unsigned char *>
        (data.c_str()), (size_t)data.length());
}

VivaldiDataSource::~VivaldiDataSource() {
}

VivaldiDataClassHandler* VivaldiDataSource::GetOrCreateDataClassHandlerForType(
    const std::string& type) {
  auto it = data_class_handlers_.find(type);
  if (it != data_class_handlers_.end()) {
    return it->second.get();
  }
  if (type == kDesktopImageType) {
#if defined(OS_WIN)
    return new DesktopWallpaperDataClassHandlerWin();
#else
    NOTIMPLEMENTED();
#endif  // defined(OS_WIN)
  } else if (type == kLocalImageType) {
    return new LocalImageDataClassHandler();
  }
  return nullptr;
}

std::string VivaldiDataSource::GetSource() const {
  return vivaldi::kVivaldiUIDataHost;
}

scoped_refptr<base::SingleThreadTaskRunner>
VivaldiDataSource::TaskRunnerForRequestPath(const std::string& path) const {
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

  VivaldiDataClassHandler* handler = GetOrCreateDataClassHandlerForType(type);
  if (!handler) {
    callback.Run(fallback_image_data_);
    return;
  }
  bool success = handler->GetData(profile_, data, callback);
  if (!success) {
    callback.Run(fallback_image_data_);
    return;
  }
}

// In a url such as chrome://vivaldi-data/desktop-image/0 type is
// "desktop-image" and data is "0".
void VivaldiDataSource::ExtractRequestTypeAndData(const std::string& path,
                                                  std::string& type,
                                                  std::string& data) {
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
  }
  // Strip all after ?
  pos = data.find('?');
  if (pos != std::string::npos) {
    data = data.substr(0, pos);
  }
}

std::string VivaldiDataSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool VivaldiDataSource::AllowCaching() const {
  return false;
}

bool VivaldiDataSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) const {
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}
/*
 * Code to handle the chrome://thumb/ protocol.
 */

VivaldiThumbDataSource::VivaldiThumbDataSource(Profile* profile)
  : VivaldiDataSource(profile), weak_ptr_factory_(this) {
}

VivaldiThumbDataSource::~VivaldiThumbDataSource() {
}

std::string VivaldiThumbDataSource::GetSource() const {
  return vivaldi::kVivaldiThumbDataHost;
}
