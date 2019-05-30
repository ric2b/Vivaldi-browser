//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/thumbnails/thumbnails_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "browser/thumbnails/vivaldi_capture_contents.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/schema/thumbnails.h"
#include "renderer/vivaldi_render_messages.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using content::BrowserThread;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;
using vivaldi::ui_tools::EncodeBitmap;
using vivaldi::ui_tools::SmartCropAndSize;

namespace {

const int kMaximumPageHeight = 30000;
const int kMaximumFilenameLength = 100;

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  delete reinterpret_cast<base::SharedMemory*>(context);
}
}  // namespace

namespace extensions {

/*
Patterns supported:

$timestamp - Long date in YYY-MM-DD HH.MM.SS format
$year - Year in YYYY format
$month - Month in MM format
$day - Day in DD format
$hour - Hour in HH format
$minute - Minute in MM format
$second - Second in SS format
$ms - Millisecond in MMM format
$longid - GUID in standard format
$shortid - Short GUID, only the last 12 characters
$host - Hostname of the active tab, eg. www.vivaldi.com

*/

enum class CaptureFilePatternType {
  TIMESTAMP,
  YEAR,
  MONTH,
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  MILLISECOND,
  LONGID,
  SHORTID,
  HOST,
  TITLE,
};

struct CaptureFilePattern {
  const char* pattern;
  CaptureFilePatternType pattern_id;
};

struct CaptureFilePattern file_patterns[] = {
  {"$timestamp", CaptureFilePatternType::TIMESTAMP},
  {"$year",     CaptureFilePatternType::YEAR},
  {"$month",    CaptureFilePatternType::MONTH},
  {"$day",      CaptureFilePatternType::DAY},
  {"$hour",     CaptureFilePatternType::HOUR},
  {"$minute",   CaptureFilePatternType::MINUTE},
  {"$second",   CaptureFilePatternType::SECOND},
  {"$ms",       CaptureFilePatternType::MILLISECOND},
  {"$longid",   CaptureFilePatternType::LONGID},
  {"$shortid",  CaptureFilePatternType::SHORTID},
  {"$host",     CaptureFilePatternType::HOST},
  {"$title",    CaptureFilePatternType::TITLE},
};

std::string ConstructCaptureArgument(CaptureFilePatternType type,
                                     const GURL& url,
                                     const base::Time::Exploded& now,
                                     const std::string& title) {
  switch (type) {
    case CaptureFilePatternType::TIMESTAMP:
      return base::StringPrintf("%d-%02d-%02d %02d.%02d.%02d", now.year,
                                now.month, now.day_of_month, now.hour,
                                now.minute, now.second);
    case CaptureFilePatternType::YEAR:
      return base::StringPrintf("%d", now.year);
    case CaptureFilePatternType::MONTH:
      return base::StringPrintf("%02d", now.month);
    case CaptureFilePatternType::DAY:
      return base::StringPrintf("%02d", now.day_of_month);
    case CaptureFilePatternType::HOUR:
      return base::StringPrintf("%02d", now.hour);
    case CaptureFilePatternType::MINUTE:
      return base::StringPrintf("%02d", now.minute);
    case CaptureFilePatternType::SECOND:
      return base::StringPrintf("%02d", now.second);
    case CaptureFilePatternType::MILLISECOND:
      return base::StringPrintf("%02d", now.millisecond);
    case CaptureFilePatternType::LONGID:
      return base::GenerateGUID();
    case CaptureFilePatternType::SHORTID: {
      std::string short_id = base::GenerateGUID();
      short_id.erase(0, short_id.length() - 12);
      return short_id;
    }
    case CaptureFilePatternType::HOST: {
      std::string host = url.host();
      // Special case the vivaldi app id.
      if (::vivaldi::IsVivaldiApp(host)) {
        host = "vivaldi";
      }
      return host;
    }
    case CaptureFilePatternType::TITLE: {
      return title;
    }
    default:
      break;
  }
  return std::string();
}

base::FilePath ConstructCaptureFilename(
    base::FilePath& base_path,
    const std::string& pattern,
    const GURL& url,
    const std::string& title,
    const base::FilePath::StringPieceType& extension) {
  if (pattern.empty()) {
    base_path = base_path.AppendASCII(base::GenerateGUID());
  } else {
    // Function might replace in-place
    std::string new_string = pattern;
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);

    for (CaptureFilePattern& pattern : file_patterns) {
      base::ReplaceSubstringsAfterOffset(
          &new_string, 0, pattern.pattern,
          ConstructCaptureArgument(pattern.pattern_id, url, now, title));
    }
    // Strip invalid characters from the generated filename.
    // Windows is the strictest OS so we use the list from
    // https://docs.microsoft.com/en-us/windows/desktop/FileIO/naming-a-file
    // This is not performance sensitive code so brute force it.
    std::string special_file_chars = "\\/?%*:|\"<>\n\r";
    for (int i = 0; i < 32; i++) {
      special_file_chars += static_cast<char>(i);
    }

    base::ReplaceChars(new_string, special_file_chars, "_", &new_string);

    // Trim the maximum filename length.
    if (new_string.length() > kMaximumFilenameLength) {
      std::string truncated;
      base::TruncateUTF8ToByteSize(new_string, kMaximumFilenameLength,
                                   &truncated);
      new_string = truncated;
    }

#if defined(OS_POSIX)
    base_path = base_path.Append(new_string);
#elif defined(OS_WIN)
    base_path = base_path.Append(base::UTF8ToUTF16(new_string));
#endif
    base_path = base_path.AddExtension(extension);
  }
  // Ensure unique filename.
  int unique_number = base::GetUniquePathNumber(
    base_path, base::FilePath::StringType());
  if (unique_number > 0) {
    base_path = base_path.InsertBeforeExtensionASCII(
      base::StringPrintf(" (%d)", unique_number));
  }
  return base_path;
}

bool ThumbnailsIsThumbnailAvailableFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::IsThumbnailAvailable::Params> params(
    vivaldi::thumbnails::IsThumbnailAvailable::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::VivaldiDataSourcesAPI* api =
      extensions::VivaldiDataSourcesAPI::GetFactoryInstance()->Get(
          GetProfile());
  vivaldi::thumbnails::ThumbnailQueryResult result;
  result.has_thumbnail = api->HasBookmarkThumbnail(params->bookmark_id);
  result.thumbnail_url = base::StringPrintf(
      "chrome://vivaldi-data/local-image/%d", params->bookmark_id);

  results_ = vivaldi::thumbnails::IsThumbnailAvailable::Results::Create(result);

  SendResponse(true);

  return true;
}

ThumbnailsIsThumbnailAvailableFunction::
  ThumbnailsIsThumbnailAvailableFunction() {}

ThumbnailsIsThumbnailAvailableFunction::
  ~ThumbnailsIsThumbnailAvailableFunction() {}

void ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete(
  const SkBitmap& bitmap) {
  if (!bitmap.drawsNothing()) {
    OnCaptureSuccess(bitmap);
    return;
  }
  OnCaptureFailure(FAILURE_REASON_UNKNOWN);
}

bool ThumbnailsCaptureUIFunction::CaptureAsync(
  content::WebContents* web_contents,
  const gfx::Rect& capture_area,
  base::OnceCallback<void(const SkBitmap&)> callback) {
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

    const gfx::NativeView native_view = view->GetNativeView();
    display::Screen* const screen = display::Screen::GetScreen();
    const float scale =
      screen->GetDisplayNearestView(native_view).device_scale_factor();
    if (scale > 1.0f)
      bitmap_size = gfx::ScaleToCeiledSize(bitmap_size, scale);

    view->CopyFromSurface(capture_area, bitmap_size, std::move(callback));
  }
  return true;
}

bool ThumbnailsCaptureUIFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::CaptureUI::Params> params(
    vivaldi::thumbnails::CaptureUI::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->params.save_file_pattern) {
    save_file_pattern_ = *params->params.save_file_pattern;
  }
  if (params->params.encode_format.get()) {
    image_format_ = *params->params.encode_format == "jpg"
      ? ImageFormat::IMAGE_FORMAT_JPEG
      : ImageFormat::IMAGE_FORMAT_PNG;
  }
  if (params->params.encode_quality.get()) {
    encode_quality_ = *params->params.encode_quality.get();
  }
  if (params->params.save_to_disk.get()) {
    save_to_disk_ = *params->params.save_to_disk;
  }
  if (save_to_disk_) {
    if (params->params.show_file_in_path) {
      show_file_in_path_ = *params->params.show_file_in_path;
    }
    const PrefService* user_prefs = GetProfile()->GetPrefs();
    save_folder_ =
      user_prefs->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.encode_to_data_url.get()) {
    encode_to_data_url_ = *params->params.encode_to_data_url;
  }
  if (params->params.copy_to_clipboard.get()) {
    copy_to_clipboard_ = *params->params.copy_to_clipboard;
  }
  int window_id = params->params.window_id;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == window_id) {
      gfx::Rect rect(params->params.pos_x, params->params.pos_y,
        params->params.width, params->params.height);
      content::WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
      if (tab) {
        url_ = tab->GetVisibleURL();
        title_ = base::UTF16ToUTF8(tab->GetTitle());
      }
      content::WebContents* contents =
        static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
      return CaptureAsync(
        contents, rect,
        base::Bind(&ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete,
          this));
    }
  }
  // Wrong id given.
  NOTREACHED();
  return false;
}

void ThumbnailsCaptureUIFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  gfx::Size final_size(bitmap.width(), bitmap.height());
  std::vector<unsigned char> data;
  std::string mime_type;
  std::string return_data;
  if (copy_to_clipboard_) {
    // Ignore everything else, we copy it raw to the clipboard.
    ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
    scw.Reset();

    if (!bitmap.empty() && !bitmap.isNull()) {
      scw.WriteImage(bitmap);
    }
    SetResult(std::make_unique<base::Value>(return_data));
    SendResponse(true);
    return;
  }
  bool encoded = EncodeBitmap(bitmap, &data, &mime_type, image_format_,
                              final_size, 100, encode_quality_, false);
  if (!encoded) {
    error_ = "Failed to capture ui: data could not be encoded";
    SendResponse(false);
    return;
  }
  if (save_to_disk_ == false) {
    // If the base path is not set, we want to encode the image as a data url.
    base::StringPiece base64_input(reinterpret_cast<const char*>(&data[0]),
                                   data.size());
    Base64Encode(base64_input, &return_data);

    if (encode_to_data_url_) {
      if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
        return_data.insert(0, "data:image/png;base64,");
      } else {
        return_data.insert(0, "data:image/jpg;base64,");
      }
    }
  } else {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath path = base::FilePath::FromUTF8Unsafe(save_folder_);

    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
    base::FilePath::StringPieceType ext = FILE_PATH_LITERAL(".jpg");
    if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
      ext = FILE_PATH_LITERAL(".png");
    }
    path =
        ConstructCaptureFilename(path, save_file_pattern_, url_, title_, ext);
#if defined(OS_POSIX)
    return_data.assign(path.value());
#elif defined(OS_WIN)
    return_data.assign(base::WideToUTF8(path.value()));
#endif
    file_path_ = path;
    base::WriteFile(path, reinterpret_cast<const char*>(&data[0]), data.size());
  }
  SetResult(std::make_unique<base::Value>(return_data));
  SendResponse(true);

  if (show_file_in_path_) {
    platform_util::ShowItemInFolder(GetProfile(), file_path_);
  }
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

