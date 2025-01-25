// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "build/build_config.h"
#include "constants/ascii.h"
#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_fwlevent.h"
#include "public/fpdf_progressive.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/embedder_test_mock_delegate.h"
#include "testing/embedder_test_timer_handling_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using pdfium::TextFormChecksum;

using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::StrEq;

using FPDFFormFillEmbedderTest = EmbedderTest;

// A base class for many related tests that involve clicking and typing into
// form fields.
class FPDFFormFillInteractiveEmbedderTest : public FPDFFormFillEmbedderTest {
 protected:
  FPDFFormFillInteractiveEmbedderTest() = default;
  ~FPDFFormFillInteractiveEmbedderTest() override = default;

  void SetUp() override {
    FPDFFormFillEmbedderTest::SetUp();
    ASSERT_TRUE(OpenDocument(GetDocumentName()));
    page_ = LoadPage(0);
    ASSERT_TRUE(page_);
    FormSanityChecks();
  }

  void TearDown() override {
    UnloadPage(page_);
    FPDFFormFillEmbedderTest::TearDown();
  }

  // Returns the name of the PDF to use.
  virtual const char* GetDocumentName() const = 0;

  // Returns the type of field(s) in the PDF.
  virtual int GetFormType() const = 0;

  // Optionally do some sanity check on the document after loading.
  virtual void FormSanityChecks() {}

  FPDF_PAGE page() { return page_; }

  int GetFormTypeAtPoint(const CFX_PointF& point) {
    return FPDFPage_HasFormFieldAtPoint(form_handle(), page_, point.x, point.y);
  }

  void ClickOnFormFieldAtPoint(const CFX_PointF& point) {
    // Click on the text field or combobox as specified by coordinates.
    FORM_OnMouseMove(form_handle(), page_, 0, point.x, point.y);
    FORM_OnLButtonDown(form_handle(), page_, 0, point.x, point.y);
    FORM_OnLButtonUp(form_handle(), page_, 0, point.x, point.y);
  }

  void DoubleClickOnFormFieldAtPoint(const CFX_PointF& point) {
    // Click on the text field or combobox as specified by coordinates.
    FORM_OnMouseMove(form_handle(), page_, 0, point.x, point.y);
    FORM_OnLButtonDoubleClick(form_handle(), page_, 0, point.x, point.y);
  }

  void TypeTextIntoTextField(int num_chars, const CFX_PointF& point) {
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(point));
    ClickOnFormFieldAtPoint(point);

    // Type text starting with 'A' to as many chars as specified by |num_chars|.
    for (int i = 0; i < num_chars; ++i) {
      FORM_OnChar(form_handle(), page_, 'A' + i, 0);
    }
  }

  // Navigates to text field using the mouse and then selects text via the
  // shift and specfied left or right arrow key.
  void SelectTextWithKeyboard(int num_chars,
                              int arrow_key,
                              const CFX_PointF& point) {
    // Navigate to starting position for selection.
    ClickOnFormFieldAtPoint(point);

    // Hold down shift (and don't release until entire text is selected).
    FORM_OnKeyDown(form_handle(), page_, FWL_VKEY_Shift, 0);

    // Select text char by char via left or right arrow key.
    for (int i = 0; i < num_chars; ++i) {
      FORM_OnKeyDown(form_handle(), page_, arrow_key, FWL_EVENTFLAG_ShiftKey);
      FORM_OnKeyUp(form_handle(), page_, arrow_key, FWL_EVENTFLAG_ShiftKey);
    }
    FORM_OnKeyUp(form_handle(), page_, FWL_VKEY_Shift, 0);
  }

  // Uses the mouse to navigate to text field and select text.
  void SelectTextWithMouse(const CFX_PointF& start, const CFX_PointF& end) {
    DCHECK_EQ(start.y, end.y);

    // Navigate to starting position and click mouse.
    FORM_OnMouseMove(form_handle(), page_, 0, start.x, start.y);
    FORM_OnLButtonDown(form_handle(), page_, 0, start.x, start.y);

    // Hold down mouse until reach end of desired selection.
    FORM_OnMouseMove(form_handle(), page_, 0, end.x, end.y);
    FORM_OnLButtonUp(form_handle(), page_, 0, end.x, end.y);
  }

  void SelectAllTextAtPoint(const CFX_PointF& point) {
    FocusOnPoint(point);
    EXPECT_TRUE(FORM_SelectAllText(form_handle(), page_));
  }

  void CheckSelection(ByteStringView expected_string) {
    unsigned long actual_len =
        FORM_GetSelectedText(form_handle(), page_, nullptr, 0);
    ASSERT_NE(actual_len, 0U);
    ASSERT_LT(actual_len, 1000U);
    ASSERT_EQ(actual_len % sizeof(FPDF_WCHAR), 0U);

    std::vector<FPDF_WCHAR> buf(actual_len / sizeof(FPDF_WCHAR));
    ASSERT_EQ(actual_len, FORM_GetSelectedText(form_handle(), page_, buf.data(),
                                               actual_len));
    EXPECT_EQ(expected_string, ByteStringView(GetPlatformString(buf.data())));
  }

  void FocusOnPoint(const CFX_PointF& point) {
    EXPECT_TRUE(FORM_OnFocus(form_handle(), page(), 0, point.x, point.y));
  }

  void CheckFocusedFieldText(ByteStringView expected_string) {
    unsigned long actual_len =
        FORM_GetFocusedText(form_handle(), page_, nullptr, 0);
    ASSERT_NE(actual_len, 0U);
    ASSERT_LT(actual_len, 1000U);
    ASSERT_EQ(actual_len % sizeof(FPDF_WCHAR), 0U);

    std::vector<FPDF_WCHAR> buf(actual_len / sizeof(FPDF_WCHAR));
    ASSERT_EQ(actual_len, FORM_GetFocusedText(form_handle(), page_, buf.data(),
                                              actual_len));
    EXPECT_EQ(expected_string, ByteStringView(GetPlatformString(buf.data())));
  }

  void CheckCanUndo(bool expected_result) {
    EXPECT_EQ(expected_result, !!FORM_CanUndo(form_handle(), page_));
  }

  void CheckCanRedo(bool expected_result) {
    EXPECT_EQ(expected_result, !!FORM_CanRedo(form_handle(), page_));
  }

  void PerformUndo() { EXPECT_TRUE(FORM_Undo(form_handle(), page_)); }

  void PerformRedo() { EXPECT_TRUE(FORM_Redo(form_handle(), page_)); }

  void SetIndexSelectedShouldSucceed(int index, bool selected) {
    EXPECT_TRUE(FORM_SetIndexSelected(form_handle(), page_, index, selected));
  }

  void SetIndexSelectedShouldFail(int index, bool selected) {
    EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page_, index, selected));
  }

  void CheckIsIndexSelected(int index, bool expected) {
    EXPECT_EQ(expected, FORM_IsIndexSelected(form_handle(), page_, index));
  }

 private:
  FPDF_PAGE page_ = nullptr;
};

