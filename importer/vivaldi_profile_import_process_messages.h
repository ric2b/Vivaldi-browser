// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditonal include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
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
