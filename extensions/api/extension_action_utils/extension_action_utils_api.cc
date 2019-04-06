// Copyright 2015-2017 Vivaldi Technologies AS. All rights reserved.

// This class is just a proxy for emitting events from the Chrome ui for
// browserAction and pageAction badges.
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

const char* const kBetaChromecastExtensionId =
    "dliochdbjfkdbacpmhlcpmleaejidimm";
const char* const kStableChromecastExtensionId =
    "boadgeojelhgndaghljhdicfkmllpafd";

namespace ToggleBrowserActionVisibility =
    vivaldi::extension_action_utils::ToggleBrowserActionVisibility;

#define WINDOWID_PRESTRING "vivaldi_window_"

// Copied from "chrome/browser/devtools/devtools_ui_bindings.cc"
std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::IntToString(SkColorGetR(color)) + "," +
         base::IntToString(SkColorGetG(color)) + "," +
         base::IntToString(SkColorGetB(color)) + "," +
         base::NumberToString(SkColorGetA(color) / 255.0) + ")";
}

/*static*/
void ExtensionActionUtil::BroadcastEvent(const std::string& eventname,
                                         std::unique_ptr<base::ListValue> args,
                                         content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args),
      context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

ExtensionActionUtil* ExtensionActionUtilFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionActionUtil*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ExtensionActionUtilFactory* ExtensionActionUtilFactory::GetInstance() {
  return base::Singleton<ExtensionActionUtilFactory>::get();
}

ExtensionActionUtilFactory::ExtensionActionUtilFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionActionUtils",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

ExtensionActionUtilFactory::~ExtensionActionUtilFactory() {}

ExtensionActionUtil::ExtensionActionUtil(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      extension_action_api_observer_(this),
      weak_ptr_factory_(this) {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  extension_action_api_observer_.Add(ExtensionActionAPI::Get(profile));
}

ExtensionActionUtil::~ExtensionActionUtil() {
  extension_registry_observer_.RemoveAll();
}

void ExtensionActionUtil::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  vivaldi::extension_action_utils::ExtensionInfo info;

  info.keyboard_shortcut.reset(new std::string(
      GetShortcutTextForExtensionAction(extension_action, browser_context)));

  // We only update the browseraction items for the active tab.
  int tab_id = ExtensionAction::kDefaultTabId;
  int windowId = extension_misc::kCurrentWindowId;

  if (!web_contents) {
    web_contents = GetCurrentWebContents();
  }

  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  if (browser) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    tab_id = SessionTabHelper::IdForTab(tab_strip->GetActiveWebContents()).id();
    windowId = browser->session_id().id();
  }

  FillInfoForTabId(&info, extension_action, tab_id, profile_);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_action->extension_id(),
          extensions::ExtensionRegistry::ENABLED);

  FillInfoFromManifest(&info, extension);

  // We need to fill in windowid to use for differentiating between the
  // different browser windows. Since the event is originating from the
  // tab-strip which is owned by the browserwindow.

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnUpdated::Create(info, windowId,
                                                         tab_id);

  BroadcastEvent(vivaldi::extension_action_utils::OnUpdated::kEventName,
                 std::move(args), browser_context);
}

/* static */

void ExtensionActionUtil::FillInfoFromManifest(
    vivaldi::extension_action_utils::ExtensionInfo* info,
    const Extension* extension) {
  if (extension) {
    info->name = std::make_unique<std::string>(extension->name());

    std::string manifest_string;
    if (extension->manifest()->GetString(manifest_keys::kHomepageURL,
                                         &manifest_string)) {
      info->homepage = std::make_unique<std::string>(manifest_string);
    }
    if (OptionsPageInfo::HasOptionsPage(extension)) {
      GURL url = OptionsPageInfo::GetOptionsPage(extension);
      info->optionspage = std::make_unique<std::string>(url.spec());

      bool new_tab = OptionsPageInfo::ShouldOpenInTab(extension);
      info->options_in_new_tab = std::make_unique<bool>(new_tab);
    }
  }
}