class FPDFFormFillTextFormEmbedderTest
    : public FPDFFormFillInteractiveEmbedderTest {
 protected:
  FPDFFormFillTextFormEmbedderTest() = default;
  ~FPDFFormFillTextFormEmbedderTest() override = default;

  const char* GetDocumentName() const override {
    // PDF with several form text fields:
    // - "Text Box" - Regular text box with no special attributes.
    // - "ReadOnly" - Ff: 1.
    // - "CharLimit" - MaxLen: 10, V: Elephant.
    return "text_form_multiple.pdf";
  }

  int GetFormType() const override { return FPDF_FORMFIELD_TEXTFIELD; }

  void FormSanityChecks() override {
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(CharLimitFormBegin()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(CharLimitFormEnd()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(RegularFormBegin()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(RegularFormEnd()));
  }

  void SelectAllCharLimitFormTextWithMouse() {
    SelectAllTextAtPoint(CharLimitFormBegin());
  }

  void SelectAllRegularFormTextWithMouse() {
    SelectAllTextAtPoint(RegularFormBegin());
  }

  const CFX_PointF& CharLimitFormBegin() const {
    static const CFX_PointF point = CharLimitFormAtX(kFormBeginX);
    return point;
  }

  const CFX_PointF& CharLimitFormEnd() const {
    static const CFX_PointF point = CharLimitFormAtX(kFormEndX);
    return point;
  }

  const CFX_PointF& RegularFormBegin() const {
    static const CFX_PointF point = RegularFormAtX(kFormBeginX);
    return point;
  }

  const CFX_PointF& RegularFormEnd() const {
    static const CFX_PointF point = RegularFormAtX(kFormEndX);
    return point;
  }

  static CFX_PointF CharLimitFormAtX(float x) {
    DCHECK(x >= kFormBeginX);
    DCHECK(x <= kFormEndX);
    return CFX_PointF(x, kCharLimitFormY);
  }

  static CFX_PointF RegularFormAtX(float x) {
    DCHECK(x >= kFormBeginX);
    DCHECK(x <= kFormEndX);
    return CFX_PointF(x, kRegularFormY);
  }

 private:
  static constexpr float kFormBeginX = 102.0;
  static constexpr float kFormEndX = 195.0;
  static constexpr float kCharLimitFormY = 60.0;
  static constexpr float kRegularFormY = 115.0;
};

class FPDFFormFillComboBoxFormEmbedderTest
    : public FPDFFormFillInteractiveEmbedderTest {
 protected:
  FPDFFormFillComboBoxFormEmbedderTest() = default;
  ~FPDFFormFillComboBoxFormEmbedderTest() override = default;

  const char* GetDocumentName() const override {
    // PDF with form comboboxes:
    // - "Combo_Editable" - Ff: 393216, 3 options with pair values.
    // - "Combo1" - Ff: 131072, 3 options with single values.
    // - "Combo_ReadOnly" - Ff: 131073, 3 options with single values.
    return "combobox_form.pdf";
  }

  int GetFormType() const override { return FPDF_FORMFIELD_COMBOBOX; }

  void FormSanityChecks() override {
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(EditableFormBegin()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(EditableFormEnd()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(EditableFormDropDown()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(NonEditableFormBegin()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(NonEditableFormEnd()));
    EXPECT_EQ(GetFormType(), GetFormTypeAtPoint(NonEditableFormDropDown()));
  }

  void SelectEditableFormOption(int item_index) {
    DCHECK(item_index >= 0);
    DCHECK(item_index < 3);
    SelectOption(item_index, EditableFormDropDown());
  }

  void SelectNonEditableFormOption(int item_index) {
    DCHECK(item_index >= 0);
    DCHECK(item_index < 26);
    SelectOption(item_index, NonEditableFormDropDown());
  }

  void SelectAllEditableFormTextWithMouse() {
    SelectAllTextAtPoint(EditableFormBegin());
  }

  void FocusOnEditableForm() { FocusOnPoint(EditableFormDropDown()); }

  void FocusOnNonEditableForm() { FocusOnPoint(NonEditableFormDropDown()); }

  const CFX_PointF& EditableFormBegin() const {
    static const CFX_PointF point = EditableFormAtX(kFormBeginX);
    return point;
  }

  const CFX_PointF& EditableFormEnd() const {
    static const CFX_PointF point = EditableFormAtX(kFormEndX);
    return point;
  }

  const CFX_PointF& EditableFormDropDown() const {
    static const CFX_PointF point(kFormDropDownX, kEditableFormY);
    return point;
  }

  const CFX_PointF& NonEditableFormBegin() const {
    static const CFX_PointF point = NonEditableFormAtX(kFormBeginX);
    return point;
  }

  const CFX_PointF& NonEditableFormEnd() const {
    static const CFX_PointF point = NonEditableFormAtX(kFormEndX);
    return point;
  }

  const CFX_PointF& NonEditableFormDropDown() const {
    static const CFX_PointF point(kFormDropDownX, kNonEditableFormY);
    return point;
  }

  static CFX_PointF EditableFormAtX(float x) {
    DCHECK(x >= kFormBeginX);
    DCHECK(x <= kFormEndX);
    return CFX_PointF(x, kEditableFormY);
  }

  static CFX_PointF NonEditableFormAtX(float x) {
    DCHECK(x >= kFormBeginX);
    DCHECK(x <= kFormEndX);
    return CFX_PointF(x, kNonEditableFormY);
  }

 private:
  // Selects one of the pre-selected values from a combobox with three options.
  // Options are specified by |item_index|, which is 0-based.
  void SelectOption(int item_index, const CFX_PointF& point) {
    // Navigate to button for drop down and click mouse to reveal options.
    ClickOnFormFieldAtPoint(point);

    // Calculate to Y-coordinate of dropdown option to be selected.
    constexpr double kChoiceHeight = 15;
    CFX_PointF option_point = point;
    option_point.y -= kChoiceHeight * (item_index + 1);

    // Move left to avoid scrollbar.
    option_point.x -= 20;

    // Navigate to option and click mouse to select it.
    ClickOnFormFieldAtPoint(option_point);
  }

  static constexpr float kFormBeginX = 102.0;
  static constexpr float kFormEndX = 183.0;
  static constexpr float kFormDropDownX = 192.0;
  static constexpr float kEditableFormY = 360.0;
  static constexpr float kNonEditableFormY = 410.0;
};

class FPDFFormFillListBoxFormEmbedderTest
    : public FPDFFormFillInteractiveEmbedderTest {
 protected:
  FPDFFormFillListBoxFormEmbedderTest() = default;
  ~FPDFFormFillListBoxFormEmbedderTest() override = default;

  const char* GetDocumentName() const override {
    // PDF with form listboxes:
    // - "Listbox_SingleSelect" - Ff: 0, 3 options with pair values.
    // - "Listbox_MultiSelect" - Ff: 2097152, 26 options with single values.
    // - "Listbox_ReadOnly" - Ff: 1, 3 options with single values.
    // - "Listbox_MultiSelectMultipleIndices" - Ff: 2097152, 5 options with
    //    single values.
    // - "Listbox_MultiSelectMultipleValues" - same configs as above.
    // - "Listbox_MultiSelectMultipleMismatch" - same configs as above.
    // - "Listbox_SingleSelectLastSelected" - Ff: 0, 10 options with single
    //    values.
    return "listbox_form.pdf";
  }

  int GetFormType() const override { return FPDF_FORMFIELD_LISTBOX; }

  void FormSanityChecks() override {
    EXPECT_EQ(GetFormType(),
              GetFormTypeAtPoint(SingleSelectFirstVisibleOption()));
    EXPECT_EQ(GetFormType(),
              GetFormTypeAtPoint(SingleSelectSecondVisibleOption()));
    EXPECT_EQ(GetFormType(),
              GetFormTypeAtPoint(MultiSelectFirstVisibleOption()));
    EXPECT_EQ(GetFormType(),
              GetFormTypeAtPoint(MultiSelectSecondVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleIndicesFirstVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleIndicesSecondVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleValuesFirstVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleValuesSecondVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleMismatchFirstVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(MultiSelectMultipleMismatchSecondVisibleOption()));
    EXPECT_EQ(GetFormType(),
              GetFormTypeAtPoint(SingleSelectLastSelectedFirstVisibleOption()));
    EXPECT_EQ(
        GetFormType(),
        GetFormTypeAtPoint(SingleSelectLastSelectedSecondVisibleOption()));
  }

  void ClickOnSingleSelectFormOption(int item_index) {
    // Only the first two indices are visible so can only click on those
    // without scrolling.
    DCHECK(item_index >= 0);
    DCHECK(item_index < 2);
    if (item_index == 0) {
      ClickOnFormFieldAtPoint(SingleSelectFirstVisibleOption());
    } else {
      ClickOnFormFieldAtPoint(SingleSelectSecondVisibleOption());
    }
  }

  void ClickOnMultiSelectFormOption(int item_index) {
    // Only the first two indices are visible so can only click on those
    // without scrolling.
    DCHECK(item_index >= 0);
    DCHECK(item_index < 2);
    if (item_index == 0) {
      ClickOnFormFieldAtPoint(MultiSelectFirstVisibleOption());
    } else {
      ClickOnFormFieldAtPoint(MultiSelectSecondVisibleOption());
    }
  }

  void ClickOnMultiSelectMultipleValuesFormOption(int item_index) {
    // Only two indices are visible so can only click on those
    // without scrolling.
    DCHECK(item_index >= 0);
    DCHECK(item_index < 2);
    if (item_index == 0) {
      ClickOnFormFieldAtPoint(MultiSelectMultipleValuesFirstVisibleOption());
    } else {
      ClickOnFormFieldAtPoint(MultiSelectMultipleValuesSecondVisibleOption());
    }
  }

  void ClickOnSingleSelectLastSelectedFormOption(int item_index) {
    // Only two indices are visible so can only click on those
    // without scrolling.
    DCHECK(item_index >= 0);
    DCHECK(item_index < 2);
    if (item_index == 0) {
      ClickOnFormFieldAtPoint(SingleSelectLastSelectedFirstVisibleOption());
    } else {
      ClickOnFormFieldAtPoint(SingleSelectLastSelectedSecondVisibleOption());
    }
  }

  void FocusOnSingleSelectForm() {
    FocusOnPoint(SingleSelectFirstVisibleOption());
  }

  void FocusOnMultiSelectForm() {
    FocusOnPoint(MultiSelectFirstVisibleOption());
  }

  void FocusOnMultiSelectMultipleIndicesForm() {
    FocusOnPoint(MultiSelectMultipleIndicesFirstVisibleOption());
  }

  void FocusOnMultiSelectMultipleValuesForm() {
    FocusOnPoint(MultiSelectMultipleValuesFirstVisibleOption());
  }

  void FocusOnMultiSelectMultipleMismatchForm() {
    FocusOnPoint(MultiSelectMultipleMismatchFirstVisibleOption());
  }

  void FocusOnSingleSelectLastSelectedForm() {
    FocusOnPoint(SingleSelectLastSelectedFirstVisibleOption());
  }

  void FocusOnPoint(const CFX_PointF& point) {
    EXPECT_EQ(true, FORM_OnFocus(form_handle(), page(), 0, point.x, point.y));
  }

  const CFX_PointF& SingleSelectFirstVisibleOption() const {
    static const CFX_PointF point(kFormBeginX, kSingleFormYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& SingleSelectSecondVisibleOption() const {
    static const CFX_PointF point(kFormBeginX, kSingleFormYSecondVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectFirstVisibleOption() const {
    static const CFX_PointF point(kFormBeginX, kMultiFormYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectSecondVisibleOption() const {
    static const CFX_PointF point(kFormBeginX, kMultiFormYSecondVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleIndicesFirstVisibleOption() const {
    static const CFX_PointF point(kFormBeginX,
                                  kMultiFormMultipleIndicesYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleIndicesSecondVisibleOption() const {
    static const CFX_PointF point(
        kFormBeginX, kMultiFormMultipleIndicesYSecondVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleValuesFirstVisibleOption() const {
    static const CFX_PointF point(kFormBeginX,
                                  kMultiFormMultipleValuesYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleValuesSecondVisibleOption() const {
    static const CFX_PointF point(kFormBeginX,
                                  kMultiFormMultipleValuesYSecondVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleMismatchFirstVisibleOption() const {
    static const CFX_PointF point(
        kFormBeginX, kMultiFormMultipleMismatchYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& MultiSelectMultipleMismatchSecondVisibleOption() const {
    static const CFX_PointF point(
        kFormBeginX, kMultiFormMultipleMismatchYSecondVisibleOption);
    return point;
  }

  const CFX_PointF& SingleSelectLastSelectedFirstVisibleOption() const {
    static const CFX_PointF point(kFormBeginX,
                                  kSingleFormLastSelectedYFirstVisibleOption);
    return point;
  }

  const CFX_PointF& SingleSelectLastSelectedSecondVisibleOption() const {
    static const CFX_PointF point(kFormBeginX,
                                  kSingleFormLastSelectedYSecondVisibleOption);
    return point;
  }

 private:
  static constexpr float kFormBeginX = 102.0;
  static constexpr float kSingleFormYFirstVisibleOption = 371.0;
  static constexpr float kSingleFormYSecondVisibleOption = 358.0;
  static constexpr float kMultiFormYFirstVisibleOption = 423.0;
  static constexpr float kMultiFormYSecondVisibleOption = 408.0;
  static constexpr float kMultiFormMultipleIndicesYFirstVisibleOption = 273.0;
  static constexpr float kMultiFormMultipleIndicesYSecondVisibleOption = 258.0;
  static constexpr float kMultiFormMultipleValuesYFirstVisibleOption = 223.0;
  static constexpr float kMultiFormMultipleValuesYSecondVisibleOption = 208.0;
  static constexpr float kMultiFormMultipleMismatchYFirstVisibleOption = 173.0;
  static constexpr float kMultiFormMultipleMismatchYSecondVisibleOption = 158.0;
  static constexpr float kSingleFormLastSelectedYFirstVisibleOption = 123.0;
  static constexpr float kSingleFormLastSelectedYSecondVisibleOption = 108.0;
};

class FPDFFormFillTextFormEmbedderTestVersion2
    : public FPDFFormFillTextFormEmbedderTest {
  void SetUp() override {
    SetFormFillInfoVersion(2);
    FPDFFormFillInteractiveEmbedderTest::SetUp();
  }
};

TEST_F(FPDFFormFillEmbedderTest, FirstTest) {
  EmbedderTestMockDelegate mock;
  EXPECT_CALL(mock, Alert(_, _, _, _)).Times(0);
  EXPECT_CALL(mock, UnsupportedHandler(_)).Times(0);
  EXPECT_CALL(mock, SetTimer(_, _)).Times(0);
  EXPECT_CALL(mock, KillTimer(_)).Times(0);
  EXPECT_CALL(mock, OnFocusChange(_, _, _)).Times(0);
  EXPECT_CALL(mock, DoURIAction(_)).Times(0);
  EXPECT_CALL(mock, DoURIActionWithKeyboardModifier(_, _, _)).Times(0);
  EXPECT_CALL(mock, DoGoToAction(_, _, _, _, _)).Times(0);
  SetDelegate(&mock);

  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
}

TEST_F(FPDFFormFillEmbedderTest, Bug487928) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_487928.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(5000);
}

TEST_F(FPDFFormFillEmbedderTest, Bug507316) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_507316.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(2);
  EXPECT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(4000);
}

TEST_F(FPDFFormFillEmbedderTest, Bug514690) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  // Test that FORM_OnMouseMove() etc. permit null HANDLES and PAGES.
  FORM_OnMouseMove(nullptr, page.get(), 0, 10.0, 10.0);
  FORM_OnMouseMove(form_handle(), nullptr, 0, 10.0, 10.0);
}

TEST_F(FPDFFormFillEmbedderTest, Bug900552) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_900552.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(4000);

  // Simulate a repaint.
  FPDF_BITMAP bitmap = FPDFBitmap_Create(512, 512, 0);
  ASSERT_TRUE(bitmap);
  FPDF_RenderPageBitmap_Start(bitmap, page.get(), 0, 0, 512, 512, 0, 0,
                              nullptr);
  FPDFBitmap_Destroy(bitmap);
}

TEST_F(FPDFFormFillEmbedderTest, Bug901654Case1) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_901654.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(4000);

  // Simulate a repaint.
  {
    ScopedFPDFBitmap bitmap(FPDFBitmap_Create(512, 512, 0));
    FPDF_RenderPageBitmap_Start(bitmap.get(), page.get(), 0, 0, 512, 512, 0, 0,
                                nullptr);
  }
}

TEST_F(FPDFFormFillEmbedderTest, Bug901654Case2) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_901654_2.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(4000);

  // Simulate a repaint.
  {
    ScopedFPDFBitmap bitmap(FPDFBitmap_Create(512, 512, 0));
    FPDF_RenderPageBitmap_Start(bitmap.get(), page.get(), 0, 0, 512, 512, 0, 0,
                                nullptr);
  }
}

TEST_F(FPDFFormFillEmbedderTest, GetFocusedAnnotation) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  std::vector<ScopedEmbedderTestPage> pages;
  for (size_t i = 0; i < 3; ++i) {
    pages.push_back(LoadScopedPage(i));
    ASSERT_TRUE(pages.back());
  }

  // Ensure that there is no focused annotation.
  FPDF_ANNOTATION annot = nullptr;
  int page_index = -2;
  ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
  EXPECT_FALSE(annot);
  EXPECT_EQ(-1, page_index);

  // Validate that nullptr values are handled properly.
  EXPECT_FALSE(FORM_GetFocusedAnnot(nullptr, &page_index, &annot));
  EXPECT_FALSE(FORM_GetFocusedAnnot(form_handle(), &page_index, nullptr));
  EXPECT_FALSE(FORM_GetFocusedAnnot(form_handle(), nullptr, &annot));

  const CFX_PointF right_bottom_annot_point(410.0f, 210.0f);
  constexpr int kExpectedAnnotIndex = 3;

  for (size_t i = 0; i < pages.size(); ++i) {
    // Invoke click on the form field to bring it to focus.
    FORM_OnMouseMove(form_handle(), pages[i].get(), 0,
                     right_bottom_annot_point.x, right_bottom_annot_point.y);
    FORM_OnLButtonDown(form_handle(), pages[i].get(), 0,
                       right_bottom_annot_point.x, right_bottom_annot_point.y);
    FORM_OnLButtonUp(form_handle(), pages[i].get(), 0,
                     right_bottom_annot_point.x, right_bottom_annot_point.y);

    ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    ASSERT_TRUE(annot);

    EXPECT_EQ(kExpectedAnnotIndex,
              FPDFPage_GetAnnotIndex(pages[i].get(), annot));
    EXPECT_EQ(static_cast<int>(i), page_index);

    FPDFPage_CloseAnnot(annot);
  }
}

TEST_F(FPDFFormFillEmbedderTest, SetFocusedAnnotation) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  std::vector<ScopedEmbedderTestPage> pages;
  for (size_t i = 0; i < 3; ++i) {
    pages.push_back(LoadScopedPage(i));
    ASSERT_TRUE(pages.back());
  }

  // Ensure that there is no focused annotation.
  FPDF_ANNOTATION annot = nullptr;
  int page_index = -2;
  ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
  EXPECT_FALSE(annot);
  EXPECT_EQ(-1, page_index);

  // Validate that nullptr values are handled properly.
  EXPECT_FALSE(FORM_SetFocusedAnnot(nullptr, annot));
  EXPECT_FALSE(FORM_SetFocusedAnnot(form_handle(), nullptr));

  constexpr int kExpectedAnnotIndex = 2;

  for (size_t i = 0; i < pages.size(); ++i) {
    // Setting focus on an annotation on page i.
    ScopedFPDFAnnotation focused_annot(
        FPDFPage_GetAnnot(pages[i].get(), kExpectedAnnotIndex));
    ASSERT_TRUE(focused_annot);

    ASSERT_TRUE(FORM_SetFocusedAnnot(form_handle(), focused_annot.get()));

    ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(kExpectedAnnotIndex,
              FPDFPage_GetAnnotIndex(pages[i].get(), annot));
    EXPECT_EQ(static_cast<int>(i), page_index);

    FPDFPage_CloseAnnot(annot);
  }
}

TEST_F(FPDFFormFillEmbedderTest, FormFillFirstTab) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));
  int page_index = -2;
  FPDF_ANNOTATION annot = nullptr;
  EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
  EXPECT_EQ(0, page_index);
  ASSERT_TRUE(annot);
  EXPECT_EQ(1, FPDFPage_GetAnnotIndex(page.get(), annot));
  FPDFPage_CloseAnnot(annot);
}

TEST_F(FPDFFormFillEmbedderTest, FormFillFirstShiftTab) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first shift-tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                             FWL_EVENTFLAG_ShiftKey));
  int page_index = -2;
  FPDF_ANNOTATION annot = nullptr;
  EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
  EXPECT_EQ(0, page_index);
  ASSERT_TRUE(annot);
  EXPECT_EQ(0, FPDFPage_GetAnnotIndex(page.get(), annot));
  FPDFPage_CloseAnnot(annot);
}

TEST_F(FPDFFormFillEmbedderTest, FormFillContinuousTab) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Tabs should iterate focus over annotations.
  for (int expected : {1, 2, 3, 0}) {
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));
    int page_index = -2;
    FPDF_ANNOTATION annot = nullptr;
    EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(0, page_index);
    ASSERT_TRUE(annot);
    EXPECT_EQ(expected, FPDFPage_GetAnnotIndex(page.get(), annot));
    FPDFPage_CloseAnnot(annot);
  }

  // Tab should not be handled as the last annotation of the page is in focus.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));
}

TEST_F(FPDFFormFillEmbedderTest, FormFillContinuousShiftTab) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Shift-tabs should iterate focus over annotations.
  for (int expected : {0, 3, 2, 1}) {
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                               FWL_EVENTFLAG_ShiftKey));
    int page_index = -2;
    FPDF_ANNOTATION annot = nullptr;
    EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(0, page_index);
    ASSERT_TRUE(annot);
    EXPECT_EQ(expected, FPDFPage_GetAnnotIndex(page.get(), annot));
    FPDFPage_CloseAnnot(annot);
  }

  // Shift-tab should not be handled as the first annotation of the page is in
  // focus.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                              FWL_EVENTFLAG_ShiftKey));
}

TEST_F(FPDFFormFillEmbedderTest, TabWithModifiers) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                              FWL_EVENTFLAG_ControlKey));

  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                              FWL_EVENTFLAG_AltKey));

  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                     (FWL_EVENTFLAG_ControlKey | FWL_EVENTFLAG_ShiftKey)));

  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                              (FWL_EVENTFLAG_AltKey | FWL_EVENTFLAG_ShiftKey)));
}

