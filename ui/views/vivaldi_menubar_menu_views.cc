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


#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

VivaldiMenubarMenuViews::VivaldiMenubarMenuViews(
    content::WebContents* web_contents,
    vivaldi::MenubarMenuParams& params,
    int id)
    : web_contents_(web_contents) {
  Browser* browser = GetBrowser();
  if (browser) {
    menubar_.reset(new vivaldi::Menubar(browser,
        params, views::MenuRunner::SHOULD_SHOW_MNEMONICS));
    menubar_->SetActiveMenu(id);
  }
}

VivaldiMenubarMenuViews::~VivaldiMenubarMenuViews() {}

bool VivaldiMenubarMenuViews::CanShow() {
  return !!menubar_;
}

void VivaldiMenubarMenuViews::Show() {
  menubar_->RunMenu(GetTopLevelWidget());
}

//static
gfx::NativeView VivaldiMenubarMenuViews::GetActiveNativeViewFromWebContents(
    content::WebContents* web_contents) {
  return web_contents->GetFullscreenRenderWidgetHostView()
      ? web_contents->GetFullscreenRenderWidgetHostView()->GetNativeView()
      : web_contents->GetNativeView();
}

views::Widget* VivaldiMenubarMenuViews::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(
      GetActiveNativeViewFromWebContents(web_contents_));
}

Browser* VivaldiMenubarMenuViews::GetBrowser() {
  views::Widget* widget = GetTopLevelWidget();
  return widget ? chrome::FindBrowserWithWindow(widget->GetNativeWindow())
                : nullptr;
}