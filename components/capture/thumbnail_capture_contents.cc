// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/capture/thumbnail_capture_contents.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

#include "components/capture/capture_page.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/vivaldi_skia_utils.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

using content::WebContents;

namespace vivaldi {

namespace {

// For thumbnails in theory we can use RenderWidgetHostView::CopyFromSurface,
// instead of our IPC capture, but it works for some reason only on Mac.
constexpr bool use_CopyFromSurface = false;

// Time intervals used by the logic that times out content loading.
constexpr base::TimeDelta kMaxWaitForPageLoad = base::Seconds(30);

// As the capture involves IPC to the renderer process, we must be prepared that
// it becomes unresponsive.
constexpr base::TimeDelta kMaxWaitForCaptureResult = base::Seconds(15);

scoped_refptr<base::RefCountedMemory> ConvertToPNGOnWorkerThread(
    SkBitmap bitmap) {
  std::vector<unsigned char> data = ::vivaldi::skia_utils::EncodeBitmap(
      std::move(bitmap), ::vivaldi::skia_utils::ImageFormat::kPNG, 100);
  if (data.empty())
    return nullptr;
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(data));
}

}  // namespace

// static
ThumbnailCaptureContents* ThumbnailCaptureContents::Capture(content::BrowserContext* browser_context,
                                       const GURL& start_url,
                                       gfx::Size initial_size,
                                       gfx::Size target_size,
                                       CaptureCallback callback) {
  ThumbnailCaptureContents* capture = new ThumbnailCaptureContents();
  capture->Start(browser_context, start_url, initial_size, target_size,
                 std::move(callback));
  return capture;
}

ThumbnailCaptureContents::ThumbnailCaptureContents() = default;

ThumbnailCaptureContents::~ThumbnailCaptureContents() {
  DVLOG(1) << "Destroying ThumbnailCaptureContents for start_url="
           << start_url_.spec();
}

void ThumbnailCaptureContents::Start(content::BrowserContext* browser_context,
                                     const GURL& start_url,
                                     gfx::Size initial_size,
                                     gfx::Size target_size,
                                     CaptureCallback callback) {
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(offscreen_tab_web_contents_.get(),
      extensions::mojom::ViewType::kOffscreenDocument);
#endif

  WebContentsObserver::Observe(offscreen_tab_web_contents_.get());

  // Set initial size, if specified.
  if (!initial_size.IsEmpty()) {
    offscreen_tab_web_contents_.get()->Resize(gfx::Rect(initial_size));
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCaptureContents::OnPageLoadTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForPageLoad);
}

void ThumbnailCaptureContents::RespondAndDelete(SkBitmap bitmap) {
  CaptureCallback callback = std::move(callback_);
  delete this;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ConvertToPNGOnWorkerThread, std::move(bitmap)),
      std::move(callback));
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

bool ThumbnailCaptureContents::ShouldFocusPageAfterCrash(
    content::WebContents* source) {
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
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Context menus should never be shown.  Do nothing, but indicate the context
  // menu was shown so that default implementation in libcontent does not
  // attempt to do so on its own.
  return true;
}

content::KeyboardEventProcessingResult
ThumbnailCaptureContents::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
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
    blink::DragOperationsMask operations_allowed) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Halt all drag attempts onto the page since there should be no direct user
  // interaction with it.
  return false;
}

void ThumbnailCaptureContents::RequestMediaAccessPermission(
    WebContents* contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  blink::mojom::StreamDevicesSet devices;

  std::move(callback).Run(
      devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
}

bool ThumbnailCaptureContents::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
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
    content::NavigationHandle* navigation_handle) {}

void ThumbnailCaptureContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // We're only interesting in the event on the top frame, so ignore others.
  if (render_frame_host->GetParent()) {
    return;
  }

  next_capture_try_wait_ = base::Seconds(1);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCaptureContents::TryCapture,
                     weak_ptr_factory_.GetWeakPtr(), false),
      next_capture_try_wait_);
}

void ThumbnailCaptureContents::TryCapture(bool last_try) {
  // We have two independent timers that call TryCapture, the one initiated
  // in DidFinishLoad with subsequent retry attempts below when last_try is
  // false and the page load timeout when last_try is true. Protect aganst one
  // of the timers expring while another has already succeeded to start the
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ThumbnailCaptureContents::TryCapture,
                       weak_ptr_factory_.GetWeakPtr(), false),
        next_capture_try_wait_);
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

  if (use_CopyFromSurface) {
    content::RenderWidgetHostView* const view =
        offscreen_tab_web_contents_->GetRenderWidgetHostView();
    if (view) {
      view->CopyFromSurface(
          gfx::Rect(),  // Copy entire surface area.
          gfx::Size(),  // Result contains device-level detail.
          base::BindOnce(&ThumbnailCaptureContents::OnCopyImageReady,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      LOG(WARNING) << "Offscreen WebContent without RenderWidgetHostView";
      CaptureViaIpc();
    }
  } else {
    CaptureViaIpc();
  }

  // Start capture timeout.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCaptureContents::OnCaptureTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForCaptureResult);
  capture_started_ = true;
}

void ThumbnailCaptureContents::OnCaptureTimeout() {
  LOG(ERROR) << "Timeout while waiting for a capture result, aborting, url="
             << start_url_;
  RespondAndDelete();
}

void ThumbnailCaptureContents::CaptureViaIpc() {
  // We only try capturing once for offscreen contents. We leave params.rect
  // empty to capture the full visible area.
  CapturePage::CaptureParams params;
  params.full_page = false;
  params.target_size = target_size_;
  CapturePage::Capture(
      offscreen_tab_web_contents_.get(), params,
      base::BindOnce(&ThumbnailCaptureContents::OnIpcCaptureDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThumbnailCaptureContents::OnCopyImageReady(const SkBitmap& bitmap) {
  if (!bitmap.drawsNothing()) {
    RespondAndDelete(bitmap);
    return;
  }

  LOG(ERROR) << "CopyFromSurface failed, use IPC fallback, url= " << start_url_;
  CaptureViaIpc();
}

void ThumbnailCaptureContents::OnIpcCaptureDone(SkBitmap bitmap) {
  RespondAndDelete(std::move(bitmap));
}

void ThumbnailCaptureContents::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status == base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED) {
    LOG(ERROR) << "render process capturing thumbnail crashed";
    RespondAndDelete();
  }
}

void ThumbnailCaptureContents::OnPageLoadTimeout() {
  // Try to start capture one last time.
  TryCapture(true);
}

}  // namespace vivaldi