/* static */
bool ExtensionActionUtil::GetWindowIdFromExtData(const std::string& extdata,
                                                 std::string* windowId) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json = base::JSONReader::Read(extdata, options);
  base::DictionaryValue* dict = NULL;
  if (json && json->GetAsDictionary(&dict)) {
    dict->GetString("ext_id", windowId);
    *windowId += WINDOWID_PRESTRING;
    return true;
  }
  return false;
}

// Called when there is a change to the extension action's visibility.
void ExtensionActionUtil::OnExtensionActionVisibilityChanged(
    const std::string& extension_id,
    bool is_now_visible) {
  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
  vivaldi::extension_action_utils::ExtensionInfo info;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);

  ExtensionAction* action =
      extension ? action_manager->GetExtensionAction(*extension) : nullptr;

  FillInfoForTabId(&info, action, ExtensionAction::kDefaultTabId, profile_);

  int windowId = extension_misc::kCurrentWindowId;

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnUpdated::Create(
          info, windowId, ExtensionAction::kDefaultTabId);
  ExtensionActionUtil::BroadcastEvent(
      vivaldi::extension_action_utils::OnUpdated::kEventName, std::move(args),
      static_cast<content::BrowserContext*>(profile_));
}

// Called when the page actions have been refreshed do to a possible change
// in count or visibility.
void ExtensionActionUtil::OnPageActionsUpdated(
    content::WebContents* web_contents) {}

// Called when the ExtensionActionAPI is shutting down, giving observers a
// chance to unregister themselves if there is not a definitive lifecycle.
void ExtensionActionUtil::OnExtensionActionAPIShuttingDown() {
  extension_action_api_observer_.RemoveAll();
}

void ExtensionActionUtil::OnImageLoaded(const std::string& extension_id,
                                        const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  extensions::vivaldi::extension_action_utils::ExtensionInfo info;

  info.id = extension_id;
  info.badge_icon.reset(
      extensions::ExtensionActionUtil::EncodeBitmapToPng(image.ToSkBitmap()));
  std::unique_ptr<base::ListValue> args =
      extensions::vivaldi::extension_action_utils::OnIconLoaded::Create(info);

  extensions::ExtensionActionUtil::BroadcastEvent(
      extensions::vivaldi::extension_action_utils::OnIconLoaded::kEventName,
      std::move(args), static_cast<content::BrowserContext*>(profile_));
}

/* static */
std::string ExtensionActionUtil::GetShortcutTextForExtensionAction(
    ExtensionAction* action,
    content::BrowserContext* browser_context) {
  bool active = false;
  extensions::Command browser_action;
  extensions::CommandService *command_service = extensions::CommandService::Get(
      browser_context);
  command_service->GetBrowserActionCommand(action->extension_id(),
          extensions::CommandService::ALL,
          &browser_action,
          &active);
  return base::UTF16ToUTF8(browser_action.accelerator().GetShortcutText());
}

