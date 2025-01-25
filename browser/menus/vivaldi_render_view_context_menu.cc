//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_render_view_context_menu.h"

#include <map>
#include "app/vivaldi_constants.h"
#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_device_menu_controller.h"
#include "browser/menus/vivaldi_extensions_menu_controller.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "browser/menus/vivaldi_menus.h"
#include "browser/menus/vivaldi_profile_menu_controller.h"
#include "browser/menus/vivaldi_pwa_link_menu_controller.h"
#include "browser/vivaldi_browser_finder.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/common/url_constants.h"
#include "chromium/content/public/browser/navigation_entry.h"
#include "components/notes/notes_submenu_observer.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/api/context_menu/context_menu_api.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(IS_MAC)
#include "browser/menus/vivaldi_speech_menu_controller.h"
#endif

const char* kGetVivaldiForMobileUrl = "https://www.vivaldi.com/mobile";

// Comment out if original chrome menu behavior is needed.
#define ENABLE_VIVALDI_CONTEXT_MENU

namespace {

bool QRCodeGeneratorEnabled(content::WebContents* web_contents) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;

  bool incognito = web_contents->GetBrowserContext()->IsOffTheRecord();
  return !incognito && qrcode_generator::QRCodeGeneratorBubbleController::
                           IsGeneratorAvailable(entry->GetURL());
}

bool DoesFormControlTypeSupportEmoji(
    blink::mojom::FormControlType form_control_type) {
  switch (form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    default:
      return false;
  }
}

content::WebContents* GetWebContentsToUse(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If we're viewing in a MimeHandlerViewGuest, use its embedder WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (guest_view)
    return guest_view->embedder_web_contents();
#endif
  return web_contents;
}

const GURL& GetDocumentURL(const content::ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

WindowOpenDisposition GetNewTabDispostion(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool open_in_background =
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kTabsOpenNewInBackground);
  return open_in_background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                            : WindowOpenDisposition::NEW_FOREGROUND_TAB;
}

// Same as RenderViewContextMenu::IsOpenLinkOTREnabled() except for no
// IsOffTheRecord() test.
bool CanOpenInPrivateWindow(content::BrowserContext* browser_context,
                            const GURL& link_url) {
  if (!link_url.is_valid())
    return false;

  if (!IsURLAllowedInIncognito(link_url))
    return false;

  policy::IncognitoModeAvailability incognito_avail =
      IncognitoModePrefs::GetAvailability(
          user_prefs::UserPrefs::Get(browser_context));
  return incognito_avail != policy::IncognitoModeAvailability::kDisabled;
}

}  // namespace

namespace vivaldi {

VivaldiRenderViewContextMenu* VivaldiRenderViewContextMenu::active_controller_ =
    nullptr;
int VivaldiRenderViewContextMenu::active_id_counter_ = 0;

// static
VivaldiRenderViewContextMenu* VivaldiRenderViewContextMenu::Create(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params,
    gfx::NativeView parent_view) {
  return new VivaldiRenderViewContextMenu(render_frame_host, params,
                                          parent_view);
}

// static
VivaldiRenderViewContextMenu* VivaldiRenderViewContextMenu::GetActive(int id) {
  return active_controller_ && active_controller_->id_ == id
             ? active_controller_
             : nullptr;
}

// static
bool VivaldiRenderViewContextMenu::Supports(
    const content::ContextMenuParams& params) {
#if defined(ENABLE_VIVALDI_CONTEXT_MENU)
  Browser* browser = chrome::FindBrowserWithActiveWindow();

  // We do not (yet) support configurable menus in a progressive web app (PWA)
  // We may want to test for Browser::app_controller() as well, but currently
  // not needed.
  if (browser && browser->is_type_app()) {
    return false;
  }
  // kVivaldiAppId match for areas in UI where we have no JS handler
  // and for editable fields in UI.
  if (params.page_url.host() == vivaldi::kVivaldiAppId) {
    return params.is_editable;
  }
  // We leave devtools alone for chromion to set up except for the edit menu.
  if (params.page_url.SchemeIs(content::kChromeDevToolsScheme)) {
    // TODO(espen): We want text field menus in dev tools to be configurable so
    // that we can use actions as from regular document text fields (eg insert
    // notes which by some are used to insert statements etc).
    // The menu code as it is now depends on being inside a vivaldi browser
    // window so we have to prevent configurability if that is not the case.
    if (params.is_editable) {
      if (browser && VivaldiBrowserWindow::FromBrowser(browser)) {
        return true;
      }
    }
    return false;
  }

  return true;
#else
  return false;
#endif
}

VivaldiRenderViewContextMenu::VivaldiRenderViewContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params,
    gfx::NativeView parent_view)
    : RenderViewContextMenu(render_frame_host, params),
      id_(active_id_counter_++),
      parent_view_(parent_view),
      embedder_web_contents_(GetWebContentsToUse(source_web_contents_)) {

  active_controller_ = this;
}

VivaldiRenderViewContextMenu::~VivaldiRenderViewContextMenu() {
  // A smart pointer assignment in the owner will allocate a new instance before
  // destroying the old so a test is necessary.
  if (this == active_controller_) {
    active_controller_ = nullptr;
  }
  if (menu_delegate_) {
    // This happens if we are destroyed while menu is open as a result of the
    // parent view being destroyed.
    menu_delegate_->OnDestroyed(this);
  }
}

bool VivaldiRenderViewContextMenu::SupportsInspectTools() {
  // kVivaldiAppId: A vivaldi document (typically edit menus).
  return params_.page_url.host() == vivaldi::kVivaldiAppId;
}

