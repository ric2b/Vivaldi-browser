//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/thumbnails/thumbnails_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/error_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

#include "app/vivaldi_apptools.h"
#include "browser/vivaldi_browser_finder.h"
#include "components/capture/capture_page.h"
#include "components/datasource/vivaldi_image_store.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_skia_utils.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace extensions {

struct CaptureData {
  // Input parameters
  ::vivaldi::skia_utils::ImageFormat image_format =
      ::vivaldi::skia_utils::ImageFormat::kPNG;
  int encode_quality = 90;
  bool show_file_in_path = false;
  bool copy_to_clipboard = false;
  bool save_to_disk = false;
  std::string save_folder;
  std::string save_file_pattern;
  GURL url;
  std::string title;

  // Output parameters
  std::optional<bool> success;
  std::string base64;
  base::FilePath image_path;
};

namespace {

const int kMaximumPageHeight = 30000;
const int kMaximumFilenameLength = 100;

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
$longid - UUID in standard format
$shortid - Short UUID, only the last 12 characters
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
    {"$year", CaptureFilePatternType::YEAR},
    {"$month", CaptureFilePatternType::MONTH},
    {"$day", CaptureFilePatternType::DAY},
    {"$hour", CaptureFilePatternType::HOUR},
    {"$minute", CaptureFilePatternType::MINUTE},
    {"$second", CaptureFilePatternType::SECOND},
    {"$ms", CaptureFilePatternType::MILLISECOND},
    {"$longid", CaptureFilePatternType::LONGID},
    {"$shortid", CaptureFilePatternType::SHORTID},
    {"$host", CaptureFilePatternType::HOST},
    {"$title", CaptureFilePatternType::TITLE},
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
      return base::Uuid::GenerateRandomV4().AsLowercaseString();
    case CaptureFilePatternType::SHORTID: {
      std::string short_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
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
    base::FilePath base_path,
    const std::string& pattern,
    const GURL& url,
    const std::string& title,
    const base::FilePath::StringPieceType& extension) {
  if (pattern.empty()) {
    base_path = base_path.AppendASCII(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
  } else {
    // Function might replace in-place
    std::string new_string = pattern;
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);

    for (CaptureFilePattern& pattern_item : file_patterns) {
      base::ReplaceSubstringsAfterOffset(
          &new_string, 0, pattern_item.pattern,
          ConstructCaptureArgument(pattern_item.pattern_id, url, now, title));
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

#if BUILDFLAG(IS_POSIX)
    base_path = base_path.Append(new_string);
#elif BUILDFLAG(IS_WIN)
    base_path = base_path.Append(base::UTF8ToWide(new_string));
#endif
  }
  return base::GetUniquePath(base_path.AddExtension(extension));
}

std::unique_ptr<CaptureData> SaveBitmapOnWorkerThread(
    std::unique_ptr<CaptureData> data,
    SkBitmap bitmap) {
  DCHECK(!data->success.has_value());
  DCHECK(!data->copy_to_clipboard);
  DCHECK(!bitmap.drawsNothing());
  do {
    if (!data->save_to_disk) {
      std::string image_data = ::vivaldi::skia_utils::EncodeBitmapAsDataUrl(
          std::move(bitmap), data->image_format, data->encode_quality);
      if (!image_data.empty()) {
        data->base64 = std::move(image_data);
        data->success = true;
      }
      break;
    }
    std::vector<unsigned char> image_bytes =
        ::vivaldi::skia_utils::EncodeBitmap(
            std::move(bitmap), data->image_format, data->encode_quality);
    if (image_bytes.empty())
      break;
    base::FilePath path = base::FilePath::FromUTF8Unsafe(data->save_folder);
    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
    base::FilePath::StringPieceType ext = FILE_PATH_LITERAL(".jpg");
    if (data->image_format == ::vivaldi::skia_utils::ImageFormat::kPNG) {
      ext = FILE_PATH_LITERAL(".png");
    }
    path = ConstructCaptureFilename(std::move(path), data->save_file_pattern,
                                    data->url, data->title, ext);
    if(!base::WriteFile(path, image_bytes)) {
      LOG(ERROR) << "Failed to write to " << path;
      break;
    }
    data->image_path = std::move(path);
    data->success = true;
  } while (false);

  if (!data->success.has_value()) {
    data->success = false;
  }
  return data;
}

using SaveCapturedBitmapCallback =
    base::OnceCallback<void(std::unique_ptr<CaptureData> data)>;

void FinishSaveBitmapOnUIThread(SaveCapturedBitmapCallback callback,
                                std::unique_ptr<CaptureData> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!*data->success) {
    LOG(ERROR) << "Failed to capture " << data->url.possibly_invalid_spec();
  }
  std::move(callback).Run(std::move(data));
}

// Order of arguments allows to bind the first two and be called with `bitmap`
// later.
void SaveCapturedBitmap(std::unique_ptr<CaptureData> data,
                        SaveCapturedBitmapCallback callback,
                        SkBitmap bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!data->success.has_value());
  if (bitmap.drawsNothing()) {
    data->success = false;
  } else if (data->copy_to_clipboard) {
    // Clipboard must be accessed on UI thread.
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.Reset();
    scw.WriteImage(bitmap);

    // We no longer need the bitmap.
    bitmap = SkBitmap();
    data->success = true;
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&SaveBitmapOnWorkerThread, std::move(data),
                       std::move(bitmap)),
        base::BindOnce(&FinishSaveBitmapOnUIThread, std::move(callback)));
    return;
  }
  FinishSaveBitmapOnUIThread(std::move(callback), std::move(data));
}

