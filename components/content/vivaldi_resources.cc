// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/download/download_resource_handler.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/throttling_resource_handler.h"
#include "content/public/browser/browser_thread.h"

namespace content {

void DetachableResourceHandler::SetOpenFlags(bool open_when_done,
                                             bool ask_for_target) {
  DCHECK(next_handler_.get());
  next_handler_->SetOpenFlags(open_when_done, ask_for_target);
}

void DownloadResourceHandler::SetOpenFlags(bool open_when_done,
                                           bool ask_for_target) {
  core_.open_when_done_ = open_when_done;
  core_.ask_for_target_ = ask_for_target;
}

void ThrottlingResourceHandler::SetDelegateOpenFlags(bool open_when_done,
                                                     bool ask_for_target) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (info) {
    info->set_ask_for_save_target(ask_for_target);
    info->set_open_when_downloaded(open_when_done);
    SetOpenFlags(open_when_done, ask_for_target);
  }
}

void LayeredResourceHandler::SetOpenFlags(bool open_when_done,
  bool ask_for_target) {
  DCHECK(next_handler_.get());
  next_handler_->SetOpenFlags(open_when_done, ask_for_target);
}

}  // namespace content
