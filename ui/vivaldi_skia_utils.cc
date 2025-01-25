// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved.

#include "ui/vivaldi_skia_utils.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"

namespace vivaldi {
namespace skia_utils {

namespace {

SkIRect GetClippingRect(SkISize source_size, SkISize desired_size) {
  float desired_aspect =
      static_cast<float>(desired_size.width()) / desired_size.height();

  // Get the clipping rect so that we can preserve the aspect ratio while
  // filling the destination.
  if (source_size.width() < desired_size.width() ||
      source_size.height() < desired_size.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    return SkIRect::MakeSize(desired_size);
  }
  float src_aspect =
      static_cast<float>(source_size.width()) / source_size.height();
  if (src_aspect > desired_aspect) {
    // Wider than tall, clip horizontally: we center the smaller
    // thumbnail in the wider screen.
    int new_width = static_cast<int>(source_size.height() * desired_aspect);
    int x_offset = (source_size.width() - new_width) / 2;
    return SkIRect::MakeXYWH(x_offset, 0, new_width, source_size.height());
  } else if (src_aspect < desired_aspect) {
    return SkIRect::MakeWH(source_size.width(),
                           source_size.width() / desired_aspect);
  } else {
    return SkIRect::MakeSize(source_size);
  }
}

SkBitmap GetClippedBitmap(const SkBitmap& bitmap,
                          int desired_width,
                          int desired_height) {
  SkIRect clipping_rect = GetClippingRect(
      bitmap.dimensions(), SkISize::Make(desired_width, desired_height));
  SkBitmap clipped_bitmap;
  bitmap.extractSubset(&clipped_bitmap, clipping_rect);
  return clipped_bitmap;
}

}  // namespace

SkBitmap SmartCropAndSize(const SkBitmap& capture,
                          int target_width,
                          int target_height) {
  // Clip it to a more reasonable position.
  SkBitmap clipped_bitmap =
      GetClippedBitmap(capture, target_width, target_height);
  // Resize the result to the target size.
  SkBitmap result = skia::ImageOperations::Resize(
      clipped_bitmap, skia::ImageOperations::RESIZE_BEST, target_width,
      target_height);

// TODO(igor@vivaldi.com): Check if this is still applicable. If not, we should
// remove this together with Chromium copyrights at the top of the file.
// NOTE(pettern): Copied from SimpleThumbnailCrop::CreateThumbnail():
#if !defined(USE_AURA)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic
  // ones in PlatformCanvas can't be assigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler
  // will return the input bitmap, which will be the reference to the
  // weird PlatformCanvas one insetad of a regular one. To get a
  // regular refcounted bitmap, we need to copy it.
  //
  // On Aura, the PlatformCanvas is platform-independent and does not have
  // any native platform resources that can't be refounted, so this issue does
  // not occur.
  //
  // Note that GetClippedBitmap() does extractSubset() but it won't copy
  // the pixels, hence we check result size == clipped_bitmap size here.
  if (clipped_bitmap.width() == result.width() &&
      clipped_bitmap.height() == result.height()) {
    clipped_bitmap.readPixels(result.info(), result.getPixels(),
                              result.rowBytes(), 0, 0);
  }
#endif
  return result;
}

std::vector<unsigned char> EncodeBitmap(SkBitmap bitmap,
                                        ImageFormat image_format,
                                        int image_quality) {
  std::optional<std::vector<unsigned char>> data;
  if (!bitmap.getPixels()) {
    LOG(ERROR) << "Cannot encode empty bitmap";
  } else {
    switch (image_format) {
      case ImageFormat::kJPEG:
        data = gfx::JPEGCodec::Encode(bitmap, image_quality);
        if (!data.has_value()) {
          LOG(ERROR) << "Failed to encode bitmap as jpeg";
        }
        break;
      case ImageFormat::kPNG:
        data = gfx::PNGCodec::EncodeBGRASkBitmap(
            bitmap, true /* discard transparency */);
        if (!data.has_value()) {
          LOG(ERROR) << "Failed to encode bitmap as png";
        }
        break;
    }
  }
  return data.value_or(std::vector<unsigned char>());
}

std::string EncodeBitmapAsDataUrl(SkBitmap bitmap,
                                  ImageFormat image_format,
                                  int image_quality) {
  std::vector<unsigned char> image_bytes =
      EncodeBitmap(std::move(bitmap), image_format, image_quality);
  if (image_bytes.empty())
    return std::string();

  const char* mime_type;
  switch (image_format) {
    case ImageFormat::kJPEG:
      mime_type = "image/jpeg";  // kMimeTypeJpeg;
      break;
    case ImageFormat::kPNG:
      mime_type = "image/png";  // kMimeTypePng;
      break;
  }

  std::string data_url;
  std::string_view base64_input(
      reinterpret_cast<const char*>(image_bytes.data()), image_bytes.size());
  data_url = base::Base64Encode(base64_input);
  data_url.insert(0, base::StringPrintf("data:%s;base64,", mime_type));
  return data_url;
}

base::FilePath EncodeBitmapToFile(base::FilePath directory,
                                  SkBitmap bitmap,
                                  ImageFormat image_format,
                                  int image_quality) {
  std::vector<unsigned char> image_bytes =
      EncodeBitmap(std::move(bitmap), image_format, image_quality);
  if (image_bytes.empty())
    return base::FilePath();

  std::string ext;
  //const char* mime_type;
  switch (image_format) {
    case ImageFormat::kJPEG:
      //mime_type = "image/jpeg";  // kMimeTypeJpeg;
      ext = ".jpg";
      break;
    case ImageFormat::kPNG:
      //mime_type = "image/png";  // kMimeTypePng;
      ext = ".png";
      break;
  }
  base::FilePath base_path(directory);

  std::string filename = "QR Code ";
  filename += base::Uuid::GenerateRandomV4().AsLowercaseString();
  filename += ext;

  base_path = base::GetUniquePath(base_path.AppendASCII(filename));

  if (!base::WriteFile(base_path, image_bytes)) {
    LOG(ERROR) << "Error writing to file: " << base_path.value();
    return base::FilePath();
  }
  return base_path;
}

}  // namespace skia_utils
}  // namespace vivaldi
