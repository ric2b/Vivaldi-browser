// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/menus/vivaldi_menus.h"

#include <string>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"
#include "chrome/common/pref_names.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/base/escape.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/vivaldi_ui_utils.h"

using blink::WebContextMenuData;
using extensions::WebViewGuest;

namespace vivaldi {

// Keep a copy of selected text. Chrome code in render_view_context_menu.cc will
// for some code paths remove whitespaces (newlines) in the ContextMenuParams
// object when setting up the menu (see search menu). We need the unmodified
// string in some cases.
static base::string16 s_selection_text;

base::string16 GetModifierStringFromEventFlags(int event_flags) {
  base::string16 modifiers;
  if (event_flags & ui::EF_CONTROL_DOWN)
    modifiers.append(base::ASCIIToUTF16("ctrl"));
  if (event_flags & ui::EF_SHIFT_DOWN) {
    if (modifiers.length() > 0)
      modifiers.append(base::ASCIIToUTF16(","));
    modifiers.append(base::ASCIIToUTF16("shift"));
  }
  if (event_flags & ui::EF_ALT_DOWN) {
    if (modifiers.length() > 0)
      modifiers.append(base::ASCIIToUTF16(","));
    modifiers.append(base::ASCIIToUTF16("alt"));
  }
  if (event_flags & ui::EF_COMMAND_DOWN) {
    if (modifiers.length() > 0)
      modifiers.append(base::ASCIIToUTF16(","));
    modifiers.append(base::ASCIIToUTF16("cmd"));
  }
  return modifiers;
}

void SendSimpleAction(WebContents* web_contents,
                      int event_flags,
                      const std::string& command,
                      const base::string16& text = base::ASCIIToUTF16(""),
                      const std::string& url = "") {
  WebViewGuest* guestView = WebViewGuest::FromWebContents(web_contents);
  DCHECK(guestView);
  base::ListValue* args = new base::ListValue;
  args->Append(std::make_unique<base::Value>(command));
  args->Append(std::make_unique<base::Value>(text));
  args->Append(std::make_unique<base::Value>(url));
  base::string16 modifiers = GetModifierStringFromEventFlags(event_flags);
  args->Append(std::make_unique<base::Value>(modifiers));
  guestView->SimpleAction(*args);
}

bool IsVivaldiMail(WebContents* web_contents) {
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(web_contents);
  return web_view_guest && web_view_guest->IsVivaldiMail();
}

bool IsVivaldiWebPanel(WebContents* web_contents) {
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(web_contents);
  return web_view_guest && web_view_guest->IsVivaldiWebPanel();
}

bool IsVivaldiCommandId(int id) {
  return (id >= IDC_VIVALDI_MENU_ENUMS_START &&
          id < IDC_VIVALDI_MENU_ENUMS_END);
}

void VivaldiInitMenu(WebContents* web_contents,
                     const ContextMenuParams& params) {
  s_selection_text = params.selection_text;
}

const GURL& GetDocumentURL(const ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

void VivaldiAddLinkItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params) {
  if (IsVivaldiRunning() && !params.link_url.is_empty()) {
    int firstIndex =
        menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
    DCHECK_GE(firstIndex, 0);
    menu->RemoveItemAt(firstIndex);
    int index =
        menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
    DCHECK_GE(index, 0);
    menu->RemoveItemAt(index);
    index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    DCHECK_GE(index, 0);
    menu->RemoveItemAt(index);

    index = firstIndex;
    menu->InsertItemWithStringIdAt(index, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                   IDS_VIV_OPEN_LINK_NEW_FOREGROUND_TAB);
    menu->InsertItemWithStringIdAt(++index,
                                   IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB,
                                   IDS_VIV_OPEN_LINK_NEW_BACKGROUND_TAB);
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_OPEN_LINK_CURRENT_TAB,
                                   IDS_VIV_OPEN_LINK_CURRENT_TAB);

    menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    menu->InsertItemWithStringIdAt(++index,
                                   IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                   IDS_VIV_OPEN_LINK_NEW_WINDOW);
    menu->InsertItemWithStringIdAt(++index,
                                   IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                                   IDS_VIV_OPEN_LINK_NEW_PRIVATE_WINDOW);
    if (!IsVivaldiWebPanel(web_contents)) {
      menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
      menu->InsertItemWithStringIdAt(++index, IDC_VIV_BOOKMARK_LINK,
                                     IDS_VIV_BOOKMARK_LINK);
      menu->InsertItemWithStringIdAt(++index, IDC_VIV_ADD_LINK_TO_WEBPANEL,
                                     IDS_VIV_ADD_LINK_TO_WEBPANEL);
#if !defined(OFFICIAL_BUILD)
      if (params.selection_text.empty()) {
        menu->InsertItemWithStringIdAt(++index, IDC_VIV_SEND_LINK_BY_MAIL,
                                       IDS_VIV_SEND_LINK_BY_MAIL);
      }
#endif
    }
  }
}

