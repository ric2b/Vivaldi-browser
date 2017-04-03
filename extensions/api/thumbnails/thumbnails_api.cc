//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/thumbnails/thumbnails_api.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/schema/thumbnails.h"
#include "renderer/vivaldi_render_messages.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/vivaldi_ui_utils.h"

using content::BrowserThread;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;
using vivaldi::ui_tools::EncodeBitmap;
using vivaldi::ui_tools::SmartCropAndSize;

namespace {

const int kMaximumPageHeight = 20000;

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  delete reinterpret_cast<base::SharedMemory*>(context);
}
}  // namespace

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



ThumbnailsCaptureTabFunction::ThumbnailsCaptureTabFunction() {
}

ThumbnailsCaptureTabFunction::~ThumbnailsCaptureTabFunction() {
}

bool ThumbnailsCaptureTabFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::CaptureTab::Params> params(
    vivaldi::thumbnails::CaptureTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->params.encode_format.get()) {
    image_format_ =
      *params->params.encode_format == "jpg"
      ? ImageFormat::IMAGE_FORMAT_JPEG
      : ImageFormat::IMAGE_FORMAT_PNG;
  }
  if (params->params.encode_quality.get()) {
    encode_quality_ = *params->params.encode_quality.get();
  }
  if (params->params.full_page.get()) {
    capture_full_page_ = *params->params.full_page;
  }
  if (params->params.width.get()) {
    width_ = *params->params.width;
  }
  if (params->params.height.get()) {
    height_ = *params->params.height;
  }
  if (params->params.save_to_base_path.get()) {
    base_path_ = *params->params.save_to_base_path;
  }
  if (capture_full_page_ == true && height_ == 0) {
    // Some sanity value so we don't capture an endless page.
    height_ = kMaximumPageHeight;
  }
  int tab_id = params->tab_id;
  if (tab_id) {
    content::WebContents *tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
    if (tabstrip_contents) {
      extensions::VivaldiPrivateTabObserver* private_tabs =
          extensions::VivaldiPrivateTabObserver::FromWebContents(
              tabstrip_contents);
      DCHECK(private_tabs);
      if (private_tabs && !private_tabs->IsCapturing()) {
        private_tabs->CaptureTab(
            gfx::Size(width_, height_), capture_full_page_,
            base::Bind(
                &ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted,
                this));
        return true;
      }
    }
  }
  return false;
}

void ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted(
    base::SharedMemoryHandle handle,
    const gfx::Size size,
    int callback_id,
    bool success) {
  if (!success || !base::SharedMemory::IsHandleValid(handle)) {
    DispatchError("Failed to capture tab: no data from the renderer process");
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ThumbnailsCaptureTabFunction::ScaleAndConvertImage, this,
                 handle, size, callback_id));
}

void ThumbnailsCaptureTabFunction::ScaleAndConvertImage(
    base::SharedMemoryHandle handle,
    const gfx::Size image_size,
    int callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::unique_ptr<base::SharedMemory> bitmap_buffer(
    new base::SharedMemory(handle, true));

  SkBitmap bitmap;
  // Let Skia do some sanity checking for (no negative widths/heights, no
  // overflows while calculating bytes per row, etc).
  if (!bitmap.setInfo(
    SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()))) {
    DispatchError(
        "Failed to capture tab: Sanity check failed on captured image data");
    return;
  }
  // Make sure the size is representable as a signed 32-bit int, so
  // SkBitmap::getSize() won't be truncated.
  if (!sk_64_isS32(bitmap.computeSize64()) || !bitmap_buffer->Map(bitmap.getSize())) {
    DispatchError("Failed to capture tab: size mismatch");
    return;
  }

  if (!bitmap.installPixels(bitmap.info(), bitmap_buffer->memory(),
                            bitmap.rowBytes(), NULL, &ReleaseSharedMemoryPixels,
                            bitmap_buffer.get())) {
    DispatchError("Failed to capture tab: data could not be copied to bitmap");
    return;
  }
  // On success, SkBitmap now owns the SharedMemory.
  ignore_result(bitmap_buffer.release());

  if (capture_full_page_ == false && width_ && height_) {
    // Scale and crop it now.
    bitmap = SmartCropAndSize(bitmap, width_, height_);
  }
  gfx::Size final_size(width_, height_);
  std::vector<unsigned char> data;
  std::string mime_type;
  bool encoded = EncodeBitmap(bitmap, &data, mime_type, image_format_,
                              final_size, 100, encode_quality_);
  if (!encoded) {
    DispatchError("Failed to capture tab: data could not be encoded");
    return;
  }
  std::string return_data;
  if (base_path_.empty()) {
    // If the base path is not set, we want to encode the image as a data url.
    base::StringPiece base64_input(reinterpret_cast<const char*>(&data[0]),
                                   data.size());
    Base64Encode(base64_input, &return_data);

    if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
      return_data.insert(0, "data:image/png;base64,");
    } else {
      return_data.insert(0, "data:image/jpg;base64,");
    }
  } else {
    base::FilePath path;
    PathService::Get(chrome::DIR_USER_PICTURES, &path);
    path = path.AppendASCII(base_path_);
    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
    path = path.AppendASCII(base::GenerateGUID());
    if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
      path = path.AddExtension(FILE_PATH_LITERAL(".png"));
    } else {
      path = path.AddExtension(FILE_PATH_LITERAL(".jpg"));
    }
#if defined(OS_POSIX)
    return_data.assign(path.value());
#elif defined(OS_WIN)
    return_data.assign(base::WideToUTF8(path.value()));
#endif
    base::WriteFile(path, reinterpret_cast<const char*>(&data[0]), data.size());
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread,
          this, return_data, callback_id));
}

void ThumbnailsCaptureTabFunction::DispatchError(const std::string& error_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ThumbnailsCaptureTabFunction::DispatchErrorOnUIThread, this,
                 error_msg));
}

void ThumbnailsCaptureTabFunction::DispatchErrorOnUIThread(
    const std::string& error_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  error_ = error_msg;
  SendResponse(false);
}

void ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread(
    const std::string image_data,
    int callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // callback_id is the tab_id
  results_ =
      vivaldi::thumbnails::CaptureTab::Results::Create(callback_id, image_data);

  SendResponse(true);
}

}  // namespace extensions
