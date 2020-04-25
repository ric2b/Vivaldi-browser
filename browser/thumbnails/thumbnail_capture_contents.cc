// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/thumbnails/thumbnail_capture_contents.h"

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
constexpr base::TimeDelta kMaxWaitForPageLoad =
    base::TimeDelta::FromSeconds(30);

}  // namespace

namespace vivaldi {

//static
void ThumbnailCaptureContents::Start(content::BrowserContext* browser_context,
                                     const GURL& start_url,
                                     gfx::Size initial_size,
                                     gfx::Size target_size,
                                     CapturePage::DoneCallback callback) {
  ThumbnailCaptureContents* capture = new ThumbnailCaptureContents();
  capture->StartImpl(browser_context, start_url, initial_size, target_size,
                     std::move(callback));
}

ThumbnailCaptureContents::ThumbnailCaptureContents()
    : weak_ptr_factory_(this) {
}

ThumbnailCaptureContents::~ThumbnailCaptureContents() {
  DVLOG(1) << "Destroying ThumbnailCaptureContents for start_url="
           << start_url_.spec();
}

void ThumbnailCaptureContents::StartImpl(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    gfx::Size initial_size,
    gfx::Size target_size,
    CapturePage::DoneCallback callback) {
  DCHECK(!initial_size.IsEmpty());
  DCHECK(!target_size.IsEmpty());
  start_url_ = start_url;
  target_size_ = target_size;
  callback_ = std::move(callback);
  DVLOG(1) << "Starting ThumbnailCaptureContents with initial size of "
           << initial_size.ToString() << " for start_url=" << start_url_.spec();

  // Create the WebContents to contain the off-screen tab's page.
  WebContents::CreateParams params(
      Profile::FromBrowserContext(browser_context));

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

  // Start load timeout.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(&ThumbnailCaptureContents::OnPageLoadTimeout,
                          weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForPageLoad);
}

void ThumbnailCaptureContents::RespondAndDelete(CapturePage::Result captured) {
  CapturePage::DoneCallback callback = std::move(callback_);
  delete this;
  std::move(callback).Run(std::move(captured));
}

void ThumbnailCaptureContents::CloseContents(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Javascript in the page called window.close().
  DVLOG(1) << "ThumbnailCaptureContents for start_url=" << start_url_.spec()
           << " will die";
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
    base::OnceCallback<void(bool)> callback) {
  // Offscreen tab pages are not allowed to download files.
  std::move(callback).Run(false);
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

bool ThumbnailCaptureContents::EmbedsFullscreenWidget() {
  return false;
}

void ThumbnailCaptureContents::RequestMediaAccessPermission(
    WebContents* contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  blink::MediaStreamDevices devices;

  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
}

bool ThumbnailCaptureContents::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(),
            content::WebContents::FromRenderFrameHost(render_frame_host));
  return type == blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
}

void ThumbnailCaptureContents::DidStartLoading() {
  DCHECK(offscreen_tab_web_contents_.get());
}

void ThumbnailCaptureContents::DidRedirectNavigation(
  content::NavigationHandle* navigation_handle) {
}

void ThumbnailCaptureContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // We're only interesting in the event on the top frame, so ignore others.
  if (render_frame_host->GetParent()) {
    return;
  }
  // If we're showing some error page, send error.
  content::NavigationEntry* active =
      offscreen_tab_web_contents_->GetController().GetVisibleEntry();
  if (active->GetPageType() == content::PageType::PAGE_TYPE_ERROR) {
    LOG(ERROR) << "page load error";
    RespondAndDelete();
    return;
  }

  next_capture_try_wait_ = base::TimeDelta::FromSeconds(1);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCaptureContents::TryCapture,
                     weak_ptr_factory_.GetWeakPtr(), false),
      next_capture_try_wait_);
}

void ThumbnailCaptureContents::TryCapture(bool last_try) {
  // We have two independent timers that call TryCapture, the one initiated
  // in DidFinishLoad with subsequent retry attempts below when last_try is
  // false and the page load timeout when last_try is true. Protect aganst one
  // of the timers expring while another has aready succeeded to start the
  // capturing.
  if (capture_started_) {
    return;
  }

  if (!offscreen_tab_web_contents_ ||
      offscreen_tab_web_contents_->IsLoading()) {
    // In some cases, the page will finish loading, then do a new
    // js initiated load after the web contents has been deleted.
    if (last_try) {
      LOG(ERROR) << "timeout loading the page";
      RespondAndDelete();
      return;
    }
    // Exponential delay increase.
    next_capture_try_wait_ += next_capture_try_wait_;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ThumbnailCaptureContents::TryCapture,
                      weak_ptr_factory_.GetWeakPtr(), false),
        next_capture_try_wait_);
    return;
  }

  // We only try capturing once for offscreen contents. We leave params.rect
  // empty to capture the full visible area.
  CapturePage::CaptureParams params;
  params.full_page = false;
  params.once_per_contents = true;
  params.target_size = target_size_;

  CapturePage::Capture(offscreen_tab_web_contents_.get(), params,
                       base::Bind(&ThumbnailCaptureContents::RespondAndDelete,
                                  weak_ptr_factory_.GetWeakPtr()));

  capture_started_ = true;
}

void ThumbnailCaptureContents::RenderProcessGone(base::TerminationStatus status) {
  if (status == base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED) {
    LOG(ERROR) << "render process capturing thumbnail crashed";
    RespondAndDelete();
  }
}

void ThumbnailCaptureContents::OnPageLoadTimeout() {
  // Try to start capture one last time.
  TryCapture(true);
}


}  // namespace namespace vivaldi
