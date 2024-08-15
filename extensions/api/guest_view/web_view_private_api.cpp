// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/guest_view/web_view_private_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"  // nogncheck
#include "content/browser/renderer_host/dip_util.h"               // nogncheck
#include "content/browser/web_contents/web_contents_impl.h"       // nogncheck
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"

#include "extensions/api/history/history_private_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/helper/vivaldi_frame_observer.h"
#include "extensions/schema/web_view_private.h"
#include "renderer/mojo/vivaldi_frame_service.mojom.h"

using content::BrowserPluginGuest;
using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::WebContents;
using content::WebContentsImpl;

namespace extensions {
namespace vivaldi {

// Copied from WebViewInternalExtensionFunction::PreRunValidation
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

bool VivaldiWebViewWithGuestFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  std::optional<int> instance_id = args()[0].GetIfInt();
  EXTENSION_FUNCTION_PRERUN_VALIDATE(instance_id.has_value());
  guest_ = WebViewGuest::FromInstanceID(
      render_frame_host()->GetProcess()->GetID(), instance_id.value());

  if (!guest_) {
    *error = "Could not find guest";
    return false;
  }
  return true;
}

namespace {
const float kDefaultThumbnailScale = 1.0f;
}  // namespace

WebViewPrivateGetThumbnailFunction::WebViewPrivateGetThumbnailFunction()
    : image_quality_(90 /*kDefaultQuality*/),  // Default quality setting.
      scale_(kDefaultThumbnailScale)  // Scale of window dimension to thumb.
{}

WebViewPrivateGetThumbnailFunction::~WebViewPrivateGetThumbnailFunction() =
    default;

ExtensionFunction::ResponseAction WebViewPrivateGetThumbnailFunction::Run() {
  using vivaldi::web_view_private::GetThumbnail::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RunImpl(params->params);
}

ExtensionFunction::ResponseAction WebViewPrivateGetThumbnailFunction::RunImpl(
    const vivaldi::web_view_private::ThumbnailParams& params) {
  if (params.scale) {
    scale_ = *params.scale;
  }
  if (params.width) {
    width_ = *params.width;
  }
  if (params.height) {
    height_ = *params.height;
  }

  WebContents* web_contents = guest_->web_contents();

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();

  if (!view) {
    return RespondNow(Error("View is not available, no screenshot taken."));
  }
  // If this happens, the guest view is not attached to a window for some
  // reason. See also VB-23154.
  DCHECK(guest_->embedder_web_contents());
  if (!guest_->embedder_web_contents()) {
    return RespondNow(
        Error("Guest view is not attached to a window, no screenshot taken."));
  }
  content::RenderWidgetHostView* embedder_view =
      guest_->embedder_web_contents()->GetRenderWidgetHostView();
  gfx::Point source_origin = view->GetViewBounds().origin() -
                             embedder_view->GetViewBounds().OffsetFromOrigin();
  gfx::Rect source_rect(source_origin, view->GetViewBounds().size());

  // Remove scrollbars from thumbnail (even if they're not here!)
  source_rect.set_width(
      std::max(1, source_rect.width() - gfx::scrollbar_size()));
  source_rect.set_height(
      std::max(1, source_rect.height() - gfx::scrollbar_size()));

  embedder_view->CopyFromSurface(
      source_rect, source_rect.size(),
      base::BindOnce(
          &WebViewPrivateGetThumbnailFunction::CopyFromBackingStoreComplete,
          this));

  return RespondLater();
}

void WebViewPrivateGetThumbnailFunction::CopyFromBackingStoreComplete(
    const SkBitmap& bitmap) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &WebViewPrivateGetThumbnailFunction::ScaleAndEncodeOnWorkerThread,
          this, bitmap),
      base::BindOnce(&WebViewPrivateGetThumbnailFunction::SendResult, this));
}

