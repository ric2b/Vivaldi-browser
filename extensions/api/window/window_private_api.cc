// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/window/window_private_api.h"

#include <memory>
#include <string>

#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "ui/base/ui_base_types.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using extensions::vivaldi::window_private::WindowState;

namespace extensions {

VivaldiWindowsAPI::VivaldiWindowsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  BrowserList::GetInstance()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
                 content::NotificationService::AllSources());
}

VivaldiWindowsAPI::~VivaldiWindowsAPI() {}

void VivaldiWindowsAPI::Shutdown() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiWindowsAPI> >::
    DestructorAtExit g_factory_window = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiWindowsAPI>*
VivaldiWindowsAPI::GetFactoryInstance() {
  return g_factory_window.Pointer();
}

void VivaldiWindowsAPI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      // Browser close was cancelled so notify the client about this
      // fact so it can re-enable chrome tab event.
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      ::vivaldi::DispatchEvent(
          Profile::FromBrowserContext(browser_context_),
          extensions::vivaldi::window_private::OnWindowCloseCancelled::
              kEventName,
          extensions::vivaldi::window_private::OnWindowCloseCancelled::Create(
              browser->session_id().id()));
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void VivaldiWindowsAPI::Notify(app_modal::JavaScriptAppModalDialog* dialog) {
  if (dialog->is_before_unload_dialog()) {
    // We notify the UI which tab opened a beforeunload dialog so
    // appropiate action can be taken.
    int id = SessionTabHelper::IdForTab(dialog->web_contents()).id();

    ::vivaldi::DispatchEvent(
        Profile::FromBrowserContext(browser_context_),
        extensions::vivaldi::window_private::OnBeforeUnloadDialogOpened::
            kEventName,
        extensions::vivaldi::window_private::OnBeforeUnloadDialogOpened::Create(
            id));
  }
}

void VivaldiWindowsAPI::OnBrowserAdded(Browser* browser) {
  // In Vivaldi we add the ExtensionActionUtil object as an tabstripobserver for
  // each browser. We fetch the correct browser for each update.
  extensions::ExtensionActionUtil* utils =
    extensions::ExtensionActionUtilFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));

  browser->tab_strip_model()->AddObserver(utils);

  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      Profile::FromBrowserContext(browser_context_));

  browser->tab_strip_model()->AddObserver(api);
}

void VivaldiWindowsAPI::OnBrowserRemoved(Browser* browser) {
  // In Vivaldi we add the ExtensionActionUtil object as an tabstripobserver for
  // each browser. We fetch the correct browser for each update.
  extensions::ExtensionActionUtil* utils =
      extensions::ExtensionActionUtilFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));

  browser->tab_strip_model()->RemoveObserver(utils);

  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
    Profile::FromBrowserContext(browser_context_));

  browser->tab_strip_model()->RemoveObserver(api);

  if (chrome::GetTotalBrowserCount() == 1) {
    BrowserList* browsers = BrowserList::GetInstance();
    for (BrowserList::const_iterator iter = browsers->begin();
         iter != browsers->end(); ++iter) {
      const Browser* browser = *iter;
      // If this is the last normal window, close the settings
      // window so shutdown can progress normally.
      if (browser->is_vivaldi() &&
          static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
              VivaldiBrowserWindow::WindowType::SETTINGS) {
        browser->window()->Close();
        break;
      }
    }
  }
}