void VivaldiAddImageItems(SimpleMenuModel* menu,
                          WebContents* web_contents,
                          const ContextMenuParams& params) {
  if (IsVivaldiRunning()) {
    int index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB);
    DCHECK_GE(index, 0);
    menu->RemoveItemAt(index);

    menu->InsertItemWithStringIdAt(index, IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB,
                                   IDS_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB);
    menu->InsertItemWithStringIdAt(++index,
                                   IDC_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB,
                                   IDS_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB);
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_OPEN_IMAGE_CURRENT_TAB,
                                   IDS_VIV_OPEN_IMAGE_CURRENT_TAB);
    menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_OPEN_IMAGE_NEW_WINDOW,
                                   IDS_VIV_OPEN_IMAGE_NEW_WINDOW);
    menu->InsertItemWithStringIdAt(++index,
                                   IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW,
                                   IDS_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW);
    menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);

    index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION);
    DCHECK_GE(index, 0);
    menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    if (!IsVivaldiWebPanel(web_contents)) {
      menu->InsertItemWithStringIdAt(++index, IDC_VIV_INSPECT_IMAGE,
                                     IDS_VIV_INSPECT_IMAGE);
      menu->InsertItemWithStringIdAt(++index, IDC_VIV_USE_IMAGE_AS_BACKGROUND,
                                     IDS_VIV_USE_IMAGE_AS_BACKGROUND);
    }
    menu->InsertItemWithStringIdAt(++index, IDC_CONTENT_CONTEXT_RELOADIMAGE,
                                   IDS_CONTENT_CONTEXT_RELOADIMAGE);
  }
}

void VivaldiAddCopyItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params) {
  if (IsVivaldiRunning() && WebViewGuest::FromWebContents(web_contents) &&
      !IsVivaldiWebPanel(web_contents)) {
    menu->AddItemWithStringId(IDC_VIV_COPY_TO_NOTE, IDS_VIV_COPY_TO_NOTE);
#if !defined(OFFICIAL_BUILD)
    menu->AddItemWithStringId(IDC_VIV_SEND_SELECTION_BY_MAIL,
                              IDS_VIV_SEND_BY_MAIL);
#endif
  }
}

void VivaldiAddPageItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params) {
  if (IsVivaldiRunning() && WebViewGuest::FromWebContents(web_contents) &&
      !IsVivaldiWebPanel(web_contents) && !IsVivaldiMail(web_contents)) {
    int index = menu->GetIndexOfCommandId(IDC_SAVE_PAGE);
    DCHECK_GE(index, 0);
    menu->InsertItemWithStringIdAt(index, IDC_VIV_BOOKMARK_PAGE,
                                   IDS_VIV_BOOKMARK_PAGE);
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_ADD_PAGE_TO_WEBPANEL,
                                   IDS_VIV_ADD_PAGE_TO_WEBPANEL);
#if !defined(OFFICIAL_BUILD)
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_SEND_PAGE_BY_MAIL,
                                   IDS_VIV_SEND_BY_MAIL);
#endif
    menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    index = menu->GetIndexOfCommandId(IDC_ROUTE_MEDIA);
    DCHECK_GE(index, 0);
    menu->InsertItemWithStringIdAt(++index, IDC_VIV_COPY_PAGE_ADDRESS,
                                   IDS_VIV_COPY_PAGE_ADDRESS);
  }
}

