// Copyright 2018 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_transformpage.h"

#include "build/build_config.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "testing/scoped_locale.h"
#endif

using pdfium::RectanglesChecksum;

namespace {

const char* ShrunkChecksum() {
  if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
    return "78c52d6029283090036e6db6683401e2";
  }
  return "f4136cc9209207ab60eb8381a3df2e69";
}

}  // namespace

class FPDFTransformEmbedderTest : public EmbedderTest {};

TEST_F(FPDFTransformEmbedderTest, GetBoundingBoxes) {
  ASSERT_TRUE(OpenDocument("cropped_text.pdf"));
  ASSERT_EQ(4, FPDF_GetPageCount(document()));

  {
    ScopedEmbedderTestPage page = LoadScopedPage(1);
    ASSERT_TRUE(page);

    FS_RECTF mediabox;
    EXPECT_TRUE(FPDFPage_GetMediaBox(page.get(), &mediabox.left,
                                     &mediabox.bottom, &mediabox.right,
                                     &mediabox.top));
    EXPECT_EQ(-50, mediabox.left);
    EXPECT_EQ(-50, mediabox.bottom);
    EXPECT_EQ(200, mediabox.right);
    EXPECT_EQ(200, mediabox.top);

    FS_RECTF cropbox;
    EXPECT_TRUE(FPDFPage_GetCropBox(page.get(), &cropbox.left, &cropbox.bottom,
                                    &cropbox.right, &cropbox.top));
    EXPECT_EQ(50, cropbox.left);
    EXPECT_EQ(50, cropbox.bottom);
    EXPECT_EQ(150, cropbox.right);
    EXPECT_EQ(150, cropbox.top);

    FS_RECTF bleedbox;
    EXPECT_TRUE(FPDFPage_GetBleedBox(page.get(), &bleedbox.left,
                                     &bleedbox.bottom, &bleedbox.right,
                                     &bleedbox.top));
    EXPECT_EQ(0, bleedbox.left);
    EXPECT_EQ(10, bleedbox.bottom);
    EXPECT_EQ(150, bleedbox.right);
    EXPECT_EQ(145, bleedbox.top);

    FS_RECTF trimbox;
    EXPECT_TRUE(FPDFPage_GetTrimBox(page.get(), &trimbox.left, &trimbox.bottom,
                                    &trimbox.right, &trimbox.top));
    EXPECT_EQ(25, trimbox.left);
    EXPECT_EQ(30, trimbox.bottom);
    EXPECT_EQ(140, trimbox.right);
    EXPECT_EQ(145, trimbox.top);

    FS_RECTF artbox;
    EXPECT_TRUE(FPDFPage_GetArtBox(page.get(), &artbox.left, &artbox.bottom,
                                   &artbox.right, &artbox.top));
    EXPECT_EQ(50, artbox.left);
    EXPECT_EQ(60, artbox.bottom);
    EXPECT_EQ(135, artbox.right);
    EXPECT_EQ(140, artbox.top);
  }

  {
    ScopedEmbedderTestPage page = LoadScopedPage(3);
    ASSERT_TRUE(page);

    FS_RECTF mediabox;
    EXPECT_TRUE(FPDFPage_GetMediaBox(page.get(), &mediabox.left,
                                     &mediabox.bottom, &mediabox.right,
                                     &mediabox.top));
    EXPECT_EQ(0, mediabox.left);
    EXPECT_EQ(0, mediabox.bottom);
    EXPECT_EQ(200, mediabox.right);
    EXPECT_EQ(200, mediabox.top);

    FS_RECTF cropbox;
    EXPECT_TRUE(FPDFPage_GetCropBox(page.get(), &cropbox.left, &cropbox.bottom,
                                    &cropbox.right, &cropbox.top));
    EXPECT_EQ(150, cropbox.left);
    EXPECT_EQ(150, cropbox.bottom);
    EXPECT_EQ(60, cropbox.right);
    EXPECT_EQ(60, cropbox.top);

    EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), nullptr, &cropbox.bottom,
                                     &cropbox.right, &cropbox.top));
    EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), &cropbox.left, nullptr,
                                     &cropbox.right, &cropbox.top));
    EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), &cropbox.left, &cropbox.bottom,
                                     nullptr, &cropbox.top));
    EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), &cropbox.left, &cropbox.bottom,
                                     &cropbox.right, nullptr));
    EXPECT_FALSE(
        FPDFPage_GetCropBox(page.get(), nullptr, nullptr, nullptr, nullptr));

    FS_RECTF bleedbox;
    EXPECT_TRUE(FPDFPage_GetBleedBox(page.get(), &bleedbox.left,
                                     &bleedbox.bottom, &bleedbox.right,
                                     &bleedbox.top));
    EXPECT_EQ(160, bleedbox.left);
    EXPECT_EQ(165, bleedbox.bottom);
    EXPECT_EQ(0, bleedbox.right);
    EXPECT_EQ(10, bleedbox.top);

    FS_RECTF trimbox;
    EXPECT_TRUE(FPDFPage_GetTrimBox(page.get(), &trimbox.left, &trimbox.bottom,
                                    &trimbox.right, &trimbox.top));
    EXPECT_EQ(155, trimbox.left);
    EXPECT_EQ(165, trimbox.bottom);
    EXPECT_EQ(25, trimbox.right);
    EXPECT_EQ(30, trimbox.top);

    FS_RECTF artbox;
    EXPECT_TRUE(FPDFPage_GetArtBox(page.get(), &artbox.left, &artbox.bottom,
                                   &artbox.right, &artbox.top));
    EXPECT_EQ(140, artbox.left);
    EXPECT_EQ(145, artbox.bottom);
    EXPECT_EQ(65, artbox.right);
    EXPECT_EQ(70, artbox.top);
  }
}

