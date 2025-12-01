#ifndef EXTENSIONS_HELPER_VIVALDI_TAB_UTILS_H_
#define EXTENSIONS_HELPER_VIVALDI_TAB_UTILS_H_

namespace content {
  class WebContents;
}

class TabStripModel;
class Browser;

namespace vivaldi {

enum struct TabType {
  WEBPANEL,

  // Sidepanel created by an extension.
  SIDEPANEL,

  // Widget on the Dashboard. This is recognized by parent_tab_id in
  // the <webview parent_tab_id=...> tag.
  WIDGET,

  // A regular tab.
  PAGE,

  // invalid means, the tab is probably just a pure WebContent instance.
  INVALID,
  NOT_SET,
};

struct TabInfo {
  int tab_id = -1;
  content::WebContents * web_contents = nullptr;
  Browser * browser = nullptr;
  TabStripModel * tab_strip = nullptr;
  int index = -1;
  TabType type = TabType::NOT_SET;
};


bool ResolveTab(content::WebContents *, TabInfo *);
bool ResolveTab(int tab_id, TabInfo *);
bool IsPanel(TabType type);
bool IsPage(TabType type);
bool IsWidget(TabType type);
TabType GetVivaldiPanelType(content::WebContents *);

} // namespace vivaldi

#endif // EXTENSIONS_HELPER_VIVALDI_TAB_UTILS_H_
