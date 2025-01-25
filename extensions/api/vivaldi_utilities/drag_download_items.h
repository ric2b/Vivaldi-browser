// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_DRAG_DOWNLOAD_ITEMS_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_DRAG_DOWNLOAD_ITEMS_H_

#include "ui/gfx/native_widget_types.h"

#include <vector>

namespace download {
class DownloadItem;
}

namespace gfx {
class Image;
}

namespace extensions {

struct DraggableDownloadItem {
  const download::DownloadItem* item;
  const gfx::Image* icon;
};

// Helper function for download views to use when acting as a drag source for a
// vector of DraggableDownloadItems.
// On Aura only one DraggableDownloadItem `icon` is going to be used (if any)
// and `downloads` item names are going to be concatenated into a title.
// On macOS function constructs a cascade of downloadable items with icons and
// paths, if no `icon` is specified for an item, the operating system will use
// the default icon.
void DragDownloadItems(std::vector<DraggableDownloadItem> downloads,
                       gfx::NativeView view);

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_DRAG_DOWNLOAD_ITEMS_H_
