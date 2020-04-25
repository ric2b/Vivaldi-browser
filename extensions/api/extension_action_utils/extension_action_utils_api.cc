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
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/api/tabs/tabs_private_api.h"
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
#include "extensions/tools/vivaldi_tools.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/vivaldi_ui_utils.h"

using vivaldi::ui_tools::EncodeBitmap;

namespace extensions {

const char* const kBetaChromecastExtensionId =
    "dliochdbjfkdbacpmhlcpmleaejidimm";
const char* const kStableChromecastExtensionId =
    "boadgeojelhgndaghljhdicfkmllpafd";

namespace {

// Copied from "chrome/browser/devtools/devtools_ui_bindings.cc"
std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::NumberToString(SkColorGetR(color)) + "," +
         base::NumberToString(SkColorGetG(color)) + "," +
         base::NumberToString(SkColorGetB(color)) + "," +
         base::NumberToString(SkColorGetA(color) / 255.0) + ")";
}

std::string GetShortcutTextForExtensionAction(
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
  ui::Accelerator accelerator = browser_action.accelerator();
  return ::vivaldi::ShortcutText(accelerator.key_code(),
                                 accelerator.modifiers(), 0);
}

// Encodes the passed bitmap as a PNG represented as a dataurl.
std::string EncodeBitmapToPng(const SkBitmap* bitmap) {
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
  base64_output.insert(0, "data:image/png;base64,");

  return base64_output;
}

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
  int target_width_dp,
  float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
    skia::ImageOperations::Resize(rep.GetBitmap(),
      skia::ImageOperations::RESIZE_BEST,
      width_px, width_px),
    target_scale);
}

#define USE_HARDCODED_SCALE 1

void FillBitmapForTabId(vivaldi::extension_action_utils::ExtensionInfo* info,
                        ExtensionAction* action,
                        int tab_id,
                        content::BrowserContext* browser_context) {
  // Icon precedence:
  //   3. default
  //   2. declarative
  //   1. explicit

  extensions::IconImage* default_icon_image = action->default_icon_image();

  gfx::Image explicit_icon = action->GetExplicitlySetIcon(tab_id);
  gfx::Image declarative_icon = action->GetDeclarativeIcon(tab_id);
  gfx::Image image;

  if (!explicit_icon.IsEmpty()) {
    image = explicit_icon;
  } else if (!declarative_icon.IsEmpty()) {
    image = declarative_icon;
  } else if (default_icon_image) {
    image = default_icon_image->image();
  }
  if (!image.IsEmpty()) {
    // Get the image from the extension that matches the DPI we're using
    // on the monitor.
#ifdef USE_HARDCODED_SCALE
    // This matches the previous behavior where we send 32x32 images to JS,
    // which scales them down to 16x16.
    float device_scale = 2.0f;
#else
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    float device_scale = ui::GetScaleFactorForNativeView(
        browser ? browser->window()->GetNativeWindow() : nullptr);
#endif  // USE_HARDCODED_SCALE
    gfx::ImageSkia skia = image.AsImageSkia();
    gfx::ImageSkiaRep rep = skia.GetRepresentation(device_scale);
    if (rep.scale() != device_scale) {
      skia.AddRepresentation(ScaleImageSkiaRep(
        rep, ExtensionAction::ActionIconSize(), device_scale));
    }
    if (rep.is_null()) {
      info->badge_icon = std::make_unique<std::string>();
    } else {
      info->badge_icon = std::make_unique<std::string>(EncodeBitmapToPng(&rep.GetBitmap()));
    }
  } else {
    info->badge_icon.reset(new std::string(""));
  }
}


void FillInfoForTabId(vivaldi::extension_action_utils::ExtensionInfo* info,
                      ExtensionAction* action,
                      int tab_id,
                      content::BrowserContext* browser_context) {
  info->keyboard_shortcut.reset(
      new std::string(GetShortcutTextForExtensionAction(
          action, browser_context)));

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
      action->extension_id(), browser_context)));

  info->action_is_hidden.reset(
      new bool(!ExtensionActionAPI::Get(browser_context)
                    ->GetBrowserActionVisibility(action->extension_id())));

  FillBitmapForTabId(info, action, tab_id, browser_context);
}

void FillInfoFromManifest(vivaldi::extension_action_utils::ExtensionInfo* info,
                          const Extension* extension) {
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

}  // namespace