/* static */
bool ExtensionActionUtil::FillInfoForTabId(
    vivaldi::extension_action_utils::ExtensionInfo* info,
    ExtensionAction* action,
    int tab_id,
    Profile* profile) {
  if (!action) {
    return false;
  }

  info->keyboard_shortcut.reset(
      new std::string(GetShortcutTextForExtensionAction(
          action, static_cast<content::BrowserContext*>(profile))));

  info->id = action->extension_id();

  // Note, all getters return default values if no explicit value has been set.
  info->badge_tooltip.reset(new std::string(action->GetTitle(tab_id)));

  info->badge_text.reset(new std::string(action->GetBadgeText(tab_id)));

  info->badge_background_color.reset(new std::string(
      SkColorToRGBAString(action->GetBadgeBackgroundColor(tab_id))));

  info->badge_text_color.reset(
      new std::string(SkColorToRGBAString(action->GetBadgeTextColor(tab_id))));

  info->action_type = action->action_type() == ActionInfo::TYPE_BROWSER
                          ? vivaldi::extension_action_utils::ACTION_TYPE_BROWSER
                          : vivaldi::extension_action_utils::ACTION_TYPE_PAGE;

  info->visible.reset(new bool(action->GetIsVisible(tab_id)));

  info->allow_in_incognito.reset(new bool(util::IsIncognitoEnabled(
      action->extension_id(), static_cast<content::BrowserContext*>(profile))));

  info->action_is_hidden.reset(
      new bool(!ExtensionActionAPI::Get(profile)->GetBrowserActionVisibility(
          action->extension_id())));

  // Icon precedence:
  //   3. default
  //   2. declarative
  //   1. explicit

  extensions::IconImage* defaultIconImage = action->default_icon_image();

  gfx::Image explicitIcon = action->GetExplicitlySetIcon(tab_id);

  gfx::Image declarativeIcon = action->GetDeclarativeIcon(tab_id);

  const SkBitmap* bitmap = nullptr;

  if (!explicitIcon.IsEmpty()) {
    bitmap = explicitIcon.CopySkBitmap();
  } else if (!declarativeIcon.IsEmpty()) {
    bitmap = declarativeIcon.CopySkBitmap();
  } else {
    if (defaultIconImage) {
      bitmap = defaultIconImage->image_skia().bitmap();
    }
  }

  if (bitmap) {
    info->badge_icon.reset(EncodeBitmapToPng(bitmap));
  } else {
    info->badge_icon.reset(new std::string(""));
  }
  return true;
}

/*static*/
std::string* ExtensionActionUtil::EncodeBitmapToPng(const SkBitmap* bitmap) {
  unsigned char* inputAddr =
      bitmap->bytesPerPixel() == 1
          ? reinterpret_cast<unsigned char*>(bitmap->getAddr8(0, 0))
          : reinterpret_cast<unsigned char*>(
                bitmap->getAddr32(0, 0));  // bpp = 4

  std::vector<unsigned char> png_data;
  std::vector<gfx::PNGCodec::Comment> comments;
  gfx::PNGCodec::Encode(inputAddr, gfx::PNGCodec::FORMAT_SkBitmap,
                        gfx::Size(bitmap->width(), bitmap->height()),
                        bitmap->rowBytes(), false, comments, &png_data);

  base::StringPiece base64_input(reinterpret_cast<const char*>(&png_data[0]),
                                 png_data.size());
  std::string base64_output;
  Base64Encode(base64_input, &base64_output);

  return new std::string("data:image/png;base64," + base64_output);
}

void ExtensionActionUtil::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  vivaldi::extension_action_utils::ExtensionInfo info;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  FillInfoForTabId(&info, action, ExtensionAction::kDefaultTabId, profile_);

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnRemoved::Create(info);

  BroadcastEvent(vivaldi::extension_action_utils::OnRemoved::kEventName,
                 std::move(args), browser_context);
}

void ExtensionActionUtil::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  vivaldi::extension_action_utils::ExtensionInfo info;
  int tab_id = ExtensionAction::kDefaultTabId;
  int icon_size = extension_misc::EXTENSION_ICON_MEDIUM;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action) {
    return;
  }

  FillInfoForTabId(&info, action, tab_id, profile_);

  FillInfoFromManifest(&info, extension);

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnAdded::Create(info);

  // Notify the client about the extension info we got so far.
  BroadcastEvent(vivaldi::extension_action_utils::OnAdded::kEventName,
                 std::move(args), browser_context);

  std::unique_ptr<extensions::ExtensionResource> icon_resource;
  ExtensionAction* pageorbrowser_action = nullptr;

  if (extension->manifest()->HasKey(
          extensions::manifest_keys::kBrowserAction)) {
    pageorbrowser_action = action_manager->GetBrowserAction(*extension);
  } else if (extension->manifest()->HasKey(
                 extensions::manifest_keys::kPageAction)) {
    pageorbrowser_action = action_manager->GetPageAction(*extension);
  }

  if (pageorbrowser_action) {
    std::set<base::FilePath> image_paths;
    if (pageorbrowser_action->default_icon()) {
      pageorbrowser_action->default_icon()->GetPaths(&image_paths);
    }

    base::FilePath icon_path;
    std::set<base::FilePath>::iterator it = image_paths.begin();
    if (it != image_paths.end()) {
      // Use the last image path, as it is the biggest.
      icon_path = *(--image_paths.end());
    }
    icon_resource.reset(new extensions::ExtensionResource(
        extension->id(), extension->path(), icon_path));

    icon_size = pageorbrowser_action->default_icon()->GetIconSizeFromPath(
        icon_path.AsUTF8Unsafe());
  }

  // If there are no browser action or page action icons, use the default icons.
  if (!icon_resource.get()) {
    icon_resource.reset(new extensions::ExtensionResource(
        extensions::IconsInfo::GetIconResource(
            extension, extension_misc::EXTENSION_ICON_MEDIUM,
            ExtensionIconSet::MATCH_BIGGER)));
  }

  if (icon_resource.get() && !icon_resource.get()->extension_root().empty()) {
    extensions::ImageLoader* loader =
        extensions::ImageLoader::Get(browser_context);
    loader->LoadImageAsync(
        extension, *icon_resource.get(), gfx::Size(icon_size, icon_size),
        base::Bind(&ExtensionActionUtil::OnImageLoaded,
                   weak_ptr_factory_.GetWeakPtr(), extension->id()));
  }
}

