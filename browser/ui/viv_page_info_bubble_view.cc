// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

// Whole function added by Vivaldi
// static
void PageInfoBubbleView::ShowPopupAtPos(gfx::Point anchor_pos,
  Profile* profile,
  content::WebContents* web_contents,
  const GURL& url,
  const security_state::SecurityInfo& security_info,
  Browser* browser,
  gfx::NativeView parent) {
  gfx::Rect anchor_rect = gfx::Rect();
  PageInfoBubbleView* thispopup = new PageInfoBubbleView(
    nullptr, anchor_rect, parent,
    profile, web_contents, url, security_info);
  thispopup->SetAnchorRect(gfx::Rect(anchor_pos, gfx::Size()));
  thispopup->GetWidget()->Show();
}