void VivaldiAddEditableItems(SimpleMenuModel* menu,
                             const ContextMenuParams& params) {
  if (IsVivaldiRunning()) {
    const bool use_paste_and_match_style =
        params.input_field_type !=
            blink::WebContextMenuData::kInputFieldTypePlainText &&
        params.input_field_type !=
            blink::WebContextMenuData::kInputFieldTypePassword;

    int index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE);
    DCHECK_GE(index, 0);

    if (params.vivaldi_input_type == "vivaldi-addressfield" ||
        params.vivaldi_input_type == "vivaldi-searchfield") {
      menu->InsertItemWithStringIdAt(++index, IDC_CONTENT_CONTEXT_PASTE_AND_GO,
                                     IDS_CONTENT_CONTEXT_PASTE_AND_GO);
    }

    index =
        menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
    if (use_paste_and_match_style) {
      DCHECK_GE(index, 0);
      menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    } else if (params.misspelled_word.empty()) {
      DCHECK_GE(index, 0);
      menu->RemoveItemAt(index);
      menu->InsertSeparatorAt(index, ui::NORMAL_SEPARATOR);
    }

    if (!params.vivaldi_keyword_url.is_empty()) {
      if (params.misspelled_word.empty()) {
        index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_SELECTALL);
      } else {
        index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE);
      }

      DCHECK_GE(index, 0);
      menu->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
      menu->InsertItemWithStringIdAt(++index,
                                     IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE,
                                     IDS_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE);
    }
  }
}

void VivaldiAddDeveloperItems(SimpleMenuModel* menu,
                              WebContents* web_contents,
                              const ContextMenuParams& params) {
  if (IsVivaldiRunning() && WebViewGuest::FromWebContents(web_contents) &&
      !IsVivaldiWebPanel(web_contents) && !IsVivaldiMail(web_contents)) {
    if (params.src_url.is_empty() && params.link_url.is_empty()) {
      int index = menu->GetIndexOfCommandId(IDC_VIEW_SOURCE);
      if (index == -1) {
        index = menu->GetIndexOfCommandId(IDC_CONTENT_CONTEXT_INSPECTELEMENT);
      }
      DCHECK_GE(index, 0);
      menu->InsertItemWithStringIdAt(index, IDC_VIV_VALIDATE_PAGE,
                                     IDS_VIV_VALIDATE_PAGE);
    }
  }
}

