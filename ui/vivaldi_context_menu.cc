#include "ui/vivaldi_context_menu.h"

#include "base/base64.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/render_widget_host_view.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/favicon_size.h"

#define ICON_SIZE 16

namespace vivaldi {

ui::ImageModel GetImageModel(const std::string icon) {
  if (icon.length() > 0) {
    std::string png_data;
    if (base::Base64Decode(icon, &png_data)) {
      gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
          base::span(reinterpret_cast<const unsigned char*>(png_data.c_str()),
          png_data.length()));
      if (img.Width() > ICON_SIZE || img.Height() > ICON_SIZE) {
        int width = img.Width();
        int height = img.Height();
        gfx::CalculateFaviconTargetSize(&width, &height);
        SkBitmap bitmap(*img.ToSkBitmap());
        img = gfx::Image::CreateFrom1xBitmap(skia::ImageOperations::Resize(
            bitmap, skia::ImageOperations::RESIZE_GOOD, width, height));
      }
      return ui::ImageModel::FromImage(img);
    }
  }
  return ui::ImageModel();
}

void ConvertMenubarButtonRectToScreen(content::WebContents* web_contents,
                                      MenubarMenuParams& bar_params) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      VivaldiMenu::GetActiveNativeViewFromWebContents(web_contents));
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(widget->GetContentsView(), &screen_loc);
  for (MenubarMenuEntry& e : bar_params.siblings) {
    gfx::Point point(e.rect.origin());
    point.Offset(screen_loc.x(), screen_loc.y());
    e.rect.set_origin(point);
  }
}

// static
gfx::NativeView VivaldiMenu::GetActiveNativeViewFromWebContents(
    content::WebContents* web_contents) {
  // We used to test for a fullscreen view pre ch88, but that function got
  // removed (WebContents::GetFullscreenRenderWidgetHostView()) with 88. Seems
  // it is no longer required but keeping this wrapper function for a while in
  // case that turns out to be wrong.
  return web_contents->GetNativeView();
}

// static
views::Widget* VivaldiMenu::GetTopLevelWidgetFromWebContents(
    content::WebContents* web_contents) {
  return views::Widget::GetTopLevelWidgetForNativeView(
      GetActiveNativeViewFromWebContents(web_contents));
}

bool VivaldiContextMenu::HasDarkTextColor() {
  return true;
}

BookmarkMenuContainer::BookmarkMenuContainer(Delegate* a_delegate)
    : edge(Below), delegate(a_delegate) {}

BookmarkMenuContainer::~BookmarkMenuContainer() {}

MenubarMenuParams::MenubarMenuParams() {}

MenubarMenuParams::~MenubarMenuParams() {}

MenubarMenuEntry* MenubarMenuParams::GetSibling(int id) {
  for (::vivaldi::MenubarMenuEntry& sibling : siblings) {
    if (sibling.id == id) {
      return &sibling;
    }
  }
  return nullptr;
}

}  // namespace vivaldi