// static
ExtensionActionUtil* ExtensionActionUtilFactory::GetForBrowserContext(
      content::BrowserContext* browser_context) {
  return static_cast<ExtensionActionUtil*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
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
// static
void ExtensionActionUtil::SendIconLoaded(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  extensions::vivaldi::extension_action_utils::ExtensionInfo info;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context);
  ExtensionAction* action = manager->GetExtensionAction(*extension);

  if (action) {
    FillBitmapForTabId(&info, action, -1, browser_context);
    info.id = extension_id;

    ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnIconLoaded::kEventName,
      vivaldi::extension_action_utils::OnIconLoaded::Create(info),
      browser_context);
  }
}

ExtensionActionUtil::ExtensionActionUtil(Profile* profile)
    : profile_(profile) {
  ExtensionRegistry::Get(profile_)->AddObserver(this);
  ExtensionActionAPI::Get(profile)->AddObserver(this);
  CommandService::Get(profile_)->AddObserver(this);
}

ExtensionActionUtil::~ExtensionActionUtil() {}

void ExtensionActionUtil::Shutdown() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  ExtensionActionAPI::Get(profile_)->RemoveObserver(this);
  CommandService::Get(profile_)->RemoveObserver(this);
}

void ExtensionActionUtil::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {

  // TODO(igor@vivaldi.com): web_contents is null when
  // extension_action->action_type() is ActionInfo::TYPE_BROWSER or
  // ActionInfo::TYPE_SYSTEM_INDICATOR when tab_id should be
  // ExtensionAction::kDefaultTabId, see ExtensionActionFunction::Run in
  // Chromium. Yet we use tabId for the last active tab in this case. Is it
  // right? See VB-52519.

  // We only update the browseraction items for the active tab.
  int tab_id = ExtensionAction::kDefaultTabId;

  if (!web_contents) {
    web_contents = current_webcontents_;
  }

  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  if (browser) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    tab_id = SessionTabHelper::IdForTab(tab_strip->GetActiveWebContents()).id();
  }

  vivaldi::extension_action_utils::ExtensionInfo info;

  info.keyboard_shortcut.reset(new std::string(
      GetShortcutTextForExtensionAction(extension_action, browser_context)));

  // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
  // not stored profile_? See VB-52519.

  FillInfoForTabId(&info, extension_action, tab_id, profile_);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_action->extension_id(),
          extensions::ExtensionRegistry::ENABLED);
  if (extension) {
    FillInfoFromManifest(&info, extension);
  }

  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnUpdated::kEventName,
      vivaldi::extension_action_utils::OnUpdated::Create(info),
      browser_context);
}

void ExtensionActionUtil::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
  // not stored profile_? See See VB-52519.

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (action) {
    vivaldi::extension_action_utils::ExtensionInfo info;
    FillInfoForTabId(&info, action, ExtensionAction::kDefaultTabId, profile_);

    ::vivaldi::BroadcastEvent(
        vivaldi::extension_action_utils::OnRemoved::kEventName,
        vivaldi::extension_action_utils::OnRemoved::Create(info),
        browser_context);
  }
}

void ExtensionActionUtil::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
  // not stored profile_? See VB-52519.

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action) {
    return;
  }

  vivaldi::extension_action_utils::ExtensionInfo info;
  int tab_id = ExtensionAction::kDefaultTabId;
  int icon_size = extension_misc::EXTENSION_ICON_MEDIUM;

  FillInfoForTabId(&info, action, tab_id, profile_);

  FillInfoFromManifest(&info, extension);

  // Notify the client about the extension info we got so far.
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnAdded::kEventName,
      vivaldi::extension_action_utils::OnAdded::Create(info),
      browser_context);

  std::unique_ptr<extensions::ExtensionResource> icon_resource;
  ExtensionAction* pageorbrowser_action =
      action_manager->GetExtensionAction(*extension);

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
        base::Bind(&ExtensionActionUtil::SendIconLoaded,
                   browser_context, extension->id()));
  }
}

void ExtensionActionUtil::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  vivaldi::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();

  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnRemoved::kEventName,
      vivaldi::extension_action_utils::OnRemoved::Create(info),
      browser_context);
}

void ExtensionActionUtil::OnExtensionCommandAdded(
    const std::string& extension_id, const Command& added_command) {
  // TODO(daniel@vivaldi.com): Currently we only support shortcuts for
  // _execute_browser_action ("Activate the Extension"). Some extensions come
  // with other keyboard shortcuts of their own. Until we add support for those,
  // only send _execute_browser_action through.
  if (added_command.command_name() != "_execute_browser_action")
    return;
  std::string shortcut_text =
      ::vivaldi::ShortcutText(added_command.accelerator().key_code(),
                              added_command.accelerator().modifiers(), 0);
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnCommandAdded::kEventName,
      vivaldi::extension_action_utils::OnCommandAdded::Create(
          extension_id, shortcut_text), profile_);
}

