// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/vivaldi_utilities/drag_download_items.h"

#import <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "components/download/public/common/download_item.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

// Used for offsetting a position of multiple drag images
static const int kDownloadItemXOffset = 8;
static const int kDownloadItemYOffset = 16;

// Cocoa intends a smart dragging source, while `DragDownloadItem()` is a simple
// "start dragging this" fire-and-forget. This is a generic source just good
// enough to satisfy AppKit.
@interface VivaldiDragDownloadItemSource : NSObject <NSDraggingSource>
@end

@implementation VivaldiDragDownloadItemSource

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return NSDragOperationEvery;
}

@end

namespace {
id<NSDraggingSource> GetDraggingSource() {
  static id<NSDraggingSource> source =
      [[VivaldiDragDownloadItemSource alloc] init];
  return source;
}
}  // namespace

namespace extensions {

void DragDownloadItems(std::vector<DraggableDownloadItem> downloads,
                       gfx::NativeView native_view) {
  if (downloads.empty()) {
    return;
  }
  DCHECK(native_view);
  NSView* view = native_view.GetNativeNSView();
  NSPoint current_position = view.window.mouseLocationOutsideOfEventStream;
  current_position =
      [view backingAlignedRect:NSMakeRect(current_position.x,
                                          current_position.y, 0, 0)
                       options:NSAlignAllEdgesOutward]
          .origin;

  // If this drag was initiated from a views::Widget, that widget may have
  // mouse capture. Drags via View::DoDrag() usually release it. The code below
  // bypasses that, so release manually. See https://crbug.com/863377.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(view);
  if (widget)
    widget->ReleaseCapture();

  NSMutableArray* file_items = [[NSMutableArray alloc] init];
  int x_offset = 0;
  int y_offset = 0;
  for (const auto& [item, icon] : downloads) {
    DCHECK(item);
    DCHECK_EQ(download::DownloadItem::COMPLETE, item->GetState());

    NSURL* file_url = base::apple::FilePathToNSURL(item->GetTargetFilePath());
    NSDraggingItem* file_item =
        [[NSDraggingItem alloc] initWithPasteboardWriter:file_url];

    if (icon) {
      NSImage* file_image = icon->ToNSImage();
      NSSize image_size = file_image.size;
      NSRect image_rect =
          NSMakeRect((current_position.x - image_size.width / 2) + x_offset,
                     (current_position.y - image_size.height / 2) - y_offset,
                     image_size.width, image_size.height);
      [file_item setDraggingFrame:image_rect contents:file_image];
    } else {
      // 16x16 placeholder, corresponding to IconLoader::IconSize::SMALL.
      NSRect placeholder_rect =
          NSMakeRect(current_position.x - 8, current_position.y - 8, 16, 16);
      [file_item setDraggingFrame:placeholder_rect contents:nil];
    }

    x_offset += kDownloadItemXOffset;
    y_offset += kDownloadItemYOffset;

    [file_items addObject:file_item];
  }
  // Synthesize a drag event, since we don't have access to the actual event
  // that initiated a drag (possibly consumed by the Web UI, for example).
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
                                          location:current_position
                                     modifierFlags:0
                                         timestamp:NSApp.currentEvent.timestamp
                                      windowNumber:view.window.windowNumber
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  // Run the drag operation.
  [view beginDraggingSessionWithItems:file_items
                                event:dragEvent
                               source:GetDraggingSource()];
}

}  // namespace extensions