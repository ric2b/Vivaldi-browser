// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_render_view_observer.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/shared_memory.h"
#include "content/child/child_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_channel_proxy.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

using blink::WebDocument;
using blink::WebLocalFrame;
using blink::WebRect;
using blink::WebView;

namespace vivaldi {

VivaldiRenderViewObserver::VivaldiRenderViewObserver(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {}

VivaldiRenderViewObserver::~VivaldiRenderViewObserver() {}

void VivaldiRenderViewObserver::OnDestruct() {
  delete this;
}

bool VivaldiRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiMsg_InsertText, OnInsertText)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_RequestThumbnailForFrame,
                        OnRequestThumbnailForFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Inserts text into input fields.
void VivaldiRenderViewObserver::OnInsertText(const base::string16& text) {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  unsigned length = text.length();
  // We do not want any selection.
  frame->SetMarkedText(blink::WebString::FromUTF16(text), length, length);
  frame->UnmarkText();                         // Or marked text.
}

namespace {

bool CopyBitmapToSharedMem(const SkBitmap& bitmap,
                           base::SharedMemoryHandle* shared_mem_handle) {
  // Only 32-bit bitmaps are supported.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  const gfx::Size size(bitmap.width(), bitmap.height());
  std::unique_ptr<base::SharedMemory> shared_buf;
  {
    void* pixels = bitmap.getPixels();
    if (!pixels)
      return false;

    base::CheckedNumeric<uint32_t> checked_buf_size = 4;
    checked_buf_size *= size.width();
    checked_buf_size *= size.height();
    if (!checked_buf_size.IsValid())
      return false;

    // Allocate a shared memory buffer to hold the bitmap bits.
    uint32_t buf_size = checked_buf_size.ValueOrDie();
    shared_buf =
        content::ChildThreadImpl::current()->AllocateSharedMemory(buf_size);
    if (!shared_buf)
      return false;
    if (!shared_buf->Map(buf_size))
      return false;
    // Copy the bits into shared memory
    DCHECK(shared_buf->memory());
    memcpy(shared_buf->memory(), pixels, buf_size);
    shared_buf->Unmap();
  }
  *shared_mem_handle =
      base::SharedMemory::DuplicateHandle(shared_buf->handle());
  return true;
}

}  // namespace

void VivaldiRenderViewObserver::OnRequestThumbnailForFrame(
    VivaldiViewMsg_RequestThumbnailForFrame_Params params) {
  base::SharedMemoryHandle shared_memory_handle;
  if (!render_view()->GetWebView() ||
      !render_view()->GetWebView()->MainFrame()) {
    Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
        routing_id(), shared_memory_handle, gfx::Size(), params.callback_id,
        false));
  }
  blink::WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
  bool capture_full_page = params.full_page;
  gfx::Size size = params.size;
  SkBitmap bitmap;
  if (main_frame->snapshotPage(bitmap, capture_full_page, size.width(),
                               size.height())) {
    SkBitmap thumbnail;
    if (bitmap.colorType() == kN32_SkColorType) {
      thumbnail = bitmap;
    } else {
      if (thumbnail.tryAllocPixels(bitmap.info())) {
        bitmap.readPixels(thumbnail.info(), thumbnail.getPixels(),
                          thumbnail.rowBytes(), 0, 0);
      }
    }
    if (CopyBitmapToSharedMem(thumbnail, &shared_memory_handle)) {
      gfx::Size size(thumbnail.width(), thumbnail.height());

      Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
          routing_id(), shared_memory_handle, size, params.callback_id, true));
    }
  } else {
    Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
        routing_id(), shared_memory_handle, gfx::Size(), params.callback_id,
        false));
  }
  return;
}

}  // namespace vivaldi