void ShowFolderIfNecessary(content::BrowserContext* browser_context,
                           CaptureData& data) {
  if (data.success && data.show_file_in_path && !data.image_path.empty()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    platform_util::ShowItemInFolder(profile, data.image_path);
  }
}

}  // namespace

ExtensionFunction::ResponseAction ThumbnailsCaptureUIFunction::Run() {
  using vivaldi::thumbnails::CaptureUI::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->params.window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  auto data = std::make_unique<CaptureData>();
  if (params->params.save_file_pattern) {
    data->save_file_pattern = *params->params.save_file_pattern;
  }
  if (params->params.encode_format.has_value()) {
    data->image_format = params->params.encode_format.value() == "jpg"
                             ? ::vivaldi::skia_utils::ImageFormat::kJPEG
                             : ::vivaldi::skia_utils::ImageFormat::kPNG;
  }
  if (params->params.encode_quality.has_value()) {
    data->encode_quality = params->params.encode_quality.value();
  }
  if (params->params.save_to_disk.has_value()) {
    data->save_to_disk = params->params.save_to_disk.value();
  }
  if (data->save_to_disk) {
    if (params->params.show_file_in_path.has_value()) {
      data->show_file_in_path = params->params.show_file_in_path.value();
    }
    Profile* profile = Profile::FromBrowserContext(browser_context());
    data->save_folder =
        profile->GetPrefs()->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.copy_to_clipboard.has_value()) {
    data->copy_to_clipboard = params->params.copy_to_clipboard.value();
  }

  content::WebContents* tab =
      window->browser()->tab_strip_model()->GetActiveWebContents();
  if (tab) {
    data->url = tab->GetVisibleURL();
    data->title = base::UTF16ToUTF8(tab->GetTitle());
  }

  gfx::RectF rect(params->params.pos_x, params->params.pos_y,
                  params->params.width, params->params.height);
  ::vivaldi::FromUICoordinates(window->web_contents(), &rect);
  ::vivaldi::CapturePage::CaptureVisible(
      window->web_contents(), rect,
      base::BindOnce(&ThumbnailsCaptureUIFunction::OnCaptureDone, this,
                     std::move(data)));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void ThumbnailsCaptureUIFunction::OnCaptureDone(
    std::unique_ptr<CaptureData> data,
    bool success,
    float device_scale_factor,
    const SkBitmap& bitmap) {
  if (!success) {
    SendResult(std::move(data));
    return;
  }

  // TODO(igor@vivaldi.com): Consider using device_scale_factor to embed
  // DPI comments into the resulting image.

  SaveCapturedBitmap(
      std::move(data),
      base::BindOnce(&ThumbnailsCaptureUIFunction::SendResult, this),
      std::move(bitmap));
}

void ThumbnailsCaptureUIFunction::SendResult(
    std::unique_ptr<CaptureData> data) {
  namespace Results = vivaldi::thumbnails::CaptureUI::Results;
  bool result = data->success ? *data->success : false;
  Respond(ArgumentList(Results::Create(result, data->base64)));
  ShowFolderIfNecessary(browser_context(), *data);
}

ExtensionFunction::ResponseAction ThumbnailsCaptureTabFunction::Run() {
  using vivaldi::thumbnails::CaptureTab::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto data = std::make_unique<CaptureData>();
  if (params->params.save_file_pattern) {
    data->save_file_pattern = *params->params.save_file_pattern;
  }
  if (params->params.encode_format) {
    data->image_format = *params->params.encode_format == "jpg"
                             ? ::vivaldi::skia_utils::ImageFormat::kJPEG
                             : ::vivaldi::skia_utils::ImageFormat::kPNG;
  }
  if (params->params.encode_quality) {
    data->encode_quality = *params->params.encode_quality;
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
    data->save_to_disk = *params->params.save_to_disk;
  }
  // If full-page capture and no crop-rect set a default height.
  if (capture_full_page && rect.height() == 0) {
    rect.set_height(kMaximumPageHeight);
  }
  // Sanitize the user input.
  rect.set_height(std::min(kMaximumPageHeight, rect.height()));
  if (data->save_to_disk) {
    if (params->params.show_file_in_path) {
      data->show_file_in_path = *params->params.show_file_in_path;
    }
    Profile* profile = Profile::FromBrowserContext(browser_context());
    data->save_folder =
        profile->GetPrefs()->GetString(vivaldiprefs::kWebpagesCaptureDirectory);
  }
  if (params->params.copy_to_clipboard) {
    data->copy_to_clipboard = *params->params.copy_to_clipboard;
  }
  if (capture_full_page && !out_dimension.IsEmpty()) {
    return RespondNow(
        Error("width or height must not be given with full_page"));
  }

  content::WebContents* tabstrip_contents = nullptr;
  int tab_id = params->tab_id;
  if (tab_id) {
    tabstrip_contents = ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
        tab_id, browser_context());
  } else {
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    tabstrip_contents =
        browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
  }
  if (!tabstrip_contents)
    return RespondNow(Error("No such tab - " + std::to_string(tab_id)));

  data->url = tabstrip_contents->GetVisibleURL();
  data->title = base::UTF16ToUTF8(tabstrip_contents->GetTitle());

  ::vivaldi::CapturePage::CaptureParams capture_params;
  capture_params.full_page = capture_full_page;

  // Need to transform the rect to pixelsize from device-size.
  double scale = 1.0f;
  display::Screen* screen = display::Screen::GetScreen();
  if (screen) {
    gfx::NativeWindow window = tabstrip_contents->GetTopLevelNativeWindow();
    display::Display display = screen->GetDisplayNearestWindow(window);
    scale = display.device_scale_factor();
  }
  capture_params.rect =
      gfx::ToEnclosingRect(gfx::ConvertRectToDips(rect, scale));

  capture_params.target_size = out_dimension;

  ::vivaldi::CapturePage::Capture(
      tabstrip_contents, capture_params,
      base::BindOnce(
          &SaveCapturedBitmap, std::move(data),
          base::BindOnce(&ThumbnailsCaptureTabFunction::SendResult, this)));

  // did_respond() is true  when the capture method above called the callback
  // immediately due to errors.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void ThumbnailsCaptureTabFunction::SendResult(
    std::unique_ptr<CaptureData> data) {
  namespace Results = vivaldi::thumbnails::CaptureTab::Results;

  Respond(ArgumentList(Results::Create(*data->success, data->base64)));
  ShowFolderIfNecessary(browser_context(), *data);
}