TEST_F(FPDFFormFillEmbedderTest, KeyPressWithNoFocusedAnnot) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // There should be no focused annotation to start with.
  int page_index = -2;
  FPDF_ANNOTATION annot = nullptr;
  EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
  EXPECT_EQ(-1, page_index);
  EXPECT_FALSE(annot);

  static constexpr int kKeysToPress[] = {
      FWL_VKEY_NewLine, FWL_VKEY_Return, FWL_VKEY_Space,
      FWL_VKEY_Delete,  FWL_VKEY_0,      FWL_VKEY_9,
      FWL_VKEY_A,       FWL_VKEY_Z,      FWL_VKEY_F1,
  };
  for (int key : kKeysToPress) {
    // Pressing random keys when there is no focus should not trigger focus.
    EXPECT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), key, 0));
    page_index = -2;
    annot = nullptr;
    EXPECT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(-1, page_index);
    EXPECT_FALSE(annot);
  }
}

#ifdef PDF_ENABLE_XFA
TEST_F(FPDFFormFillEmbedderTest, XFAFormFillFirstTab) {
  ASSERT_TRUE(OpenDocument("xfa/email_recommended.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));
}

TEST_F(FPDFFormFillEmbedderTest, XFAFormFillFirstShiftTab) {
  ASSERT_TRUE(OpenDocument("xfa/email_recommended.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first shift-tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                             FWL_EVENTFLAG_ShiftKey));
}

TEST_F(FPDFFormFillEmbedderTest, XFAFormFillContinuousTab) {
  ASSERT_TRUE(OpenDocument("xfa/email_recommended.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));

  // Subsequent tabs should move focus over annotations.
  for (size_t i = 0; i < 9; ++i)
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));

  // Tab should not be handled as the last annotation of the page is in focus.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));
}

TEST_F(FPDFFormFillEmbedderTest, XFAFormFillContinuousShiftTab) {
  ASSERT_TRUE(OpenDocument("xfa/email_recommended.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Invoking first shift-tab on the page.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                             FWL_EVENTFLAG_ShiftKey));

  // Subsequent shift-tabs should move focus over annotations.
  for (size_t i = 0; i < 9; ++i) {
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                               FWL_EVENTFLAG_ShiftKey));
  }

  // Shift-tab should not be handled as the first annotation of the page is in
  // focus.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                              FWL_EVENTFLAG_ShiftKey));
}
#endif  // PDF_ENABLE_XFA

class DoURIActionBlockedDelegate final : public EmbedderTest::Delegate {
 public:
  void DoURIAction(FPDF_BYTESTRING uri) override {
    FAIL() << "Navigated to " << uri;
  }
};

TEST_F(FPDFFormFillEmbedderTest, Bug851821) {
  DoURIActionBlockedDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("redirect.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();
}

TEST_F(FPDFFormFillEmbedderTest, CheckReadOnlyInCheckbox) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("click_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  {
    // Check for read-only checkbox.
    ScopedFPDFAnnotation focused_annot(FPDFPage_GetAnnot(page.get(), 1));
    ASSERT_TRUE(FORM_SetFocusedAnnot(form_handle(), focused_annot.get()));

    // Shift-tab to the previous control.
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab,
                               FWL_EVENTFLAG_ShiftKey));
    FPDF_ANNOTATION annot = nullptr;
    int page_index = -1;
    ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(0, FPDFPage_GetAnnotIndex(page.get(), annot));

    // The read-only checkbox is initially in checked state.
    EXPECT_TRUE(FPDFAnnot_IsChecked(form_handle(), annot));

    EXPECT_TRUE(
        FORM_OnChar(form_handle(), page.get(), pdfium::ascii::kReturn, 0));
    EXPECT_TRUE(FPDFAnnot_IsChecked(form_handle(), annot));

    EXPECT_TRUE(
        FORM_OnChar(form_handle(), page.get(), pdfium::ascii::kSpace, 0));
    EXPECT_TRUE(FPDFAnnot_IsChecked(form_handle(), annot));

    FPDFPage_CloseAnnot(annot);
  }
}

TEST_F(FPDFFormFillEmbedderTest, CheckReadOnlyInRadiobutton) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("click_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  {
    // Check for read-only radio button.
    ScopedFPDFAnnotation focused_annot(FPDFPage_GetAnnot(page.get(), 1));
    ASSERT_TRUE(FORM_SetFocusedAnnot(form_handle(), focused_annot.get()));

    // Tab to the next control.
    ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page.get(), FWL_VKEY_Tab, 0));

    FPDF_ANNOTATION annot = nullptr;
    int page_index = -1;
    ASSERT_TRUE(FORM_GetFocusedAnnot(form_handle(), &page_index, &annot));
    EXPECT_EQ(2, FPDFPage_GetAnnotIndex(page.get(), annot));
    // The read-only radio button is initially in checked state.
    EXPECT_FALSE(FPDFAnnot_IsChecked(form_handle(), annot));

    EXPECT_TRUE(
        FORM_OnChar(form_handle(), page.get(), pdfium::ascii::kReturn, 0));
    EXPECT_FALSE(FPDFAnnot_IsChecked(form_handle(), annot));

    EXPECT_TRUE(
        FORM_OnChar(form_handle(), page.get(), pdfium::ascii::kSpace, 0));
    EXPECT_FALSE(FPDFAnnot_IsChecked(form_handle(), annot));

    FPDFPage_CloseAnnot(annot);
  }
}

#ifdef PDF_ENABLE_V8
TEST_F(FPDFFormFillEmbedderTest, DisableJavaScript) {
  // Test that timers and intervals can't fire without JS.
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocumentWithoutJavaScript("bug_551248.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0U, alerts.size());

  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
}

TEST_F(FPDFFormFillEmbedderTest, DocumentAActions) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("document_aactions.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0U, alerts.size());

  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_WS);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_DS);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_WP);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_DP);

  ASSERT_EQ(4U, alerts.size());
  EXPECT_EQ(L"Will Save", alerts[0].message);
  EXPECT_EQ(L"Did Save", alerts[1].message);
  EXPECT_EQ(L"Will Print", alerts[2].message);
  EXPECT_EQ(L"Did Print", alerts[3].message);
}

TEST_F(FPDFFormFillEmbedderTest, DocumentAActionsDisableJavaScript) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocumentWithoutJavaScript("document_aactions.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0U, alerts.size());

  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_WS);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_DS);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_WP);
  FORM_DoDocumentAAction(form_handle(), FPDFDOC_AACTION_DP);

  ASSERT_EQ(0U, alerts.size());
}

TEST_F(FPDFFormFillEmbedderTest, Bug551248) {
  // Test that timers fire once and intervals fire repeatedly.
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_551248.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0U, alerts.size());

  delegate.AdvanceTime(1000);
  EXPECT_EQ(0U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(1U, alerts.size());  // interval fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(2U, alerts.size());  // timer fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(3U, alerts.size());  // interval fired again.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(3U, alerts.size());  // nothing fired.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(4U, alerts.size());  // interval fired again.
  delegate.AdvanceTime(1000);
  EXPECT_EQ(4U, alerts.size());  // nothing fired.

  ASSERT_EQ(4U, alerts.size());  // nothing else fired.

  EXPECT_EQ(L"interval fired", alerts[0].message);
  EXPECT_EQ(L"Alert", alerts[0].title);
  EXPECT_EQ(0, alerts[0].type);
  EXPECT_EQ(0, alerts[0].icon);

  EXPECT_EQ(L"timer fired", alerts[1].message);
  EXPECT_EQ(L"Alert", alerts[1].title);
  EXPECT_EQ(0, alerts[1].type);
  EXPECT_EQ(0, alerts[1].icon);

  EXPECT_EQ(L"interval fired", alerts[2].message);
  EXPECT_EQ(L"Alert", alerts[2].title);
  EXPECT_EQ(0, alerts[2].type);
  EXPECT_EQ(0, alerts[2].icon);

  EXPECT_EQ(L"interval fired", alerts[3].message);
  EXPECT_EQ(L"Alert", alerts[3].title);
  EXPECT_EQ(0, alerts[3].type);
  EXPECT_EQ(0, alerts[3].icon);
}

TEST_F(FPDFFormFillEmbedderTest, Bug620428) {
  // Test that timers and intervals are cancelable.
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_620428.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();
  delegate.AdvanceTime(5000);

  const auto& alerts = delegate.GetAlerts();
  ASSERT_EQ(1U, alerts.size());
  EXPECT_EQ(L"done", alerts[0].message);
}

TEST_F(FPDFFormFillEmbedderTest, Bug634394) {
  // Cancel timer inside timer callback.
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_634394.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();

  // Timers fire at most once per AdvanceTime(), allow intervals
  // to fire several times if possible.
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(2U, alerts.size());
}

TEST_F(FPDFFormFillEmbedderTest, Bug634716) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_634716.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);
  DoOpenActions();

  // Timers fire at most once per AdvanceTime(), allow intervals
  // to fire several times if possible.
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(2U, alerts.size());
}

TEST_F(FPDFFormFillEmbedderTest, Bug679649) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_679649.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  delegate.SetFailNextTimer();
  DoOpenActions();
  delegate.AdvanceTime(2000);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0u, alerts.size());
}

TEST_F(FPDFFormFillEmbedderTest, Bug707673) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_707673.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  DoOpenActions();
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 140, 590);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 140, 590);
  delegate.AdvanceTime(1000);

  const auto& alerts = delegate.GetAlerts();
  EXPECT_EQ(0u, alerts.size());
}

