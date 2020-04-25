// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/thumbnails/capture_page.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/api/extension_types.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/vivaldi_ui_utils.h"

using content::BrowserThread;
using content::RenderWidgetHost;
using content::WebContents;

namespace vivaldi {

namespace {

// Time to wait for the capture result before reporting an error.
constexpr base::TimeDelta kMaxWaitForCapture = base::TimeDelta::FromSeconds(30);

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  // Let std::unique_ptr destructor to release the mapping.
  std::unique_ptr<base::ReadOnlySharedMemoryMapping> mapping(
      static_cast<base::ReadOnlySharedMemoryMapping*>(context));
  DCHECK(mapping->memory() == addr);
}

}  // namespace

int CapturePage::s_callback_id = 0;

CapturePage::CapturePage() : weak_ptr_factory_(this) {}

CapturePage::~CapturePage() {}

// static
void CapturePage::Capture(content::WebContents* contents,
                          const CaptureParams& params,
                          DoneCallback callback) {
  DCHECK(callback);
  CapturePage* capture_page = new CapturePage();

  capture_page->CaptureImpl(contents, params, std::move(callback));
}

void CapturePage::CaptureImpl(content::WebContents* contents,
                              const CaptureParams& input_params,
                              DoneCallback callback) {
  DCHECK(!callback.is_null());
  capture_callback_ = std::move(callback);
  callback_id_ = ++s_callback_id;
  once_per_contents_ = input_params.once_per_contents;
  target_size_ = input_params.target_size;

  VivaldiViewMsg_RequestThumbnailForFrame_Params param;
  param.callback_id = callback_id_;
  param.rect = input_params.rect;
  param.full_page = input_params.full_page;
  param.target_size = input_params.target_size;

  WebContentsObserver::Observe(contents);

  contents->GetRenderViewHost()->Send(
      new VivaldiViewMsg_RequestThumbnailForFrame(
          contents->GetRenderViewHost()->GetRoutingID(), param));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(&CapturePage::OnCaptureTimeout,
                          weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForPageLoad);
}

void CapturePage::RespondAndDelete(Result captured) {
  // Delete before calling the callback to release all resources ASAP.
  DoneCallback callback = std::move(capture_callback_);
  delete this;
  std::move(callback).Run(std::move(captured));
}

void CapturePage::WebContentsDestroyed() {
  LOG(ERROR) << "WebContents was destroyed before the renderer replied";
  RespondAndDelete();
}

void CapturePage::RenderViewHostChanged(content::RenderViewHost* old_host,
                                        content::RenderViewHost* new_host) {
  LOG(ERROR) << "RenderViewHost was replaced before the renderer replied";
  RespondAndDelete();
}

bool CapturePage::OnMessageReceived(const IPC::Message& message) {
  DCHECK(WebContentsObserver::web_contents());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CapturePage, message)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_RequestThumbnailForFrame_ACK,
                        OnRequestThumbnailForFrameResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CapturePage::OnRequestThumbnailForFrameResponse(
    int callback_id,
    gfx::Size image_size,
    base::ReadOnlySharedMemoryRegion region) {
  Result captured;
  do {
    if (callback_id != callback_id_) {
      if (!once_per_contents_)
        return;
      LOG(ERROR) << "unexpected callback id " << callback_id << " when "
                << callback_id_ << " was expected";
      break;
    }

    if (!region.IsValid() || image_size.IsEmpty()) {
      LOG(ERROR) << "no data from the renderer process";
      break;
    }

    if (!target_size_.IsEmpty() && target_size_ != image_size) {
      LOG(ERROR) << "unexpected image size " << image_size.width() << "x"
                 << image_size.height() << " when " << target_size_.width()
                 << "x" << target_size_.height() << " was expected";
      break;
    }

    SkImageInfo info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    if (info.computeMinByteSize() != region.GetSize()) {
      LOG(ERROR) << "The image size does not match allocated memory";
      break;
    }

    captured.image_info_ = info;
    captured.region_ = std::move(region);
  } while (false);

  RespondAndDelete(std::move(captured));
}

void CapturePage::OnCaptureTimeout() {
  LOG(ERROR) << "timeout waiting for capture result";
  RespondAndDelete();
}

CapturePage::Result::Result() = default;
CapturePage::Result::~Result() = default;
CapturePage::Result::Result(Result&& other) = default;
CapturePage::Result& CapturePage::Result::operator=(Result&& other) = default;

bool CapturePage::Result::MovePixelsToBitmap(SkBitmap* bitmap) {
  if (!region_.IsValid())
    return false;

  // We transfer the ownership of the mapping into the bitmap, hence we need
  // an instance on the heap.
  auto mapping =
      std::make_unique<base::ReadOnlySharedMemoryMapping>(region_.Map());

  // Release the region now as the mapping is independent of it.
  region_ = base::ReadOnlySharedMemoryRegion();

  if (!mapping->IsValid()) {
    LOG(ERROR) << "failed to map the captured image data";
    return false;
  }

  // installPixels uses void*, not const void* for pixels.
  void* pixels = const_cast<void*>(mapping->memory());

  // SkBitmap calls the release function when it no longer access the memory
  // including failure cases hence calling mapping.release() does not leak
  // if installPixels returns false.
  void* sk_release_context = mapping.release();
  if (!bitmap->installPixels(image_info_, pixels, image_info_.minRowBytes(),
                             ReleaseSharedMemoryPixels, sk_release_context)) {
    LOG(ERROR) << "data could not be copied to bitmap";
    return false;
  }

  return true;
}

}  // namespace namespace vivaldi
