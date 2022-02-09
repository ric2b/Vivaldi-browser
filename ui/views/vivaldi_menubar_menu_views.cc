//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "ui/views/vivaldi_menubar_menu_views.h"

#include "browser/menus/vivaldi_menubar.h"
#include "browser/vivaldi_browser_finder.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/views/widget/widget.h"

namespace vivaldi {

VivaldiMenubarMenu* CreateVivaldiMenubarMenu(content::WebContents* web_contents,
                                             MenubarMenuParams& params,
                                             int id) {
  return new VivaldiMenubarMenuViews(web_contents, params, id);
}

void ConvertMenubarButtonRectToScreen(content::WebContents* web_contents,
                                      MenubarMenuParams& params) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      VivaldiMenu::GetActiveNativeViewFromWebContents(web_contents));
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(widget->GetContentsView(), &screen_loc);
  for (::vivaldi::MenubarMenuEntry& e : params.siblings) {
    gfx::Point point(e.rect.origin());
    point.Offset(screen_loc.x(), screen_loc.y());
    e.rect.set_origin(point);
  }
}

VivaldiMenubarMenuViews::VivaldiMenubarMenuViews(
    content::WebContents* web_contents,
    MenubarMenuParams& params,
    int id)
    : web_contents_(web_contents) {
  Browser* browser = vivaldi::FindBrowserForEmbedderWebContents(web_contents_);
  if (browser) {
    menubar_.reset(
        new Menubar(browser, params, views::MenuRunner::SHOULD_SHOW_MNEMONICS));
    menubar_->SetActiveMenu(id);
  }
}

VivaldiMenubarMenuViews::~VivaldiMenubarMenuViews() {}

bool VivaldiMenubarMenuViews::CanShow() {
  return !!menubar_;
}

void VivaldiMenubarMenuViews::Show() {
  menubar_->RunMenu(GetTopLevelWidgetFromWebContents(web_contents_));
}

}  // namespace vivaldi
