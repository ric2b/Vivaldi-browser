//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/thumbnails/thumbnails_api.h"

#include <string>

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/schema/thumbnails.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace extensions {

bool ThumbnailsIsThumbnailAvailableFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::IsThumbnailAvailable::Params> params(
      vivaldi::thumbnails::IsThumbnailAvailable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> service(
      ThumbnailServiceFactory::GetForProfile(profile));
  GURL url;
  if (!params->bookmark_id.empty()) {
    std::string url_str = "http://bookmark_thumbnail/" + params->bookmark_id;
    GURL local(url_str);
    url.Swap(&local);
  } else if (!params->url.empty()) {
    GURL local(params->url);
    url.Swap(&local);
  }
  bool has_thumbnail = service->HasPageThumbnail(url);
  vivaldi::thumbnails::ThumbnailQueryResult result;
  result.has_thumbnail = has_thumbnail;
  result.thumbnail_url = url.spec();

  results_ = vivaldi::thumbnails::IsThumbnailAvailable::Results::Create(result);

  SendResponse(true);

  return true;
}

ThumbnailsIsThumbnailAvailableFunction::
    ThumbnailsIsThumbnailAvailableFunction() {
}

ThumbnailsIsThumbnailAvailableFunction::
    ~ThumbnailsIsThumbnailAvailableFunction() {
}

void ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete(
  const SkBitmap& bitmap,
  content::ReadbackResponse response) {
  if (response == content::READBACK_SUCCESS) {
    OnCaptureSuccess(bitmap);
    return;
  }
  // TODO(wjmaclean): Improve error reporting. Why aren't we passing more
  // information here?
  std::string reason;
  switch (response) {
  case content::READBACK_FAILED:
    reason = "READBACK_FAILED";
    break;
  case content::READBACK_SURFACE_UNAVAILABLE:
    reason = "READBACK_SURFACE_UNAVAILABLE";
    break;
  case content::READBACK_BITMAP_ALLOCATION_FAILURE:
    reason = "READBACK_BITMAP_ALLOCATION_FAILURE";
    break;
  default:
    reason = "<unknown>";
  }
  OnCaptureFailure(FAILURE_REASON_UNKNOWN);
}

bool ThumbnailsCaptureUIFunction::CaptureAsync(
  content::WebContents* web_contents,
  gfx::Rect& capture_area,
  const content::ReadbackRequestCallback callback) {
  if (!web_contents)
    return false;

  RenderWidgetHostView* const view = web_contents->GetRenderWidgetHostView();
  RenderWidgetHost* const host = view ? view->GetRenderWidgetHost() : nullptr;
  if (!view || !host) {
    OnCaptureFailure(FAILURE_REASON_VIEW_INVISIBLE);
    return false;
  } else {
    // By default, the requested bitmap size is the view size in screen
    // coordinates.  However, if there's more pixel detail available on the
    // current system, increase the requested bitmap size to capture it all.
    gfx::Size bitmap_size(capture_area.width(), capture_area.height());

    host->CopyFromBackingStore(capture_area, bitmap_size, callback,
                               kN32_SkColorType);
  }
  return true;
}

bool ThumbnailsCaptureUIFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::CaptureUI::Params> params(
    vivaldi::thumbnails::CaptureUI::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string app_window_id = params->params.app_window_id;
  AppWindowRegistry* registry = AppWindowRegistry::Get(GetProfile());
  for (AppWindow* app_window : registry->app_windows()) {
    if (app_window->window_key() == app_window_id) {
      gfx::Rect rect(params->params.pos_x, params->params.pos_y,
                     params->params.width, params->params.height);
      content::WebContents* contents = app_window->web_contents();
      return CaptureAsync(
        contents, rect,
        base::Bind(&ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete,
                   this));
    }
  }
  return false;
}

bool ThumbnailsCaptureUIFunction::EncodeBitmap(const SkBitmap& bitmap,
                                               std::string* base64_result) {
  DCHECK(base64_result);
  std::vector<unsigned char> data;
  SkAutoLockPixels screen_capture_lock(bitmap);
  bool encoded = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data);
  if (!encoded)
    return false;

  base::StringPiece stream_as_string(reinterpret_cast<const char*>(data.data()),
                                     data.size());

  base::Base64Encode(stream_as_string, base64_result);

  return true;
}

void ThumbnailsCaptureUIFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  std::string base64_result;
  if (!EncodeBitmap(bitmap, &base64_result)) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  SetResult(base::MakeUnique<base::StringValue>(base64_result));
  SendResponse(true);
}

void ThumbnailsCaptureUIFunction::OnCaptureFailure(FailureReason reason) {
  const char* reason_description = "internal error";
  switch (reason) {
  case FAILURE_REASON_UNKNOWN:
    reason_description = "unknown error";
    break;
  case FAILURE_REASON_ENCODING_FAILED:
    reason_description = "encoding failed";
    break;
  case FAILURE_REASON_VIEW_INVISIBLE:
    reason_description = "view is invisible";
    break;
  }
  error_ = ErrorUtils::FormatErrorMessage("Failed to capture tab: *",
                                          reason_description);
  SendResponse(false);
}


ThumbnailsCaptureUIFunction::ThumbnailsCaptureUIFunction() {
}

ThumbnailsCaptureUIFunction::~ThumbnailsCaptureUIFunction() {
}

}  // namespace extensions