void VivaldiAddFullscreenItems(SimpleMenuModel* menu,
                               WebContents* web_contents,
                               const ContextMenuParams& params) {
  if (IsVivaldiRunning() && WebViewGuest::FromWebContents(web_contents) &&
      !IsVivaldiWebPanel(web_contents) && !IsVivaldiMail(web_contents)) {
    menu->AddSeparator(ui::NORMAL_SEPARATOR);
    menu->AddItemWithStringId(IDC_VIV_FULLSCREEN, IDS_VIV_FULLSCREEN);
    menu->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

bool IsVivaldiCommandIdEnabled(const SimpleMenuModel& menu,
                               const ContextMenuParams& params,
                               int id,
                               bool* enabled) {
  switch (id) {
    default:
      return false;

    case IDC_VIV_OPEN_LINK_CURRENT_TAB:
    case IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB:
      *enabled = params.link_url.is_valid();
      break;
    case IDC_CONTENT_CONTEXT_RELOADIMAGE:
    case IDC_VIV_OPEN_IMAGE_CURRENT_TAB:
    case IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB:
    case IDC_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB:
    case IDC_VIV_OPEN_IMAGE_NEW_WINDOW:
    case IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW:
      *enabled =
          params.has_image_contents == WebContextMenuData::kMediaTypeImage;
      break;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO: {
      if (!(params.edit_flags & WebContextMenuData::kCanPaste)) {
        *enabled = false;
        break;
      }

      std::vector<base::string16> types;
      bool ignore;
      ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &types, &ignore);
      *enabled = !types.empty();
      break;
    }

    case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE:
      *enabled = !params.vivaldi_keyword_url.is_empty();
      break;

    case IDC_VIV_COPY_TO_NOTE:
      *enabled = !params.selection_text.empty();
      break;

    case IDC_VIV_BOOKMARK_PAGE:
    case IDC_VIV_BOOKMARK_LINK:
    case IDC_VIV_ADD_PAGE_TO_WEBPANEL:
    case IDC_VIV_ADD_LINK_TO_WEBPANEL:
    case IDC_VIV_SEND_LINK_BY_MAIL:
    case IDC_VIV_SEND_PAGE_BY_MAIL:
    case IDC_VIV_SEND_SELECTION_BY_MAIL:
    case IDC_VIV_COPY_PAGE_ADDRESS:
    case IDC_VIV_INSPECT_IMAGE:
    case IDC_VIV_USE_IMAGE_AS_BACKGROUND:
    case IDC_VIV_VALIDATE_PAGE:
    case IDC_VIV_FULLSCREEN:
      *enabled = true;
      break;
  }
  return true;
}

bool VivaldiExecuteCommand(RenderViewContextMenu* context_menu,
                           const ContextMenuParams& params,
                           WebContents* source_web_contents,
                           int event_flags,
                           int id,
                           const OpenURLCall& openurl) {
  switch (id) {
    default:
      return false;

    case IDC_VIV_OPEN_LINK_CURRENT_TAB:
      openurl.Run(params.link_url, GetDocumentURL(params),
                  WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB:
      openurl.Run(params.link_url, GetDocumentURL(params),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB,
                  ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_CONTENT_CONTEXT_RELOADIMAGE:
    {
      // params.x and params.y position the context menu and are always in root
      // root coordinates. Convert to content coordinates.
      gfx::PointF p = source_web_contents->GetRenderViewHost()->GetWidget()->
          GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(
              params.x, params.y));
      source_web_contents->GetRenderViewHost()->LoadImageAt(
          static_cast<int>(p.x()), static_cast<int>(p.y()));
      break;
    }
    case IDC_VIV_OPEN_IMAGE_CURRENT_TAB:
      openurl.Run(params.src_url, GetDocumentURL(params),
                  WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_FOREGROUND_TAB:
      openurl.Run(params.src_url, GetDocumentURL(params),
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_BACKGROUND_TAB:
      openurl.Run(params.src_url, GetDocumentURL(params),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB,
                  ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_WINDOW:
      openurl.Run(params.src_url, GetDocumentURL(params),
                  WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_VIV_OPEN_IMAGE_NEW_PRIVATE_WINDOW:
      openurl.Run(params.src_url, GetDocumentURL(params),
                  WindowOpenDisposition::OFF_THE_RECORD,
                  ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO:
      if (IsVivaldiRunning()) {
        base::string16 text;
        ui::Clipboard::GetForCurrentThread()->ReadText(
            ui::CLIPBOARD_TYPE_COPY_PASTE, &text);
        base::string16 target;
        if (params.vivaldi_input_type == "vivaldi-addressfield")
          target = base::ASCIIToUTF16("url");
        else if (params.vivaldi_input_type == "vivaldi-searchfield")
          target = base::ASCIIToUTF16("search");
        base::string16 modifiers = GetModifierStringFromEventFlags(event_flags);
        base::ListValue* args = new base::ListValue;
        args->Append(std::make_unique<base::Value>(text));
        args->Append(std::make_unique<base::Value>(target));
        args->Append(std::make_unique<base::Value>(modifiers));
        extensions::WebViewGuest* current_webviewguest =
            vivaldi::ui_tools::GetActiveWebViewGuest();
        if (current_webviewguest) {
          current_webviewguest->PasteAndGo(*args);
        }
      }
      break;

    case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE: {
      base::string16 keyword(TemplateURL::GenerateKeyword(params.page_url));
      WebViewGuest* vivGuestView =
          WebViewGuest::FromWebContents(source_web_contents);
      base::ListValue* args = new base::ListValue;
      args->Append(std::make_unique<base::Value>(keyword));
      args->Append(
          std::make_unique<base::Value>(params.vivaldi_keyword_url.spec()));

      vivGuestView->CreateSearch(*args);
    } break;

    case IDC_VIV_COPY_TO_NOTE:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "copyToNote",
                         s_selection_text, params.page_url.spec());
      }
      break;

    case IDC_VIV_BOOKMARK_PAGE:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags,
                         "addActivePageToBookmarks");
      }
      break;

    case IDC_VIV_BOOKMARK_LINK:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "addUrlToBookmarks",
                         params.link_text, params.link_url.spec());
      }
      break;

    case IDC_VIV_ADD_PAGE_TO_WEBPANEL:
    case IDC_VIV_ADD_LINK_TO_WEBPANEL:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "addUrlToWebPanel",
                         base::ASCIIToUTF16(""),
                         id == IDC_VIV_ADD_PAGE_TO_WEBPANEL
                             ? params.page_url.spec()
                             : params.link_url.spec());
      }
      break;

    case IDC_VIV_SEND_PAGE_BY_MAIL:
    case IDC_VIV_SEND_LINK_BY_MAIL:
      if (IsVivaldiRunning()) {
        SendSimpleAction(
            source_web_contents, event_flags, "sendLinkByMail",
            id == IDC_VIV_SEND_PAGE_BY_MAIL ? source_web_contents->GetTitle()
                                            : params.link_text,
            id == IDC_VIV_SEND_PAGE_BY_MAIL ? params.page_url.spec()
                                            : params.link_url.spec());
      }
      break;

    case IDC_VIV_SEND_SELECTION_BY_MAIL:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags,
                         "sendSelectionByMail", params.selection_text,
                         base::UTF16ToUTF8(source_web_contents->GetTitle()));
      }
      break;

    case IDC_VIV_COPY_PAGE_ADDRESS:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "copyUrlToClipboard",
                         base::ASCIIToUTF16(""), params.page_url.spec());
      }
      break;

    case IDC_VIV_INSPECT_IMAGE:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "inspectImageUrl",
                         base::ASCIIToUTF16(""), params.src_url.spec());
      }
      break;

    case IDC_VIV_USE_IMAGE_AS_BACKGROUND:
      if (IsVivaldiRunning()) {
        // The app does not have access to file:// schemes, so handle it
        // differently.
        if (params.src_url.SchemeIs(url::kFileScheme)) {
          // PathExists() triggers IO restriction.
          base::ThreadRestrictions::ScopedAllowIO allow_io;

          // Strip any url encoding.
          std::string filename = net::UnescapeURLComponent(
              params.src_url.GetContent(),
              net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
                  net::UnescapeRule::REPLACE_PLUS_WITH_SPACE |
                  net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
#if defined(OS_POSIX)
          base::FilePath path(filename);
#elif defined(OS_WIN)
          base::FilePath path(base::UTF8ToWide(filename));
#endif
          if (!base::PathExists(path)) {
            if (filename[0] == '/') {
              // It might be in a format /D:/somefile.png so strip the first
              // slash.
              filename = filename.substr(1, std::string::npos);
            }
          }
          SendSimpleAction(source_web_contents, event_flags,
                           "useLocalImageAsBackground", base::ASCIIToUTF16(""),
                           filename);
        } else {
          SendSimpleAction(source_web_contents, event_flags,
                           "useImageAsBackground", base::ASCIIToUTF16(""),
                           params.src_url.spec());
        }
      }
      break;

    case IDC_VIV_VALIDATE_PAGE:
      if (IsVivaldiRunning()) {
        SendSimpleAction(source_web_contents, event_flags, "validateUrl",
                         base::ASCIIToUTF16(""), params.page_url.spec());
      }
      break;

    case IDC_VIV_FULLSCREEN:
      SendSimpleAction(source_web_contents, event_flags, "fullscreen");
      break;
  }
  return true;
}

