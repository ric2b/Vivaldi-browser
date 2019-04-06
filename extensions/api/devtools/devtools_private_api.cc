// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/devtools/devtools_private_api.h"

#include "app/vivaldi_apptools.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

bool DevtoolsPrivateGetDockingStateSizesFunction::RunAsync() {
  std::unique_ptr<vivaldi::devtools_private::GetDockingStateSizes::Params>
      params(vivaldi::devtools_private::GetDockingStateSizes::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
  DevToolsContentsResizingStrategy strategy;
  DevToolsWindow::GetInTabWebContents(contents, &strategy);

  gfx::Size container_size(params->container_width, params->container_height);

  vivaldi::devtools_private::DevtoolResizingStrategy sizes;

  // bounds is the size of the web page contents here.
  const gfx::Rect& bounds = strategy.bounds();

  // The devtools bounds is expected to be the same size as the container
  // with the inspected contents being overlaid at the given rect below.
  sizes.inspected_width = bounds.width();
  sizes.inspected_height = bounds.height();
  sizes.inspected_top = bounds.y();
  sizes.inspected_left = bounds.x();
  sizes.hide_inspected_contents = strategy.hide_inspected_contents();

  results_ =
      vivaldi::devtools_private::GetDockingStateSizes::Results::Create(sizes);

  SendResponse(true);
  return true;
}

bool DevtoolsPrivateCloseDevtoolsFunction::RunAsync() {
  std::unique_ptr<vivaldi::devtools_private::CloseDevtools::Params>
    params(vivaldi::devtools_private::CloseDevtools::Params::Create(
      *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  bool success = false;
  content::WebContents* contents;

  if (params->window_id) {
    int window_id = *params->window_id;
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser->session_id().id() == window_id) {
        TabStripModel *tabs = browser->tab_strip_model();
        for (int n = 0; n < tabs->count(); n++) {
          content::WebContents* contents = tabs->GetWebContentsAt(n);
          DevToolsWindow* window =
            DevToolsWindow::GetInstanceForInspectedWebContents(contents);
          if (window) {
            window->ForceCloseWindow();
            std::unique_ptr<base::ListValue> args =
                vivaldi::devtools_private::OnClosed::Create(
                    SessionTabHelper::IdForTab(contents).id());

            DevtoolsConnectorAPI::BroadcastEvent(
              vivaldi::devtools_private::OnClosed::kEventName, std::move(args),
              GetProfile());
          }
        }
        success = true;
        break;
      }
    }
  } else {
    Browser* browser;
    int tab_index;

    if (extensions::ExtensionTabUtil::GetTabById(
            tab_id, GetProfile(), true, &browser, NULL, &contents,
            &tab_index)) {
      DevToolsWindow* window =
          DevToolsWindow::GetInstanceForInspectedWebContents(contents);
      if (window) {
        window->ForceCloseWindow();
        success = true;
        std::unique_ptr<base::ListValue> args =
          vivaldi::devtools_private::OnClosed::Create(tab_id);

        DevtoolsConnectorAPI::BroadcastEvent(
          vivaldi::devtools_private::OnClosed::kEventName, std::move(args),
          GetProfile());
      }
    }
  }
  results_ =
    vivaldi::devtools_private::CloseDevtools::Results::Create(success);

  SendResponse(true);
  return true;
}

bool DevtoolsPrivateToggleDevtoolsFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());
  content::WebContents* current_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  std::string host = net::GetHostOrSpecFromURL(current_tab->GetURL());
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(current_tab);
  if (window) {
    window->ForceCloseWindow();
  } else {
    // Trying to inspect the Vivaldi app using shortcuts or the menu. We fake a
    // inspect element to get into the code path that leads to a
    // separate window.
    if (::vivaldi::IsVivaldiApp(host)) {
      DevToolsWindow::InspectElement(
          static_cast<VivaldiBrowserWindow*>(browser->window())
          ->web_contents()->GetMainFrame(), 0, 0);
    } else {
      DevToolsWindow::OpenDevToolsWindow(current_tab,
                                         DevToolsToggleAction::Show());
    }
  }
  SendResponse(true);
  return true;
}


}  // namespace extensions
