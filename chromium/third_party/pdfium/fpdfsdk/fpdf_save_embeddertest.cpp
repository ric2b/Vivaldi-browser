// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

#include "core/fxcrt/fx_string.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_ppo.h"
#include "public/fpdf_save.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;

class FPDFSaveEmbedderTest : public EmbedderTest {};

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDoc) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocWithVersion) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocWithBadVersion) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, -1));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));

  ClearString();
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));

  ClearString();
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 0, 18));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocIncremental) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_INCREMENTAL, 14));
  // Version gets taken as-is from input document.
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\n%\xa0\xf2\xa4\xf4"));
  // Additional output produced vs. non incremental.
  EXPECT_EQ(985u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocNoIncremental) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_NO_INCREMENTAL, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocRemoveSecurity) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, FPDF_REMOVE_SECURITY, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveSimpleDocBadFlags) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveWithVersion(document(), this, 999999, 14));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.4\r\n"));
  EXPECT_EQ(805u, GetString().size());
}

TEST_F(FPDFSaveEmbedderTest, SaveCopiedDoc) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_ImportPages(output_doc.get(), document(), "1", 0));
  EXPECT_TRUE(FPDF_SaveAsCopy(output_doc.get(), this, 0));

}

TEST_F(FPDFSaveEmbedderTest, Bug42271133) {
  ASSERT_TRUE(OpenDocument("bug_42271133.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Arbitrarily remove the first page object.
  auto text_object = FPDFPage_GetObject(page.get(), 0);
  ASSERT_TRUE(text_object);
  ASSERT_TRUE(FPDFPage_RemoveObject(page.get(), text_object));
  FPDFPageObj_Destroy(text_object);

  // Regenerate dirty stream and save the document.
  ASSERT_TRUE(FPDFPage_GenerateContent(page.get()));
  ASSERT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // Reload saved document.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  ASSERT_TRUE(saved_page);

  // Assert path fill color is not changed to black.
  auto path_obj = FPDFPage_GetObject(saved_page, 0);
  ASSERT_TRUE(path_obj);
  unsigned int r;
  unsigned int g;
  unsigned int b;
  unsigned int a;
  ASSERT_TRUE(FPDFPageObj_GetFillColor(path_obj, &r, &g, &b, &a));
  EXPECT_EQ(180u, r);
  EXPECT_EQ(180u, g);
  EXPECT_EQ(180u, b);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFSaveEmbedderTest, SaveLinearizedDoc) {
  const int kPageCount = 3;
  std::array<std::string, kPageCount> original_md5;

  ASSERT_TRUE(OpenDocument("linearized.pdf"));
  for (int i = 0; i < kPageCount; ++i) {
    ScopedEmbedderTestPage page = LoadScopedPage(i);
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    EXPECT_EQ(612, FPDFBitmap_GetWidth(bitmap.get()));
    EXPECT_EQ(792, FPDFBitmap_GetHeight(bitmap.get()));
    original_md5[i] = HashBitmap(bitmap.get());
  }

  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.6\r\n"));
  EXPECT_THAT(GetString(), HasSubstr("/Root "));
  EXPECT_THAT(GetString(), HasSubstr("/Info "));
  EXPECT_THAT(GetString(), HasSubstr("/Size 37"));
  EXPECT_THAT(GetString(), HasSubstr("35 0 obj"));
  EXPECT_THAT(GetString(), HasSubstr("36 0 obj"));
  EXPECT_THAT(GetString(), Not(HasSubstr("37 0 obj")));
  EXPECT_THAT(GetString(), Not(HasSubstr("38 0 obj")));
  EXPECT_EQ(7996u, GetString().size());

  // Make sure new document renders the same as the old one.
  ASSERT_TRUE(OpenSavedDocument());
  for (int i = 0; i < kPageCount; ++i) {
    FPDF_PAGE page = LoadSavedPage(i);
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap = RenderSavedPage(page);
    EXPECT_EQ(original_md5[i], HashBitmap(bitmap.get()));
    CloseSavedPage(page);
  }
  CloseSavedDocument();
}

TEST_F(FPDFSaveEmbedderTest, Bug1409) {
  ASSERT_TRUE(OpenDocument("jpx_lzw.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  while (FPDFPage_CountObjects(page.get()) > 0) {
    ScopedFPDFPageObject object(FPDFPage_GetObject(page.get(), 0));
    ASSERT_TRUE(object);
    ASSERT_TRUE(FPDFPage_RemoveObject(page.get(), object.get()));
  }
  ASSERT_TRUE(FPDFPage_GenerateContent(page.get()));

  ASSERT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // The new document should render as empty.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  ASSERT_TRUE(saved_page);
  ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
  EXPECT_EQ(pdfium::kBlankPage612By792Checksum, HashBitmap(bitmap.get()));
  CloseSavedPage(saved_page);
  CloseSavedDocument();

  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  EXPECT_THAT(GetString(), HasSubstr("/Root "));
  EXPECT_THAT(GetString(), Not(HasSubstr("/Image")));
  EXPECT_LT(GetString().size(), 600u);
}

#ifdef PDF_ENABLE_XFA
TEST_F(FPDFSaveEmbedderTest, SaveXFADoc) {
  ASSERT_TRUE(OpenDocument("simple_xfa.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), StartsWith("%PDF-1.7\r\n"));
  ASSERT_TRUE(OpenSavedDocument());
  // TODO(tsepez): check for XFA forms in document
  CloseSavedDocument();
}
#endif  // PDF_ENABLE_XFA

TEST_F(FPDFSaveEmbedderTest, Bug342) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("0000000000 65535 f\r\n"));
  EXPECT_THAT(GetString(), Not(HasSubstr("0000000000 65536 f\r\n")));
}

TEST_F(FPDFSaveEmbedderTest, Bug905142) {
  ASSERT_TRUE(OpenDocument("bug_905142.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("/Length 0"));
}

// Should not trigger a DCHECK() failure in CFX_FileBufferArchive.
// Fails because the PDF is malformed.
TEST_F(FPDFSaveEmbedderTest, Bug1328389) {
  ASSERT_TRUE(OpenDocument("bug_1328389.pdf"));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  EXPECT_THAT(GetString(), HasSubstr("/Foo/"));
}
