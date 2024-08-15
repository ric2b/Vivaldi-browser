// included by the class definition in
// "chrome/browser/render_context_menu/render_context_menu.h"

// class RenderViewContextMenu .... {
// other declarations

// private
  std::unique_ptr<NotesSubMenuObserver> insert_note_submenu_observer_;

 public:
  void VivaldiAppendSpellingSuggestionItems() {
    AppendSpellingSuggestionItems();
  }
  void VivaldiAppendLanguageSettings() {
    AppendLanguageSettings();
  }
  void VivaldiAppendSearchProvider() {
    AppendSearchProvider();
  }
  void VivaldiAppendLinkToTextItems() {
    AppendLinkToTextItems();
  }

  std::vector<base::FilePath>& get_profile_link_paths() {
    return profile_link_paths_;
  }
  const extensions::Extension* VivaldiGetExtension() const {
    return GetExtension();
  }
  static bool VivaldiMenuItemMatchesParams(
                                    const content::ContextMenuParams& params,
                                    const extensions::MenuItem* item) {
    return MenuItemMatchesParams(params, item);
  }

// };