TEST_F(FPDFFormFillEmbedderTest, Bug765384) {
  ASSERT_TRUE(OpenDocument("bug_765384.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  DoOpenActions();
  FORM_OnLButtonDown(form_handle(), page.get(), 0, 140, 590);
  FORM_OnLButtonUp(form_handle(), page.get(), 0, 140, 590);
}

// Test passes if DCHECK() not hit.
TEST_F(FPDFFormFillEmbedderTest, Bug1477093) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_1477093.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  EXPECT_TRUE(page);

  DoOpenActions();
  delegate.AdvanceTime(1000);
  delegate.AdvanceTime(1000);
}

#endif  // PDF_ENABLE_V8

TEST_F(FPDFFormFillEmbedderTest, FormText) {
  const char* focused_text_form_with_abc_checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "8b743c7a6186360862ca6f6db8f55c8f";
#elif BUILDFLAG(IS_APPLE)
      return "d8cf4e7ef7e1c287441bf350006e66d6";
#else
      return "b9fb2245a98ac48146da84237a37f8cc";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "9fb14198d75ca0a107060c60ca21b0c7";
#else
    return "6e6f790bb14c4fc6107faf8c17d23dbd";
#endif
  }();
  const char* unfocused_text_form_with_abc_checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "37328bf7614d6fc05b03893ee030aec4";
#elif BUILDFLAG(IS_APPLE)
      return "b9702814ac50dc5ef413ea2e9c4002f1";
#else
      return "5f3205f0189d9dde54665f970838f614";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "3c3209357e0c057a0620afa7d83eb784";
#else
    return "94b7e10ac8c662b73e33628ca2f5e63b";
#endif
  }();
  {
    ASSERT_TRUE(OpenDocument("text_form.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap1 = RenderLoadedPage(page.get());
    CompareBitmap(bitmap1.get(), 300, 300, TextFormChecksum());

    // Click on the textfield
    EXPECT_EQ(
        FPDF_FORMFIELD_TEXTFIELD,
        FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 120.0, 120.0));
    EXPECT_EQ(0, FPDFPage_FormFieldZOrderAtPoint(form_handle(), page.get(),
                                                 120.0, 120.0));
    FORM_OnMouseMove(form_handle(), page.get(), 0, 120.0, 120.0);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 120.0, 120.0);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 120.0, 120.0);

    // Write "ABC"
    FORM_OnChar(form_handle(), page.get(), 'A', 0);
    FORM_OnChar(form_handle(), page.get(), 'B', 0);
    FORM_OnChar(form_handle(), page.get(), 'C', 0);
    ScopedFPDFBitmap bitmap2 = RenderLoadedPage(page.get());
    CompareBitmap(bitmap2.get(), 300, 300, focused_text_form_with_abc_checksum);

    // Focus remains despite right clicking out of the textfield
    FORM_OnMouseMove(form_handle(), page.get(), 0, 15.0, 15.0);
    FORM_OnRButtonDown(form_handle(), page.get(), 0, 15.0, 15.0);
    FORM_OnRButtonUp(form_handle(), page.get(), 0, 15.0, 15.0);
    ScopedFPDFBitmap bitmap3 = RenderLoadedPage(page.get());
    CompareBitmap(bitmap3.get(), 300, 300, focused_text_form_with_abc_checksum);

    // Take out focus by clicking out of the textfield
    FORM_OnMouseMove(form_handle(), page.get(), 0, 15.0, 15.0);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 15.0, 15.0);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 15.0, 15.0);
    ScopedFPDFBitmap bitmap4 = RenderLoadedPage(page.get());
    CompareBitmap(bitmap4.get(), 300, 300,
                  unfocused_text_form_with_abc_checksum);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  }
  // Check saved document
  VerifySavedDocument(300, 300, unfocused_text_form_with_abc_checksum);
}

// Tests using FPDF_REVERSE_BYTE_ORDER with FPDF_FFLDraw(). The two rendered
// bitmaps should be different.
TEST_F(FPDFFormFillEmbedderTest, Bug1281) {
  const char* reverse_byte_order_checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
      return "8077970bbd10333f18186a9bb459bbe6";
    }
    return "24fff03d1e663b7ece5f6e69ad837124";
  }();

  ASSERT_TRUE(OpenDocument("bug_890322.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap_normal = RenderLoadedPage(page.get());
  CompareBitmap(bitmap_normal.get(), 200, 200, pdfium::Bug890322Checksum());

  ScopedFPDFBitmap bitmap_reverse_byte_order =
      RenderLoadedPageWithFlags(page.get(), FPDF_REVERSE_BYTE_ORDER);
  CompareBitmap(bitmap_reverse_byte_order.get(), 200, 200,
                reverse_byte_order_checksum);
}

TEST_F(FPDFFormFillEmbedderTest, Bug1302455RenderOnly) {
  const char* checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
      return "520c4415c9977f40d6b4af5a0a94d764";
    }
    return "bbee92af1daec2340c81f482878744d8";
  }();
  {
    ASSERT_TRUE(OpenDocument("bug_1302455.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    CompareBitmap(bitmap.get(), 300, 300, checksum);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  }
  VerifySavedDocument(300, 300, checksum);
}

TEST_F(FPDFFormFillEmbedderTest, Bug1302455EditFirstForm) {
  const char* checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "2e5d64e4280ca954eb528e82f92abb75";
#elif BUILDFLAG(IS_APPLE)
      return "79538a800f8eb0b4965d43a052303592";
#else
      return "143c2bb79fcaecf24f5aa104dce27beb";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "bf5423874f188427d2500a2bc4abebbe";
#else
    return "6a4ac9a15d2c34589616c8f2b05fbedd";
#endif
  }();
  {
    ASSERT_TRUE(OpenDocument("bug_1302455.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    EXPECT_EQ(
        FPDF_FORMFIELD_TEXTFIELD,
        FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 110, 110));
    FORM_OnMouseMove(form_handle(), page.get(), 0, 110, 110);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 110, 110);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 110, 110);
    FORM_OnChar(form_handle(), page.get(), 'A', 0);

    FORM_ForceToKillFocus(form_handle());
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    CompareBitmap(bitmap.get(), 300, 300, checksum);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  }
  VerifySavedDocument(300, 300, checksum);
}

TEST_F(FPDFFormFillEmbedderTest, Bug1302455EditSecondForm) {
  const char* checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "8408fc1796bf17d48b947ed0e4d65ef2";
#elif BUILDFLAG(IS_APPLE)
      return "074449f4bd27611a2e12aef3ad121cd8";
#else
      return "e36726414acb616dc203e8851b510e2c";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "8a0fd8772dba6e1e952e49d159cc64b5";
#else
    return "45a7694933c2ba3c5dc8f6cc18b79175";
#endif
  }();
  {
    ASSERT_TRUE(OpenDocument("bug_1302455.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    EXPECT_EQ(
        FPDF_FORMFIELD_TEXTFIELD,
        FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 110, 170));
    FORM_OnMouseMove(form_handle(), page.get(), 0, 110, 170);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 110, 170);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 110, 170);
    FORM_OnChar(form_handle(), page.get(), 'B', 0);

    FORM_ForceToKillFocus(form_handle());
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    CompareBitmap(bitmap.get(), 300, 300, checksum);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  }
  VerifySavedDocument(300, 300, checksum);
}

TEST_F(FPDFFormFillEmbedderTest, Bug1302455EditBothForms) {
  const char* checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "1c2b618f68d1ad2cfa01fcf38efc4831";
#elif BUILDFLAG(IS_APPLE)
      return "f5a3b8d2db662cad38b4573ef1dd3f1d";
#else
      return "f82a807c056e22aa55d3d7228eedfe6f";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "1f422ee1c520ad74b1a993b64bd4dc4a";
#else
    return "13984969b1e141079ab5f4aa80185463";
#endif
  }();
  {
    ASSERT_TRUE(OpenDocument("bug_1302455.pdf"));
    ScopedEmbedderTestPage page = LoadScopedPage(0);
    ASSERT_TRUE(page);

    EXPECT_EQ(
        FPDF_FORMFIELD_TEXTFIELD,
        FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 110, 110));
    FORM_OnMouseMove(form_handle(), page.get(), 0, 110, 110);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 110, 110);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 110, 110);
    FORM_OnChar(form_handle(), page.get(), 'A', 0);

    EXPECT_EQ(
        FPDF_FORMFIELD_TEXTFIELD,
        FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 110, 170));
    FORM_OnMouseMove(form_handle(), page.get(), 0, 110, 170);
    FORM_OnLButtonDown(form_handle(), page.get(), 0, 110, 170);
    FORM_OnLButtonUp(form_handle(), page.get(), 0, 110, 170);
    FORM_OnChar(form_handle(), page.get(), 'B', 0);

    FORM_ForceToKillFocus(form_handle());
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
    CompareBitmap(bitmap.get(), 300, 300, checksum);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  }
  VerifySavedDocument(300, 300, checksum);
}

TEST_F(FPDFFormFillEmbedderTest, RemoveFormFieldHighlight) {
  const char* no_highlight_checksum = []() {
    if (CFX_DefaultRenderDevice::UseSkiaRenderer()) {
#if BUILDFLAG(IS_WIN)
      return "2235e2ba8349552de0c818ae53257949";
#elif BUILDFLAG(IS_APPLE)
      return "e0ad5b4fe007e2e2c27cf6c6fb5b6529";
#else
      return "3bfddb2529085021ad283b7e65f71525";
#endif
    }
#if BUILDFLAG(IS_APPLE)
    return "5c82aa43e3b478aa1e4c94bb9ef1f11f";
#else
    return "a6268304f7eedfa9ee98fac3caaf2efb";
#endif
  }();

  ASSERT_TRUE(OpenDocument("text_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  ScopedFPDFBitmap bitmap1 = RenderLoadedPage(page.get());
  CompareBitmap(bitmap1.get(), 300, 300, TextFormChecksum());

  // Removing the highlight changes the rendering.
  FPDF_RemoveFormFieldHighlight(form_handle());
  ScopedFPDFBitmap bitmap2 = RenderLoadedPage(page.get());
  CompareBitmap(bitmap2.get(), 300, 300, no_highlight_checksum);

  // Restoring it gives the original rendering.
  SetInitialFormFieldHighlight(form_handle());
  ScopedFPDFBitmap bitmap3 = RenderLoadedPage(page.get());
  CompareBitmap(bitmap3.get(), 300, 300, TextFormChecksum());
}

TEST_F(FPDFFormFillEmbedderTest, HasFormInfoNone) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_EQ(FORMTYPE_NONE, FPDF_GetFormType(document()));
}

TEST_F(FPDFFormFillEmbedderTest, HasFormInfoAcroForm) {
  ASSERT_TRUE(OpenDocument("text_form.pdf"));
  EXPECT_EQ(FORMTYPE_ACRO_FORM, FPDF_GetFormType(document()));
}

TEST_F(FPDFFormFillEmbedderTest, HasFormInfoXFAFull) {
  ASSERT_TRUE(OpenDocument("simple_xfa.pdf"));
  EXPECT_EQ(FORMTYPE_XFA_FULL, FPDF_GetFormType(document()));
}

TEST_F(FPDFFormFillEmbedderTest, HasFormInfoXFAForeground) {
  ASSERT_TRUE(OpenDocument("bug_216.pdf"));
  EXPECT_EQ(FORMTYPE_XFA_FOREGROUND, FPDF_GetFormType(document()));
}

TEST_F(FPDFFormFillEmbedderTest, BadApiInputsText) {
  ASSERT_TRUE(OpenDocument("text_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_FALSE(FORM_SetIndexSelected(nullptr, nullptr, 0, true));
  EXPECT_FALSE(FORM_SetIndexSelected(nullptr, page.get(), 0, true));
  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), nullptr, 0, true));
  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page.get(), -1, true));
  EXPECT_FALSE(FORM_IsIndexSelected(nullptr, nullptr, 0));
  EXPECT_FALSE(FORM_IsIndexSelected(nullptr, page.get(), 0));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), nullptr, 0));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), page.get(), -1));
}

TEST_F(FPDFFormFillEmbedderTest, BadApiInputsComboBox) {
  ASSERT_TRUE(OpenDocument("combobox_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page.get(), -1, true));
  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page.get(), 100, true));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), page.get(), -1));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), page.get(), 100));
}

TEST_F(FPDFFormFillEmbedderTest, BadApiInputsListBox) {
  ASSERT_TRUE(OpenDocument("listbox_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page.get(), -1, true));
  EXPECT_FALSE(FORM_SetIndexSelected(form_handle(), page.get(), 100, true));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), page.get(), -1));
  EXPECT_FALSE(FORM_IsIndexSelected(form_handle(), page.get(), 100));
}

TEST_F(FPDFFormFillEmbedderTest, HasFormFieldAtPointForXFADoc) {
  ASSERT_TRUE(OpenDocument("simple_xfa.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  EXPECT_EQ(-1,
            FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 612, 792));

#ifdef PDF_ENABLE_XFA
  constexpr int kExpectedFieldType = FPDF_FORMFIELD_XFA_TEXTFIELD;
#else
  constexpr int kExpectedFieldType = -1;
#endif
  EXPECT_EQ(kExpectedFieldType,
            FPDFPage_HasFormFieldAtPoint(form_handle(), page.get(), 50, 30));
}

