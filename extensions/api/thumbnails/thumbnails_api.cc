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
#include "ui/vivaldi_skia_utils.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using content::BrowserThread;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;
using vivaldi::skia_utils::SmartCropAndSize;
using vivaldi::ui_tools::EncodeBitmap;

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

ThumbnailsCaptureUIFunction::ThumbnailsCaptureUIFunction() = default;

ThumbnailsCaptureUIFunction::~ThumbnailsCaptureUIFunction() = default;

void ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete(
  const SkBitmap& bitmap) {
  if (!bitmap.drawsNothing()) {
    OnCaptureSuccess(bitmap);
    return;
  }
  Respond(Error(CaptureError("empty bitmap was returned")));
}

void ThumbnailsCaptureUIFunction::CaptureAsync(
    content::RenderWidgetHostView* view,
    const gfx::Rect& capture_area) {
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

  view->CopyFromSurface(
      capture_area, bitmap_size,
      base::BindOnce(&ThumbnailsCaptureUIFunction::CopyFromBackingStoreComplete,
                     this));
}

ExtensionFunction::ResponseAction ThumbnailsCaptureUIFunction::Run() {
  using vivaldi::thumbnails::CaptureUI::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

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
    Profile* profile = Profile::FromBrowserContext(browser_context());
    save_folder_ =
        profile->GetPrefs()->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.encode_to_data_url.get()) {
    encode_to_data_url_ = *params->params.encode_to_data_url;
  }
  if (params->params.copy_to_clipboard.get()) {
    copy_to_clipboard_ = *params->params.copy_to_clipboard;
  }
  int window_id = params->params.window_id;
  Browser* browser = ::vivaldi::FindBrowserByWindowId(window_id);
  if (!browser) {
    return RespondNow(
        Error(CaptureError("No such window - " + std::to_string(window_id))));
  }
  content::WebContents* web_contents =
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
  if (!web_contents) {
    return RespondNow(Error(CaptureError("Window witout web_context - " +
                                         std::to_string(window_id))));
  }
  RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (!view || !view->GetRenderWidgetHost())
    return RespondNow(Error(CaptureError("view is invisible")));

  gfx::Rect rect(params->params.pos_x, params->params.pos_y,
    params->params.width, params->params.height);
  content::WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
  if (tab) {
    url_ = tab->GetVisibleURL();
    title_ = base::UTF16ToUTF8(tab->GetTitle());
  }
  CaptureAsync(view, rect);
  return RespondLater();
}

void ThumbnailsCaptureUIFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  namespace Results = vivaldi::thumbnails::CaptureUI::Results;

  if (copy_to_clipboard_) {
    // Ignore everything else, we copy it raw to the clipboard.
    ui::ScopedClipboardWriter scw(ui::ClipboardType::kCopyPaste);
    scw.Reset();

    if (!bitmap.empty() && !bitmap.isNull()) {
      scw.WriteImage(bitmap);
    }
    Respond(ArgumentList(Results::Create(std::string())));
    return;
  }
  std::vector<unsigned char> data;
  std::string mime_type;
  bool encoded =
      EncodeBitmap(bitmap, &data, &mime_type, image_format_, encode_quality_);
  if (!encoded) {
    Respond(Error(CaptureError()));
    return;
  }
  std::string return_data;
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
  Respond(ArgumentList(Results::Create(return_data)));

  if (show_file_in_path_) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    platform_util::ShowItemInFolder(profile, file_path_);
  }
}

std::string ThumbnailsCaptureUIFunction::CaptureError(
    base::StringPiece details) {
  std::string error = "Failed to capture UI";
  if (!details.empty()) {
    error += " - ";
    details.AppendToString(&error);
  }
  LOG(ERROR) << error;
  return error;
}

ThumbnailsCaptureTabFunction::ThumbnailsCaptureTabFunction() = default;

ThumbnailsCaptureTabFunction::~ThumbnailsCaptureTabFunction() = default;

