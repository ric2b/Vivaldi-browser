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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/history/history_private_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/helper/vivaldi_frame_observer.h"
#include "extensions/schema/web_view_private.h"
#include "renderer/vivaldi_render_messages.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/vivaldi_ui_utils.h"

using content::BrowserPluginGuest;
using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::WebContents;
using content::WebContentsImpl;
using vivaldi::ui_tools::EncodeBitmap;
using vivaldi::ui_tools::SmartCropAndSize;

namespace extensions {
namespace vivaldi {

// Copied from WebViewInternalExtensionFunction::PreRunValidation
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

bool VivaldiWebViewChromeAsyncExtensionFunction::PreRunValidation(
    std::string* error) {
  if (!UIThreadExtensionFunction::PreRunValidation(error))
    return false;

  int instance_id = 0;
  EXTENSION_FUNCTION_PRERUN_VALIDATE(args_->GetInteger(0, &instance_id));
  guest_ = WebViewGuest::From(render_frame_host()->GetProcess()->GetID(),
                              instance_id);

  if (!guest_) {
    *error = "Could not find guest";
    return false;
  }
  guest_->InitListeners();  // Make sure we set a mouse event callback.
                            // Note: This can be removed if all mouse-gestures
                            //       are moved to the client.
  return true;
}

WebViewPrivateSetVisibleFunction::WebViewPrivateSetVisibleFunction() {}

WebViewPrivateSetVisibleFunction::~WebViewPrivateSetVisibleFunction() {}

bool WebViewPrivateSetVisibleFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::SetVisible::Params> params(
      vivaldi::web_view_private::SetVisible::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetVisible(params->is_visible);
  SendResponse(true);
  return true;
}

namespace {
const float kDefaultThumbnailScale = 1.0f;
}  // namespace

WebViewInternalThumbnailFunction::WebViewInternalThumbnailFunction()
    : image_quality_(90 /*kDefaultQuality*/),  // Default quality setting.
      scale_(kDefaultThumbnailScale),  // Scale of window dimension to thumb.
      height_(0),
      width_(0),
      is_incognito_(false),
      image_format_(api::extension_types::IMAGE_FORMAT_JPEG)  // Default
                                                              // format is
                                                              // PNG.
{}

WebViewInternalThumbnailFunction::~WebViewInternalThumbnailFunction() {}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewInternalThumbnailFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  std::vector<unsigned char> data;
  std::string mime_type;
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;
  bool need_resize = true;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ScaleToRoundedSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_);
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
    need_resize = false;
  } else if (width_ != 0 && height_ != 0) {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
    need_resize = false;
  } else {
    bitmap = screen_capture;
  }
  gfx::Size size(width_, height_);
  bool encoded = EncodeBitmap(bitmap, &data, &mime_type, image_format_, size,
                              scale_, image_quality_, need_resize);
  if (!encoded) {
    error_ = "Internal Thumbnail error";
    SendResponse(false);
    return;
  }
  std::string base64_result;
  base::StringPiece stream_as_string(reinterpret_cast<const char*>(data.data()),
                                     data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(
      0, base::StringPrintf("data:%s;base64,", mime_type.c_str()));
  SetResult(std::make_unique<base::Value>(base64_result));
  SendResponse(true);
}

bool WebViewInternalThumbnailFunction::InternalRunAsyncSafe(
    const std::unique_ptr<vivaldi::web_view_private::ThumbnailParams>& params) {
  if (params.get()) {
    if (params->scale.get()) {
      scale_ = *params->scale.get();
    }
    if (params->width.get()) {
      width_ = *params->width.get();
    }
    if (params.get()->height.get()) {
      height_ = *params->height.get();
    }
    Profile* profile = Profile::FromBrowserContext(browser_context());
    is_incognito_ = profile->IsOffTheRecord();
    // The thumbnail service should not store data in incognito so
    // only allow overriding it if we're not in incognito already.
    if (is_incognito_ == false && params->incognito.get()) {
      is_incognito_ = *params->incognito.get();
    }
  }
  WebContents* web_contents = guest_->web_contents();

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();

  if (!view) {
    error_ = "Error: View is not available, no screenshot taken.";
    return false;
  }
  if (!guest_->IsVisible()) {
    error_ = "Error: Guest is not visible, no screenshot taken.";
    return false;
  }
  // If this happens, the guest view is not attached to a window for some
  // reason. See also VB-23154.
  DCHECK(guest_->embedder_web_contents());
  if (!guest_->embedder_web_contents()) {
    error_ =
        "Error: Guest view is not attached to a window, no screenshot taken.";
    return false;
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
      base::Bind(
          &WebViewInternalThumbnailFunction::CopyFromBackingStoreComplete,
          this));

  return true;
}