void VivaldiRenderViewContextMenu::InitMenu() {
  using namespace extensions::vivaldi;
  Browser* browser = FindBrowserWithTab(embedder_web_contents_);
  if (!browser) {
    // This happens when we request a menu from the UI document (edit fields).
    VivaldiBrowserWindow* window =
        FindWindowForEmbedderWebContents(embedder_web_contents_);
    if (!window) {
      return;
    }
    browser = window->browser();
  }

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(embedder_web_contents_);
  std::unique_ptr<ContextMenuContentType> content_type =
      ContextMenuContentTypeFactory::Create(GetRenderFrameHost(), params_);

  bool is_vivaldi_origin = params_.page_url.host() == vivaldi::kVivaldiAppId;
  bool is_chrome_extension = params_.page_url.SchemeIs("chrome-extension");

  extensions::vivaldi::context_menu::DocumentParams request;
  request.linkurl = params_.link_url.spec();
  request.linktitle = base::UTF16ToUTF8(params_.link_text);
  request.pageurl =
      is_chrome_extension
          ? (is_vivaldi_origin ? ""
                               : embedder_web_contents_->GetVisibleURL().spec())
          : params_.page_url.spec();
  request.pagetitle = base::UTF16ToUTF8(source_web_contents_->GetTitle());
  request.srcurl = params_.src_url.spec();
  request.selection = base::UTF16ToUTF8(params_.selection_text);
  if (request.selection.length() > 0) {
    AutocompleteMatch match;
    AutocompleteClassifierFactory::GetForProfile(GetProfile())
        ->Classify(params_.selection_text, false, false,
                   metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
    if (match.destination_url.is_valid() &&
        match.destination_url != params_.link_url &&
        !AutocompleteMatch::IsSearchType(match.type) &&
        content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
            match.destination_url.scheme())) {
      request.selectionurl = match.destination_url.spec();
    }
    base::TrimWhitespace(params_.selection_text, base::TRIM_ALL,
                         &params_.selection_text);
    base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                       u" ", &params_.selection_text);
    std::u16string printable_selection_text = PrintableSelectionText();
    EscapeAmpersands(&printable_selection_text);
    request.printableselection = base::UTF16ToUTF8(printable_selection_text);
  }

  request.keywordurl = params_.vivaldi_keyword_url.spec(),
  request.isdevtools =
      params_.page_url.SchemeIs(content::kChromeDevToolsScheme);
  request.iseditable =
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_EDITABLE);
  request.isimage = content_type->SupportsGroup(
      ContextMenuContentType::
          ITEM_GROUP_MEDIA_IMAGE);  // params.has_image_contents;
  request.isframe =
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_FRAME);
  request.ismailcontent = web_view_guest && web_view_guest->IsVivaldiMail();
  request.iswebpanel = web_view_guest && web_view_guest->IsVivaldiWebPanel();
  request.iswebpagewidget =
      web_view_guest && web_view_guest->IsVivaldiWebPageWidget();
  request.ismailto = params_.link_url.SchemeIs(url::kMailToScheme);
  request.support.copy =
      content_type->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY);
  request.support.extensions =
      content_type->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION) ||
      content_type->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION);
  request.support.sendtodevices = send_tab_to_self::ShouldDisplayEntryPoint(
      embedder_web_contents_);

  request.support.qrcode = QRCodeGeneratorEnabled(embedder_web_contents_);
  request.support.emoji = params_.form_control_type.has_value()
    ? DoesFormControlTypeSupportEmoji(*params_.form_control_type) &&
        ui::IsEmojiPanelSupported()
    : false;
  request.support.editoptions =
      params_.misspelled_word.empty() &&
      !content_type->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN);
  request.support.audio = content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO);
  request.support.video = content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO);
  request.support.pictureinpicture = true;
  request.support.plugin = content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN);
  request.support.canvas = content_type->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_CANVAS);
  request.support.highlight = params_.opened_from_highlight;
  request.support.paste =
      request.iseditable &&
      (params_.edit_flags & blink::ContextMenuDataEditFlags::kCanPaste);
  if (request.iseditable) {
    if (is_vivaldi_origin) {
      if (params_.vivaldi_input_type == "vivaldi-addressfield") {
        request.textfield = context_menu::TextfieldType::kAddressfield;
      } else if (params_.vivaldi_input_type == "vivaldi-searchfield") {
        request.textfield = context_menu::TextfieldType::kSearchfield;
      } else {
        request.textfield = context_menu::TextfieldType::kRegular;
      }
    } else {
      request.textfield = context_menu::TextfieldType::kDocument;
      password_manager::ContentPasswordManagerDriver* driver =
          password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
              GetRenderFrameHost());
      request.support.password =
        driver &&
        driver->IsPasswordFieldForPasswordManager(
                        autofill::FieldRendererId(params_.field_renderer_id),
                        params_.form_control_type);
      if (request.support.password) {
        request.support.passwordgeneration =
            password_manager_util::ManualPasswordGenerationEnabled(driver);
        request.support.passwordshowall =
            password_manager_util::ShowAllSavedPasswordsContextMenuEnabled(
                driver);
      }
    }
  }

  if (request.textfield == context_menu::TextfieldType::kAddressfield ||
      request.textfield == context_menu::TextfieldType::kSearchfield ||
      request.textfield == context_menu::TextfieldType::kRegular) {
    gfx::Point point(0, 0);
    developertools_controller_.reset(
        new DeveloperToolsMenuController(embedder_web_contents_, point));
    developertools_controller_->SetHandleInspectElement(false);
  }

  is_webpage_widget_ = request.iswebpagewidget;

  extensions::ContextMenuAPI::RequestMenu(
      GetBrowserContext(), browser->session_id().id(), id_, request);
}