ExtensionFunction::ResponseAction ThumbnailsCaptureTabFunction::Run() {
  using vivaldi::thumbnails::CaptureTab::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->params.save_file_pattern) {
    save_file_pattern_ = *params->params.save_file_pattern;
  }
  if (params->params.encode_format) {
    image_format_ = *params->params.encode_format == "jpg"
                        ? ImageFormat::IMAGE_FORMAT_JPEG
                        : ImageFormat::IMAGE_FORMAT_PNG;
  }
  if (params->params.encode_quality) {
    encode_quality_ = *params->params.encode_quality;
  }
  bool capture_full_page = false;
  if (params->params.full_page) {
    capture_full_page = *params->params.full_page;
  }
  gfx::Rect rect;
  if (params->params.rect) {
    rect.set_x(params->params.rect->left);
    rect.set_y(params->params.rect->top);
    rect.set_width(params->params.rect->width);
    rect.set_height(params->params.rect->height);
  }
  // This is scale out size.
  gfx::Size out_dimension;
  if (params->params.height) {
    out_dimension.set_height(*params->params.height);
  }
  if (params->params.width) {
    out_dimension.set_width(*params->params.width);
  }
  if (params->params.save_to_disk) {
    save_to_disk_ = *params->params.save_to_disk;
  }
  // If full-page capture and no crop-rect set a default height.
  if (capture_full_page && rect.height() == 0) {
    rect.set_height(kMaximumPageHeight);
  }
  // Sanitize the user input.
  rect.set_height(std::min(kMaximumPageHeight, rect.height()));
  if (save_to_disk_) {
    if (params->params.show_file_in_path) {
      show_file_in_path_ = *params->params.show_file_in_path;
    }
    Profile* profile = Profile::FromBrowserContext(browser_context());
    save_folder_ =
        profile->GetPrefs()->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.copy_to_clipboard) {
    copy_to_clipboard_ = *params->params.copy_to_clipboard;
  }
  if (capture_full_page && !out_dimension.IsEmpty()) {
    return RespondNow(
        Error("width or height must not be given with full_page"));
  }

  int tab_id = params->tab_id;
  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id,
                                                      browser_context());
  if (!tabstrip_contents)
    return RespondNow(Error("No such tab - " + std::to_string(tab_id)));

  url_ = tabstrip_contents->GetVisibleURL();
  title_ = base::UTF16ToUTF8(tabstrip_contents->GetTitle());

  ::vivaldi::CapturePage::CaptureParams capture_params;
  capture_params.full_page = capture_full_page;
  capture_params.rect = rect;
  capture_params.target_size = out_dimension;

  ::vivaldi::CapturePage::Capture(
      tabstrip_contents, capture_params,
      base::BindOnce(
          &ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted,
          this));
  return RespondLater();
}

void ThumbnailsCaptureTabFunction::OnThumbnailsCaptureCompleted(
    ::vivaldi::CapturePage::Result captured) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailsCaptureTabFunction::ConvertImageOnWorkerThread,
                     this, std::move(captured)));
}

void ThumbnailsCaptureTabFunction::ConvertImageOnWorkerThread(
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
          &ThumbnailsCaptureTabFunction::OnImageConverted,
          this, std::move(return_bitmap), std::move(return_data),
          success));
}

void ThumbnailsCaptureTabFunction::OnImageConverted(
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
    ui::ScopedClipboardWriter scw(ui::ClipboardType::kCopyPaste);
    scw.Reset();

    if (!bitmap.empty() && !bitmap.isNull()) {
      scw.WriteImage(bitmap);
    }
  }
  Respond(ArgumentList(Results::Create(image_data)));

  if (show_file_in_path_ && !image_data.empty() && !copy_to_clipboard_) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    platform_util::ShowItemInFolder(profile, file_path_);
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
  if (initial_size.IsEmpty())
    return RespondNow(Error("initial width or height are not positive"));
  gfx::Size scaled_size(params->params.scaled_width,
                        params->params.scaled_height);
  if (scaled_size.IsEmpty())
    return RespondNow(Error("scaled width or height are not positive"));
  url_ = GURL(params->params.url);

  ::vivaldi::ThumbnailCaptureContents::Start(
      browser_context(), url_, initial_size, scaled_size,
      base::BindOnce(&ThumbnailsCaptureUrlFunction::OnCaptured, this));

  return RespondLater();
}

void ThumbnailsCaptureUrlFunction::OnCaptured(
    ::vivaldi::CapturePage::Result captured) {
  scoped_refptr<VivaldiDataSourcesAPI> api =
      VivaldiDataSourcesAPI::FromBrowserContext(browser_context());
  DCHECK(api);
  if (!api) {
    SendResult(false);
    return;
  }
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailsCaptureUrlFunction::ConvertImageOnWorkerThread,
                     this, std::move(api), std::move(captured)));
}

void ThumbnailsCaptureUrlFunction::ConvertImageOnWorkerThread(
    scoped_refptr<VivaldiDataSourcesAPI> api,
    ::vivaldi::CapturePage::Result captured) {
  do {
    SkBitmap bitmap;
    if (!captured.MovePixelsToBitmap(&bitmap))
      break;
    std::vector<unsigned char> data;
    std::string mime_type;
    bool encoded = EncodeBitmap(bitmap, &data, &mime_type,
                                ImageFormat::IMAGE_FORMAT_PNG, 100);
    if (!encoded)
      break;
    api->AddImageDataForBookmark(
        bookmark_id_, base::RefCountedBytes::TakeVector(&data),
        base::BindOnce(&ThumbnailsCaptureUrlFunction::SendResult, this));
    return;
  } while (false);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ThumbnailsCaptureUrlFunction::SendResult, this,
                     false));
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