// Returns true if an accelerator has been asigned. The accelerator can be empty
// meaning we do not want an accelerator.
bool VivaldiGetAcceleratorForCommandId(const RenderViewContextMenuViews* view,
                                       int command_id,
                                       ui::Accelerator* accel) {
  // TODO(espen@vivaldi.com): We can not get away with hardcoded shortcuts in
  // in menus in chromium code as long as we do not have synchronous access to
  // our settings. We can either turn them off or let them be the same as as the
  // default values in vivaldi.
  if (!IsVivaldiRunning())
    return false;

  switch (command_id) {
    case IDC_BACK:
      *accel = ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_REDO:
      *accel = ui::Accelerator(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_CUT:
      *accel = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_COPY:
      *accel = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      *accel =
          ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator();
      return true;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO:
      *accel =
          ui::Accelerator(ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_ROTATECCW:
      *accel = ui::Accelerator(ui::VKEY_OEM_4, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_ROTATECW:
      *accel = ui::Accelerator(ui::VKEY_OEM_6, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_FORWARD:
      *accel = ui::Accelerator(ui::VKEY_RIGHT, ui::EF_ALT_DOWN);
      return true;
    case IDC_PRINT:
      *accel = ui::Accelerator(ui::VKEY_P, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_RELOAD:
      *accel = ui::Accelerator(ui::VKEY_R, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_SAVE_PAGE:
      *accel = ui::Accelerator(ui::VKEY_S, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_VIEW_SOURCE:
      *accel = ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_VIV_COPY_TO_NOTE:
      *accel =
          ui::Accelerator(ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_VIV_BOOKMARK_PAGE:
      *accel = ui::Accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_VIV_FULLSCREEN:
      *accel = ui::Accelerator(ui::VKEY_F11, ui::EF_NONE);
      return true;

    default:
      return false;
  }
}

}  // namespace vivaldi