void ExtensionActionUtil::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  vivaldi::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();
  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnRemoved::Create(info);

  BroadcastEvent(vivaldi::extension_action_utils::OnRemoved::kEventName,
                 std::move(args), browser_context);
}

void ExtensionActionUtil::ActiveTabChanged(content::WebContents* old_contents,
                                           content::WebContents* new_contents,
                                           int index,
                                           int reason) {
  set_current_webcontents(new_contents);

  // loop through the extensions and update the actions based on the tabid
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile_)->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);
    if (action) {
      OnExtensionActionUpdated(action, new_contents,
                               static_cast<content::BrowserContext*>(profile_));
    }
  }
}

bool ExtensionActionUtil::HasComponentAction(
    const std::string& action_id) const {
  auto action = component_extension_actions_.find(action_id);
  return action != component_extension_actions_.end();
}

void ExtensionActionUtil::AddComponentAction(const std::string& action_id) {
  component_extension_actions_.insert(action_id);

  std::unique_ptr<vivaldi::extension_action_utils::ExtensionInfo> info(
      new vivaldi::extension_action_utils::ExtensionInfo());

  FillInfoFromComponentExtension(&action_id, info.get(), profile_);

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnAdded::Create(*info.get());

  content::BrowserContext* browser_context =
      static_cast<content::BrowserContext*>(profile_);

  BroadcastEvent(vivaldi::extension_action_utils::OnAdded::kEventName,
                 std::move(args), browser_context);
}

void ExtensionActionUtil::RemoveComponentAction(const std::string& action_id) {
  if (action_id != ComponentToolbarActionsFactory::kMediaRouterActionId) {
    NOTREACHED(); // Check the new component extension!
  }

  vivaldi::extension_action_utils::ExtensionInfo info;
  info.id = ComponentToolbarActionsFactory::kMediaRouterActionId;
  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnRemoved::Create(info);

  content::BrowserContext* browser_context =
      static_cast<content::BrowserContext*>(profile_);
  BroadcastEvent(vivaldi::extension_action_utils::OnRemoved::kEventName,
                 std::move(args), browser_context);

  component_extension_actions_.erase(action_id);
  media_router_component_action_.reset(nullptr);
}