void ExtensionActionUtil::OnExtensionCommandRemoved(
    const std::string& extension_id, const Command& removed_command) {
  if (removed_command.command_name() != "_execute_browser_action")
    return;
  std::string shortcut_text =
      ::vivaldi::ShortcutText(removed_command.accelerator().key_code(),
                              removed_command.accelerator().modifiers(), 0);

  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnCommandRemoved::kEventName,
      vivaldi::extension_action_utils::OnCommandRemoved::Create(
          extension_id, shortcut_text),
      profile_);
}

void ExtensionActionUtil::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return;

  current_webcontents_ = selection.new_contents;

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
      OnExtensionActionUpdated(action, selection.new_contents, profile_);
    }
  }
}

//////////////////////////////////////////

namespace {

std::string NoSuchExtension(const std::string& extension_id) {
  return "Failed to find an extension with id " + extension_id;
}

std::string NoSuchWindow(int window_id) {
  return "Failed to find a browser window with window_id " +
         std::to_string(window_id);
}

std::string NoExtensionAction(const std::string& extension_id) {
  return "No action for the extension with id " + extension_id;
}

}  // namespace


ExtensionFunction::ResponseAction
ExtensionActionUtilsGetToolbarExtensionsFunction::Run() {
  namespace Results =
      vivaldi::extension_action_utils::GetToolbarExtensions::Results;

  std::vector<vivaldi::extension_action_utils::ExtensionInfo>
      toolbar_extensionactions;

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(browser_context())
          ->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser_context());

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);

    if (action &&
        !extensions::Manifest::IsComponentLocation(extension->location())) {
      vivaldi::extension_action_utils::ExtensionInfo info;

      FillInfoForTabId(
          &info, action, ExtensionAction::kDefaultTabId, browser_context());
      info.name.reset(new std::string(extension->name()));

      std::string manifest_string;
      if (extension->manifest()->GetString(manifest_keys::kHomepageURL,
                                            &manifest_string)) {
        info.homepage.reset(new std::string(manifest_string));
      }
      if (extension->manifest()->GetString(manifest_keys::kOptionsPage,
                                            &manifest_string)) {
        info.optionspage.reset(new std::string(manifest_string));
      }

      toolbar_extensionactions.push_back(std::move(info));
    }
  }

  return RespondNow(ArgumentList(Results::Create(toolbar_extensionactions)));
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsExecuteExtensionActionFunction::Run() {
  using vivaldi::extension_action_utils::ExecuteExtensionAction::Params;
  namespace Results =
      vivaldi::extension_action_utils::ExecuteExtensionAction::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  // Note; we cannot use GetAssociatedWebContents since the extension is not
  // running in a tab.
  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser_context());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action)
    return RespondNow(Error(NoExtensionAction(params->extension_id)));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  std::string popup_url_str;
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner && action_runner->RunAction(extension, true) ==
                           ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url =
        action->GetPopupUrl(SessionTabHelper::IdForTab(web_contents).id());
    popup_url_str = popup_url.spec();
  }

  return RespondNow(ArgumentList(Results::Create(popup_url_str)));
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsToggleBrowserActionVisibilityFunction::Run() {
  using vivaldi::extension_action_utils::ToggleBrowserActionVisibility::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser_context());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action)
    return RespondNow(Error(NoExtensionAction(params->extension_id)));

  ExtensionActionAPI* api = ExtensionActionAPI::Get(browser_context());
  bool toggled_visibility =
      !api->GetBrowserActionVisibility(params->extension_id);
  api->SetBrowserActionVisibility(params->extension_id, toggled_visibility);
  api->NotifyChange(action, nullptr, browser_context());
  return RespondNow(NoArguments());
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
    uninstall_dialog_ = ExtensionUninstallDialog::Create(
        browser->profile(), browser->window()->GetNativeWindow(), this);
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

ExtensionFunction::ResponseAction ExtensionActionUtilsRemoveExtensionFunction::Run() {
  using vivaldi::extension_action_utils::RemoveExtension::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  UninstallDialogHelper::UninstallExtension(browser, extension);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsShowExtensionOptionsFunction::Run() {
  using vivaldi::extension_action_utils::ShowExtensionOptions::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  DCHECK(OptionsPageInfo::HasOptionsPage(extension));

  ExtensionTabUtil::OpenOptionsPage(extension, browser);

  return RespondNow(NoArguments());
}

}  // namespace extensions
