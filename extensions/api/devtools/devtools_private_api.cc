// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/devtools/devtools_private_api.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/vivaldi_browser_component_wrapper.h"
#include "net/base/url_util.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

using extensions::vivaldi::devtools_private::PanelType;

namespace extensions {

ExtensionFunction::ResponseAction
DevtoolsPrivateGetDockingStateSizesFunction::Run() {
  using vivaldi::devtools_private::GetDockingStateSizes::Params;
  namespace Results = vivaldi::devtools_private::GetDockingStateSizes::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id;

  std::string error;
  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, browser_context(),
                                                      &error);
  if (!contents)
    return RespondNow(Error(error));

  DevToolsContentsResizingStrategy strategy;

  // If the call returns null, contiue with default values for the strategy.
  DevToolsWindow::GetInTabWebContents(contents, &strategy);

  // bounds is the size of the web page contents here.
  const gfx::Rect& bounds = strategy.bounds();

  // The devtools bounds is expected to be the same size as the container
  // with the inspected contents being overlaid at the given rect below.
  vivaldi::devtools_private::DevtoolResizingStrategy sizes;
  sizes.inspected_width = bounds.width();
  sizes.inspected_height = bounds.height();
  sizes.inspected_top = bounds.y();
  sizes.inspected_left = bounds.x();
  sizes.hide_inspected_contents = strategy.hide_inspected_contents();

  return RespondNow(ArgumentList(Results::Create(sizes)));
}

ExtensionFunction::ResponseAction DevtoolsPrivateCloseDevtoolsFunction::Run() {
  using vivaldi::devtools_private::CloseDevtools::Params;
  namespace Results = vivaldi::devtools_private::CloseDevtools::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id;
  bool success = false;

  if (params->window_id) {
    Browser* browser = ::vivaldi::FindBrowserByWindowId(
            *params->window_id);
    if (browser) {
      TabStripModel* tabs = browser->tab_strip_model();
      for (int n = 0; n < tabs->count(); n++) {
        content::WebContents* contents = tabs->GetWebContentsAt(n);
        DevToolsWindow* window =
            DevToolsWindow::GetInstanceForInspectedWebContents(contents);
        if (window) {
          window->ForceCloseWindow();
          int tab_id_w = sessions::SessionTabHelper::IdForTab(contents).id();
          DevtoolsConnectorAPI::SendClosed(browser_context(), tab_id_w);
        }
      }
      success = true;
    }
  } else {
    content::WebContents* contents = nullptr;
    WindowController* browser;
    int tab_index;

    if (VivaldiBrowserComponentWrapper::GetInstance()
            ->ExtensionTabUtilGetTabById(tab_id, browser_context(), true,
                                         &browser, &contents, &tab_index)) {
      DevToolsWindow* window =
          DevToolsWindow::GetInstanceForInspectedWebContents(contents);
      if (window) {
        window->ForceCloseWindow();
        success = true;
        DevtoolsConnectorAPI::SendClosed(browser_context(), tab_id);
      }
    }
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction DevtoolsPrivateToggleDevtoolsFunction::Run() {
  using vivaldi::devtools_private::ToggleDevtools::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PanelType panelType = params->panel_type;
  int windowId = params->window_id;

  Browser* browser =
      ::vivaldi::FindBrowserByWindowId(windowId);

  content::WebContents* current_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(current_tab);
  if (window) {
    if (window->IsDocked() && panelType != PanelType::kInspect) {
      // NOTE(tomas@vivaldi)
      // VB-93529 Don't close if opening elements tab (kInspect)
      // That is the behaviour in chromium and is expected by users
      window->ForceCloseWindow();
    } else {
      // Will activate the existing devtools.
      DevToolsWindow::OpenDevToolsWindow(
          current_tab, DevToolsOpenedByAction::kContextMenuInspect);
    }
  } else {
    std::string host = net::GetHostOrSpecFromURL(current_tab->GetURL());
    content::WebContents* contents_to_inspect = current_tab;
    if (::vivaldi::IsVivaldiApp(host) || VIVALDI_WEBUI_URL_HOST == host) {
      // Trying to inspect the Vivaldi app using shortcuts or the menu.
      // Use the UI web contents
      contents_to_inspect =
          static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
    }
    if (panelType == PanelType::kDefault) {
      DevToolsWindow::OpenDevToolsWindow(contents_to_inspect,
          DevToolsToggleAction::Show(),
          DevToolsOpenedByAction::kContextMenuInspect);
    } else if (panelType == PanelType::kInspect) {
      DevToolsWindow::OpenDevToolsWindow(contents_to_inspect,
          DevToolsToggleAction::Inspect(),
          DevToolsOpenedByAction::kContextMenuInspect);
    } else if (panelType == PanelType::kConsole) {
      DevToolsWindow::OpenDevToolsWindow(contents_to_inspect,
          DevToolsToggleAction::ShowConsolePanel(),
          DevToolsOpenedByAction::kContextMenuInspect);
    }
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
