// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/thumbnails/vivaldi_capture_contents.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

// Time intervals used by the logic that times out content loading.
constexpr base::TimeDelta kMaxWaitForCapture = base::TimeDelta::FromSeconds(30);

}  // namespace

namespace vivaldi {

int ThumbnailCaptureContents::s_capture_request_id = 1;

ThumbnailCaptureContents::ThumbnailCaptureContents(
    content::BrowserContext* context)
    : browser_context_(context),
      capture_page_(base::MakeRefCounted<CaptureLoadedPage>(browser_context_)) {
}

ThumbnailCaptureContents::~ThumbnailCaptureContents() {
  DVLOG(1) << "Destroying ThumbnailCaptureContents for start_url=" << start_url_.spec();
}

void ThumbnailCaptureContents::Start(const GURL& start_url,
                                     const gfx::Size& initial_size,
                                     const gfx::Size& scaled_size,
                                     CapturePageAndScaleDoneCallback callback) {
  start_url_ = start_url;
  scaled_size_ = scaled_size;
  capture_size_ = initial_size;
  callback_ = std::move(callback);
  DVLOG(1) << "Starting ThumbnailCaptureContents with initial size of "
           << initial_size.ToString() << " for start_url=" << start_url_.spec();

  // Create the WebContents to contain the off-screen tab's page.
  WebContents::CreateParams params(
      Profile::FromBrowserContext(browser_context_));

  offscreen_tab_web_contents_ = WebContents::Create(params);
  offscreen_tab_web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(offscreen_tab_web_contents_.get());

  // Set initial size, if specified.
  if (!initial_size.IsEmpty()) {
    ResizeWebContents(offscreen_tab_web_contents_.get(),
                      gfx::Rect(initial_size));
  }

  // Mute audio output.  When tab capture starts, the audio will be
  // automatically unmuted, but will be captured into the MediaStream.
  offscreen_tab_web_contents_->SetAudioMuted(true);

  // Navigate to the initial URL.
  content::NavigationController::LoadURLParams load_params(start_url_);
  load_params.should_replace_current_entry = true;
  load_params.should_clear_history_list = true;
  offscreen_tab_web_contents_->GetController().LoadURLWithParams(load_params);
}

void ThumbnailCaptureContents::ResetWebContents() {
  offscreen_tab_web_contents_.reset();
}

void ThumbnailCaptureContents::Close() {
  if (capture_timeout_timer_.IsRunning()) {
    capture_timeout_timer_.Stop();
  }
  if (offscreen_tab_web_contents_) {
    offscreen_tab_web_contents_->ClosePage();
  }
}

void ThumbnailCaptureContents::CloseContents(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Javascript in the page called window.close().
  DVLOG(1) << "ThumbnailCaptureContents for start_url=" << start_url_.spec()
           << " will die";
  // |this| is no longer valid.
}

bool ThumbnailCaptureContents::ShouldSuppressDialogs(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Suppress all because there is no possible direct user interaction with
  // dialogs.
  // TODO(crbug.com/734191): This does not suppress window.print().
  return true;
}

bool ThumbnailCaptureContents::ShouldFocusLocationBarByDefault(
    WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Indicate the location bar should be focused instead of the page, even
  // though there is no location bar.  This will prevent the page from
  // automatically receiving input focus, which should never occur since there
  // is not supposed to be any direct user interaction.
  return true;
}

bool ThumbnailCaptureContents::ShouldFocusPageAfterCrash() {
  // Never focus the page.  Not even after a crash.
  return false;
}

void ThumbnailCaptureContents::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::RepeatingCallback<void(bool)>& callback) {
  // Offscreen tab pages are not allowed to download files.
  callback.Run(false);
}

bool ThumbnailCaptureContents::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Context menus should never be shown.  Do nothing, but indicate the context
  // menu was shown so that default implementation in libcontent does not
  // attempt to do so on its own.
  return true;
}

content::KeyboardEventProcessingResult
ThumbnailCaptureContents::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all keyboard events before they can be sent to the
  // renderer.
  return content::KeyboardEventProcessingResult::HANDLED;
}

bool ThumbnailCaptureContents::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all gesture events before they can be sent to the
  // renderer.
  return true;
}

bool ThumbnailCaptureContents::CanDragEnter(
    WebContents* source,
    const content::DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Halt all drag attempts onto the page since there should be no direct user
  // interaction with it.
  return false;
}

