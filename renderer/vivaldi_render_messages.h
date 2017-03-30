// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
// Multiply-included file, no traditional include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START VivaldiMsgStart

IPC_MESSAGE_ROUTED1(VivaldiMsg_InsertText, base::string16)
IPC_MESSAGE_ROUTED3(VivaldiMsg_SetPinchZoom,
                    float /* scale */,
                    int /* x */,
                    int /* y */)
