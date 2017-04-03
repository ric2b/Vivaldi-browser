// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_handler.h"

#include "content/browser/loader/resource_request_info_impl.h"

namespace content {

ResourceHandler::~ResourceHandler() {}

ResourceHandler::ResourceHandler(net::URLRequest* request)
    : controller_(NULL),
      request_(request) {
}

void ResourceHandler::SetController(ResourceController* controller) {
  controller_ = controller;
}

ResourceRequestInfoImpl* ResourceHandler::GetRequestInfo() const {
  return ResourceRequestInfoImpl::ForRequest(request_);
}

int ResourceHandler::GetRequestID() const {
  return GetRequestInfo()->GetRequestID();
}

ResourceMessageFilter* ResourceHandler::GetFilter() const {
  return GetRequestInfo()->filter();
}

/* NOTE(yngve): Risk of infinite loop, should only be a problem for us,
 * if we add new subclasses, chromium will still use the abstract definition.
 */
bool ResourceHandler::OnResponseStarted(ResourceResponse* response,
                       bool* defer) {
  return OnResponseStarted(response, defer, false, false);
}

bool ResourceHandler::OnResponseStarted(ResourceResponse* response,
                       bool* defer,
                       bool open_when_done,
                       bool ask_for_target) {
  return OnResponseStarted(response, defer);
}


}  // namespace content
