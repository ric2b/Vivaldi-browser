// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/menus/vivaldi_menus.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_resources.h"

#include "browser/menus/vivaldi_menu_enums.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"
#include "chrome/common/pref_names.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "ui/base/clipboard/clipboard.h"

using blink::WebContextMenuData;
using extensions::WebViewGuest;

namespace vivaldi {
bool IsVivaldiCommandId(int id) {
  return (id >= IDC_VIVALDI_MENU_ENUMS_START &&
          id < IDC_VIVALDI_MENU_ENUMS_END);
}

const GURL& GetDocumentURL(const ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

void VivaldiAddLinkItems(SimpleMenuModel &menu,
                         const ContextMenuParams &params) {

  int index = menu.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  DCHECK(index>=0);
  menu.InsertItemWithStringIdAt(++index,
                               IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB,
                               IDS_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB);
}


void VivaldiAddImageItems(SimpleMenuModel &menu,
                          const ContextMenuParams &params) {
  menu.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOADIMAGE,
                                  IDS_CONTENT_CONTEXT_RELOADIMAGE);
}

void VivaldiAddEditableItems(SimpleMenuModel &menu,
                             const ContextMenuParams &params) {
  const bool use_spellcheck_and_search = !chrome::IsRunningInForcedAppMode();
  const bool use_paste_and_match_style =
    params.input_field_type !=
        blink::WebContextMenuData::InputFieldTypePlainText &&
    params.input_field_type !=
        blink::WebContextMenuData:: InputFieldTypePassword;

  int index = menu.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE);
  DCHECK(index>=0);

 if (params.isVivaldiAddressfield){
    menu.InsertItemWithStringIdAt(++index,
                                  IDC_CONTENT_CONTEXT_PASTE_AND_GO,
                                  IDS_CONTENT_CONTEXT_PASTE_AND_GO);
  }

  if (IsVivaldiRunning()) {
    index = menu.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
    DCHECK(index>=0);
    if (use_paste_and_match_style) {
      menu.InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
    } else {
      menu.RemoveItemAt(index);
      menu.InsertSeparatorAt(index, ui::NORMAL_SEPARATOR);
    }
  }

  index = menu.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_SELECTALL);
  DCHECK(index>=0);

  if (IsVivaldiRunning()) {
    if (use_spellcheck_and_search && !params.keyword_url.is_empty()) {
      menu.InsertSeparatorAt(++index,ui::NORMAL_SEPARATOR);
      menu.InsertItemWithStringIdAt(++index,
                                 IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE,
                                 IDS_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE);
    }
  }
}

bool IsVivaldiCommandIdEnabled(const SimpleMenuModel &menu,
                               const ContextMenuParams &params,
                               int id,
                               bool &enabled) {
  switch(id) {
    default:
      return false;

    case IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB:
      enabled = params.link_url.is_valid();
      break;
    case IDC_CONTENT_CONTEXT_RELOADIMAGE:
      enabled = params.has_image_contents == WebContextMenuData::MediaTypeImage;
      break;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO: {
      if (!(params.edit_flags & WebContextMenuData::CanPaste)) {
        enabled = false;
        break;
      }

      std::vector<base::string16> types;
      bool ignore;
      ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
        ui::CLIPBOARD_TYPE_COPY_PASTE, &types, &ignore);
      enabled = !types.empty();
      break;
    }

    case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE:
      enabled = !params.keyword_url.is_empty();
      break;

  }
  return true;
}

bool VivaldiExecuteCommand(RenderViewContextMenu* context_menu,
                           const ContextMenuParams& params,
                           WebContents* source_web_contents,
                           int id,
                           const OpenURLCall& openurl) {
  switch(id) {
    default:
      return false;

    case IDC_CONTENT_CONTEXT_OPENLINKBACKGROUNDTAB:
      openurl.Run(params.link_url,
        GetDocumentURL(params),
        NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_LINK);
      break;
    case IDC_CONTENT_CONTEXT_RELOADIMAGE:
      source_web_contents->GetRenderViewHost()->LoadImageAt(
        params.x, params.y);
      break;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO:
      if (IsVivaldiRunning()) {
        base::string16 text;
        ui::Clipboard::GetForCurrentThread()->ReadText(
            ui::CLIPBOARD_TYPE_COPY_PASTE, &text);

        base::ListValue* args = new base::ListValue;
        args->Append(new base::StringValue(text));
        DCHECK(s_current_webviewguest);
        s_current_webviewguest->PasteAndGo(*args);
      }
      break;
    case IDC_VIV_CONTENT_CONTEXT_ADDSEARCHENGINE: {
      base::string16 keyword(TemplateURL::GenerateKeyword(
          params.page_url, prefs::kAcceptLanguages));
      WebViewGuest *vivGuestView =
          WebViewGuest::FromWebContents(source_web_contents);
      base::ListValue *args = new base::ListValue;
      args->Append(new base::StringValue(keyword));
      args->Append(new base::StringValue(params.keyword_url.spec()));

      vivGuestView->CreateSearch(*args);

      break;
    }
  }
  return true;
}

// Returns true if an accelerator has been asigned. The accelerator can be empty
// meaning we do not want an accelerator.
bool VivaldiGetAcceleratorForCommandId(
    const RenderViewContextMenuViews* view,
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
      *accel = ui::Accelerator(ui::VKEY_Y,ui::EF_CONTROL_DOWN);
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
      *accel = ui::Accelerator(ui::VKEY_I,
                                ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;
    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator();
      return true;
    case IDC_CONTENT_CONTEXT_PASTE_AND_GO:
      *accel = ui::Accelerator(ui::VKEY_V,
                                ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
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
    default:
      return false;
  }
}


} // namespace vivaldi