void WebViewInternalThumbnailFunction::CopyFromBackingStoreComplete(
    const SkBitmap& bitmap) {
  if (!bitmap.drawsNothing()) {
    VLOG(1) << "captureVisibleTab() got image from backing store.";
    SendResultFromBitmap(bitmap);
    return;
  }
}

WebViewPrivateGetThumbnailFunction::WebViewPrivateGetThumbnailFunction() {}

WebViewPrivateGetThumbnailFunction::~WebViewPrivateGetThumbnailFunction() {
  if (!did_respond()) {
    SendInternalError();
  }
}

bool WebViewPrivateGetThumbnailFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::GetThumbnail::Params> params(
      vivaldi::web_view_private::GetThumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return WebViewInternalThumbnailFunction::InternalRunAsyncSafe(params->params);
}

void WebViewPrivateGetThumbnailFunction::SendInternalError() {
  error_ = "Internal Thumbnail error";
  SendResponse(false);
}

WebViewPrivateGetThumbnailFromServiceFunction::
    WebViewPrivateGetThumbnailFromServiceFunction() {}

WebViewPrivateGetThumbnailFromServiceFunction::
    ~WebViewPrivateGetThumbnailFromServiceFunction() {}

bool WebViewPrivateGetThumbnailFromServiceFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::AddToThumbnailService::Params>
      params(vivaldi::web_view_private::AddToThumbnailService::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  url_ = guest_->web_contents()->GetURL();

  return WebViewInternalThumbnailFunction::InternalRunAsyncSafe(params->params);
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewPrivateGetThumbnailFromServiceFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  // NOTE(pettern): We partially use the thumbnail_service, as we take our own
  // thumbnail but store it in the service. See also (http://crbug.com//327035).

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Scale the  bitmap.
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ScaleToRoundedSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_);
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
  }
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  scoped_refptr<thumbnails::ThumbnailingContext> context(
      new thumbnails::ThumbnailingContext(url_, false, true));
  context->score.force_update = true;

  if (!context->url.is_valid()) {
    SendResponse(false);
    return;
  }
  // NOTE(pettern@vivaldi.com): Do not store any urls for private windows.
  if (!is_incognito_) {
    thumbnail_service->AddForcedURL(context->url);
    thumbnail_service->SetPageThumbnail(*context, image);
  }

  SetResult(std::make_unique<base::Value>(std::string("chrome://thumb/") +
                                          context->url.spec()));
  SendResponse(true);
}

WebViewPrivateAddToThumbnailServiceFunction::
    WebViewPrivateAddToThumbnailServiceFunction() {}

WebViewPrivateAddToThumbnailServiceFunction::
    ~WebViewPrivateAddToThumbnailServiceFunction() {}

bool WebViewPrivateAddToThumbnailServiceFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::AddToThumbnailService::Params>
      params(vivaldi::web_view_private::AddToThumbnailService::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->key.empty())
    key_ = params->key;

  url_ = guest_->web_contents()->GetURL();

  return WebViewInternalThumbnailFunction::InternalRunAsyncSafe(params->params);
}

void WebViewPrivateAddToThumbnailServiceFunction::SetPageThumbnailOnUIThread(
    bool send_result,
    scoped_refptr<thumbnails::ThumbnailService> thumbnail_service,
    scoped_refptr<thumbnails::ThumbnailingContext> context,
    const gfx::Image& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  thumbnail_service->SetPageThumbnail(*context, thumbnail);

  if (send_result) {
    SetResult(std::make_unique<base::Value>(std::string("chrome://thumb/") +
                                            context->url.spec()));
    SendResponse(true);
  }
  Release();
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewPrivateAddToThumbnailServiceFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Scale the  bitmap.
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ScaleToRoundedSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_);
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
  }
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

  scoped_refptr<thumbnails::ThumbnailingContext> context(
      new thumbnails::ThumbnailingContext(GURL(key_), false, true));
  context->score.force_update = true;

  if (!context->url.is_valid()) {
    SendResponse(false);
    return;
  }
  // Do not store any urls for private windows.
  if (is_incognito_) {
    SetResult(std::make_unique<base::Value>(std::string("chrome://thumb/") +
                                            context->url.spec()));
    SendResponse(true);
  } else {
    AddRef();

    thumbnail_service->AddForcedURL(context->url);

    // NOTE(pettern): AddForcedURL is async, so we need to ensure adding the
    // thumbnail is too, to avoid it being added as a non-known url.
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&WebViewPrivateAddToThumbnailServiceFunction::
                       SetPageThumbnailOnUIThread,
                   this, true, thumbnail_service, context, image),
        base::TimeDelta::FromMilliseconds(200));
  }
}

WebViewPrivateShowPageInfoFunction::WebViewPrivateShowPageInfoFunction() {}

WebViewPrivateShowPageInfoFunction::~WebViewPrivateShowPageInfoFunction() {
  Respond(NoArguments());
}

bool WebViewPrivateShowPageInfoFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::ShowPageInfo::Params> params(
      vivaldi::web_view_private::ShowPageInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  gfx::Point pos(params->position.left, params->position.top);
  guest_->ShowPageInfo(pos);
  return true;
}

WebViewPrivateSetIsFullscreenFunction::WebViewPrivateSetIsFullscreenFunction() {
}

WebViewPrivateSetIsFullscreenFunction::
    ~WebViewPrivateSetIsFullscreenFunction() {}

bool WebViewPrivateSetIsFullscreenFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::SetIsFullscreen::Params> params(
      vivaldi::web_view_private::SetIsFullscreen::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest_->SetIsFullscreen(params->is_fullscreen, params->skip_window_state);
  SendResponse(true);
  return true;
}

WebViewPrivateGetPageHistoryFunction::WebViewPrivateGetPageHistoryFunction() {}

WebViewPrivateGetPageHistoryFunction::~WebViewPrivateGetPageHistoryFunction() {}

bool WebViewPrivateGetPageHistoryFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::GetPageHistory::Params> params(
      vivaldi::web_view_private::GetPageHistory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NavigationController& controller =
      guest_->web_contents()->GetController();

  int currentEntryIndex = controller.GetCurrentEntryIndex();

  std::vector<
      vivaldi::web_view_private::GetPageHistory::Results::PageHistoryType>
      history;

  for (int i = 0; i < controller.GetEntryCount(); i++) {
    content::NavigationEntry* entry = controller.GetEntryAtIndex(i);
    base::string16 name = entry->GetTitleForDisplay();
    std::unique_ptr<
        vivaldi::web_view_private::GetPageHistory::Results::PageHistoryType>
        history_entry(new vivaldi::web_view_private::GetPageHistory::Results::
                          PageHistoryType);
    history_entry->name = base::UTF16ToUTF8(name);
    std::string urlstr = entry->GetVirtualURL().spec();
    history_entry->url = urlstr;
    history_entry->index = i;
    history.push_back(std::move(*history_entry));
  }
  results_ = vivaldi::web_view_private::GetPageHistory::Results::Create(
      currentEntryIndex, history);

  SendResponse(true);
  return true;
}

WebViewPrivateAllowBlockedInsecureContentFunction::
    WebViewPrivateAllowBlockedInsecureContentFunction() {}

WebViewPrivateAllowBlockedInsecureContentFunction::
    ~WebViewPrivateAllowBlockedInsecureContentFunction() {}

bool WebViewPrivateAllowBlockedInsecureContentFunction::RunAsync() {
  guest_->AllowRunningInsecureContent();
  SendResponse(true);
  return true;
}

WebViewPrivateGetFocusedElementInfoFunction::
    WebViewPrivateGetFocusedElementInfoFunction() {}

WebViewPrivateGetFocusedElementInfoFunction::
    ~WebViewPrivateGetFocusedElementInfoFunction() {}

bool WebViewPrivateGetFocusedElementInfoFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::GetFocusedElementInfo::Params>
      params(vivaldi::web_view_private::GetFocusedElementInfo::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ::vivaldi::VivaldiFrameObserver* frame_observer = NULL;
  if (guest_->web_contents()) {
    frame_observer = ::vivaldi::VivaldiFrameObserver::FromWebContents(
        guest_->web_contents());
  }
  std::string tagname = "";
  std::string type = "";
  bool editable = false;
  std::string role = "";
  if (frame_observer != NULL) {
    frame_observer->GetFocusedElementInfo(&tagname, &type, &editable, &role);
  }
  results_ = vivaldi::web_view_private::GetFocusedElementInfo ::Results::Create(
      tagname, type, editable, role);
  SendResponse(true);
  return true;
}

WebViewPrivateSendRequestFunction::WebViewPrivateSendRequestFunction() {}

WebViewPrivateSendRequestFunction::~WebViewPrivateSendRequestFunction() {}

bool WebViewPrivateSendRequestFunction::RunAsync() {
  std::unique_ptr<vivaldi::web_view_private::SendRequest::Params> params(
      vivaldi::web_view_private::SendRequest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ui::PageTransition transition =
      HistoryPrivateAPI::PrivateHistoryTransitionToUiTransition(
          params->transition_type);
  // All the arguments passed to the constructor are ultimately ignored
  content::OpenURLParams url_params(GURL(params->url), content::Referrer(),
                                    WindowOpenDisposition::UNKNOWN, transition,
                                    false);

  url_params.uses_post = params->use_post;
  url_params.post_data = network::ResourceRequestBody::CreateFromBytes(
      params->post_data.c_str(), params->post_data.length());
  url_params.extra_headers = params->extra_headers;

  guest_->NavigateGuest(params->url, true, transition, nullptr, &url_params);
  SendResponse(true);
  return true;
}

}  // namespace vivaldi
}  // namespace extensions
