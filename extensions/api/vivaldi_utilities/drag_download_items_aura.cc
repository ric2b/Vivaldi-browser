// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/vivaldi_utilities/drag_download_items.h"

#include <string>

#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "net/base/mime_util.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/button_drag_utils.h"
#include "url/gurl.h"

namespace extensions {

void DragDownloadItems(std::vector<DraggableDownloadItem> downloads,
                       gfx::NativeView view) {
  if (downloads.empty()) {
    return;
  }
  // Set up our OLE machinery.
  auto data = std::make_unique<ui::OSExchangeData>();
  std::vector<ui::FileInfo> file_infos;

  aura::Window* root_window = view->GetRootWindow();
  if (!root_window || !aura::client::GetDragDropClient(root_window))
    return;

  std::u16string title;
  gfx::ImageSkia drag_icon;
  for (const auto [item, icon] : downloads) {
    DCHECK(item);
    DCHECK_EQ(download::DownloadItem::COMPLETE, item->GetState());

    base::FilePath full_path = item->GetTargetFilePath();
    file_infos.emplace_back(full_path, item->GetFileNameToReportUser());

    title.append(
        item->GetFileNameToReportUser().BaseName().LossyDisplayName().append(
            u" "));
    drag_icon = icon ? icon->AsImageSkia() : drag_icon;
  }

  data->SetFilenames(file_infos);

  button_drag_utils::SetDragImage(GURL(), title, drag_icon, nullptr,
                                  data.get());

  gfx::Point location = display::Screen::GetScreen()->GetCursorScreenPoint();
  aura::client::GetDragDropClient(root_window)
      ->StartDragAndDrop(std::move(data), root_window, view, location,
                         ui::DragDropTypes::DRAG_MOVE |
                             ui::DragDropTypes::DRAG_COPY |
                             ui::DragDropTypes::DRAG_LINK,
                         ui::mojom::DragEventSource::kMouse);
}

}  // namespace extensions
