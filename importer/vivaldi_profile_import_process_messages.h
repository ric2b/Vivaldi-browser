// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

// Multiply-included message file, no traditonal include guard.
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/values.h"
#include "importer/profile_vivaldi_import_process_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

// Force multiple inclusion of the param traits file to generate all methods.
#undef VIVALDI_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_

#define IPC_MESSAGE_START ProfileImportMsgStart

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_NotifyNotesImportStart,
                     base::string16  /* first folder name */,
                     int       /* total number of notes */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyNotesImportGroup,
                     std::vector<ImportedNotesEntry>)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifySpeedDialImportStart,
                     int       /* total number of speed dials */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifySpeedDialImportGroup,
                     std::vector<ImportedSpeedDialEntry>)

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_ImportItem_Failed,
                     int  /* ImportItem */,
                     std::string  /* error message */)

IPC_MESSAGE_CONTROL2(ProfileImportProcessMsg_ReportImportItemFailed,
                     int  /* ImportItem */,
                     std::string  /* error message */)