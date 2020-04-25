// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

void RenderViewContextMenu::AppendInsertNoteSubMenu() {
  if (!insert_note_submenu_observer_) {
    insert_note_submenu_observer_.reset(
        new NotesSubMenuObserver(this, toolkit_delegate()));
  }

  insert_note_submenu_observer_->InitMenu(params_);
  observers_.AddObserver(insert_note_submenu_observer_.get());
}