void VivaldiRenderViewContextMenu::Show() {}

void VivaldiRenderViewContextMenu::AddMenuModelToMap(
    int command_id,
    ui::SimpleMenuModel* menu_model) {
  menu_model_map_[command_id] = menu_model;
}

ui::SimpleMenuModel* VivaldiRenderViewContextMenu::GetMappedMenuModel(
    int command_id) {
  auto it = menu_model_map_.find(command_id);
  return it != menu_model_map_.end() ? it->second : nullptr;
}

void VivaldiRenderViewContextMenu::AddMenuItem(int command_id,
                                               const std::u16string& title) {
  if (populating_menu_model_) {
    AddMenuModelToMap(command_id, populating_menu_model_);
    populating_menu_model_->AddItem(command_id, title);
  } else {
    RenderViewContextMenu::AddMenuItem(command_id, title);
  }
}

void VivaldiRenderViewContextMenu::AddMenuItemWithIcon(
    int command_id,
    const std::u16string& title,
    const ui::ImageModel& icon) {
  if (populating_menu_model_) {
    AddMenuModelToMap(command_id, populating_menu_model_);
    populating_menu_model_->AddItemWithIcon(command_id, title, icon);
  } else {
    RenderViewContextMenu::AddMenuItemWithIcon(command_id, title, icon);
  }
}

void VivaldiRenderViewContextMenu::AddCheckItem(int command_id,
                                                const std::u16string& title) {
  if (populating_menu_model_) {
    AddMenuModelToMap(command_id, populating_menu_model_);
    populating_menu_model_->AddCheckItem(command_id, title);
  } else {
    RenderViewContextMenu::AddCheckItem(command_id, title);
  }
}

void VivaldiRenderViewContextMenu::AddSeparator() {
  if (populating_menu_model_) {
    populating_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  } else {
    RenderViewContextMenu::AddSeparator();
  }
}

void VivaldiRenderViewContextMenu::AddSubMenu(int command_id,
                                              const std::u16string& label,
                                              ui::MenuModel* model) {
  if (populating_menu_model_) {
    AddMenuModelToMap(command_id, populating_menu_model_);
    populating_menu_model_->AddSubMenu(command_id, label, model);
  } else {
    RenderViewContextMenu::AddSubMenu(command_id, label, model);
  }
}

void VivaldiRenderViewContextMenu::AddSubMenuWithStringIdAndIcon(
    int command_id,
    int message_id,
    ui::MenuModel* model,
    const ui::ImageModel& icon) {
  if (populating_menu_model_) {
    AddMenuModelToMap(command_id, populating_menu_model_);
    populating_menu_model_->AddSubMenuWithStringIdAndIcon(
        command_id, message_id, model, icon);
  } else {
    RenderViewContextMenu::AddSubMenuWithStringIdAndIcon(command_id, message_id,
                                                         model, icon);
  }
}

void VivaldiRenderViewContextMenu::UpdateMenuItem(int command_id,
                                                  bool enabled,
                                                  bool hidden,
                                                  const std::u16string& title) {
  ui::SimpleMenuModel* menu_model = GetMappedMenuModel(command_id);
  if (menu_model) {
    auto index = menu_model->GetIndexOfCommandId(command_id);
    if (!index.has_value()) {
      return;
    }
    menu_model->SetLabel(index.value(), title);
    menu_model->SetEnabledAt(index.value(), enabled);
    menu_model->SetVisibleAt(index.value(), !hidden);
    if (toolkit_delegate()) {
#if BUILDFLAG(IS_MAC)
      toolkit_delegate()->UpdateMenuItem(command_id, enabled, hidden, title);
#else
      toolkit_delegate()->RebuildMenu();
#endif
    }
  } else {
    RenderViewContextMenu::UpdateMenuItem(command_id, enabled, hidden, title);
  }
}

void VivaldiRenderViewContextMenu::UpdateMenuIcon(int command_id,
                                                  const ui::ImageModel& icon) {
  ui::SimpleMenuModel* menu_model = GetMappedMenuModel(command_id);
  if (menu_model) {
    auto index = menu_model->GetIndexOfCommandId(command_id);
    if (!index.has_value()) {
      return;
    }
    menu_model->SetIcon(index.value(), icon);
  } else {
    RenderViewContextMenu::UpdateMenuIcon(command_id, icon);
  }
}

void VivaldiRenderViewContextMenu::RemoveMenuItem(int command_id) {
  ui::SimpleMenuModel* menu_model = GetMappedMenuModel(command_id);
  if (menu_model) {
    auto index = menu_model->GetIndexOfCommandId(command_id);
    if (!index.has_value()) {
      return;
    }
    menu_model->RemoveItemAt(index.value());
    if (toolkit_delegate()) {
      toolkit_delegate()->RebuildMenu();
    }
  } else {
    RenderViewContextMenu::RemoveMenuItem(command_id);
  }
}

void VivaldiRenderViewContextMenu::AddSpellCheckServiceItem(bool is_checked) {
  if (populating_menu_model_) {
    RenderViewContextMenu::AddSpellCheckServiceItem(populating_menu_model_,
                                                    is_checked);
  } else {
    RenderViewContextMenu::AddSpellCheckServiceItem(is_checked);
  }
}

void VivaldiRenderViewContextMenu::AddAccessibilityLabelsServiceItem(
    bool is_checked) {
  RenderViewContextMenu::AddAccessibilityLabelsServiceItem(is_checked);
}

void VivaldiRenderViewContextMenu::SetCommandId(int command_id, int api_id) {
  command_id_map_[command_id] = api_id;
}