TEST_F(FPDFFormFillEmbedderTest, SelectAllText) {
  ASSERT_TRUE(OpenDocument("text_form.pdf"));
  ScopedEmbedderTestPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);

  // Test bad arguments.
  EXPECT_FALSE(FORM_SelectAllText(nullptr, nullptr));
  EXPECT_FALSE(FORM_SelectAllText(form_handle(), nullptr));
  EXPECT_FALSE(FORM_SelectAllText(nullptr, page.get()));

  // Focus on the text field and add some text.
  EXPECT_TRUE(FORM_OnFocus(form_handle(), page.get(), 0, 115, 115));
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page.get(), text_to_insert.get());

  // Sanity check text field data.
  uint16_t buffer[6];
  ASSERT_EQ(12u, FORM_GetFocusedText(form_handle(), page.get(), nullptr, 0));
  ASSERT_EQ(12u, FORM_GetFocusedText(form_handle(), page.get(), buffer,
                                     sizeof(buffer)));
  EXPECT_EQ("Hello", GetPlatformString(buffer));

  // Check there is no selection.
  ASSERT_EQ(2u, FORM_GetSelectedText(form_handle(), page.get(), nullptr, 0));
  ASSERT_EQ(2u, FORM_GetSelectedText(form_handle(), page.get(), buffer,
                                     sizeof(buffer)));
  EXPECT_EQ("", GetPlatformString(buffer));

  // Check FORM_SelectAllText() works.
  EXPECT_TRUE(FORM_SelectAllText(form_handle(), page.get()));
  ASSERT_EQ(12u, FORM_GetSelectedText(form_handle(), page.get(), nullptr, 0));
  ASSERT_EQ(12u, FORM_GetSelectedText(form_handle(), page.get(), buffer,
                                      sizeof(buffer)));
  EXPECT_EQ("Hello", GetPlatformString(buffer));
}

TEST_F(FPDFFormFillTextFormEmbedderTest, GetSelectedTextEmptyAndBasicKeyboard) {
  // Test empty selection.
  CheckFocusedFieldText("");
  CheckSelection("");

  // Test basic selection.
  TypeTextIntoTextField(3, RegularFormBegin());
  CheckFocusedFieldText("ABC");
  SelectTextWithKeyboard(3, FWL_VKEY_Left, RegularFormAtX(123.0));
  CheckSelection("ABC");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, GetSelectedTextEmptyAndBasicMouse) {
  // Test empty selection.
  CheckFocusedFieldText("");
  CheckSelection("");

  // Test basic selection.
  TypeTextIntoTextField(3, RegularFormBegin());
  CheckFocusedFieldText("ABC");
  SelectTextWithMouse(RegularFormAtX(125.0), RegularFormBegin());
  CheckSelection("ABC");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, GetSelectedTextFragmentsKeyBoard) {
  TypeTextIntoTextField(12, RegularFormBegin());
  CheckFocusedFieldText("ABCDEFGHIJKL");

  // Test selecting first character in forward direction.
  SelectTextWithKeyboard(1, FWL_VKEY_Right, RegularFormBegin());
  CheckSelection("A");

  // Test selecting entire long string in backwards direction.
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("ABCDEFGHIJKL");

  // Test selecting middle section in backwards direction.
  SelectTextWithKeyboard(6, FWL_VKEY_Left, RegularFormAtX(170.0));
  CheckSelection("DEFGHI");

  // Test selecting middle selection in forward direction.
  SelectTextWithKeyboard(6, FWL_VKEY_Right, RegularFormAtX(125.0));
  CheckSelection("DEFGHI");

  // Test selecting last character in backwards direction.
  SelectTextWithKeyboard(1, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("L");
  CheckFocusedFieldText("ABCDEFGHIJKL");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, GetSelectedTextFragmentsMouse) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Test selecting first character in forward direction.
  SelectTextWithMouse(RegularFormBegin(), RegularFormAtX(106.0));
  CheckSelection("A");

  // Test selecting entire long string in backwards direction.
  SelectAllRegularFormTextWithMouse();
  CheckSelection("ABCDEFGHIJKL");

  // Test selecting middle section in backwards direction.
  SelectTextWithMouse(RegularFormAtX(170.0), RegularFormAtX(125.0));
  CheckSelection("DEFGHI");

  // Test selecting middle selection in forward direction.
  SelectTextWithMouse(RegularFormAtX(125.0), RegularFormAtX(170.0));
  CheckSelection("DEFGHI");

  // Test selecting last character in backwards direction.
  SelectTextWithMouse(RegularFormEnd(), RegularFormAtX(186.0));
  CheckSelection("L");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextEmptyAndBasicNormalComboBox) {
  // Test empty selection.
  CheckSelection("");
  CheckFocusedFieldText("");

  // Non-editable comboboxes don't allow selection with keyboard.
  SelectTextWithMouse(NonEditableFormBegin(), NonEditableFormAtX(142.0));
  CheckFocusedFieldText("Banana");
  CheckSelection("Banana");

  // Select other another provided option.
  SelectNonEditableFormOption(0);
  CheckFocusedFieldText("Apple");
  CheckSelection("Apple");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextEmptyAndBasicEditableComboBoxKeyboard) {
  // Test empty selection.
  CheckSelection("");
  CheckFocusedFieldText("");

  // Test basic selection of text within user editable combobox using keyboard.
  TypeTextIntoTextField(3, EditableFormBegin());
  CheckFocusedFieldText("ABC");
  SelectTextWithKeyboard(3, FWL_VKEY_Left, EditableFormAtX(128.0));
  CheckSelection("ABC");

  // Select a provided option.
  SelectEditableFormOption(1);
  CheckSelection("Bar");
  CheckFocusedFieldText("Bar");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextEmptyAndBasicEditableComboBoxMouse) {
  // Test empty selection.
  CheckSelection("");

  // Test basic selection of text within user editable combobox using mouse.
  TypeTextIntoTextField(3, EditableFormBegin());
  SelectTextWithMouse(EditableFormAtX(128.0), EditableFormBegin());
  CheckSelection("ABC");

  // Select a provided option.
  SelectEditableFormOption(2);
  CheckFocusedFieldText("Qux");
  CheckSelection("Qux");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextFragmentsNormalComboBox) {
  CheckFocusedFieldText("");

  // Test selecting first character in forward direction.
  SelectTextWithMouse(NonEditableFormBegin(), NonEditableFormAtX(107.0));
  CheckFocusedFieldText("Banana");
  CheckSelection("B");

  // Test selecting entire string in backwards direction.
  SelectTextWithMouse(NonEditableFormAtX(142.0), NonEditableFormBegin());
  CheckSelection("Banana");

  // Test selecting middle section in backwards direction.
  SelectTextWithMouse(NonEditableFormAtX(135.0), NonEditableFormAtX(117.0));
  CheckSelection("nan");

  // Test selecting middle section in forward direction.
  SelectTextWithMouse(NonEditableFormAtX(117.0), NonEditableFormAtX(135.0));
  CheckSelection("nan");

  // Test selecting last character in backwards direction.
  SelectTextWithMouse(NonEditableFormAtX(142.0), NonEditableFormAtX(138.0));
  CheckSelection("a");
  CheckFocusedFieldText("Banana");

  // Select another option and then reset selection as first three chars.
  SelectNonEditableFormOption(2);
  CheckFocusedFieldText("Cherry");
  CheckSelection("Cherry");
  SelectTextWithMouse(NonEditableFormBegin(), NonEditableFormAtX(122.0));
  CheckSelection("Che");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextFragmentsEditableComboBoxKeyboard) {
  CheckFocusedFieldText("");
  TypeTextIntoTextField(10, EditableFormBegin());
  CheckFocusedFieldText("ABCDEFGHIJ");

  // Test selecting first character in forward direction.
  SelectTextWithKeyboard(1, FWL_VKEY_Right, EditableFormBegin());
  CheckSelection("A");

  // Test selecting entire long string in backwards direction.
  SelectTextWithKeyboard(10, FWL_VKEY_Left, EditableFormEnd());
  CheckSelection("ABCDEFGHIJ");

  // Test selecting middle section in backwards direction.
  SelectTextWithKeyboard(5, FWL_VKEY_Left, EditableFormAtX(168.0));
  CheckSelection("DEFGH");

  // Test selecting middle selection in forward direction.
  SelectTextWithKeyboard(5, FWL_VKEY_Right, EditableFormAtX(127.0));
  CheckSelection("DEFGH");

  // Test selecting last character in backwards direction.
  SelectTextWithKeyboard(1, FWL_VKEY_Left, EditableFormEnd());
  CheckSelection("J");

  // Select a provided option and then reset selection as first two chars.
  SelectEditableFormOption(0);
  CheckSelection("Foo");
  SelectTextWithKeyboard(2, FWL_VKEY_Right, EditableFormBegin());
  CheckSelection("Fo");
  CheckFocusedFieldText("Foo");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       GetSelectedTextFragmentsEditableComboBoxMouse) {
  TypeTextIntoTextField(10, EditableFormBegin());

  // Test selecting first character in forward direction.
  SelectTextWithMouse(EditableFormBegin(), EditableFormAtX(107.0));
  CheckSelection("A");

  // Test selecting entire long string in backwards direction.
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEFGHIJ");

  // Test selecting middle section in backwards direction.
  SelectTextWithMouse(EditableFormAtX(168.0), EditableFormAtX(127.0));
  CheckSelection("DEFGH");

  // Test selecting middle selection in forward direction.
  SelectTextWithMouse(EditableFormAtX(127.0), EditableFormAtX(168.0));
  CheckSelection("DEFGH");

  // Test selecting last character in backwards direction.
  SelectTextWithMouse(EditableFormEnd(), EditableFormAtX(174.0));
  CheckSelection("J");
  CheckFocusedFieldText("ABCDEFGHIJ");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       SetSelectionProgrammaticallyNonEditableField) {
  // Focus on non-editable form field and check that the value is as expected.
  // This is the value that is present in the field upon opening, we have not
  // changed it by setting focus.
  FocusOnNonEditableForm();
  CheckFocusedFieldText("Banana");

  // Make selections to change the value of the focused annotation
  // programmatically.
  SetIndexSelectedShouldSucceed(0, true);
  CheckFocusedFieldText("Apple");

  // Selecting an index that is already selected is success.
  SetIndexSelectedShouldSucceed(0, true);
  CheckFocusedFieldText("Apple");

  SetIndexSelectedShouldSucceed(9, true);
  CheckFocusedFieldText("Jackfruit");

  // Cannot deselect a combobox field - value unchanged.
  SetIndexSelectedShouldFail(9, false);
  CheckFocusedFieldText("Jackfruit");

  // Cannot select indices that are out of range - value unchanged.
  SetIndexSelectedShouldFail(100, true);
  SetIndexSelectedShouldFail(-100, true);
  CheckFocusedFieldText("Jackfruit");

  // Check that above actions are interchangeable with click actions, should be
  // able to use a combination of both.
  SelectNonEditableFormOption(1);
  CheckFocusedFieldText("Banana");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       SetSelectionProgrammaticallyEditableField) {
  // Focus on editable form field and check that the value is as expected.
  // This is the value that is present in the field upon opening, we have not
  // changed it by setting focus.
  FocusOnEditableForm();
  CheckFocusedFieldText("");

  // Make selections to change value of the focused annotation
  // programmatically.
  SetIndexSelectedShouldSucceed(0, true);
  CheckFocusedFieldText("Foo");

  SetIndexSelectedShouldSucceed(1, true);
  CheckFocusedFieldText("Bar");

  // Selecting an index that is already selected is success.
  SetIndexSelectedShouldSucceed(1, true);
  CheckFocusedFieldText("Bar");

  // Cannot deselect a combobox field - value unchanged.
  SetIndexSelectedShouldFail(0, false);
  CheckFocusedFieldText("Bar");

  // Cannot select indices that are out of range - value unchanged.
  SetIndexSelectedShouldFail(100, true);
  SetIndexSelectedShouldFail(-100, true);
  CheckFocusedFieldText("Bar");

  // Check that above actions are interchangeable with click actions, should be
  // able to use a combination of both.
  SelectEditableFormOption(0);
  CheckFocusedFieldText("Foo");

  // Check that above actions are interchangeable with typing actions, should
  // be able to use a combination of both. Typing text into a text field after
  // selecting indices programmatically should be equivalent to doing so after
  // a user selects an index via click on the dropdown.
  SetIndexSelectedShouldSucceed(1, true);
  CheckFocusedFieldText("Bar");
  TypeTextIntoTextField(5, EditableFormBegin());
  CheckFocusedFieldText("ABCDEBar");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       CheckIfIndexSelectedNonEditableField) {
  // Non-editable field is set to 'Banana' (index 1) upon opening.
  ClickOnFormFieldAtPoint(NonEditableFormBegin());
  for (int i = 0; i < 26; i++) {
    bool expected = i == 1;
    CheckIsIndexSelected(i, expected);
  }

  SelectNonEditableFormOption(0);
  CheckIsIndexSelected(0, true);
  for (int i = 1; i < 26; i++) {
    CheckIsIndexSelected(i, false);
  }
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       CheckIfIndexSelectedEditableField) {
  // Editable field has nothing selected upon opening.
  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);

  SelectEditableFormOption(0);
  CheckIsIndexSelected(0, true);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DeleteTextFieldEntireSelection) {
  // Select entire contents of text field.
  TypeTextIntoTextField(12, RegularFormBegin());
  SelectAllRegularFormTextWithMouse();
  CheckFocusedFieldText("ABCDEFGHIJKL");
  CheckSelection("ABCDEFGHIJKL");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("");

  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DeleteTextFieldSelectionMiddle) {
  // Select middle section of text.
  TypeTextIntoTextField(12, RegularFormBegin());
  SelectTextWithMouse(RegularFormAtX(170.0), RegularFormAtX(125.0));
  CheckFocusedFieldText("ABCDEFGHIJKL");
  CheckSelection("DEFGHI");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("ABCJKL");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("ABCJKL");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DeleteTextFieldSelectionLeft) {
  // Select first few characters of text.
  TypeTextIntoTextField(12, RegularFormBegin());
  SelectTextWithMouse(RegularFormBegin(), RegularFormAtX(132.0));
  CheckSelection("ABCD");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("EFGHIJKL");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("EFGHIJKL");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DeleteTextFieldSelectionRight) {
  // Select last few characters of text.
  TypeTextIntoTextField(12, RegularFormBegin());
  SelectTextWithMouse(RegularFormEnd(), RegularFormAtX(165.0));
  CheckSelection("IJKL");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("ABCDEFGH");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("ABCDEFGH");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DeleteEmptyTextFieldSelection) {
  // Do not select text.
  TypeTextIntoTextField(12, RegularFormBegin());
  CheckSelection("");

  // Test that attempt to delete empty text selection has no effect.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("ABCDEFGHIJKL");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("ABCDEFGHIJKL");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       DeleteEditableComboBoxEntireSelection) {
  // Select entire contents of user-editable combobox text field.
  TypeTextIntoTextField(10, EditableFormBegin());
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEFGHIJ");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       DeleteEditableComboBoxSelectionMiddle) {
  // Select middle section of text.
  TypeTextIntoTextField(10, EditableFormBegin());
  SelectTextWithMouse(EditableFormAtX(168.0), EditableFormAtX(127.0));
  CheckSelection("DEFGH");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("ABCIJ");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCIJ");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       DeleteEditableComboBoxSelectionLeft) {
  // Select first few characters of text.
  TypeTextIntoTextField(10, EditableFormBegin());
  SelectTextWithMouse(EditableFormBegin(), EditableFormAtX(132.0));
  CheckSelection("ABCD");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  SelectAllEditableFormTextWithMouse();
  CheckSelection("EFGHIJ");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       DeleteEditableComboBoxSelectionRight) {
  // Select last few characters of text.
  TypeTextIntoTextField(10, EditableFormBegin());
  SelectTextWithMouse(EditableFormEnd(), EditableFormAtX(152.0));
  CheckSelection("GHIJ");

  // Test deleting current text selection. Select what remains after deletion to
  // check that remaining text is as expected.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEF");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       DeleteEmptyEditableComboBoxSelection) {
  // Do not select text.
  TypeTextIntoTextField(10, EditableFormBegin());
  CheckSelection("");

  // Test that attempt to delete empty text selection has no effect.
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEFGHIJ");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, InsertTextInEmptyTextField) {
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");

  // Test inserting text into empty text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Hello");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("Hello");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, InsertTextInPopulatedTextFieldLeft) {
  TypeTextIntoTextField(8, RegularFormBegin());
  CheckFocusedFieldText("ABCDEFGH");

  // Click on the leftmost part of the text field.
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("ABCDEFGH");

  // Test inserting text in front of existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("HelloABCDEFGH");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("HelloABCDEFGH");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, InsertTextInPopulatedTextFieldMiddle) {
  TypeTextIntoTextField(8, RegularFormBegin());

  // Click on the middle of the text field.
  ClickOnFormFieldAtPoint(RegularFormAtX(134.0));

  // Test inserting text in the middle of existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("ABCDHelloEFGH");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("ABCDHelloEFGH");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, InsertTextInPopulatedTextFieldRight) {
  TypeTextIntoTextField(8, RegularFormBegin());

  // Click on the rightmost part of the text field.
  ClickOnFormFieldAtPoint(RegularFormAtX(166.0));

  // Test inserting text behind existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("ABCDEFGHHello");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("ABCDEFGHHello");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedTextFieldWhole) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select entire string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("ABCDEFGHIJKL");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Hello");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("Hello");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedTextFieldLeft) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select left portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(6, FWL_VKEY_Left, RegularFormAtX(148.0));
  CheckSelection("ABCDEF");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("HelloGHIJKL");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("HelloGHIJKL");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedTextFieldMiddle) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select middle portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(6, FWL_VKEY_Left, RegularFormAtX(171.0));
  CheckSelection("DEFGHI");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("ABCHelloJKL");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedTextFieldRight) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select right portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(6, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("GHIJKL");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllRegularFormTextWithMouse();
  CheckSelection("ABCDEFHello");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextInEmptyEditableComboBox) {
  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckFocusedFieldText("");

  // Test inserting text into empty user-editable combobox.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Hello");

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("Hello");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextInPopulatedEditableComboBoxLeft) {
  TypeTextIntoTextField(6, EditableFormBegin());

  // Click on the leftmost part of the user-editable combobox.
  ClickOnFormFieldAtPoint(EditableFormBegin());

  // Test inserting text in front of existing text in user-editable combobox.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("HelloABCDEF");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextInPopulatedEditableComboBoxMiddle) {
  TypeTextIntoTextField(6, EditableFormBegin());

  // Click on the middle of the user-editable combobox.
  ClickOnFormFieldAtPoint(EditableFormAtX(126.0));

  // Test inserting text in the middle of existing text in user-editable
  // combobox.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCHelloDEF");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextInPopulatedEditableComboBoxRight) {
  TypeTextIntoTextField(6, EditableFormBegin());

  // Click on the rightmost part of the user-editable combobox.
  ClickOnFormFieldAtPoint(EditableFormEnd());

  // Test inserting text behind existing text in user-editable combobox.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEFHello");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedEditableComboBoxWhole) {
  TypeTextIntoTextField(10, EditableFormBegin());

  // Select entire string in user-editable combobox.
  CheckSelection("");
  SelectTextWithKeyboard(10, FWL_VKEY_Left, EditableFormEnd());
  CheckSelection("ABCDEFGHIJ");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("Hello");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedEditableComboBoxLeft) {
  TypeTextIntoTextField(10, EditableFormBegin());

  // Select left portion of string in user-editable combobox.
  CheckSelection("");
  SelectTextWithKeyboard(5, FWL_VKEY_Left, EditableFormAtX(142.0));
  CheckSelection("ABCDE");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("HelloFGHIJ");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedEditableComboBoxMiddle) {
  TypeTextIntoTextField(10, EditableFormBegin());

  // Select middle portion of string in user-editable combobox.
  CheckSelection("");
  SelectTextWithKeyboard(5, FWL_VKEY_Left, EditableFormAtX(167.0));
  CheckSelection("DEFGH");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCHelloIJ");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedEditableComboBoxRight) {
  TypeTextIntoTextField(10, EditableFormBegin());

  // Select right portion of string in user-editable combobox.
  CheckSelection("");
  SelectTextWithKeyboard(5, FWL_VKEY_Left, EditableFormEnd());
  CheckSelection("FGHIJ");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of user-editable combobox text field to check that
  // insertion worked as expected.
  CheckSelection("");
  SelectAllEditableFormTextWithMouse();
  CheckSelection("ABCDEHello");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       CheckIfEnterAndSpaceKeyAreHandled) {
  // Non-editable field is set to 'Banana' (index 1) upon opening.
  ClickOnFormFieldAtPoint(NonEditableFormBegin());
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);

  // Verify that the Return character is handled.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kReturn, 0));

  // Change the selection in the combo-box using the arrow down key.
  EXPECT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Down, 0));
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, true);

  // Tab to the next control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab, 0));

  // Shift-tab to the previous control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab,
                          FWL_EVENTFLAG_ShiftKey));

  // Verify that the selection is unchanged.
  CheckIsIndexSelected(2, true);

  // Verify that the Space character is handled.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kSpace, 0));

  // Change the selection in the combo-box using the arrow down key.
  EXPECT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Down, 0));
  CheckIsIndexSelected(3, true);

  // Tab to the next control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab, 0));

  // Shift-tab to the previous control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab,
                          FWL_EVENTFLAG_ShiftKey));

  // Verify that the selection is unchanged.
  CheckIsIndexSelected(3, true);
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest,
       CheckIfEnterAndSpaceKeyAreHandledOnEditableFormField) {
  // Non-editable field is set to 'Banana' (index 1) upon opening.
  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);

  // Verify that the Return character is handled.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kReturn, 0));

  // Change the selection in the combo-box using the arrow down key.
  EXPECT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Down, 0));
  CheckIsIndexSelected(0, true);
  CheckIsIndexSelected(1, false);

  // Tab to the next control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab, 0));

  // Shift-tab to the previous control.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kTab,
                          FWL_EVENTFLAG_ShiftKey));

  // Verify that the selection is unchanged.
  CheckIsIndexSelected(0, true);

  // Verify that the Space character is handled.
  EXPECT_TRUE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kSpace, 0));

  CheckFocusedFieldText(" ");
  CheckIsIndexSelected(0, false);
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextInEmptyCharLimitTextFieldOverflow) {
  // Click on the textfield.
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("Elephant");

  // Delete pre-filled contents of text field with char limit.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Elephant");
  FORM_ReplaceSelection(form_handle(), page(), nullptr);
  CheckFocusedFieldText("");

  // Test inserting text into now empty text field so text to be inserted
  // exceeds the char limit and is cut off.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Hippopotam");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Hippopotam");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextInEmptyCharLimitTextFieldFit) {
  // Click on the textfield.
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("Elephant");

  // Delete pre-filled contents of text field with char limit.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Elephant");
  FORM_ReplaceSelection(form_handle(), page(), nullptr);

  // Test inserting text into now empty text field so text to be inserted
  // exceeds the char limit and is cut off.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Zebra");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Zebra");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Zebra");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextInPopulatedCharLimitTextFieldLeft) {
  // Click on the leftmost part of the text field.
  ClickOnFormFieldAtPoint(CharLimitFormBegin());

  // Test inserting text in front of existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("HiElephant");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextInPopulatedCharLimitTextFieldMiddle) {
  CheckFocusedFieldText("");
  TypeTextIntoTextField(8, RegularFormBegin());
  CheckFocusedFieldText("ABCDEFGH");

  // Click on the middle of the text field.
  ClickOnFormFieldAtPoint(CharLimitFormAtX(134.0));
  CheckFocusedFieldText("Elephant");

  // Test inserting text in the middle of existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("ElephHiant");

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("ElephHiant");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextInPopulatedCharLimitTextFieldRight) {
  TypeTextIntoTextField(8, RegularFormBegin());

  // Click on the rightmost part of the text field.
  ClickOnFormFieldAtPoint(CharLimitFormAtX(166.0));

  // Test inserting text behind existing text in text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("ElephantHi");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedCharLimitTextFieldWhole) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select entire string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(12, FWL_VKEY_Left, CharLimitFormEnd());
  CheckSelection("Elephant");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Hippopotam");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedCharLimitTextFieldLeft) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select left portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(4, FWL_VKEY_Left, CharLimitFormAtX(122.0));
  CheckSelection("Elep");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("Hippophant");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedCharLimitTextFieldMiddle) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select middle portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(4, FWL_VKEY_Left, CharLimitFormAtX(136.0));
  CheckSelection("epha");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("ElHippopnt");
}

