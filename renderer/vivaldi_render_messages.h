// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
// Multiply-included file, no traditional include guard.

#ifndef RENDER_VIVALDI_RENDER_MESSAGES_H_
#define RENDER_VIVALDI_RENDER_MESSAGES_H_

#include <string>

#include "base/memory/shared_memory_handle.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/size.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START VivaldiMsgStart

IPC_STRUCT_BEGIN(VivaldiViewMsg_RequestThumbnailForFrame_Params)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(bool, full_page)
  IPC_STRUCT_MEMBER(int, callback_id)
IPC_STRUCT_END()

IPC_MESSAGE_ROUTED1(VivaldiMsg_InsertText, base::string16)

IPC_MESSAGE_ROUTED4(VivaldiMsg_DidUpdateFocusedElementInfo,
                    std::string,
                    std::string,
                    bool,
                    std::string)

// Asks the renderer for a snapshot of web page. If |full_page| is true,
// the full page is captured and not scaled down, |size|
// is then ignored.  If |full_page| is false, only the visible part of the page
// is captured and scaled.
// The possibly downsampled image will be
// returned in a VivaldiViewHostMsg_RequestThumbnailForFrame_ACK message.
IPC_MESSAGE_ROUTED1(VivaldiViewMsg_RequestThumbnailForFrame,
                    VivaldiViewMsg_RequestThumbnailForFrame_Params)

// Responds to the request for a thumbnail.
// Thumbnail data will be empty if a thumbnail could not be produced.
IPC_MESSAGE_ROUTED4(VivaldiViewHostMsg_RequestThumbnailForFrame_ACK,
                    base::SharedMemoryHandle /* handle */,
                    gfx::Size /* original size of the image */,
                    int /* ID of the callback */,
                    bool /* true if success */)

IPC_MESSAGE_ROUTED0(VivaldiFrameHostMsg_ResumeParser)

#endif // RENDER_VIVALDI_RENDER_MESSAGES_H_