bool VivaldiRenderViewContextMenu::IsCommandIdStatic(int command_id) const {
  auto it = command_id_map_.find(command_id);
  // Must be static if not defined from api (not in map) or when the map has
  // a valid api fallback value (which it only has if command id is static).
  return it == command_id_map_.end() || it->second != -1;
}

bool VivaldiRenderViewContextMenu::IsCommandIdChecked(int command_id) const {
  // All items with a dynamic id will match here if checked.
  if (model_delegate_ && model_delegate_->IsCommandIdChecked(command_id)) {
    return true;
  }

  // Items with static ids and extensions.
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      return (params_.writing_direction_default &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_RTL:
      return (params_.writing_direction_right_to_left &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_LTR:
      return (params_.writing_direction_left_to_right &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    default:
      if (RenderViewContextMenu::IsCommandIdChecked(command_id)) {
        return true;
      }
      if (extensions_controller_ &&
          extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
              command_id) &&
          extensions_controller_->get_extension_items()->IsCommandIdChecked(
              command_id)) {
        return true;
      }
      return false;
  }
}

bool VivaldiRenderViewContextMenu::IsCommandIdVisible(int command_id) const {
  // We remove all content that is not visible in JS except extensions.
  if (extensions_controller_ &&
      extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return extensions_controller_->get_extension_items()->IsCommandIdVisible(
        command_id);
  }
  return true;
}

bool VivaldiRenderViewContextMenu::IsCommandIdEnabled(int command_id) const {
  // Image profile controller is a vivaldi feature.
  bool enabled;
  if (image_profile_controller_ &&
      image_profile_controller_->IsCommandIdEnabled(command_id, params_,
                                                    &enabled)) {
    return enabled;
  }
  if (sendtopage_controller_ &&
      sendtopage_controller_->IsCommandIdEnabled(command_id, params_,
                                                 &enabled)) {
    return enabled;
  }
  if (sendtolink_controller_ &&
      sendtolink_controller_->IsCommandIdEnabled(command_id, params_,
                                                 &enabled)) {
    return enabled;
  }
  if (extensions_controller_ &&
      extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return extensions_controller_->get_extension_items()->IsCommandIdEnabled(
        command_id);
  }
#if BUILDFLAG(IS_MAC)
  if (speech_controller_ &&
      speech_controller_->IsCommandIdEnabled(command_id, &enabled)) {
    return enabled;
  }
#endif

  // Test for static command ids that are 1: Vivaldi specific, or
  // 2: not tested for in chromium code, or 3: (the default stage) ids that we
  // share with chromium. The ids are defined in GetStaticIdForAction()
  if (IsCommandIdStatic(command_id)) {
    switch (command_id) {
      // Other static commands that are vivaldi specific, have vivaldi specific
      // behaviour and/or where we need to test for extra states.
      case IDC_VIV_OPEN_IMAGE_NEW_WINDOW: {
        bool isGuest = Profile::FromBrowserContext(
                           embedder_web_contents_->GetBrowserContext())
                           ->IsGuestSession();
        bool isPrivate =
            embedder_web_contents_->GetBrowserContext()->IsOffTheRecord();
        return params_.media_type ==
                   blink::mojom::ContextMenuDataMediaType::kImage &&
               (isGuest || (!isGuest && !isPrivate));
      }
      // For these two we prefer to modify the behaviour wrt chrome. That is,
      // open in private window is enabled in a private window and open in new
      // window is disabled. Chrome does it the other way around.
      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        bool isGuest = Profile::FromBrowserContext(
                           embedder_web_contents_->GetBrowserContext())
                           ->IsGuestSession();
        bool isPrivate =
            embedder_web_contents_->GetBrowserContext()->IsOffTheRecord();
        return isPrivate && !isGuest
                   ? false
                   : RenderViewContextMenu::IsCommandIdEnabled(command_id);
      }
      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
        return embedder_web_contents_->GetBrowserContext()->IsOffTheRecord()
                   ? CanOpenInPrivateWindow(
                         embedder_web_contents_->GetBrowserContext(),
                         params_.link_url)
                   : RenderViewContextMenu::IsCommandIdEnabled(command_id);
      // Other static commands that are vivaldi specific and/or where we need to
      // test for extra states.
      case IDC_VIV_OPEN_LINK_CURRENT_TAB:
      case IDC_VIV_OPEN_LINK_BACKGROUND_TAB:
        return params_.link_url.is_valid();
      case IDC_VIV_RELOAD_IMAGE:
        return params_.src_url.is_valid() &&
               (params_.src_url.scheme() != content::kChromeUIScheme);
      case IDC_VIV_OPEN_IMAGE_CURRENT_TAB:
      case IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB:
      case IDC_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB:
      case IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW:
        return params_.media_type ==
               blink::mojom::ContextMenuDataMediaType::kImage;
      case IDC_RELOAD: {
        extensions::WebViewGuest* guest_view =
            extensions::WebViewGuest::FromWebContents(source_web_contents_);
        if (guest_view && guest_view->IsVivaldiWebPanel()) {
          return true;
        } else {
          return RenderViewContextMenu::IsCommandIdEnabled(command_id);
        }
      }
      case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE:
        return !params_.vivaldi_keyword_url.is_empty();
      case IDC_VIV_COPY_TO_NOTE:
      case IDC_VIV_ADD_AS_EVENT:
        return !params_.selection_text.empty();
      case IDC_VIV_SEND_SELECTION_BY_MAIL:
      case IDC_VIV_COPY_PAGE_ADDRESS:
        return true;
      case IDC_VIV_USE_IMAGE_AS_BACKGROUND:
        return params_.media_type ==
               blink::mojom::ContextMenuDataMediaType::kImage;
      // These are views specific ations. We should probably have a views class
      // on top instead of mixing it in here.
      case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
        return params_.writing_direction_default &
               blink::ContextMenuData::kCheckableMenuItemEnabled;
      case IDC_WRITING_DIRECTION_RTL:
        return params_.writing_direction_right_to_left &
               blink::ContextMenuData::kCheckableMenuItemEnabled;
      case IDC_WRITING_DIRECTION_LTR:
        return params_.writing_direction_left_to_right &
               blink::ContextMenuData::kCheckableMenuItemEnabled;
      case IDC_CONTENT_CONTEXT_LOOK_UP:
      case IDC_VIV_INSPECT_SERVICE_WORKER:
      case IDC_VIV_INSPECT_PORTAL_DOCUMENT:
        return true;
      default:
        return RenderViewContextMenu::IsCommandIdEnabled(command_id);
    }
  } else {
    return model_delegate_ ? model_delegate_->IsCommandIdEnabled(command_id)
                           : false;
  }
}

bool VivaldiRenderViewContextMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (is_webpage_widget_) {
    return false; // Always
  }

  if (menu_delegate_ && !menu_delegate_->GetShowShortcuts()) {
    return false;
  }
  // Prefer accelerators from delegate as those can be configured in JS.
  if (model_delegate_ &&
      model_delegate_->GetAcceleratorForCommandId(command_id, accelerator)) {
    return true;
  } else {
    // Accelerators that have to match hardcoded shortcuts in chromium.
    return GetFixedAcceleratorForCommandId(command_id, accelerator);
  }
}

void VivaldiRenderViewContextMenu::VivaldiCommandIdHighlighted(int command_id) {
  std::string text;
  if (sendtopage_controller_ &&
      sendtopage_controller_->GetHighlightText(command_id, text)) {
  } else if (sendtolink_controller_ &&
      sendtolink_controller_->GetHighlightText(command_id, text)) {
  }

  extensions::MenubarMenuAPI::SendHover(GetProfile(), window_id_, text);
}

void VivaldiRenderViewContextMenu::ExecuteCommand(int command_id,
                                                  int event_flags) {
  // Some actions in HandleCommand below will cause a recursive call
  // to ExecuteCommand to invoke chromium.
  if (is_executing_command_) {
    RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
    return;
  }
  is_executing_command_ = true;

  auto it = command_id_map_.find(command_id);
  if (it != command_id_map_.end()) {
    // Command id has been set up in api/JS.
    if (it->second == -1) {
      // With no fallback the command_id is a dynamic id that is sent to api/JS.
      if (model_delegate_) {
        model_delegate_->ExecuteCommand(command_id, event_flags);
      }
    } else {
      // There is a fallback value. In this case command_id is a static (IDC_)
      // value and the fallback a dynamic id. A fixed command may require
      // handling in C++ before being handed over to api/JS.
      if (HandleCommand(command_id, event_flags) == ActionChain::kContinue) {
        if (model_delegate_) {
          model_delegate_->ExecuteCommand(it->second, event_flags);
        }
      }
    }
  } else {
    // Not mapped. It means the command comes from a container/controller item.
    HandleCommand(command_id, event_flags);
  }
}

void VivaldiRenderViewContextMenu::OnMenuWillShow(ui::SimpleMenuModel* source) {
  if (model_delegate_) {
    model_delegate_->OnMenuWillShow(source);
  }
  RenderViewContextMenu::OnMenuWillShow(source);
}

void VivaldiRenderViewContextMenu::MenuClosed(ui::SimpleMenuModel* source) {
  if (model_delegate_) {
    model_delegate_->MenuClosed(source);
  }
  RenderViewContextMenu::MenuClosed(source);
}