ThumbnailsCaptureBookmarkFunction::ThumbnailsCaptureBookmarkFunction() =
    default;

ThumbnailsCaptureBookmarkFunction::~ThumbnailsCaptureBookmarkFunction() {
  auto *profile = Profile::FromBrowserContext(browser_context());
  if (profile) {
    profile->RemoveObserver(this);
  }
}

void ThumbnailsCaptureBookmarkFunction::OnProfileWillBeDestroyed(Profile* profile) {
  content::BrowserContext* browser_context =
      tcc_->GetWebContents()->GetBrowserContext();

  if (Profile::FromBrowserContext(browser_context) == profile) {
    tcc_->RespondAndDelete();
    profile->RemoveObserver(this);
  }
}

ExtensionFunction::ResponseAction ThumbnailsCaptureBookmarkFunction::Run() {
  using vivaldi::thumbnails::CaptureBookmark::Params;

  std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t bookmark_id;
  if (!base::StringToInt64(params->params.bookmark_id, &bookmark_id) ||
      bookmark_id <= 0) {
    return RespondNow(Error("bookmarkId is not a valid positive integer - " +
                            params->params.bookmark_id));
  }
  url_ = GURL(params->params.url);

  tcc_ = VivaldiImageStore::CaptureBookmarkThumbnail(
      browser_context(), bookmark_id, url_,
      base::BindOnce(&ThumbnailsCaptureBookmarkFunction::SendResult, this));

  content::BrowserContext* browser_context =
      tcc_->GetWebContents()->GetBrowserContext();

  Profile::FromBrowserContext(browser_context)->AddObserver(this);

  return RespondLater();
}

void ThumbnailsCaptureBookmarkFunction::SendResult(std::string data_url) {
  namespace Results = vivaldi::thumbnails::CaptureBookmark::Results;

  bool success = !data_url.empty();
  if (!success) {
    LOG(ERROR) << "Failed to capture url " << url_.possibly_invalid_spec();
  }
  Respond(ArgumentList(Results::Create(success)));
}

}  // namespace extensions