bool ExtensionActionUtil::FillInfoFromComponentExtension(
    const std::string* action_id,
    vivaldi::extension_action_utils::ExtensionInfo* info,
    Profile* profile) {
  Browser* browser = nullptr;
  for (auto* browser_it : *BrowserList::GetInstance()) {
    if (browser_it->profile()->GetOriginalProfile() == profile) {
      browser = browser_it;
      break;
    }
  }

  if (!browser) {
    NOTREACHED()
        << "Querying info about component-extensions need a browser object!";
    return false;
  }

  if (!media_router_component_action_) {
    media_router_component_action_.reset(
        new MediaRouterAction(browser, nullptr));
    media_router_component_action_.get()->SetDelegate(this);
  }
  info->extension_type =
      vivaldi::extension_action_utils::EXTENSION_TYPE_COMPONENTACTION;

  info->id = *action_id;

  info->badge_tooltip.reset(new std::string(base::UTF16ToUTF8(
      media_router_component_action_.get()->GetTooltip(nullptr))));

  info->name.reset(new std::string(base::UTF16ToUTF8(
      media_router_component_action_.get()->GetActionName())));

  info->badge_text.reset(new std::string(""));

  info->action_type = vivaldi::extension_action_utils::ACTION_TYPE_BROWSER;

  int icon_size = extension_misc::EXTENSION_ICON_LARGE;

  gfx::Image icon_image = media_router_component_action_.get()->GetIcon(
      nullptr /*web_contents*/, gfx::Size(icon_size, icon_size));

  const SkBitmap* bitmap = nullptr;

  bitmap = icon_image.CopySkBitmap();

  if (bitmap)
    info->badge_icon.reset(EncodeBitmapToPng(bitmap));

  // See MediaRouterContextualMenu::ExecuteCommand()
  const char googlecasthomepage[] =
      "https://www.google.com/chrome/devices/chromecast/";

  info->homepage.reset(new std::string(googlecasthomepage));

  return true;
}


content::WebContents* ExtensionActionUtil::GetCurrentWebContents() const {
  return current_webcontents_;
}

// Updates the view to reflect current state.
void ExtensionActionUtil::UpdateState() {
  // This is only used for component extensions.
  if (!media_router_component_action_.get()) {
    return;
  }

  vivaldi::extension_action_utils::ExtensionInfo info;

  int tab_id = ExtensionAction::kDefaultTabId;
  int windowId = extension_misc::kCurrentWindowId;

  Browser *browser =
      current_webcontents_
          ? chrome::FindBrowserWithWebContents(current_webcontents_)
          : nullptr;
  if (browser) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    tab_id = SessionTabHelper::IdForTab(tab_strip->GetActiveWebContents()).id();
    windowId = browser->session_id().id();
  }

  std::string action_id = ComponentToolbarActionsFactory::kMediaRouterActionId;

  FillInfoFromComponentExtension(&action_id, &info, profile_);

  std::unique_ptr<base::ListValue> args =
      vivaldi::extension_action_utils::OnUpdated::Create(info, windowId,
                                                         tab_id);

  content::BrowserContext* browser_context =
      static_cast<content::BrowserContext*>(profile_);

  BroadcastEvent(vivaldi::extension_action_utils::OnUpdated::kEventName,
                 std::move(args), browser_context);

}

// Returns true if a context menu is running.
bool ExtensionActionUtil::IsMenuRunning() const {
  // does not really make sense for us, but mandatory
  return false;
}

//////////////////////////////////////////

ExtensionActionUtilsGetToolbarExtensionsFunction::
    ExtensionActionUtilsGetToolbarExtensionsFunction() {}

ExtensionActionUtilsGetToolbarExtensionsFunction::
    ~ExtensionActionUtilsGetToolbarExtensionsFunction() {}

bool ExtensionActionUtilsGetToolbarExtensionsFunction::RunAsync() {
  std::vector<vivaldi::extension_action_utils::ExtensionInfo>
      toolbar_extensionactions;

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(GetProfile())->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);

    if (!extensions::Manifest::IsComponentLocation(extension->location())) {
      std::unique_ptr<vivaldi::extension_action_utils::ExtensionInfo> info(
          new vivaldi::extension_action_utils::ExtensionInfo());

      if (ExtensionActionUtil::FillInfoForTabId(info.get(), action,
                                                ExtensionAction::kDefaultTabId,
                                                GetProfile())) {
        info->name.reset(new std::string(extension->name()));

        std::string manifest_string;
        if (extension->manifest()->GetString(manifest_keys::kHomepageURL,
                                             &manifest_string)) {
          info->homepage.reset(new std::string(manifest_string));
        }
        if (extension->manifest()->GetString(manifest_keys::kOptionsPage,
                                             &manifest_string)) {
          info->optionspage.reset(new std::string(manifest_string));
        }

        toolbar_extensionactions.push_back(std::move(*info));
      }
    }
  }

  results_ =
      vivaldi::extension_action_utils::GetToolbarExtensions::Results::Create(
          toolbar_extensionactions);

  SendResponse(true);

  return true;
}