VivaldiRenderViewContextMenu::ActionChain
VivaldiRenderViewContextMenu::HandleCommand(int command_id, int event_flags) {
  // Test controllers first.
  if (extensions_controller_ &&
      extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    content::RenderFrameHost* render_frame_host = GetRenderFrameHost();
    if (render_frame_host) {
      extensions_controller_->get_extension_items()->ExecuteCommand(
          command_id, source_web_contents_, render_frame_host, params_);
    }
    return ActionChain::kStop;
  } else if (link_profile_controller_ &&
             link_profile_controller_->HandleCommand(command_id, event_flags)) {
    return ActionChain::kStop;
  } else if (image_profile_controller_ &&
             image_profile_controller_->HandleCommand(command_id,
                                                      event_flags)) {
    return ActionChain::kStop;
  } else if (sendtopage_controller_ &&
             sendtopage_controller_->HandleCommand(command_id, event_flags)) {
    return ActionChain::kStop;
  } else if (sendtolink_controller_ &&
             sendtolink_controller_->HandleCommand(command_id, event_flags)) {
    return ActionChain::kStop;
  }  // Let RenderViewContextMenu::ExecuteCommand handle
     // IDC_CONTENT_CONTEXT_INSPECTELEMENT as it has the coordinates.
  else if (developertools_controller_ &&
           developertools_controller_->HandleCommand(command_id)) {
    return ActionChain::kStop;
#if BUILDFLAG(IS_MAC)
  } else if (speech_controller_ &&
             speech_controller_->HandleCommand(command_id, event_flags)) {
    return ActionChain::kStop;
#endif
  }

  switch (command_id) {
    // Hook for commands that are not supported by chromium or those that can
    // not be executed without vivaldi specific handling.
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              url::Origin(),
                              GetNewTabDispostion(source_web_contents_),
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_LINK_BACKGROUND_TAB:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_LINK_CURRENT_TAB:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::CURRENT_TAB,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      // Open a new incognito window. Reuse chrome code for this action, but we
      // have to replace the command with IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW
      // as its handler will always open a window of the same type from the
      // window where it is called. IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD
      // will only open a new incognito window if the window is a
      // non-incognito window and otherwise a new tab in the existing window.
      RenderViewContextMenu::ExecuteCommand(
          embedder_web_contents_->GetBrowserContext()->IsOffTheRecord()
              ? IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW
              : command_id,
          event_flags);
      break;
    case IDC_VIV_OPEN_IMAGE_CURRENT_TAB:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::CURRENT_TAB,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              url::Origin(),
                              GetNewTabDispostion(GetWebContents()),
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_WINDOW:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::NEW_WINDOW,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW:
      OpenURLWithExtraHeaders(params_.src_url, GetDocumentURL(params_),
                              url::Origin(),
                              WindowOpenDisposition::OFF_THE_RECORD,
                              ui::PAGE_TRANSITION_LINK, "", true);
      break;
    case IDC_VIV_USE_IMAGE_AS_BACKGROUND:
      return ActionChain::kContinue;
    case IDC_RELOAD: {
      // Test for web panel and handle that case here if so.
      extensions::WebViewGuest* guest_view =
          extensions::WebViewGuest::FromWebContents(source_web_contents_);
      if (guest_view && guest_view->IsVivaldiWebPanel()) {
        guest_view->Reload();
      } else {
        RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
      }
      break;
    }
    case IDC_VIV_RELOAD_IMAGE: {
      // params.x and params.y position the context menu and are always in root
      // coordinates. Convert to content coordinates.
      gfx::PointF p = source_web_contents_->GetRenderViewHost()
                          ->GetWidget()
                          ->GetView()
                          ->TransformPointToRootCoordSpaceF(
                              gfx::PointF(params_.x, params_.y));
      source_web_contents_->GetRenderViewHost()->LoadImageAt(
          static_cast<int>(p.x()), static_cast<int>(p.y()));
      break;
    }
    case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE: {
      extensions::WebViewGuest* guest_view =
          extensions::WebViewGuest::FromWebContents(source_web_contents_);
      if (guest_view) {
        std::u16string keyword(TemplateURL::GenerateKeyword(params_.page_url));
        base::Value::List args;
        args.Append(keyword);
        args.Append(params_.vivaldi_keyword_url.spec());
        guest_view->CreateSearch(std::move(args));
      }
      break;
    }
    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      //break;
    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      content::RenderFrameHost* rfh = GetRenderFrameHost();
      rfh->GetRenderWidgetHost()->UpdateTextDirection(
          (command_id == IDC_WRITING_DIRECTION_RTL)
              ? base::i18n::RIGHT_TO_LEFT
              : base::i18n::LEFT_TO_RIGHT);
      rfh->GetRenderWidgetHost()->NotifyTextDirection();
      break;
    }
#if BUILDFLAG(IS_MAC)
    case IDC_CONTENT_CONTEXT_LOOK_UP: {
      content::RenderFrameHost* rfh = GetRenderFrameHost();
      content::RenderWidgetHostView* view =
          rfh->GetRenderViewHost()->GetWidget()->GetView();
      if (view) {
        view->ShowDefinitionForSelection();
      }
      break;
    }
#endif
    default:
      // Let chromium execute the rest.
      RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
  }
  return ActionChain::kStop;
}

void VivaldiRenderViewContextMenu::AddNotesController(
    ui::SimpleMenuModel* menu_model,
    int id,
    bool is_folder) {
  if (!note_submenu_observer_) {
    note_submenu_observer_.reset(
        new NotesSubMenuObserver(this, toolkit_delegate()));
  }

  note_submenu_observer_->SetRootModel(menu_model, id, is_folder);
  note_submenu_observer_->InitMenu(params_);
  observers_.AddObserver(note_submenu_observer_.get());
}

void VivaldiRenderViewContextMenu::ContainerWillOpen(
    ui::SimpleMenuModel* menu_model) {
  if (note_submenu_observer_ &&
      note_submenu_observer_->get_root_model() == menu_model) {
    note_submenu_observer_->RootMenuWillOpen();
  }
}

bool VivaldiRenderViewContextMenu::HasContainerContent(
    const Container& container) {
  namespace context_menu = extensions::vivaldi::context_menu;
  switch (container.content) {
    case context_menu::ContainerContent::kLinkinprofile:
      return !params_.link_url.is_empty() &&
             ProfileMenuController::HasRemoteProfile(GetProfile());
    case context_menu::ContainerContent::kImageinprofile:
      return params_.has_image_contents &&
             ProfileMenuController::HasRemoteProfile(GetProfile());
    case context_menu::ContainerContent::kLinkinpwa:
      return !params_.link_url.is_empty();
    case context_menu::ContainerContent::kSpeech:
#if BUILDFLAG(IS_MAC)
      return true;
#else
      return false;
#endif
    case context_menu::ContainerContent::kNotes:
    case context_menu::ContainerContent::kExtensions:
    case context_menu::ContainerContent::kLinktohighlight:
    case context_menu::ContainerContent::kSendpagetodevices:
    case context_menu::ContainerContent::kSendlinktodevices:
    case context_menu::ContainerContent::kSendimagetodevices:
    case context_menu::ContainerContent::kSpellcheck:
    case context_menu::ContainerContent::kSpellcheckoptions:
      return true;
    default:
      return false;
  }
}