TEST_F(FPDFFormFillTextFormEmbedderTest,
       InsertTextAndReplaceSelectionInPopulatedCharLimitTextFieldRight) {
  TypeTextIntoTextField(12, RegularFormBegin());

  // Select right portion of string in text field.
  CheckSelection("");
  SelectTextWithKeyboard(4, FWL_VKEY_Left, CharLimitFormAtX(152.0));
  CheckSelection("hant");

  // Test replacing text selection with text to be inserted.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hippopotamus");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());

  // Select entire contents of text field to check that insertion worked
  // as expected.
  CheckSelection("");
  SelectAllCharLimitFormTextWithMouse();
  CheckSelection("ElepHippop");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, DoubleClickInTextField) {
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");

  // Test inserting text into empty text field.
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"Hello World");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("Hello World");

  // Make sure double clicking selects the entire line.
  CheckSelection("");
  DoubleClickOnFormFieldAtPoint(RegularFormBegin());
  CheckSelection("Hello World");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, FocusAnnotationUpdateToEmbedder) {
  testing::NiceMock<EmbedderTestMockDelegate> mock;
  SetDelegate(&mock);
  CheckFocusedFieldText("");

#ifdef PDF_ENABLE_XFA
  EXPECT_CALL(mock, OnFocusChange(_, _, 0)).Times(1);
#else   // PDF_ENABLE_XFA
  EXPECT_CALL(mock, OnFocusChange(_, _, 0)).Times(0);
#endif  // PDF_ENABLE_XFA

  ClickOnFormFieldAtPoint(RegularFormBegin());
}

TEST_F(FPDFFormFillTextFormEmbedderTestVersion2,
       FocusAnnotationUpdateToEmbedder) {
  testing::NiceMock<EmbedderTestMockDelegate> mock;
  SetDelegate(&mock);
  CheckFocusedFieldText("");

  EXPECT_CALL(mock, OnFocusChange(_, _, 0)).Times(1);
  ClickOnFormFieldAtPoint(RegularFormBegin());
}