bool ThumbnailCaptureContents::ShouldCreateWebContents(
    content::WebContents* web_contents,
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), web_contents);
  // Disallow creating separate WebContentses.  The WebContents implementation
  // uses this to spawn new windows/tabs, which is also not allowed for
  // offscreen tabs.
  return false;
}

bool ThumbnailCaptureContents::EmbedsFullscreenWidget() const {
  return false;
}

void ThumbnailCaptureContents::RequestMediaAccessPermission(
    WebContents* contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  blink::MediaStreamDevices devices;

  std::move(callback).Run(devices, blink::MEDIA_DEVICE_INVALID_STATE, nullptr);
}

bool ThumbnailCaptureContents::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::MediaStreamType type) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(),
            content::WebContents::FromRenderFrameHost(render_frame_host));
  return type == blink::MEDIA_GUM_TAB_AUDIO_CAPTURE ||
         type == blink::MEDIA_GUM_TAB_VIDEO_CAPTURE;
}

void ThumbnailCaptureContents::DidStartLoading() {
  DCHECK(offscreen_tab_web_contents_.get());

  // Start timeout.
  capture_timeout_timer_.Start(
    FROM_HERE, kMaxWaitForCapture,
    base::BindRepeating(&ThumbnailCaptureContents::OnCaptureTimeout, this));
}

void ThumbnailCaptureContents::DidRedirectNavigation(
  content::NavigationHandle* navigation_handle) {
}

void ThumbnailCaptureContents::CapturedAndScaledCallback(const SkBitmap& bitmap,
                                                         int callback_id,
                                                         bool success) {
  DCHECK_EQ(callback_id, current_request_id_);
  DCHECK(!callback_.is_null());

  if (callback_id == current_request_id_) {
    callback_.Run(bitmap, success);

    ResetWebContents();
  }
}

void ThumbnailCaptureContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // We're only interesting in the event on the top frame, so ignore others.
  if (render_frame_host->GetParent()) {
    return;
  }
  if (capture_timeout_timer_.IsRunning()) {
    capture_timeout_timer_.Stop();
  }
  // If we're showing some error page, send error.
  content::NavigationEntry* active =
      offscreen_tab_web_contents_->GetController().GetVisibleEntry();
  if (active->GetPageType() == content::PageType::PAGE_TYPE_ERROR) {
    callback_.Run(SkBitmap(), false);
    ResetWebContents();
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&ThumbnailCaptureContents::OnCapture, this),
      base::TimeDelta::FromSeconds(1));
}

void ThumbnailCaptureContents::OnCapture() {
  if (!offscreen_tab_web_contents_) {
    // In some cases, the page will finish loading, then do a new
    // js initiated load after the web contents has been deleted.
    return;
  }
  if (offscreen_tab_web_contents_->IsLoading()) {
    // We're loading again, delay capturing.
    return;
  }
  CaptureLoadedPage::CaptureParams params;

  params.scaled_size_ = scaled_size_;
  params.full_page_ = false;
  params.capture_size_ = capture_size_;

  DCHECK_EQ(current_request_id_, 0);

  current_request_id_ = s_capture_request_id++;
  capture_page_->CaptureAndScale(
    offscreen_tab_web_contents_.get(), params, current_request_id_,
    base::Bind(&ThumbnailCaptureContents::CapturedAndScaledCallback, this));
}

void ThumbnailCaptureContents::RenderProcessGone(base::TerminationStatus status) {
  if (status == base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED) {
    LOG(ERROR) << "Render process capturing thumbnail crashed on " << start_url_.spec();
    callback_.Run(SkBitmap(), false);
    ResetWebContents();

    if (capture_timeout_timer_.IsRunning()) {
      capture_timeout_timer_.Stop();
    }
  }
}

void ThumbnailCaptureContents::OnCaptureTimeout() {
  if (offscreen_tab_web_contents_ &&
      !offscreen_tab_web_contents_->IsLoading()) {
    // If it's not loading anymore, capture it.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&ThumbnailCaptureContents::OnCapture, this),
      base::TimeDelta::FromSeconds(1));
    return;
  }

  LOG(WARNING) << "Timeout capturing " << start_url_.spec();
  callback_.Run(SkBitmap(), false);
  ResetWebContents();
}

}  // namespace namespace vivaldi