ui::WindowShowState ConvertToWindowShowState(WindowState state) {
  switch (state) {
    case WindowState::WINDOW_STATE_NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case WindowState::WINDOW_STATE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case WindowState::WINDOW_STATE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case WindowState::WINDOW_STATE_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case WindowState::WINDOW_STATE_NONE:
      return ui::SHOW_STATE_DEFAULT;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

VivaldiBrowserWindow::WindowType ConvertToVivaldiWindowType(
    vivaldi::window_private::WindowType type) {
  switch (type) {
    case vivaldi::window_private::WindowType::WINDOW_TYPE_NORMAL:
      return VivaldiBrowserWindow::WindowType::NORMAL;
    case vivaldi::window_private::WindowType::WINDOW_TYPE_SETTINGS:
      return VivaldiBrowserWindow::WindowType::SETTINGS;
    case vivaldi::window_private::WindowType::WINDOW_TYPE_NONE:
      return VivaldiBrowserWindow::WindowType::NORMAL;
  }
  NOTREACHED();
  return VivaldiBrowserWindow::WindowType::NORMAL;
}

bool WindowPrivateCreateFunction::RunAsync() {
  std::unique_ptr<vivaldi::window_private::Create::Params> params(
      vivaldi::window_private::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int top = 0;
  int left = 0;
  int min_width = 0;
  int min_height = 0;

  if (params->options.bounds.top.get()) {
    top = *params->options.bounds.top.get();
  }
  if (params->options.bounds.left.get()) {
    left = *params->options.bounds.left.get();
  }
  if (params->options.bounds.min_width.get()) {
    min_width = *params->options.bounds.min_width.get();
  }
  if (params->options.bounds.min_height.get()) {
    min_height= *params->options.bounds.min_height.get();
  }
  bool incognito = false;
  bool focused = true;
  std::string tab_url;
  std::string ext_data;
  vivaldi::window_private::WindowType type =
      vivaldi::window_private::WindowType::WINDOW_TYPE_NORMAL;

  if (params->options.incognito.get()) {
    incognito = *params->options.incognito.get();
  }
  if (params->options.focused.get()) {
    focused = *params->options.focused.get();
  }
  if (params->options.tab_url.get()) {
    tab_url = *params->options.tab_url.get();
  }
  if (params->options.ext_data.get()) {
    ext_data = *params->options.ext_data.get();
  }
  type = params->type;
  Profile* profile = GetProfile();
  if (incognito) {
    profile = profile->GetOffTheRecordProfile();
  } else {
    profile = profile->GetOriginalProfile();
  }
  gfx::Rect bounds(left, top, params->options.bounds.width,
                   params->options.bounds.height);
  Browser::CreateParams create_params(Browser::TYPE_POPUP, profile, false);

  // App window specific parameters
  extensions::AppWindow::CreateParams app_params;

  app_params.creator_process_id = render_frame_host()->GetProcess()->GetID();
  app_params.focused = focused;
  if (params->options.window_decoration.get()) {
    bool window_decoration = *params->options.window_decoration.get();
    app_params.frame =
        window_decoration ? AppWindow::FRAME_CHROME : AppWindow::FRAME_NONE;
  } else {
    if (GetProfile()->GetPrefs()->GetBoolean(
            vivaldiprefs::kWindowsUseNativeDecoration)) {
      app_params.frame = extensions::AppWindow::FRAME_CHROME;
    } else {
      app_params.frame = extensions::AppWindow::FRAME_NONE;
    }
  }
  app_params.content_spec.bounds = bounds;
  app_params.content_spec.minimum_size = gfx::Size(min_width, min_height);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::CreateVivaldiBrowserWindow(nullptr, params->url,
                                                       app_params);

  create_params.initial_bounds = bounds;
  create_params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  create_params.is_session_restore = false;
  create_params.is_vivaldi = true;
  create_params.window = window;
  create_params.ext_data = ext_data;

  Browser* browser = new Browser(create_params);

  window->set_browser(browser);
  window->set_type(ConvertToVivaldiWindowType(type));

  // Create the document now.
  window->CreateWebContents(render_frame_host());

  if (tab_url.empty()) {
    tab_url = "about:blank";
  }
  content::OpenURLParams urlparams(GURL(tab_url), content::Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  browser->OpenURL(urlparams);

  // TODO(pettern): If we ever need to open unfocused windows, we need to
  // add a new method for open delayed and unfocused.
  //  window->Show(focused ? AppWindow::SHOW_ACTIVE : AppWindow::SHOW_INACTIVE);
  results_ = vivaldi::window_private::Create::Results::Create(
      browser->session_id().id());

  SendResponse(true);
  return true;
}

bool WindowPrivateGetCurrentIdFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  DCHECK(browser);
  if (browser) {
    results_ = vivaldi::window_private::GetCurrentId::Results::Create(
        browser->session_id().id());
    SendResponse(true);
    return true;
  }
  SendResponse(false);
  return true;
}

bool WindowPrivateSetStateFunction::RunAsync() {
  std::unique_ptr<vivaldi::window_private::SetState::Params> params(
      vivaldi::window_private::SetState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  Browser* browser;
  if (!windows_util::GetBrowserFromWindowID(
          this, params->window_id, WindowController::GetAllWindowFilter(),
          &browser, &error_)) {
    results_ = vivaldi::window_private::SetState::Results::Create(false);
    SendResponse(false);
    return true;
  }
  ui::WindowShowState show_state = ConvertToWindowShowState(params->state);

  bool was_fullscreen = browser->window()->IsFullscreen();
  if (show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_DEFAULT)
    browser->extension_window_controller()->SetFullscreenMode(
        false, extension()->url());

  switch (show_state) {
    case ui::SHOW_STATE_MINIMIZED:
      browser->extension_window_controller()->window()->Minimize();
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      browser->extension_window_controller()->window()->Maximize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      browser->extension_window_controller()->SetFullscreenMode(
          true, extension()->url());
      break;
    case ui::SHOW_STATE_NORMAL:
      if (was_fullscreen) {
        browser->extension_window_controller()->SetFullscreenMode(
            false, extension()->url());
      } else {
        browser->window()->Restore();
      }
      break;
    default:
      break;
  }
  SendResponse(true);
  return true;
}
}  // namespace extensions
