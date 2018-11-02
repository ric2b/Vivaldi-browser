// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/throttling_resource_handler.h"

#include <utility>

#include "content/browser/loader/resource_controller.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_response.h"

#include "content/browser/loader/resource_request_info_impl.h"

namespace content {

ThrottlingResourceHandler::ThrottlingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request,
    std::vector<std::unique_ptr<ResourceThrottle>> throttles)
    : LayeredResourceHandler(request, std::move(next_handler)),
      deferred_stage_(DEFERRED_NONE),
      throttles_(std::move(throttles)),
      next_index_(0),
      cancelled_by_resource_throttle_(false) {
  for (const auto& throttle : throttles_) {
    throttle->set_delegate(this);
    // Throttles must have a name, as otherwise, bugs where a throttle fails
    // to resume a request can be very difficult to debug.
    DCHECK(throttle->GetNameForLogging());
  }
}

ThrottlingResourceHandler::~ThrottlingResourceHandler() {
}

void ThrottlingResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());
  DCHECK(!cancelled_by_resource_throttle_);

  HoldController(std::move(controller));
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    bool defer = false;
    throttles_[index]->WillRedirectRequest(redirect_info, &defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return;

    if (defer) {
      OnRequestDeferred(index);
      deferred_stage_ = DEFERRED_REDIRECT;
      deferred_redirect_ = redirect_info;
      deferred_response_ = response;
      return;
    }
  }

  next_index_ = 0;  // Reset for next time.

  next_handler_->OnRequestRedirected(redirect_info, response,
                                     ReleaseController());
}

void ThrottlingResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!cancelled_by_resource_throttle_);
  DCHECK(!has_controller());

  HoldController(std::move(controller));
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    bool defer = false;
    throttles_[index]->WillStartRequest(&defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return;
    if (defer) {
      OnRequestDeferred(index);
      deferred_stage_ = DEFERRED_START;
      deferred_url_ = url;
      return;
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnWillStart(url, ReleaseController());
}

void ThrottlingResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller,
    bool open_when_done,
    bool ask_for_target) {
  DCHECK(!cancelled_by_resource_throttle_);
  DCHECK(!has_controller());

  HoldController(std::move(controller));
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    bool defer = false;
    throttles_[index]->WillProcessResponse(&defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return;
    if (defer) {
      OnRequestDeferred(index);
      deferred_stage_ = DEFERRED_RESPONSE;
      deferred_response_ = response;
      return;
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnResponseStarted(response, ReleaseController(),
                                          open_when_done, ask_for_target);
}

void ThrottlingResourceHandler::Cancel() {
  if (!has_controller()) {
    OutOfBandCancel(net::ERR_ABORTED, false /* tell_renderer */);
    return;
  }
  cancelled_by_resource_throttle_ = true;
  ResourceHandler::Cancel();
}

void ThrottlingResourceHandler::CancelWithError(int error_code) {
  if (!has_controller()) {
    OutOfBandCancel(error_code, false /* tell_renderer */);
    return;
  }
  cancelled_by_resource_throttle_ = true;
  ResourceHandler::CancelWithError(error_code);
}

void ThrottlingResourceHandler::Resume(bool open_when_done, bool ask_for_target) {
  // Throttles expect to be able to cancel requests out-of-band, so just do
  // nothing if one request resumes after another cancels. Can't even recognize
  // out-of-band cancels and for synchronous teardown, since don't know if the
  // currently active throttle called Cancel() or if it was another one.
  if (cancelled_by_resource_throttle_)
    return;

  DCHECK(has_controller());

  ResourceRequestInfoImpl* info = GetRequestInfo();
  if(info) {
    info->set_ask_for_save_target(ask_for_target);
    info->set_open_when_downloaded(open_when_done);
  }
  DeferredStage last_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  // Clear information about the throttle that delayed the request.
  request()->LogUnblocked();
  switch (last_deferred_stage) {
    case DEFERRED_NONE:
      NOTREACHED();
      break;
    case DEFERRED_START:
      ResumeStart();
      break;
    case DEFERRED_REDIRECT:
      ResumeRedirect();
      break;
    case DEFERRED_RESPONSE:
      ResumeResponse(open_when_done, ask_for_target);
      break;
  }
}

void ThrottlingResourceHandler::ResumeStart() {
  DCHECK(!cancelled_by_resource_throttle_);
  DCHECK(has_controller());

  GURL url = deferred_url_;
  deferred_url_ = GURL();

  OnWillStart(url, ReleaseController());
}

void ThrottlingResourceHandler::ResumeRedirect() {
  DCHECK(!cancelled_by_resource_throttle_);
  DCHECK(has_controller());

  net::RedirectInfo redirect_info = deferred_redirect_;
  deferred_redirect_ = net::RedirectInfo();
  scoped_refptr<network::ResourceResponse> response;
  deferred_response_.swap(response);

  OnRequestRedirected(redirect_info, response.get(), ReleaseController());
}

void ThrottlingResourceHandler::ResumeResponse(bool open_when_done, bool ask_for_target) {
  DCHECK(!cancelled_by_resource_throttle_);
  DCHECK(has_controller());

  scoped_refptr<network::ResourceResponse> response;
  deferred_response_.swap(response);

  OnResponseStarted(response.get(), ReleaseController(),
                    open_when_done, ask_for_target);
}

void ThrottlingResourceHandler::OnRequestDeferred(int throttle_index) {
  request()->LogBlockedBy(throttles_[throttle_index]->GetNameForLogging());
}

}  // namespace content