ExtensionActionUtilsExecuteExtensionActionFunction::
    ExtensionActionUtilsExecuteExtensionActionFunction() {}

ExtensionActionUtilsExecuteExtensionActionFunction::
    ~ExtensionActionUtilsExecuteExtensionActionFunction() {}

bool ExtensionActionUtilsExecuteExtensionActionFunction::RunAsync() {
  std::unique_ptr<
      vivaldi::extension_action_utils::ExecuteExtensionAction::Params>
      params(vivaldi::extension_action_utils::ExecuteExtensionAction::Params::
                 Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);

  // Check if there is a component extension and run it's action.
  if (!extension) {
    extensions::ExtensionActionUtil* extensionactionutils =
        extensions::ExtensionActionUtilFactory::GetForProfile(GetProfile());

    if (extensionactionutils->media_router_component_action()) {
      extensionactionutils->media_router_component_action()->ExecuteAction(
          true);
      vivaldi::extension_action_utils::ExtensionInfo info;
      results_ = vivaldi::extension_action_utils::ExecuteExtensionAction::
          Results::Create(info);
      SendResponse(true);
      return true;
    }
  }

  DCHECK(extension);
  if (!extension)
    return false;

  // Note; we cannot use GetAssociatedWebContents since the extension is not
  // running in a tab.
  Browser* browser = nullptr;
  for (auto* browser_it : *BrowserList::GetInstance()) {
    if (ExtensionTabUtil::GetWindowId(browser_it) == *params->window_id.get() &&
        browser_it->window()) {
      browser = browser_it;
    }
  }
  DCHECK(browser);
  if (!browser) {
    return false;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  DCHECK(action);
  if (!action)
    return false;

  vivaldi::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();

  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner && action_runner->RunAction(extension, true) ==
                           ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = action->GetPopupUrl(SessionTabHelper::IdForTab(
        browser->tab_strip_model()->GetActiveWebContents()).id());
    info.popup_url.reset(new std::string(popup_url.spec()));
  }

  results_ =
      vivaldi::extension_action_utils::ExecuteExtensionAction::Results::Create(
          info);

  SendResponse(true);

  return true;
}

KeyedService* ExtensionActionUtilFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ExtensionActionUtil(static_cast<Profile*>(profile));
}

bool ExtensionActionUtilFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool ExtensionActionUtilFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* ExtensionActionUtilFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

ExtensionActionUtilsToggleBrowserActionVisibilityFunction::
    ExtensionActionUtilsToggleBrowserActionVisibilityFunction() {}

ExtensionActionUtilsToggleBrowserActionVisibilityFunction::
    ~ExtensionActionUtilsToggleBrowserActionVisibilityFunction() {}

bool ExtensionActionUtilsToggleBrowserActionVisibilityFunction::RunAsync() {
  std::unique_ptr<ToggleBrowserActionVisibility::Params> params(
      ToggleBrowserActionVisibility::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->extension_id ==
          ComponentToolbarActionsFactory::kMediaRouterActionId ||
      params->extension_id == kBetaChromecastExtensionId ||
      params->extension_id == kStableChromecastExtensionId) {

    extensions::ExtensionActionUtil* extensionactionutils =
        extensions::ExtensionActionUtilFactory::GetForProfile(GetProfile());

    // To be able to show the action again in the same session.
    extensionactionutils->RemoveComponentAction(params->extension_id);

    // Synthesize an event to remove the button right away.
    vivaldi::extension_action_utils::ExtensionInfo info;
    info.id = params->extension_id;
    std::unique_ptr<base::ListValue> args =
        vivaldi::extension_action_utils::OnRemoved::Create(info);
    ExtensionActionUtil::BroadcastEvent(
        vivaldi::extension_action_utils::OnRemoved::kEventName, std::move(args),
        static_cast<content::BrowserContext*>(GetProfile()));
    SendResponse(true);
    return true;
  }

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);

  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  DCHECK(action);
  if (!action) {
    SendResponse(false);
    return false;
  }
  ExtensionActionAPI* api = ExtensionActionAPI::Get(GetProfile());
  bool toggled_visibility =
      !api->GetBrowserActionVisibility(params->extension_id);
  api->SetBrowserActionVisibility(params->extension_id, toggled_visibility);
  api->NotifyChange(action, nullptr,
                    static_cast<content::BrowserContext*>(GetProfile()));
  SendResponse(true);
  return true;
}