ThumbnailsCaptureUIFunction::ThumbnailsCaptureUIFunction() {}

ThumbnailsCaptureUIFunction::~ThumbnailsCaptureUIFunction() {}

ThumbnailsCaptureTabFunction::ThumbnailsCaptureTabFunction() {}

ThumbnailsCaptureTabFunction::~ThumbnailsCaptureTabFunction() {
  if (!did_respond()) {
    SendResponse(false);
  }
}

bool ThumbnailsCaptureTabFunction::RunAsync() {
  std::unique_ptr<vivaldi::thumbnails::CaptureTab::Params> params(
      vivaldi::thumbnails::CaptureTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->params.save_file_pattern) {
    save_file_pattern_ = *params->params.save_file_pattern;
  }
  if (params->params.encode_format.get()) {
    image_format_ = *params->params.encode_format == "jpg"
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
  if (params->params.save_to_disk.get()) {
    save_to_disk_ = *params->params.save_to_disk;
  }
  if (capture_full_page_ == true && height_ == 0) {
    // Some sanity value so we don't capture an endless page.
    height_ = kMaximumPageHeight;
  }
  height_ = std::min(kMaximumPageHeight, height_);
  if (save_to_disk_) {
    if (params->params.show_file_in_path) {
      show_file_in_path_ = *params->params.show_file_in_path;
    }
    const PrefService* user_prefs = GetProfile()->GetPrefs();
    save_folder_ =
        user_prefs->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.copy_to_clipboard.get()) {
    copy_to_clipboard_ = *params->params.copy_to_clipboard;
  }
  int tab_id = params->tab_id;
  if (tab_id) {
    content::WebContents* tabstrip_contents =
        ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
    if (tabstrip_contents) {
      url_ = tabstrip_contents->GetVisibleURL();
      title_ = base::UTF16ToUTF8(tabstrip_contents->GetTitle());
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
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::Bind(&ThumbnailsCaptureTabFunction::ScaleAndConvertImage, this,
                 handle, size, callback_id));
}

void ThumbnailsCaptureTabFunction::ScaleAndConvertImage(
    base::SharedMemoryHandle handle,
    const gfx::Size image_size,
    int callback_id) {
  std::unique_ptr<base::SharedMemory> bitmap_buffer(
      new base::SharedMemory(handle, true));
  bool resize_needed = capture_full_page_ == false;

  SkBitmap bitmap;
  // Let Skia do some sanity checking for (no negative widths/heights, no
  // overflows while calculating bytes per row, etc).
  if (!bitmap.setInfo(SkImageInfo::MakeN32Premul(image_size.width(),
                                                 image_size.height()))) {
    DispatchError(
        "Failed to capture tab: Sanity check failed on captured image data");
    return;
  }
  // Make sure the size is representable as a signed 32-bit int, so
  // SkBitmap::getSize() won't be truncated.
  if (!bitmap_buffer->Map(bitmap.computeByteSize())) {
    DispatchError("Failed to capture tab: size mismatch");
    return;
  }

  if (!bitmap.installPixels(bitmap.info(), bitmap_buffer->memory(),
                            bitmap.rowBytes(), &ReleaseSharedMemoryPixels,
                            bitmap_buffer.get())) {
    DispatchError("Failed to capture tab: data could not be copied to bitmap");
    return;
  }
  // On success, SkBitmap now owns the SharedMemory.
  ignore_result(bitmap_buffer.release());

  std::string return_data;
  if (copy_to_clipboard_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(
            &ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread,
            this, bitmap, return_data, callback_id));
    return;
  }

  if (capture_full_page_ == false && width_ && height_) {
    // Scale and crop it now.
    bitmap = SmartCropAndSize(bitmap, width_, height_);
    resize_needed = false;
  }
  gfx::Size final_size(bitmap.width(), bitmap.height());
  std::vector<unsigned char> data;
  std::string mime_type;
  bool encoded = EncodeBitmap(bitmap, &data, &mime_type, image_format_,
                              final_size, 100, encode_quality_, resize_needed);
  if (!encoded) {
    DispatchError("Failed to capture tab: data could not be encoded");
    return;
  }
  if (save_to_disk_ == false) {
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
    base::FilePath path = base::FilePath::FromUTF8Unsafe(save_folder_);

    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
    base::FilePath::StringPieceType ext = FILE_PATH_LITERAL(".jpg");
    if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
      ext = FILE_PATH_LITERAL(".png");
    }
    path =
        ConstructCaptureFilename(path, save_file_pattern_, url_, title_, ext);
#if defined(OS_POSIX)
    return_data.assign(path.value());
#elif defined(OS_WIN)
    return_data.assign(base::WideToUTF8(path.value()));
#endif
    file_path_ = path;
    base::WriteFile(path, reinterpret_cast<const char*>(&data[0]), data.size());
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(
          &ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread,
          this, bitmap, return_data, callback_id));
}

