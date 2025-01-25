// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved.
// For inclusion in chromium/ui/views/controls/menu/menu_controller.h

public:
 static void VivaldiSetCompactLayout(bool compact_layout);
 static bool VivaldiGetCompactLayout();
 static void VivaldiSetContextMenu(bool context_menu);
 static bool VivaldiGetContextMenu();

 void VivaldiOpenMenu(MenuItemView* item);

private:
 static bool vivaldi_compact_layout_;
 static bool vivaldi_context_menu_;
 bool VivaldiHandleKeyPressed(ui::KeyboardCode key_code);
 bool VivaldiStepSiblingMenu(bool next);
 bool VivaldiHandleSynthesizedEvent(const ui::MouseEvent& event);
 void VivaldiAdjustMenubarMenuGeometry(
     gfx::Rect* menu_bounds,
     const gfx::Rect& monitor_bounds,
     const gfx::Rect& anchor_bounds);