namespace {
// Copied from
// chromium/chrome/browser/extensions/extension_context_menu_model.cc
class UninstallDialogHelper : public ExtensionUninstallDialog::Delegate {
 public:
  // Kicks off the asynchronous process to confirm and uninstall the given
  // |extension|.
  static void UninstallExtension(Browser* browser, const Extension* extension) {
    UninstallDialogHelper* helper = new UninstallDialogHelper();
    helper->BeginUninstall(browser, extension);
  }

 private:
  // This class handles its own lifetime.
  UninstallDialogHelper() {}
  ~UninstallDialogHelper() override {}

  void BeginUninstall(Browser* browser, const Extension* extension) {
    uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
        browser->profile(), browser->window()->GetNativeWindow(), this));
    uninstall_dialog_->ConfirmUninstall(extension,
                                        UNINSTALL_REASON_USER_INITIATED,
                                        UNINSTALL_SOURCE_TOOLBAR_CONTEXT_MENU);
  }

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error) override {
    delete this;
  }

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UninstallDialogHelper);
};
}  // namespace

ExtensionActionUtilsRemoveExtensionFunction::
    ExtensionActionUtilsRemoveExtensionFunction() {}

ExtensionActionUtilsRemoveExtensionFunction::
    ~ExtensionActionUtilsRemoveExtensionFunction() {}

bool ExtensionActionUtilsRemoveExtensionFunction::RunAsync() {
  std::unique_ptr<vivaldi::extension_action_utils::RemoveExtension::Params>
      params(vivaldi::extension_action_utils::RemoveExtension::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);

  Browser* browser = nullptr;
  if (params->window_id) {
    for (auto* browser_it : *BrowserList::GetInstance()) {
      if (ExtensionTabUtil::GetWindowId(browser_it) ==
              *params->window_id.get() &&
          browser_it->window()) {
        browser = browser_it;
      }
    }
  } else {
    browser = BrowserList::GetInstance()->GetLastActive();
  }
  DCHECK(browser);
  DCHECK(extension);

  if (!browser || !extension) {
    return false;
  }

  UninstallDialogHelper::UninstallExtension(browser, extension);

  SendResponse(true);
  return true;
}

bool ExtensionActionUtilsShowExtensionOptionsFunction::RunAsync() {
  std::unique_ptr<vivaldi::extension_action_utils::ShowExtensionOptions::Params>
      params(
          vivaldi::extension_action_utils::ShowExtensionOptions::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
    extensions::ExtensionRegistry::Get(GetProfile())
    ->GetExtensionById(params->extension_id,
      extensions::ExtensionRegistry::ENABLED);

  Browser* browser = nullptr;
  if (params->window_id) {
    for (auto* browser_it : *BrowserList::GetInstance()) {
      if (ExtensionTabUtil::GetWindowId(browser_it) ==
        *params->window_id.get() &&
        browser_it->window()) {
        browser = browser_it;
      }
    }
  }
  else {
    browser = BrowserList::GetInstance()->GetLastActive();
  }
  DCHECK(browser);
  DCHECK(extension);

  if (!browser || !extension) {
    return false;
  }
  DCHECK(OptionsPageInfo::HasOptionsPage(extension));

  ExtensionTabUtil::OpenOptionsPage(extension, browser);

  SendResponse(true);
  return true;
}

}  // namespace extensions