void ThumbnailsCaptureTabFunction::DispatchError(const std::string& error_msg) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
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
    const SkBitmap bitmap,
    const std::string image_data,
    int callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (copy_to_clipboard_) {
    // Ignore everything else, we copy it raw to the clipboard.
    ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
    scw.Reset();

    if (!bitmap.empty() && !bitmap.isNull()) {
      scw.WriteImage(bitmap);
    }
  }
  // callback_id is the tab_id
  results_ =
      vivaldi::thumbnails::CaptureTab::Results::Create(callback_id, image_data);

  SendResponse(true);

  if (show_file_in_path_ && !image_data.empty() && !copy_to_clipboard_) {
    platform_util::ShowItemInFolder(GetProfile(), file_path_);
  }
}

ThumbnailsCaptureUrlFunction::ThumbnailsCaptureUrlFunction()
    : capture_page_(base::MakeRefCounted<::vivaldi::ThumbnailCaptureContents>(
          browser_context())) {
}

ThumbnailsCaptureUrlFunction::~ThumbnailsCaptureUrlFunction() {

}

ExtensionFunction::ResponseAction ThumbnailsCaptureUrlFunction::Run() {
  using vivaldi::thumbnails::CaptureUrl::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  capture_page_->set_browser_context(browser_context());

  bookmark_id_ = params->params.bookmark_id;
  gfx::Size initial_size(params->params.width, params->params.height);
  gfx::Size scaled_size(params->params.scaled_width,
                        params->params.scaled_height);

  capture_page_->Start(
      GURL(params->params.url), initial_size, scaled_size,
      base::Bind(&ThumbnailsCaptureUrlFunction::OnCapturedAndScaled, this));

  return RespondLater();
}

void ThumbnailsCaptureUrlFunction::OnCapturedAndScaled(const SkBitmap& bitmap,
                                                       bool success) {
  using vivaldi::thumbnails::CaptureUrl::Results::Create;
  if (!success) {
    // Unsuccessful capture, might be an error page.
    Respond(ArgumentList(Create(bookmark_id_, false, std::string())));
    return;
  }
  VivaldiDataSourcesAPI* api =
      VivaldiDataSourcesAPI::GetFactoryInstance()->Get(browser_context());
  std::unique_ptr<SkBitmap> bitmap_ptr(new SkBitmap(bitmap));

  api->AddImageDataForBookmark(
      bookmark_id_, std::move(bitmap_ptr),
      base::Bind(&ThumbnailsCaptureUrlFunction::OnBookmarkThumbnailStored,
                 this));
}

void ThumbnailsCaptureUrlFunction::OnBookmarkThumbnailStored(
    int bookmark_id,
    std::string& image_url) {
  using vivaldi::thumbnails::CaptureUrl::Results::Create;

  Respond(ArgumentList(Create(bookmark_id, !image_url.empty(), image_url)));
}

}  // namespace extensions
