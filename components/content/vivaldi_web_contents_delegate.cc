// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "content/public/browser/web_contents_delegate.h"

#include "base/functional/bind.h"

namespace content {

void WebContentsDelegate::SetDownloadInformation(
    const content::DownloadInformation& info) {
  download_info_ = info;
}

DownloadInformation::DownloadInformation(
    int64_t size,
    const std::string& mimetype,
    const std::u16string& suggested_filename)
    : size(size), mime_type(mimetype), suggested_filename(suggested_filename) {}

DownloadInformation::DownloadInformation(const DownloadInformation& old)
    : size(old.size),
      mime_type(old.mime_type),
      suggested_filename(old.suggested_filename) {}

DownloadInformation::DownloadInformation() : size(-1) {}

DownloadInformation::~DownloadInformation() {}

}  // namespace content