TEST_F(FPDFTransformEmbedderTest, NoCropBox) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  FS_RECTF cropbox = {-1.0f, 0.0f, 3.0f, -2.0f};
  EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), &cropbox.left, &cropbox.bottom,
                                   &cropbox.right, &cropbox.top));
  EXPECT_EQ(-1.0f, cropbox.left);
  EXPECT_EQ(-2.0f, cropbox.bottom);
  EXPECT_EQ(3.0f, cropbox.right);
  EXPECT_EQ(0.0f, cropbox.top);
}

TEST_F(FPDFTransformEmbedderTest, NoBleedBox) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  FS_RECTF bleedbox = {-1.0f, 10.f, 3.0f, -1.0f};
  EXPECT_FALSE(FPDFPage_GetBleedBox(page.get(), &bleedbox.left,
                                    &bleedbox.bottom, &bleedbox.right,
                                    &bleedbox.top));
  EXPECT_EQ(-1.0f, bleedbox.left);
  EXPECT_EQ(-1.0f, bleedbox.bottom);
  EXPECT_EQ(3.0f, bleedbox.right);
  EXPECT_EQ(10.0f, bleedbox.top);
}

TEST_F(FPDFTransformEmbedderTest, NoTrimBox) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  FS_RECTF trimbox = {-11.0f, 0.0f, 3.0f, -10.0f};
  EXPECT_FALSE(FPDFPage_GetTrimBox(page.get(), &trimbox.left, &trimbox.bottom,
                                   &trimbox.right, &trimbox.top));
  EXPECT_EQ(-11.0f, trimbox.left);
  EXPECT_EQ(-10.0f, trimbox.bottom);
  EXPECT_EQ(3.0f, trimbox.right);
  EXPECT_EQ(0.0f, trimbox.top);
}

TEST_F(FPDFTransformEmbedderTest, NoArtBox) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  FS_RECTF artbox = {-1.0f, 0.0f, 3.0f, -1.0f};
  EXPECT_FALSE(FPDFPage_GetArtBox(page.get(), &artbox.left, &artbox.bottom,
                                  &artbox.right, &artbox.top));
  EXPECT_EQ(-1.0f, artbox.left);
  EXPECT_EQ(-1.0f, artbox.bottom);
  EXPECT_EQ(3.0f, artbox.right);
  EXPECT_EQ(0.0f, artbox.top);
}

