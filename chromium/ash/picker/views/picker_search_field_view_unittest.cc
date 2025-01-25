// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <memory>
#include <string>

#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

class PickerSearchFieldViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerSearchFieldViewTest, HasTextFieldRole) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.textfield_for_testing().GetAccessibleRole(),
            ax::mojom::Role::kTextField);
}

TEST_F(PickerSearchFieldViewTest, ClearButtonHasTooltip) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.clear_button_for_testing().GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_SEARCH_FIELD_CLEAR_BUTTON_TOOLTIP_TEXT));
}

TEST_F(PickerSearchFieldViewTest, BackButtonHasTooltip) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.back_button_for_testing().GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_SEARCH_FIELD_BACK_BUTTON_TOOLTIP_TEXT));
}

TEST_F(PickerSearchFieldViewTest, DoesNotTriggerSearchOnConstruction) {
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(future.GetRepeatingCallback(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(PickerSearchFieldViewTest, TriggersSearchOnContentsChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      future.GetRepeatingCallback(), base::DoNothing(), &key_event_handler,
      &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);

  EXPECT_EQ(future.Get(), u"a");
}

TEST_F(PickerSearchFieldViewTest, SetPlaceholderText) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetPlaceholderText(u"hello");

  EXPECT_EQ(view.textfield_for_testing().GetPlaceholderText(), u"hello");
}

TEST_F(PickerSearchFieldViewTest, SetQueryText) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetQueryText(u"test");

  EXPECT_EQ(view.textfield_for_testing().GetText(), u"test");
}

TEST_F(PickerSearchFieldViewTest, SetQueryTextDoesNotTriggerSearch) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  base::test::TestFuture<const std::u16string&> future;
  PickerSearchFieldView view(future.GetRepeatingCallback(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetQueryText(u"test");

  EXPECT_FALSE(future.IsReady());
}

TEST_F(PickerSearchFieldViewTest, DoesNotShowClearButtonInitially) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(view.clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, DoesNotShowBackButtonInitially) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(view.back_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, ShowsClearButtonWithQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);

  EXPECT_TRUE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, HidesClearButtonWithEmptyQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_BACK);

  EXPECT_TRUE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest,
       ClickingClearButtonResetsQueryAndHidesButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      future.GetRepeatingCallback(), base::DoNothing(), &key_event_handler,
      &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);
  ASSERT_EQ(future.Take(), u"a");

  ViewDrawnWaiter().Wait(&view->clear_button_for_testing());
  LeftClickOn(view->clear_button_for_testing());

  EXPECT_EQ(future.Get(), u"");
  EXPECT_EQ(view->textfield_for_testing().GetText(), u"");
  EXPECT_FALSE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, ClickingBackButtonTriggersCallback) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  base::test::TestFuture<void> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), future.GetRepeatingCallback(), &key_event_handler,
      &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->SetBackButtonVisible(true);

  ViewDrawnWaiter().Wait(&view->back_button_for_testing());
  LeftClickOn(view->back_button_for_testing());

  EXPECT_TRUE(future.Wait());
}

}  // namespace
}  // namespace ash
