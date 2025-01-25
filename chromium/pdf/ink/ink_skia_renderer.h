// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_SKIA_RENDERER_H_
#define PDF_INK_INK_SKIA_RENDERER_H_

#include <memory>

class SkCanvas;

namespace chrome_pdf {

class InkInProgressStroke;
class InkStroke;
struct InkAffineTransform;

class InkSkiaRenderer {
 public:
  static std::unique_ptr<InkSkiaRenderer> Create();

  InkSkiaRenderer(const InkSkiaRenderer&) = delete;
  InkSkiaRenderer& operator=(const InkSkiaRenderer&) = delete;
  virtual ~InkSkiaRenderer() = default;

  // Note that the context parameter has been omitted, as it is always set to
  // nullptr.
  virtual bool Draw(const InkInProgressStroke& stroke,
                    const InkAffineTransform& object_to_canvas,
                    SkCanvas& canvas) = 0;
  virtual bool Draw(const InkStroke& stroke,
                    const InkAffineTransform& object_to_canvas,
                    SkCanvas& canvas) = 0;

 protected:
  InkSkiaRenderer() = default;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_SKIA_RENDERER_H_
