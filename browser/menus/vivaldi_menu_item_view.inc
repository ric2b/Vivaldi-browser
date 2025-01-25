// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

void views::MenuItemView::UpdateCheckBoxVisibility() {
  if (radio_check_image_view_) {
    bool show_check_radio_icon =
        type_ == Type::kRadio || (type_ == Type::kCheckbox && GetDelegate() &&
                                  GetDelegate()->IsItemChecked(GetCommand()));
    radio_check_image_view_->SetVisible(show_check_radio_icon);
    radio_check_image_view_->SetCanProcessEventsWithinSubtree(false);
    UpdateAccessibleCheckedState();
    // Update selection to ensure proper layering of various elements. The
    // GetColorProvider() is used in this code and will be nil unless the item
    // has been visible at least once so we test.
    if (GetColorProvider()) {
      UpdateSelectionBasedState(IsSelected());
    }
    SchedulePaint();
  }
}