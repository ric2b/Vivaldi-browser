// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved.

#include "ui/vivaldi_skia_utils.h"

#include "skia/ext/image_operations.h"

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

}  // namespace skia_utils
}  // namespace vivaldi
