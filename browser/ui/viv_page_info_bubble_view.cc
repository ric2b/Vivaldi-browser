// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

void OnPageInfoBubbleClosed(views::Widget::ClosedReason closed_reason,
                            bool reload_prompt) {}

// Whole function added by Vivaldi
// static
void PageInfoBubbleView::ShowPopupAtPos(gfx::Point anchor_pos,
                                        Profile* profile,
                                        content::WebContents* web_contents,
                                        const GURL& url,
                                        Browser* browser,
                                        gfx::NativeView parent) {
  gfx::Rect anchor_rect = gfx::Rect();
  PageInfoBubbleView* thispopup = new PageInfoBubbleView(
      nullptr, anchor_rect, parent, web_contents, url, base::DoNothing(),
      base::BindOnce(&OnPageInfoBubbleClosed), false);
  thispopup->SetAnchorRect(gfx::Rect(anchor_pos, gfx::Size()));
  thispopup->GetWidget()->Show();
}