void VivaldiRenderViewContextMenu::PopulateContainer(
    const Container& container,
    int id,
    bool dark_text_color,
    ui::SimpleMenuModel* menu_model) {
  namespace context_menu = extensions::vivaldi::context_menu;
  switch (container.content) {
    case context_menu::ContainerContent::kLinkinprofile:
      link_profile_controller_.reset(
          new ProfileMenuController(this, GetProfile(), false));
      link_profile_controller_->Populate(base::UTF8ToUTF16(container.name),
                                         menu_model, this);
      break;
    case context_menu::ContainerContent::kImageinprofile:
      image_profile_controller_.reset(
          new ProfileMenuController(this, GetProfile(), true));
      image_profile_controller_->Populate(base::UTF8ToUTF16(container.name),
                                          menu_model, this);
      break;
    case context_menu::ContainerContent::kLinkinpwa:
      pwa_link_controller_ = std::make_unique<PWALinkMenuController>(
          PWALinkMenuController(this, GetProfile()));
      pwa_link_controller_->Populate(
          GetBrowser(), base::UTF8ToUTF16(container.name), menu_model);
      break;
    case context_menu::ContainerContent::kNotes:
      AddNotesController(
          menu_model, id,
          container.mode ==
              extensions::vivaldi::context_menu::ContainerMode::kFolder);
      break;
    case context_menu::ContainerContent::kLinktohighlight:
      VivaldiAppendLinkToTextItems();
      break;
    case context_menu::ContainerContent::kExtensions: {
      std::u16string text = PrintableSelectionText();
      EscapeAmpersands(&text);
      extensions_controller_.reset(new ExtensionsMenuController(this));
      extensions_controller_->Populate(
          menu_model, this, VivaldiGetExtension(), source_web_contents_, text,
          base::BindRepeating(VivaldiMenuItemMatchesParams, params_));
      break;
    }
    case context_menu::ContainerContent::kSendpagetodevices:
      sendtopage_controller_.reset(
          new DeviceMenuController(
              this,
              params_.page_url,
              base::UTF16ToUTF8(source_web_contents_->GetTitle())));
      if (GetBrowser()) {
        sendtopage_controller_->Populate(
            GetBrowser(), base::UTF8ToUTF16(container.name), container.icons,
            dark_text_color, menu_model, this);
      }
      break;
    case context_menu::ContainerContent::kSendlinktodevices:
      sendtolink_controller_.reset(
          new DeviceMenuController(
              this,
              params_.link_url,
              base::UTF16ToUTF8(source_web_contents_->GetTitle())));
      if (GetBrowser()) {
        sendtolink_controller_->Populate(
            GetBrowser(), base::UTF8ToUTF16(container.name), container.icons,
            dark_text_color, menu_model, this);
      }
      break;
    case context_menu::ContainerContent::kSendimagetodevices:
      sendtolink_controller_.reset(
          new DeviceMenuController(
              this,
              params_.src_url,
              base::UTF16ToUTF8(source_web_contents_->GetTitle())));
      if (GetBrowser()) {
        sendtolink_controller_->Populate(
            GetBrowser(), base::UTF8ToUTF16(container.name), container.icons,
            dark_text_color, menu_model, this);
      }
      break;
    case context_menu::ContainerContent::kSpeech:
#if BUILDFLAG(IS_MAC)
      speech_controller_.reset(new SpeechMenuController(this));
      speech_controller_->Populate(menu_model);
#endif
      break;
    case context_menu::ContainerContent::kSpellcheck:
      populating_menu_model_ = menu_model;
      VivaldiAppendSpellingSuggestionItems();
      populating_menu_model_ = nullptr;
      break;
    case context_menu::ContainerContent::kSpellcheckoptions:
      populating_menu_model_ = menu_model;
      VivaldiAppendLanguageSettings();
      populating_menu_model_ = nullptr;
      break;
    default:
      // Prevent compile error
      break;
  }
}

