// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class Browser;

namespace tab_groups {
enum class TabGroupColorId;
class TabGroupId;
}  // namespace tab_groups

namespace views {
class ToggleButton;
}  // namespace views

class ColorPickerView;
class TabGroupHeader;

// A dialog for changing a tab group's visual parameters.
class TabGroupEditorBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(TabGroupEditorBubbleView, views::BubbleDialogDelegateView)

 public:
  static constexpr int TAB_GROUP_HEADER_CXMENU_SAVE_GROUP = 13;
  static constexpr int TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP = 14;
  static constexpr int TAB_GROUP_HEADER_CXMENU_UNGROUP = 15;
  static constexpr int TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP = 16;
  static constexpr int TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW = 17;

  using Colors =
      std::vector<std::pair<tab_groups::TabGroupColorId, std::u16string>>;

  // Shows the editor for |group|. Returns a *non-owning* pointer to the
  // bubble's widget.
  static views::Widget* Show(
      const Browser* browser,
      const tab_groups::TabGroupId& group,
      TabGroupHeader* header_view,
      std::optional<gfx::Rect> anchor_rect = std::nullopt,
      // If not provided, will be set to |header_view|.
      views::View* anchor_view = nullptr,
      bool stop_context_menu_propagation = false);

  // views::BubbleDialogDelegateView:
  views::View* GetInitiallyFocusedView() override;
  gfx::Rect GetAnchorRect() const override;
  // This needs to be added as it does not know the correct theme color until it
  // is added to widget.
  void AddedToWidget() override;

 private:
  TabGroupEditorBubbleView(const Browser* browser,
                           const tab_groups::TabGroupId& group,
                           views::View* anchor_view,
                           std::optional<gfx::Rect> anchor_rect,
                           bool stop_context_menu_propagation);
  ~TabGroupEditorBubbleView() override;

  void UpdateGroup();
  const std::u16string GetTextForCloseButton();
  const std::u16string GetSaveToggleAccessibleName();

  void OnSaveTogglePressed();
  void NewTabInGroupPressed();
  void UngroupPressed();
  void CloseGroupPressed();
  void DeleteGroupPressed();
  void MoveGroupToNewWindowPressed();

  // The action for moving a group to a new window is only enabled when the
  // tabstrip contains more than just the tabs in the current group.
  bool CanMoveGroupToNewWindow();

  // If the saved tab group service exists, this method disconnects the group
  // from the saved tab group so that actions can be performed on the group
  // without updating the saved group. If the service doesnt exist, it does
  // nothing.
  void MaybeDisconnectSavedGroup();

  // Closes all of the tabs in the tab group in the tabstrip. If the tab group
  // Is the only thing in the tabstrip, adds a new tab first so that the window
  // isn't closed.
  void DeleteGroupFromTabstrip();

  void OnBubbleClose();

  // Returns the view responsible for being able to save a tab group. It
  // most notably contains a toggle button to save and unsave the group.
  views::View* CreateSavedTabGroupItem();

  // the implementation of the ungroup command. This method is static so that
  // it can be called from dialogs as a callback.
  static void Ungroup(const Browser* browser, tab_groups::TabGroupId group);

  const raw_ptr<const Browser> browser_;
  const tab_groups::TabGroupId group_;

  class TitleFieldController : public views::TextfieldController {
   public:
    explicit TitleFieldController(TabGroupEditorBubbleView* parent)
        : parent_(parent) {}
    ~TitleFieldController() override = default;

    // views::TextfieldController:
    void ContentsChanged(views::Textfield* sender,
                         const std::u16string& new_contents) override;
    bool HandleKeyEvent(views::Textfield* sender,
                        const ui::KeyEvent& key_event) override;

   private:
    const raw_ptr<TabGroupEditorBubbleView> parent_;
  };

  TitleFieldController title_field_controller_;

  class TitleField : public views::Textfield {
    METADATA_HEADER(TitleField, views::Textfield)

   public:
    explicit TitleField(bool stop_context_menu_propagation)
        : stop_context_menu_propagation_(stop_context_menu_propagation) {}
    ~TitleField() override = default;

    // views::Textfield:
    void ShowContextMenu(const gfx::Point& p,
                         ui::MenuSourceType source_type) override;

   private:
    // Whether the context menu should be hidden the first time it shows.
    // Needed because there is no easy way to stop the propagation of a
    // ShowContextMenu event, which is sometimes used to open the bubble
    // itself.
    bool stop_context_menu_propagation_;
  };

  raw_ptr<TitleField> title_field_;

  class Footer : public views::View {
    METADATA_HEADER(Footer, views::View)
   public:
    explicit Footer(const Browser* browser_);
    ~Footer() override = default;

    static void OpenLearnMorePage(const Browser* browser_);
  };
  raw_ptr<Footer> footer_;

  Colors colors_;
  raw_ptr<ColorPickerView> color_selector_;

  raw_ptr<views::ToggleButton> save_group_toggle_ = nullptr;
  raw_ptr<views::ImageView> save_group_icon_ = nullptr;
  raw_ptr<views::Label> save_group_label_ = nullptr;
  std::vector<raw_ptr<views::LabelButton>> menu_items_;

  // If true will use the |anchor_rect_| provided in the constructor, otherwise
  // fall back to using the anchor view bounds.
  const bool use_set_anchor_rect_;

  // Creates the set of tab group colors to display and returns the color that
  // is initially selected.
  tab_groups::TabGroupColorId InitColorSet();

  std::u16string title_at_opening_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
