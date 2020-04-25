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
#include "browser/thumbnails/thumbnail_capture_contents.h"
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
  std::vector<unsigned char> data;
  std::string mime_type;
  bool encoded =
      EncodeBitmap(bitmap, &data, &mime_type, image_format_, encode_quality_);
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
  if (params->params.rect.get()) {
    rect_.set_x(params->params.rect->left);
    rect_.set_y(params->params.rect->top);
    rect_.set_width(params->params.rect->width);
    rect_.set_height(params->params.rect->height);
  }
  // This is scale out size.
  if (params->params.height.get()) {
    out_dimension_.set_height(*params->params.height);
  }
  if (params->params.width.get()) {
    out_dimension_.set_width(*params->params.width);
  }
  if (params->params.save_to_disk.get()) {
    save_to_disk_ = *params->params.save_to_disk;
  }
  // If full-page capture and no crop-rect set a default height.
  if (capture_full_page_ && rect_.height() == 0) {
    rect_.set_height(kMaximumPageHeight);
  }
  // Sanitize the user input.
  rect_.set_height(std::min(kMaximumPageHeight, rect_.height()));
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

      ::vivaldi::CapturePage::CaptureParams params;
      params.full_page = capture_full_page_;
      params.rect = rect_;

      ::vivaldi::CapturePage::Capture(
          tabstrip_contents, params,
          base::BindOnce(
              &ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted,
              this));
      return true;
    }
  }
  return false;
}

void ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted(
    ::vivaldi::CapturePage::Result captured) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailsCaptureTabFunction::ScaleAndConvertImage, this,
                     std::move(captured)));
}

void ThumbnailsCaptureTabFunction::ScaleAndConvertImage(
    ::vivaldi::CapturePage::Result captured) {

  bool success = false;
  SkBitmap return_bitmap;
  std::string return_data;
  do {
    SkBitmap bitmap;
    if (!captured.MovePixelsToBitmap(&bitmap))
      break;

    if (copy_to_clipboard_) {
      return_bitmap = std::move(bitmap);
      success = true;
      break;
    }

    if (capture_full_page_ == false && out_dimension_.width() &&
        out_dimension_.height()) {
      // Scale and crop it now.
      bitmap = SmartCropAndSize(bitmap, out_dimension_.width(),
                                out_dimension_.height());
    }
    std::vector<unsigned char> data;
    std::string mime_type;
    bool encoded =
        EncodeBitmap(bitmap, &data, &mime_type, image_format_, encode_quality_);
    if (!encoded)
      break;
    if (save_to_disk_ == false) {
      // If the base path is not set, we want to encode the image as a data url.
      base::StringPiece base64_input(reinterpret_cast<const char*>(&data[0]),
                                    data.size());
      std::string data;
      Base64Encode(base64_input, &data);

      if (image_format_ == ImageFormat::IMAGE_FORMAT_PNG) {
        data.insert(0, "data:image/png;base64,");
      } else {
        data.insert(0, "data:image/jpg;base64,");
      }
      return_data = std::move(data);
      success = true;
      break;
    }
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
    success = true;
  } while (false);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread,
          this, std::move(return_bitmap), std::move(return_data),
          success));
}

void ThumbnailsCaptureTabFunction::ScaleAndConvertImageDoneOnUIThread(
    const SkBitmap& bitmap,
    const std::string& image_data,
    bool success) {
  namespace Results = vivaldi::thumbnails::CaptureTab::Results;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!success) {
    std::string error = "Failed to capture tab " + url_.possibly_invalid_spec();
    LOG(ERROR) << error;
    Respond(Error(error));
    return;
  }

  if (copy_to_clipboard_) {
    // Ignore everything else, we copy it raw to the clipboard.
    ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
    scw.Reset();

    if (!bitmap.empty() && !bitmap.isNull()) {
      scw.WriteImage(bitmap);
    }
  }
  Respond(ArgumentList(Results::Create(image_data)));

  if (show_file_in_path_ && !image_data.empty() && !copy_to_clipboard_) {
    platform_util::ShowItemInFolder(GetProfile(), file_path_);
  }
}

ThumbnailsCaptureUrlFunction::ThumbnailsCaptureUrlFunction() = default;

ThumbnailsCaptureUrlFunction::~ThumbnailsCaptureUrlFunction() = default;

ExtensionFunction::ResponseAction ThumbnailsCaptureUrlFunction::Run() {
  using vivaldi::thumbnails::CaptureUrl::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!base::StringToInt64(params->params.bookmark_id, &bookmark_id_) ||
      bookmark_id_ <= 0) {
    return RespondNow(Error("bookmarkId is not a valid positive integer - " +
                            params->params.bookmark_id));
  }
  gfx::Size initial_size(params->params.width, params->params.height);
  scaled_size_ =
      gfx::Size(params->params.scaled_width, params->params.scaled_height);
  url_ = GURL(params->params.url);

  ::vivaldi::ThumbnailCaptureContents::Start(
      browser_context(), url_, initial_size,
      base::BindOnce(&ThumbnailsCaptureUrlFunction::OnCaptured, this));

  return RespondLater();
}

void ThumbnailsCaptureUrlFunction::OnCaptured(
    ::vivaldi::CapturePage::Result captured) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &ThumbnailsCaptureUrlFunction::ScaleAndConvertImageOnWorkerThread,
          this, std::move(captured)));
}

void ThumbnailsCaptureUrlFunction::ScaleAndConvertImageOnWorkerThread(
    ::vivaldi::CapturePage::Result captured) {

  bool success = false;
  scoped_refptr<base::RefCountedMemory> thumbnail;
  do {
    SkBitmap bitmap;
    if (!captured.MovePixelsToBitmap(&bitmap))
      break;
    if (scaled_size_.width() != 0 && scaled_size_.height() != 0) {
      bitmap =
          SmartCropAndSize(bitmap, scaled_size_.width(), scaled_size_.height());
    }
    std::vector<unsigned char> data;
    std::string mime_type;
    bool encoded = EncodeBitmap(bitmap, &data, &mime_type,
                                ImageFormat::IMAGE_FORMAT_PNG, 100);
    if (!encoded)
      break;
    thumbnail = base::RefCountedBytes::TakeVector(&data);
    success = true;
  } while (false);

  // TODO(igor@vivaldi.com): call
  // VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread here directly to
  // avoid returning to UI thread just to call
  // VivaldiDataSourcesAPI::AddImageDataForBookmark which goes to a worker
  // thread again.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ThumbnailsCaptureUrlFunction::OnCapturedAndScaled, this,
                     std::move(thumbnail), success));
}

void ThumbnailsCaptureUrlFunction::OnCapturedAndScaled(
    scoped_refptr<base::RefCountedMemory> thumbnail,
    bool success) {
  if (!success) {
    SendResult(false);
    return;
  }

  VivaldiDataSourcesAPI::AddImageDataForBookmark(
      browser_context(), bookmark_id_, std::move(thumbnail),
      base::BindOnce(&ThumbnailsCaptureUrlFunction::SendResult, this));
}

void ThumbnailsCaptureUrlFunction::SendResult(bool success) {
  namespace Results = vivaldi::thumbnails::CaptureUrl::Results;

  std::string thumbnail_url;
  if (!success) {
    LOG(ERROR) << "Failed to capture url " << url_.possibly_invalid_spec();
  } else {
    thumbnail_url =
        VivaldiDataSourcesAPI::GetBookmarkThumbnailUrl(bookmark_id_);
  }
  Respond(ArgumentList(Results::Create(thumbnail_url)));
}

}  // namespace extensions
