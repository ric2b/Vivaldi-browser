// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_create_info.h"

#include <string>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace content {

DownloadCreateInfo::DownloadCreateInfo(const base::Time& start_time,
                                       const net::BoundNetLog& bound_net_log,
                                       scoped_ptr<DownloadSaveInfo> save_info,
                                       bool open_when_done)
    : download_id(DownloadItem::kInvalidId),
      start_time(start_time),
      total_bytes(0),
      has_user_gesture(false),
      transition_type(ui::PAGE_TRANSITION_LINK),
      result(DOWNLOAD_INTERRUPT_REASON_NONE),
      save_info(std::move(save_info)),
      request_bound_net_log(bound_net_log),
      open_when_finished(open_when_done) {}

DownloadCreateInfo::DownloadCreateInfo()
    : DownloadCreateInfo(base::Time(),
                         net::BoundNetLog(),
                         make_scoped_ptr(new DownloadSaveInfo),
                         false) {}

DownloadCreateInfo::~DownloadCreateInfo() {}

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}

}  // namespace content