TEST_F(FPDFTransformEmbedderTest, SetCropBox) {
  const char* cropped_checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
      return "4b9d2d2246be61c583f454245fe3172f";
    }
    return "9937883715d5144c079fb8f7e3d4f395";
  }();
  {
    ASSERT_TRUE(OpenDocument("rectangles.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    {
      // Render the page as is.
      FS_RECTF cropbox;
      EXPECT_FALSE(FPDFPage_GetCropBox(page.get(), &cropbox.left,
                                       &cropbox.bottom, &cropbox.right,
                                       &cropbox.top));
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }

    FPDFPage_SetCropBox(page.get(), 10, 20, 100, 150);

    {
      // Render the page after setting the CropBox.
      // Note that the change affects the rendering, as expected.
      // It behaves just like the case below, rather than the case above.
      FS_RECTF cropbox;
      EXPECT_TRUE(FPDFPage_GetCropBox(page.get(), &cropbox.left,
                                      &cropbox.bottom, &cropbox.right,
                                      &cropbox.top));
      EXPECT_EQ(10, cropbox.left);
      EXPECT_EQ(20, cropbox.bottom);
      EXPECT_EQ(100, cropbox.right);
      EXPECT_EQ(150, cropbox.top);
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(90, page_width);
      EXPECT_EQ(130, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height, cropped_checksum);
    }
  }

  {
    // Save a copy, open the copy, and render it.
    // Note that it renders the rotation.
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    ASSERT_TRUE(OpenSavedDocument());
    FPDF_PAGE saved_page = LoadSavedPage(0);
    ASSERT_TRUE(saved_page);

    FS_RECTF cropbox;
    EXPECT_TRUE(FPDFPage_GetCropBox(saved_page, &cropbox.left, &cropbox.bottom,
                                    &cropbox.right, &cropbox.top));
    EXPECT_EQ(10, cropbox.left);
    EXPECT_EQ(20, cropbox.bottom);
    EXPECT_EQ(100, cropbox.right);
    EXPECT_EQ(150, cropbox.top);
    const int page_width = static_cast<int>(FPDF_GetPageWidth(saved_page));
    const int page_height = static_cast<int>(FPDF_GetPageHeight(saved_page));
    EXPECT_EQ(90, page_width);
    EXPECT_EQ(130, page_height);
    ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
    CompareBitmap(bitmap.get(), page_width, page_height, cropped_checksum);

    CloseSavedPage(saved_page);
    CloseSavedDocument();
  }
}

TEST_F(FPDFTransformEmbedderTest, SetMediaBox) {
  const char* shrunk_checksum_set_media_box = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
      return "9f28f0610a7f789c24cfd5f9bd5dc3de";
    }
    return "eab5958f62f7ce65d7c32de98389fee1";
  }();

  {
    ASSERT_TRUE(OpenDocument("rectangles.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    {
      // Render the page as is.
      FS_RECTF mediabox;
      EXPECT_FALSE(FPDFPage_GetMediaBox(page.get(), &mediabox.left,
                                        &mediabox.bottom, &mediabox.right,
                                        &mediabox.top));
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }

    FPDFPage_SetMediaBox(page.get(), 20, 30, 100, 150);

    {
      // Render the page after setting the MediaBox.
      // Note that the change affects the rendering, as expected.
      // It behaves just like the case below, rather than the case above.
      FS_RECTF mediabox;
      EXPECT_TRUE(FPDFPage_GetMediaBox(page.get(), &mediabox.left,
                                       &mediabox.bottom, &mediabox.right,
                                       &mediabox.top));
      EXPECT_EQ(20, mediabox.left);
      EXPECT_EQ(30, mediabox.bottom);
      EXPECT_EQ(100, mediabox.right);
      EXPECT_EQ(150, mediabox.top);
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(80, page_width);
      EXPECT_EQ(120, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    shrunk_checksum_set_media_box);
    }
  }

  {
    // Save a copy, open the copy, and render it.
    // Note that it renders the rotation.
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    ASSERT_TRUE(OpenSavedDocument());
    FPDF_PAGE saved_page = LoadSavedPage(0);
    ASSERT_TRUE(saved_page);

    FS_RECTF mediabox;
    EXPECT_TRUE(FPDFPage_GetMediaBox(saved_page, &mediabox.left,
                                     &mediabox.bottom, &mediabox.right,
                                     &mediabox.top));
    EXPECT_EQ(20, mediabox.left);
    EXPECT_EQ(30, mediabox.bottom);
    EXPECT_EQ(100, mediabox.right);
    EXPECT_EQ(150, mediabox.top);
    const int page_width = static_cast<int>(FPDF_GetPageWidth(saved_page));
    const int page_height = static_cast<int>(FPDF_GetPageHeight(saved_page));
    EXPECT_EQ(80, page_width);
    EXPECT_EQ(120, page_height);
    ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
    CompareBitmap(bitmap.get(), page_width, page_height,
                  shrunk_checksum_set_media_box);

    CloseSavedPage(saved_page);
    CloseSavedDocument();
  }
}

TEST_F(FPDFTransformEmbedderTest, ClipPath) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFClipPath clip(FPDF_CreateClipPath(10.0f, 10.0f, 90.0f, 90.0f));
  EXPECT_TRUE(clip);

  // NULL arg call is a no-op.
  FPDFPage_InsertClipPath(nullptr, clip.get());

  // Do actual work.
  FPDFPage_InsertClipPath(page.get(), clip.get());

  // TODO(tsepez): test how inserting path affects page rendering.
}

