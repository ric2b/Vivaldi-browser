// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "ui/content/vivaldi_tab_check.h"

#include "app/vivaldi_apptools.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/web_contents.h"
#include "extensions/helper/vivaldi_panel_helper.h"

const int VivaldiTabCheck::kVivaldiTabObserverContextKey = 0;
const int VivaldiTabCheck::kDevToolContextKey = 0;
const int VivaldiTabCheck::kVivaldiPanelHelperContextKey = 0;

// static
bool VivaldiTabCheck::IsVivaldiTab(content::WebContents* web_contents) {
  return web_contents->GetUserData(&kVivaldiTabObserverContextKey);
}

bool VivaldiTabCheck::IsVivaldiPanel(content::WebContents* web_contents) {
  return web_contents->GetUserData(&kVivaldiPanelHelperContextKey);
}

// static
content::WebContents* VivaldiTabCheck::GetOuterVivaldiTab(
    content::WebContents* web_contents) {
  while (web_contents && !IsVivaldiTab(web_contents)) {
    web_contents = web_contents->GetOuterWebContents();
  }
  return web_contents;
}

// static
bool VivaldiTabCheck::IsOwnedByTabStripOrDevTools(
    content::WebContents* web_contents) {
  return IsVivaldiTab(web_contents) ||
         web_contents->GetUserData(&kDevToolContextKey);
}

// static
bool VivaldiTabCheck::IsOwnedByDevTools(
    content::WebContents* web_contents) {
  return web_contents->GetUserData(&kDevToolContextKey);
}

// static
void VivaldiTabCheck::MarkAsDevToolContents(
    content::WebContents* web_contents) {
  DCHECK(!IsVivaldiTab(web_contents));
  web_contents->SetUserData(&kDevToolContextKey,
                            std::make_unique<base::SupportsUserData::Data>());
}