int VivaldiRenderViewContextMenu::GetStaticIdForAction(std::string command) {
  static const std::map<std::string, int> map{
      {"DOCUMENT_BACK", IDC_BACK},
      {"DOCUMENT_FORWARD", IDC_FORWARD},
      {"DOCUMENT_RELOAD", IDC_RELOAD},
      {"DOCUMENT_SAVE", IDC_SAVE_PAGE},
      {"DOCUMENT_PRINT", IDC_PRINT},
      {"DOCUMENT_CAST", IDC_ROUTE_MEDIA},
      {"DOCUMENT_VIEW_SOURCE", IDC_VIEW_SOURCE},
      {"DOCUMENT_VIEW_FRAME_SOURCE", IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE},
      {"DOCUMENT_RELOAD_FRAME", IDC_CONTENT_CONTEXT_RELOADFRAME},
      {"DOCUMENT_INSPECT", IDC_CONTENT_CONTEXT_INSPECTELEMENT},
      {"DOCUMENT_OPEN_IN_NEW_TAB", IDC_CONTENT_CONTEXT_OPENLINKNEWTAB},
      {"DOCUMENT_OPEN_IN_NEW_BACKGROUND_TAB", IDC_VIV_OPEN_LINK_BACKGROUND_TAB},
      {"DOCUMENT_OPEN_IN_TAB", IDC_VIV_OPEN_LINK_CURRENT_TAB},
      {"DOCUMENT_OPEN_IN_NEW_WINDOW", IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW},
      {"DOCUMENT_OPEN_IN_PRIVATE_WINDOW",
       IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD},
      {"DOCUMENT_COPY_LINK_ADDRESS", IDC_CONTENT_CONTEXT_COPYLINKLOCATION},
      {"DOCUMENT_SAVE_LINK", IDC_CONTENT_CONTEXT_SAVELINKAS},
      {"DOCUMENT_COPY", IDC_CONTENT_CONTEXT_COPY},
      {"DOCUMENT_OPEN_IMAGE_IN_NEW_TAB", IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB},
      {"DOCUMENT_OPEN_IMAGE_IN_NEW_BACKGROUND_TAB",
       IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB},
      {"DOCUMENT_OPEN_IMAGE_IN_TAB", IDC_VIV_OPEN_IMAGE_CURRENT_TAB},
      {"DOCUMENT_OPEN_IMAGE_IN_NEW_WINDOW", IDC_VIV_OPEN_IMAGE_NEW_WINDOW},
      {"DOCUMENT_OPEN_IMAGE_IN_PRIVATE_WINDOW",
       IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW},
      {"DOCUMENT_SAVE_IMAGE", IDC_CONTENT_CONTEXT_SAVEIMAGEAS},
      {"DOCUMENT_COPY_IMAGE", IDC_CONTENT_CONTEXT_COPYIMAGE},
      {"DOCUMENT_COPY_IMAGE_ADDRESS", IDC_CONTENT_CONTEXT_COPYIMAGELOCATION},
      {"DOCUMENT_USE_IMAGE_AS_STARTPAGE_BACKGROUND",
       IDC_VIV_USE_IMAGE_AS_BACKGROUND},
      {"DOCUMENT_RELOAD_IMAGE", IDC_VIV_RELOAD_IMAGE},
      {"DOCUMENT_SEARCH_FOR IMAGE", IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE},
      {"DOCUMENT_ADD_AS_SEARCH_ENGINE",
       IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE},
      {"DOCUMENT_UNDO", IDC_CONTENT_CONTEXT_UNDO},
      {"DOCUMENT_REDO", IDC_CONTENT_CONTEXT_REDO},
      {"DOCUMENT_CUT", IDC_CONTENT_CONTEXT_CUT},
      {"DOCUMENT_COPY", IDC_CONTENT_CONTEXT_COPY},
      {"DOCUMENT_PASTE", IDC_CONTENT_CONTEXT_PASTE},
      {"DOCUMENT_PASTE_AS_PLAIN_TEXT",
       IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE},
      {"DOCUMENT_SELECT_ALL", IDC_CONTENT_CONTEXT_SELECTALL},
      {"DOCUMENT_QR_CODE", IDC_CONTENT_CONTEXT_GENERATE_QR_CODE},
      {"DOCUMENT_COPY_LINK_TEXT", IDC_CONTENT_CONTEXT_COPYLINKTEXT},
      {"DOCUMENT_EMOJI", IDC_CONTENT_CONTEXT_EMOJI},
      {"DOCUMENT_LANGUAGE_SETTINGS", IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS},
      {"DOCUMENT_DIRECTION_DEFAULT", IDC_WRITING_DIRECTION_DEFAULT},
      {"DOCUMENT_DIRECTION_LTR", IDC_WRITING_DIRECTION_LTR},
      {"DOCUMENT_DIRECTION_RTL", IDC_WRITING_DIRECTION_RTL},
      {"DOCUMENT_LOOP", IDC_CONTENT_CONTEXT_LOOP},
      {"DOCUMENT_SHOW_CONTROLS", IDC_CONTENT_CONTEXT_CONTROLS},
      {"DOCUMENT_OPEN_AV_NEW_TAB", IDC_CONTENT_CONTEXT_OPENAVNEWTAB},
      {"DOCUMENT_SAVE_AV", IDC_CONTENT_CONTEXT_SAVEAVAS},
      {"DOCUMENT_COPY_AV_ADDRESS", IDC_CONTENT_CONTEXT_COPYAVLOCATION},
      {"DOCUMENT_PICTURE_IN_PICTURE", IDC_CONTENT_CONTEXT_PICTUREINPICTURE},
      {"DOCUMENT_ROTATE_CLOCKWISE", IDC_CONTENT_CONTEXT_ROTATECW},
      {"DOCUMENT_ROTATE_COUNTERCLOCKWISE", IDC_CONTENT_CONTEXT_ROTATECCW},
      {"DOCUMENT_LOOK_UP", IDC_CONTENT_CONTEXT_LOOK_UP},
      {"DOCUMENT_SUGGEST_PASSWORD", IDC_CONTENT_CONTEXT_GENERATEPASSWORD},
  };

  auto it = map.find(command);
  if (it != map.end()) {
    return it->second;
  } else {
    return -1;
  }
}

ui::ImageModel VivaldiRenderViewContextMenu::GetImageForAction(
    std::string command) {
#if BUILDFLAG(IS_MAC)
  return ui::ImageModel();
#else
  switch (GetStaticIdForAction(command)) {
    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE:
      return ui::ImageModel::FromVectorIcon(kQrcodeGeneratorIcon);
    default:
      return ui::ImageModel();
  }
#endif
}

ui::ImageModel VivaldiRenderViewContextMenu::GetImageForContainer(
    const Container& container) {
  namespace context_menu = extensions::vivaldi::context_menu;
#if BUILDFLAG(IS_MAC)
  return ui::ImageModel();
#else
  switch (container.content) {
    case context_menu::ContainerContent::kSendpagetodevices:
    case context_menu::ContainerContent::kSendlinktodevices:
    case context_menu::ContainerContent::kSendimagetodevices:
      return ui::ImageModel::FromVectorIcon(vector_icons::kDevicesIcon);
    default:
      return ui::ImageModel();
  }
#endif
}

void VivaldiRenderViewContextMenu::OnGetMobile() {
  OpenURLWithExtraHeaders(GURL(kGetVivaldiForMobileUrl),
                          GetDocumentURL(params_),
                          url::Origin(),
                          GetNewTabDispostion(GetWebContents()),
                          ui::PAGE_TRANSITION_LINK, "", true);
}

}  // namespace vivaldi