TEST_F(FPDFTransformEmbedderTest, TransFormWithClip) {
  const FS_MATRIX half_matrix{0.5, 0, 0, 0.5, 0, 0};
  const FS_RECTF clip_rect = {0.0f, 0.0f, 20.0f, 10.0f};

  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_FALSE(FPDFPage_TransFormWithClip(nullptr, nullptr, nullptr));
  EXPECT_FALSE(FPDFPage_TransFormWithClip(nullptr, &half_matrix, nullptr));
  EXPECT_FALSE(FPDFPage_TransFormWithClip(nullptr, nullptr, &clip_rect));
  EXPECT_FALSE(FPDFPage_TransFormWithClip(nullptr, &half_matrix, &clip_rect));
  EXPECT_FALSE(FPDFPage_TransFormWithClip(page.get(), nullptr, nullptr));
  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), &half_matrix, nullptr));
  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), nullptr, &clip_rect));
  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), &half_matrix, &clip_rect));
}

TEST_F(FPDFTransformEmbedderTest, TransFormWithClipWithPatterns) {
  const FS_MATRIX half_matrix{0.5, 0, 0, 0.5, 0, 0};
  const FS_RECTF clip_rect = {0.0f, 0.0f, 20.0f, 10.0f};

  ASSERT_TRUE(OpenDocument("bug_547706.pdf"));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), &half_matrix, nullptr));
  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), nullptr, &clip_rect));
  EXPECT_TRUE(FPDFPage_TransFormWithClip(page.get(), &half_matrix, &clip_rect));
}

TEST_F(FPDFTransformEmbedderTest, TransFormWithClipAndSave) {
  {
    ASSERT_TRUE(OpenDocument("rectangles.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    {
      // Render the page as is.
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }

    {
      // Render the page after transforming.
      // Note that the change should affect the rendering, but does not.
      // It should behaves just like the case below, rather than the case above.
      // TODO(crbug.com/pdfium/1328): The checksum after invoking
      // `FPDFPage_TransFormWithClip()` below should match `ShrunkChecksum()`.
      const FS_MATRIX half_matrix{0.5, 0, 0, 0.5, 0, 0};
      EXPECT_TRUE(
          FPDFPage_TransFormWithClip(page.get(), &half_matrix, nullptr));
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }
  }

  {
    // Save a copy, open the copy, and render it.
    // Note that it renders the transform.
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    ASSERT_TRUE(OpenSavedDocument());
    FPDF_PAGE saved_page = LoadSavedPage(0);
    ASSERT_TRUE(saved_page);

    const int page_width = static_cast<int>(FPDF_GetPageWidth(saved_page));
    const int page_height = static_cast<int>(FPDF_GetPageHeight(saved_page));
    EXPECT_EQ(200, page_width);
    EXPECT_EQ(300, page_height);
    ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
    CompareBitmap(bitmap.get(), page_width, page_height, ShrunkChecksum());

    CloseSavedPage(saved_page);
    CloseSavedDocument();
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(FPDFTransformEmbedderTest, TransFormWithClipAndSaveWithLocale) {
  pdfium::ScopedLocale scoped_locale("da_DK.UTF-8");

  {
    ASSERT_TRUE(OpenDocument("rectangles.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    {
      // Render the page as is.
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }

    {
      // Render the page after transforming.
      // Note that the change should affect the rendering, but does not.
      // It should behaves just like the case below, rather than the case above.
      // TODO(crbug.com/pdfium/1328): The checksum after invoking
      // `FPDFPage_TransFormWithClip()` below should match `ShrunkChecksum()`.
      const FS_MATRIX half_matrix{0.5, 0, 0, 0.5, 0, 0};
      EXPECT_TRUE(
          FPDFPage_TransFormWithClip(page.get(), &half_matrix, nullptr));
      const int page_width = static_cast<int>(FPDF_GetPageWidth(page.get()));
      const int page_height = static_cast<int>(FPDF_GetPageHeight(page.get()));
      EXPECT_EQ(200, page_width);
      EXPECT_EQ(300, page_height);
      ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
      CompareBitmap(bitmap.get(), page_width, page_height,
                    RectanglesChecksum());
    }
  }

  {
    // Save a copy, open the copy, and render it.
    // Note that it renders the transform.
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    ASSERT_TRUE(OpenSavedDocument());
    FPDF_PAGE saved_page = LoadSavedPage(0);
    ASSERT_TRUE(saved_page);

    const int page_width = static_cast<int>(FPDF_GetPageWidth(saved_page));
    const int page_height = static_cast<int>(FPDF_GetPageHeight(saved_page));
    EXPECT_EQ(200, page_width);
    EXPECT_EQ(300, page_height);
    ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
    CompareBitmap(bitmap.get(), page_width, page_height, ShrunkChecksum());

    CloseSavedPage(saved_page);
    CloseSavedDocument();
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
