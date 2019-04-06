// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "content/public/browser/web_contents_delegate.h"

#include "base/bind.h"

namespace content {

bool WebContentsDelegate::HasOwnerShipOfContents() {
  return true;
}

void WebContentsDelegate::SetDownloadInformation(
    const content::DownloadInformation& info) {
  download_info_ = info;
}

DownloadInformation::DownloadInformation(
    int64_t size,
    const std::string& mimetype,
    const base::string16& suggested_filename)
    : size(size), mime_type(mimetype), suggested_filename(suggested_filename) {}

DownloadInformation::DownloadInformation(const DownloadInformation& old)
    : size(old.size),
      mime_type(old.mime_type),
      suggested_filename(old.suggested_filename),
      open_flags_cb(old.open_flags_cb) {}

DownloadInformation::DownloadInformation() : size(0) {}

DownloadInformation::~DownloadInformation() {}

}  // namespace content
