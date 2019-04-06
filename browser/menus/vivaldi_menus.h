// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_MENUS_VIVALDI_MENUS_H_
#define BROWSER_MENUS_VIVALDI_MENUS_H_

#include "base/callback.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

class RenderViewContextMenuViews;
using ui::SimpleMenuModel;
using content::ContextMenuParams;
using content::WebContents;

namespace vivaldi {
typedef base::Callback<void(const GURL&,
                            const GURL&,
                            WindowOpenDisposition disposition,
                            ui::PageTransition transition)>
    OpenURLCall;
bool IsVivaldiCommandId(int id);
void VivaldiInitMenu(WebContents* web_contents,
                     const ContextMenuParams& params);
void VivaldiAddLinkItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params);
void VivaldiAddImageItems(SimpleMenuModel* menu,
                          WebContents* web_contents,
                          const ContextMenuParams& params);
void VivaldiAddCopyItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params);
void VivaldiAddPageItems(SimpleMenuModel* menu,
                         WebContents* web_contents,
                         const ContextMenuParams& params);
void VivaldiAddEditableItems(SimpleMenuModel* menu,
                             const ContextMenuParams& params);
void VivaldiAddDeveloperItems(SimpleMenuModel* menu,
                              WebContents* web_contents,
                              const ContextMenuParams& params);
void VivaldiAddFullscreenItems(SimpleMenuModel* menu,
                               WebContents* web_contents,
                               const ContextMenuParams& params);
bool IsVivaldiCommandIdEnabled(const SimpleMenuModel& menu,
                               const ContextMenuParams& params,
                               int id,
                               bool* enabled);
bool VivaldiExecuteCommand(RenderViewContextMenu* context_menu,
                           const ContextMenuParams& params,
                           WebContents* source_web_contents_,
                           int event_flags,
                           int id,
                           const OpenURLCall& openurl);
bool VivaldiGetAcceleratorForCommandId(const RenderViewContextMenuViews* view,
                                       int command_id,
                                       ui::Accelerator* accel);
}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_MENUS_H_