TEST_F(FPDFFormFillTextFormEmbedderTest, FocusChanges) {
  static const CFX_PointF kNonFormPoint(1, 1);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("Elephant");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");
  TypeTextIntoTextField(3, CharLimitFormBegin());
  CheckFocusedFieldText("ABElephant");
  TypeTextIntoTextField(5, RegularFormBegin());
  CheckFocusedFieldText("ABCDE");
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("ABElephant");
  ClickOnFormFieldAtPoint(kNonFormPoint);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(kNonFormPoint);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(CharLimitFormBegin());
  CheckFocusedFieldText("ABElephant");
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("ABElephant");
  ClickOnFormFieldAtPoint(RegularFormEnd());
  CheckFocusedFieldText("ABCDE");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("ABCDE");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("ABCDE");
  ClickOnFormFieldAtPoint(CharLimitFormBegin());
  CheckFocusedFieldText("ABElephant");
  FORM_ForceToKillFocus(form_handle());
  CheckFocusedFieldText("");
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest, FocusChanges) {
  static const CFX_PointF kNonFormPoint(1, 1);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(NonEditableFormBegin());
  CheckFocusedFieldText("Banana");
  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(NonEditableFormEnd());
  CheckFocusedFieldText("Banana");
  ClickOnFormFieldAtPoint(NonEditableFormBegin());
  CheckFocusedFieldText("Banana");
  FORM_ForceToKillFocus(form_handle());
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckFocusedFieldText("");
  TypeTextIntoTextField(3, EditableFormBegin());
  CheckFocusedFieldText("ABC");
  ClickOnFormFieldAtPoint(kNonFormPoint);
  CheckFocusedFieldText("");
  TypeTextIntoTextField(3, EditableFormEnd());
  CheckFocusedFieldText("ABCABC");
  ClickOnFormFieldAtPoint(kNonFormPoint);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(EditableFormDropDown());
  CheckFocusedFieldText("ABCABC");
  FORM_ForceToKillFocus(form_handle());
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(NonEditableFormDropDown());
  CheckFocusedFieldText("Banana");
  ClickOnFormFieldAtPoint(kNonFormPoint);
  CheckFocusedFieldText("");
  ClickOnFormFieldAtPoint(NonEditableFormEnd());
  CheckFocusedFieldText("Banana");

  // Typing into non-editable field results in selecting a different option.
  TypeTextIntoTextField(1, NonEditableFormEnd());
  CheckFocusedFieldText("Apple");
  TypeTextIntoTextField(3, NonEditableFormEnd());
  CheckFocusedFieldText("Cherry");
  TypeTextIntoTextField(2, NonEditableFormEnd());
  CheckFocusedFieldText("Banana");

  SelectEditableFormOption(0);
  CheckFocusedFieldText("Foo");
  SelectEditableFormOption(1);
  CheckFocusedFieldText("Bar");
  SelectEditableFormOption(2);
  CheckFocusedFieldText("Qux");
  SelectNonEditableFormOption(1);
  CheckFocusedFieldText("Banana");
  SelectNonEditableFormOption(0);
  CheckFocusedFieldText("Apple");
  SelectNonEditableFormOption(2);
  CheckFocusedFieldText("Cherry");

  // Typing into an editable field changes the text in the option.
  SelectEditableFormOption(0);
  CheckFocusedFieldText("Foo");
  TypeTextIntoTextField(5, EditableFormBegin());
  CheckFocusedFieldText("ABCDEFoo");
  SelectEditableFormOption(2);
  CheckFocusedFieldText("Qux");
  TypeTextIntoTextField(2, EditableFormEnd());
  CheckFocusedFieldText("QuxAB");

  // But a previously edited option is reset when selected again.
  SelectEditableFormOption(0);
  CheckFocusedFieldText("Foo");
  TypeTextIntoTextField(1, EditableFormBegin());
  CheckFocusedFieldText("AFoo");
  SelectEditableFormOption(0);
  CheckFocusedFieldText("Foo");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, UndoRedo) {
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(false);

  TypeTextIntoTextField(5, RegularFormBegin());
  CheckFocusedFieldText("ABCDE");
  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("ABCD");
  CheckCanUndo(true);
  CheckCanRedo(true);
  PerformUndo();
  CheckFocusedFieldText("ABC");
  CheckCanUndo(true);
  CheckCanRedo(true);

  PerformRedo();
  CheckFocusedFieldText("ABCD");
  CheckCanUndo(true);
  CheckCanRedo(true);
  PerformRedo();
  CheckFocusedFieldText("ABCDE");
  CheckCanUndo(true);
  CheckCanRedo(false);
}

// This action only applies to Listboxes and Comboboxes so should fail
// gracefully for Textboxes by returning false to all operations.
TEST_F(FPDFFormFillTextFormEmbedderTest, SetIndexSelectedShouldFailGracefully) {
  // set focus and read text to confirm we have it
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("Elephant");

  SetIndexSelectedShouldFail(0, true);
  SetIndexSelectedShouldFail(0, false);
  SetIndexSelectedShouldFail(1, true);
  SetIndexSelectedShouldFail(1, false);
  SetIndexSelectedShouldFail(-1, true);
  SetIndexSelectedShouldFail(-1, false);
}

// This action only applies to Listboxes and Comboboxes so should fail
// gracefully for Textboxes by returning false to all operations.
TEST_F(FPDFFormFillTextFormEmbedderTest, IsIndexSelectedShouldFailGracefully) {
  // set focus and read text to confirm we have it
  ClickOnFormFieldAtPoint(CharLimitFormEnd());
  CheckFocusedFieldText("Elephant");

  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(-1, false);
}

TEST_F(FPDFFormFillComboBoxFormEmbedderTest, UndoRedo) {
  ClickOnFormFieldAtPoint(NonEditableFormBegin());
  CheckFocusedFieldText("Banana");
  CheckCanUndo(false);
  CheckCanRedo(false);

  ClickOnFormFieldAtPoint(EditableFormBegin());
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(false);

  TypeTextIntoTextField(3, EditableFormBegin());
  CheckFocusedFieldText("ABC");
  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("AB");
  CheckCanUndo(true);
  CheckCanRedo(true);
  PerformUndo();
  CheckFocusedFieldText("A");
  CheckCanUndo(true);
  CheckCanRedo(true);
  PerformUndo();
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(true);

  PerformRedo();
  CheckFocusedFieldText("A");
  CheckCanUndo(true);
  CheckCanRedo(true);
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest,
       CheckIfIndexSelectedSingleSelectField) {
  // Nothing is selected in single select field upon opening.
  FocusOnSingleSelectForm();
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);

  ClickOnSingleSelectFormOption(1);
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest,
       CheckIfIndexSelectedMultiSelectField) {
  // Multiselect field set to 'Banana' (index 1) upon opening.
  FocusOnMultiSelectForm();
  for (int i = 0; i < 26; i++) {
    bool expected = i == 1;
    CheckIsIndexSelected(i, expected);
  }

  // TODO(bug_1377): Behavior should be changed to the one described below.
  // Multiselect field set to 'Cherry' (index 2), which is index 1 among the
  // visible form options because the listbox is scrolled down to have 'Banana'
  // (index 1) at the top.
  ClickOnMultiSelectFormOption(1);
  for (int i = 0; i < 26; i++) {
    bool expected = i == 1;
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest,
       SetSelectionProgrammaticallySingleSelectField) {
  // Nothing is selected in single select field upon opening.
  FocusOnSingleSelectForm();
  CheckFocusedFieldText("");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);

  // Make selections to change the value of the focused annotation
  // programmatically showing that only one value remains selected at a time.
  SetIndexSelectedShouldSucceed(0, true);
  CheckFocusedFieldText("Foo");
  CheckIsIndexSelected(0, true);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);

  SetIndexSelectedShouldSucceed(1, true);
  CheckFocusedFieldText("Bar");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);

  // Selecting/deselecting an index that is already selected/deselected is
  // success.
  SetIndexSelectedShouldSucceed(1, true);
  CheckFocusedFieldText("Bar");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);

  SetIndexSelectedShouldSucceed(2, false);
  CheckFocusedFieldText("Bar");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);

  // Cannot select indices that are out of range.
  SetIndexSelectedShouldFail(100, true);
  SetIndexSelectedShouldFail(-100, true);
  SetIndexSelectedShouldFail(100, false);
  SetIndexSelectedShouldFail(-100, false);
  // Confirm that previous values were not changed.
  CheckFocusedFieldText("Bar");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);

  // Unlike combobox, should be able to deselect all indices.
  SetIndexSelectedShouldSucceed(1, false);
  CheckFocusedFieldText("");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, false);
  CheckIsIndexSelected(2, false);

  // Check that above actions are interchangeable with click actions, should be
  // able to use a combination of both.
  ClickOnSingleSelectFormOption(1);
  CheckFocusedFieldText("Bar");
  CheckIsIndexSelected(0, false);
  CheckIsIndexSelected(1, true);
  CheckIsIndexSelected(2, false);
}

