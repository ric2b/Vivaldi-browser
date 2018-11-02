// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_handler.h"

#include "content/browser/loader/resource_request_info_impl.h"

namespace content {

ResourceHandler::Delegate::Delegate() {}

ResourceHandler::Delegate::~Delegate() {}

void ResourceHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

ResourceHandler::~ResourceHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ResourceHandler::ResourceHandler(net::URLRequest* request)
    : request_(request) {}

void ResourceHandler::HoldController(
    std::unique_ptr<ResourceController> controller) {
  controller_ = std::move(controller);
}

std::unique_ptr<ResourceController> ResourceHandler::ReleaseController() {
  DCHECK(controller_);

  return std::move(controller_);
}

void ResourceHandler::Resume() {
  ReleaseController()->Resume();
}

void ResourceHandler::Cancel() {
  ReleaseController()->Cancel();
}

void ResourceHandler::CancelAndIgnore() {
  ReleaseController()->CancelAndIgnore();
}

void ResourceHandler::CancelWithError(int error_code) {
  ReleaseController()->CancelWithError(error_code);
}

void ResourceHandler::OutOfBandCancel(int error_code, bool tell_renderer) {
  delegate_->OutOfBandCancel(error_code, tell_renderer);
}

ResourceRequestInfoImpl* ResourceHandler::GetRequestInfo() const {
  return ResourceRequestInfoImpl::ForRequest(request_);
}

int ResourceHandler::GetRequestID() const {
  return GetRequestInfo()->GetRequestID();
}

ResourceMessageFilter* ResourceHandler::GetFilter() const {
  return GetRequestInfo()->requester_info()->filter();
}

/* NOTE(yngve): Risk of infinite loop, should only be a problem for us,
 * if we add new subclasses, chromium will still use the abstract definition.
 */
void ResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  OnResponseStarted(response, std::move(controller), false, false);
}

void ResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller,
    bool open_when_done,
    bool ask_for_target) {
  OnResponseStarted(response, std::move(controller));
}


}  // namespace content