void WebViewPrivateGetThumbnailFunction::ScaleAndEncodeOnWorkerThread(
    SkBitmap bitmap) {
  if (bitmap.drawsNothing()) {
    LOG(ERROR) << "No image from backing store.";
    return;
  }
  VLOG(1) << "captureVisibleTab() got image from backing store.";

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    gfx::Size dst_size_pixels = gfx::ScaleToRoundedSize(
        gfx::Size(bitmap.width(), bitmap.height()), scale_);
    bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BEST, dst_size_pixels.width(),
        dst_size_pixels.height());
  } else if (width_ != 0 && height_ != 0) {
    bitmap = ::vivaldi::skia_utils::SmartCropAndSize(bitmap, width_, height_);
  }

  base64_result_ = ::vivaldi::skia_utils::EncodeBitmapAsDataUrl(
      std::move(bitmap), image_format_, image_quality_);
}

void WebViewPrivateGetThumbnailFunction::SendResult() {
  namespace Results = vivaldi::web_view_private::GetThumbnail::Results;

  if (base64_result_.empty()) {
    Respond(Error("Internal Thumbnail error"));
    return;
  }
  Respond(ArgumentList(Results::Create(base64_result_)));
}

ExtensionFunction::ResponseAction WebViewPrivateShowPageInfoFunction::Run() {
  using vivaldi::web_view_private::ShowPageInfo::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  gfx::Point pos(params->position.left, params->position.top);
  guest_->ShowPageInfo(pos);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction WebViewPrivateSetIsFullscreenFunction::Run() {
  using vivaldi::web_view_private::SetIsFullscreen::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  guest_->SetIsFullscreen(params->is_fullscreen);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction WebViewPrivateGetPageHistoryFunction::Run() {
  using vivaldi::web_view_private::GetPageHistory::Params;
  namespace Results = vivaldi::web_view_private::GetPageHistory::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::NavigationController& controller =
      guest_->web_contents()->GetController();

  int currentEntryIndex = controller.GetCurrentEntryIndex();

  std::vector<Results::PageHistoryType> history(controller.GetEntryCount());
  for (int i = 0; i < controller.GetEntryCount(); i++) {
    content::NavigationEntry* entry = controller.GetEntryAtIndex(i);
    std::u16string name = entry->GetTitleForDisplay();
    Results::PageHistoryType* history_entry = &history[i];
    history_entry->name = base::UTF16ToUTF8(name);
    std::string urlstr = entry->GetVirtualURL().spec();
    history_entry->url = urlstr;
    history_entry->index = i;
  }
  return RespondNow(ArgumentList(Results::Create(currentEntryIndex, history)));
}

ExtensionFunction::ResponseAction
WebViewPrivateAllowBlockedInsecureContentFunction::Run() {
  using vivaldi::web_view_private::AllowBlockedInsecureContent::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  guest_->AllowRunningInsecureContent();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction WebViewPrivateSendRequestFunction::Run() {
  using vivaldi::web_view_private::SendRequest::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ui::PageTransition transition =
      HistoryPrivateAPI::PrivateHistoryTransitionToUiTransition(
          params->transition_type);
  if (params->from_url_field) {
    transition = ui::PageTransitionFromInt(
        transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  }

  // All the arguments passed to the constructor are ultimately ignored
  content::OpenURLParams url_params(GURL(params->url), content::Referrer(),
                                    WindowOpenDisposition::UNKNOWN, transition,
                                    false);

  if (params->use_post) {
    url_params.post_data = network::ResourceRequestBody::CreateFromBytes(
        params->post_data.c_str(), params->post_data.length());
  }
  url_params.extra_headers = params->extra_headers;

  guest_->NavigateGuest(params->url, /* navigation_handle = */ {}, true,
                        transition, url_params);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
WebViewPrivateGetPageSelectionFunction::Run() {
  content::RenderWidgetHostView* rwhv =
      guest_->web_contents()->GetRenderWidgetHostView();
    std::u16string text(*rwhv->GetVisibleSelectedText());
    return RespondNow(WithArguments(std::move(text)));
}

}  // namespace vivaldi
}  // namespace extensions