// Re: Focus Field Text - For multiselect listboxes a caret is set on the last
// item to be selected/deselected. The text of that item should be returned.
TEST_F(FPDFFormFillListBoxFormEmbedderTest,
       SetSelectionProgrammaticallyMultiSelectField) {
  // Multiselect field set to 'Banana' (index 1) upon opening.
  FocusOnMultiSelectForm();
  for (int i = 0; i < 26; i++) {
    bool expected = i == 1;
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Banana");

  // Select some more options.
  SetIndexSelectedShouldSucceed(5, true);
  SetIndexSelectedShouldSucceed(6, true);
  SetIndexSelectedShouldSucceed(20, true);
  for (int i = 0; i < 26; i++) {
    bool expected = (i == 1 || i == 5 || i == 6 || i == 20);
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Ugli Fruit");

  // Selecting indices that are already selected is success - changes nothing.
  SetIndexSelectedShouldSucceed(5, true);
  SetIndexSelectedShouldSucceed(6, true);
  SetIndexSelectedShouldSucceed(20, true);
  for (int i = 0; i < 26; i++) {
    bool expected = (i == 1 || i == 5 || i == 6 || i == 20);
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Ugli Fruit");

  // Deselect some options.
  SetIndexSelectedShouldSucceed(20, false);
  SetIndexSelectedShouldSucceed(1, false);
  for (int i = 0; i < 26; i++) {
    bool expected = (i == 5 || i == 6);
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Banana");

  // Deselecting indices that already aren't selected is success - does not
  // change the selected values but moves the focus text caret to last item we
  // executed on.
  SetIndexSelectedShouldSucceed(1, false);
  SetIndexSelectedShouldSucceed(3, false);
  for (int i = 0; i < 26; i++) {
    bool expected = (i == 5 || i == 6);
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Date");

  // Cannot select indices that are out of range.
  SetIndexSelectedShouldFail(100, true);
  SetIndexSelectedShouldFail(-100, true);
  SetIndexSelectedShouldFail(100, false);
  SetIndexSelectedShouldFail(-100, false);
  // Confirm that previous values were not changed.
  CheckFocusedFieldText("Date");
  for (int i = 0; i < 26; i++) {
    bool expected = (i == 5 || i == 6);
    CheckIsIndexSelected(i, expected);
  }

  // Check that above actions are interchangeable with click actions, should be
  // able to use a combination of both.
  // TODO(bug_1377): Change to click on form option 0 instead of form option 1
  ClickOnMultiSelectFormOption(1);
  for (int i = 0; i < 26; i++) {
    bool expected = i == 1;
    CheckIsIndexSelected(i, expected);
  }
  CheckFocusedFieldText("Banana");
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest, CheckIfMultipleSelectedIndices) {
  // Multiselect field set to 'Belgium' (index 1) and 'Denmark' (index 3) upon
  // opening.
  FocusOnMultiSelectMultipleIndicesForm();
  for (int i = 0; i < 5; i++) {
    bool expected = (i == 1 || i == 3);
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest, CheckIfMultipleSelectedValues) {
  // Multiselect field set to 'Gamma' (index 2) and 'Epsilon' (index 4) upon
  // opening.
  FocusOnMultiSelectMultipleValuesForm();
  for (int i = 0; i < 5; i++) {
    bool expected = (i == 2 || i == 4);
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest, CheckIfMultipleSelectedMismatch) {
  // Multiselect field set to 'Alligator' (index 0) and 'Cougar' (index 2) upon
  // opening.
  FocusOnMultiSelectMultipleMismatchForm();
  for (int i = 0; i < 5; i++) {
    bool expected = (i == 0 || i == 2);
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest,
       CheckIfVerticalScrollIsAtFirstSelected) {
  // Multiselect field set to 'Gamma' (index 2) and 'Epsilon' (index 4) upon
  // opening.

  // TODO(bug_1377): Behavior should be changed to the one described below.
  // The top visible option is 'Gamma' (index 2), so the first selection should
  // not change. The second selection, 'Epsilon,' should be deselected.
  ClickOnMultiSelectMultipleValuesFormOption(0);
  for (int i = 0; i < 5; i++) {
    bool expected = i == 0;
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillListBoxFormEmbedderTest, CheckForNoOverscroll) {
  // Only the last option in the list, 'Saskatchewan', is selected.
  FocusOnSingleSelectLastSelectedForm();
  for (int i = 0; i < 10; i++) {
    bool expected = i == 9;
    CheckIsIndexSelected(i, expected);
  }

  // Even though the top index is specified to be at 'Saskatchewan' (index 9),
  // the top visible option will be the one above it, 'Quebec' (index 8), to
  // prevent overscrolling. Therefore, clicking on the first visible option of
  // the list should select 'Quebec' instead of 'Saskatchewan.'
  ClickOnSingleSelectLastSelectedFormOption(0);
  for (int i = 0; i < 10; i++) {
    bool expected = i == 8;
    CheckIsIndexSelected(i, expected);
  }
}

TEST_F(FPDFFormFillTextFormEmbedderTest, ReplaceAndKeepSelection) {
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"XYZ");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckCanUndo(false);
  CheckCanRedo(false);

  TypeTextIntoTextField(2, RegularFormBegin());
  CheckFocusedFieldText("AB");
  CheckSelection("");
  SelectTextWithKeyboard(1, FWL_VKEY_Right, RegularFormBegin());
  CheckSelection("A");

  FORM_ReplaceAndKeepSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("XYZB");
  CheckSelection("XYZ");
  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("AB");
  CheckCanUndo(true);
  CheckCanRedo(true);

  SelectTextWithKeyboard(1, FWL_VKEY_Left, RegularFormEnd());
  CheckSelection("B");

  FORM_ReplaceAndKeepSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("AXYZ");
  CheckSelection("XYZ");
  CheckCanUndo(true);
  CheckCanRedo(false);
}

TEST_F(FPDFFormFillTextFormEmbedderTest, ContinuouslyReplaceAndKeepSelection) {
  ScopedFPDFWideString text_to_insert1 = GetFPDFWideString(L"UVW");

  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(false);

  FORM_ReplaceAndKeepSelection(form_handle(), page(), text_to_insert1.get());
  CheckFocusedFieldText("UVW");
  CheckSelection("UVW");

  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("");

  CheckCanUndo(false);
  CheckCanRedo(true);
  PerformRedo();
  CheckFocusedFieldText("UVW");
  CheckSelection("");

  ScopedFPDFWideString text_to_insert2 = GetFPDFWideString(L"XYZ");
  FORM_ReplaceAndKeepSelection(form_handle(), page(), text_to_insert2.get());
  CheckFocusedFieldText("UVWXYZ");
  CheckSelection("XYZ");

  CheckCanUndo(true);
  PerformUndo();
  CheckFocusedFieldText("UVW");
  CheckSelection("");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, ReplaceSelection) {
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"XYZ");
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckCanUndo(false);
  CheckCanRedo(false);

  TypeTextIntoTextField(2, RegularFormBegin());
  CheckFocusedFieldText("AB");
  CheckSelection("");
  SelectTextWithKeyboard(1, FWL_VKEY_Right, RegularFormBegin());
  CheckSelection("A");

  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
  CheckFocusedFieldText("XYZB");
  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("AB");
  CheckCanUndo(true);
  CheckCanRedo(true);

  PerformUndo();
  CheckFocusedFieldText("A");
  CheckCanUndo(true);
  CheckCanRedo(true);

  PerformUndo();
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(true);

  PerformRedo();
  CheckFocusedFieldText("A");
  CheckCanUndo(true);
  CheckCanRedo(true);

  PerformRedo();
  CheckFocusedFieldText("AB");
  CheckCanUndo(true);
  CheckCanRedo(true);

  PerformRedo();
  CheckFocusedFieldText("XYZB");
  CheckCanUndo(true);
  CheckCanRedo(false);
}

TEST_F(FPDFFormFillTextFormEmbedderTest, ContinuouslyReplaceSelection) {
  ScopedFPDFWideString text_to_insert1 = GetFPDFWideString(L"UVW");

  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckFocusedFieldText("");
  CheckCanUndo(false);
  CheckCanRedo(false);

  FORM_ReplaceSelection(form_handle(), page(), text_to_insert1.get());
  CheckFocusedFieldText("UVW");
  CheckSelection("");

  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("");

  CheckCanUndo(false);
  CheckCanRedo(true);
  PerformRedo();
  CheckFocusedFieldText("UVW");
  CheckSelection("");

  ScopedFPDFWideString text_to_insert2 = GetFPDFWideString(L"XYZ");
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert2.get());
  CheckFocusedFieldText("UVWXYZ");

  CheckCanUndo(true);
  CheckCanRedo(false);

  PerformUndo();
  CheckFocusedFieldText("UVW");
  CheckSelection("");
}

TEST_F(FPDFFormFillTextFormEmbedderTest, SelectAllWithKeyboardShortcut) {
  // Start with a couple of letters in the text form.
  TypeTextIntoTextField(2, RegularFormBegin());
  CheckFocusedFieldText("AB");
  CheckSelection("");

  // Select all with the keyboard shortcut.
#if BUILDFLAG(IS_APPLE)
  constexpr int kCorrectModifier = FWL_EVENTFLAG_MetaKey;
#else
  constexpr int kCorrectModifier = FWL_EVENTFLAG_ControlKey;
#endif
  FORM_OnChar(form_handle(), page(), pdfium::ascii::kControlA,
              kCorrectModifier);
  CheckSelection("AB");

  // Reset the selection again.
  ClickOnFormFieldAtPoint(RegularFormBegin());
  CheckSelection("");

  // Select all with the keyboard shortcut using the wrong modifier key.
#if BUILDFLAG(IS_APPLE)
  constexpr int kWrongModifier = FWL_EVENTFLAG_ControlKey;
#else
  constexpr int kWrongModifier = FWL_EVENTFLAG_MetaKey;
#endif
  FORM_OnChar(form_handle(), page(), pdfium::ascii::kControlA, kWrongModifier);
  CheckSelection("");
}

class FPDFXFAFormBug1055869EmbedderTest
    : public FPDFFormFillInteractiveEmbedderTest {
 protected:
  FPDFXFAFormBug1055869EmbedderTest() = default;
  ~FPDFXFAFormBug1055869EmbedderTest() override = default;

  const char* GetDocumentName() const override { return "bug_1055869.pdf"; }
  int GetFormType() const override { return FORMTYPE_XFA_FULL; }
};

TEST_F(FPDFXFAFormBug1055869EmbedderTest, Paste) {
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"XYZ");
  DoubleClickOnFormFieldAtPoint(CFX_PointF(100, 100));
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
}

class FPDFXFAFormBug1058653EmbedderTest
    : public FPDFFormFillInteractiveEmbedderTest {
 protected:
  FPDFXFAFormBug1058653EmbedderTest() = default;
  ~FPDFXFAFormBug1058653EmbedderTest() override = default;

  const char* GetDocumentName() const override { return "bug_1058653.pdf"; }
  int GetFormType() const override { return FORMTYPE_XFA_FULL; }
};

TEST_F(FPDFXFAFormBug1058653EmbedderTest, Paste) {
  ScopedFPDFWideString text_to_insert = GetFPDFWideString(L"");
  DoubleClickOnFormFieldAtPoint(CFX_PointF(22, 22));
  FORM_ReplaceSelection(form_handle(), page(), text_to_insert.get());
}

class FPDFFormFillActionUriTest : public EmbedderTest {
 protected:
  FPDFFormFillActionUriTest() = default;
  ~FPDFFormFillActionUriTest() override = default;

  void SetUp() override {
    EmbedderTest::SetUp();
    ASSERT_TRUE(OpenDocument("annots_action_handling.pdf"));
    page_ = LoadPage(0);
    ASSERT_TRUE(page_);

    // Set Widget and Link as supported tabbable annots.
    constexpr FPDF_ANNOTATION_SUBTYPE kFocusableSubtypes[] = {FPDF_ANNOT_WIDGET,
                                                              FPDF_ANNOT_LINK};
    constexpr size_t kSubtypeCount = std::size(kFocusableSubtypes);
    ASSERT_TRUE(FPDFAnnot_SetFocusableSubtypes(
        form_handle(), kFocusableSubtypes, kSubtypeCount));
  }

  void TearDown() override {
    UnloadPage(page_);
    EmbedderTest::TearDown();
  }

  void SetFocusOnNthAnnot(size_t n) {
    DCHECK_NE(n, 0u);
    // Setting focus on first annot.
    FORM_OnMouseMove(form_handle(), page(), /*modifier=*/0, 100, 680);
    FORM_OnLButtonDown(form_handle(), page(), /*modifier=*/0, 100, 680);
    FORM_OnLButtonUp(form_handle(), page(), /*modifier=*/0, 100, 680);
    for (size_t i = 1; i < n; i++)
      ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Tab, 0));
  }

  FPDF_PAGE page() { return page_; }

 private:
  FPDF_PAGE page_ = nullptr;
};

TEST_F(FPDFFormFillActionUriTest, ButtonActionInvokeTest) {
  NiceMock<EmbedderTestMockDelegate> mock;
  // TODO(crbug.com/1028991): DoURIAction expect call should be 1.
  EXPECT_CALL(mock, DoURIAction(_)).Times(0);
  SetDelegate(&mock);

  SetFocusOnNthAnnot(1);

  // Tab once from first form to go to button widget.
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Tab, 0));

  // TODO(crbug.com/1028991): Following should be changed to ASSERT_TRUE after
  // handling key press implementation on buttons.
  ASSERT_FALSE(FORM_OnChar(form_handle(), page(), pdfium::ascii::kReturn, 0));
}

TEST_F(FPDFFormFillActionUriTest, LinkActionInvokeTest) {
  NiceMock<EmbedderTestMockDelegate> mock;
  {
    InSequence sequence;
    const char kExpectedUri[] = "https://cs.chromium.org/";
#ifdef PDF_ENABLE_XFA
    EXPECT_CALL(mock,
                DoURIActionWithKeyboardModifier(_, StrEq(kExpectedUri), _))
        .Times(4);
#else   // PDF_ENABLE_XFA
    EXPECT_CALL(mock, DoURIAction(StrEq(kExpectedUri))).Times(4);
    EXPECT_CALL(mock, DoURIActionWithKeyboardModifier(_, _, _)).Times(0);
#endif  // PDF_ENABLE_XFA
  }
  SetDelegate(&mock);
  SetFocusOnNthAnnot(3);
  int modifier = 0;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ShiftKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier |= FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));

  ASSERT_FALSE(FORM_OnKeyDown(nullptr, page(), FWL_VKEY_Return, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), nullptr, FWL_VKEY_Return, modifier));
  // Following checks should be changed to ASSERT_TRUE if FORM_OnKeyDown starts
  // handling for Shift/Space/Control.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Shift, modifier));
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Space, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Control, modifier));
}

TEST_F(FPDFFormFillActionUriTest, InternalLinkActionInvokeTest) {
  NiceMock<EmbedderTestMockDelegate> mock;
  EXPECT_CALL(mock, DoGoToAction(_, _, 1, _, _)).Times(12);
  SetDelegate(&mock);

  SetFocusOnNthAnnot(4);
  int modifier = 0;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ShiftKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier |= FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));

  SetFocusOnNthAnnot(5);
  modifier = 0;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ShiftKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier |= FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));

  SetFocusOnNthAnnot(6);
  modifier = 0;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ShiftKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier |= FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));

  ASSERT_FALSE(FORM_OnKeyDown(nullptr, page(), FWL_VKEY_Return, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), nullptr, FWL_VKEY_Return, modifier));
  // Following checks should be changed to ASSERT_TRUE if FORM_OnKeyDown starts
  // handling for Shift/Space/Control.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Shift, modifier));
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Space, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Control, modifier));
}

class FPDFFormFillActionUriTestVersion2 : public FPDFFormFillActionUriTest {
  void SetUp() override {
    SetFormFillInfoVersion(2);
    FPDFFormFillActionUriTest::SetUp();
  }
};

TEST_F(FPDFFormFillActionUriTestVersion2, LinkActionInvokeTest) {
  NiceMock<EmbedderTestMockDelegate> mock;
  {
    InSequence sequence;
    EXPECT_CALL(mock, DoURIAction(_)).Times(0);
    const char kExpectedUri[] = "https://cs.chromium.org/";
    EXPECT_CALL(mock,
                DoURIActionWithKeyboardModifier(_, StrEq(kExpectedUri), 0));
    EXPECT_CALL(mock, DoURIActionWithKeyboardModifier(
                          _, StrEq(kExpectedUri), FWL_EVENTFLAG_ControlKey));
    EXPECT_CALL(mock, DoURIActionWithKeyboardModifier(_, StrEq(kExpectedUri),
                                                      FWL_EVENTFLAG_ShiftKey));
    EXPECT_CALL(mock,
                DoURIActionWithKeyboardModifier(_, StrEq(kExpectedUri), 3));
  }
  SetDelegate(&mock);
  SetFocusOnNthAnnot(3);
  int modifier = 0;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier = FWL_EVENTFLAG_ShiftKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));
  modifier |= FWL_EVENTFLAG_ControlKey;
  ASSERT_TRUE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Return, modifier));

  ASSERT_FALSE(FORM_OnKeyDown(nullptr, page(), FWL_VKEY_Return, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), nullptr, FWL_VKEY_Return, modifier));
  // Following checks should be changed to ASSERT_TRUE if FORM_OnKeyDown starts
  // handling for Shift/Space/Control.
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Shift, modifier));
  ASSERT_FALSE(FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Space, modifier));
  ASSERT_FALSE(
      FORM_OnKeyDown(form_handle(), page(), FWL_VKEY_Control, modifier));
}
