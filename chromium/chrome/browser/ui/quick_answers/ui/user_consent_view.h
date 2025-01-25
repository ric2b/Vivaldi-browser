// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {

// |intent_type| and |intent_text| are used to generate the consent title
// including predicted intent information. Fallback to title without intent
// information if any of these two strings are empty.
class UserConsentView : public chromeos::ReadWriteCardsView {
  METADATA_HEADER(UserConsentView, chromeos::ReadWriteCardsView)

 public:
  static constexpr char kWidgetName[] = "UserConsentViewWidget";
  UserConsentView(const std::u16string& intent_type,
                  const std::u16string& intent_text,
                  base::WeakPtr<QuickAnswersUiController> controller);

  // Disallow copy and assign.
  UserConsentView(const UserConsentView&) = delete;
  UserConsentView& operator=(const UserConsentView&) = delete;

  ~UserConsentView() override;

  // chromeos::ReadWriteCardsView:
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void UpdateBoundsForQuickAnswers() override;

  views::LabelButton* allow_button_for_test() { return allow_button_; }
  views::LabelButton* no_thanks_button_for_test() { return no_thanks_button_; }

 private:
  void InitLayout();
  void InitContent();
  void InitButtonBar();

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Cached title text.
  std::u16string title_text_;

  base::WeakPtr<QuickAnswersUiController> controller_;
  chromeos::editor_menu::FocusSearch focus_search_;

  // Owned by view hierarchy.
  raw_ptr<views::View> main_view_ = nullptr;
  raw_ptr<views::FlexLayoutView> content_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::LabelButton> no_thanks_button_ = nullptr;
  raw_ptr<views::LabelButton> allow_button_ = nullptr;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
