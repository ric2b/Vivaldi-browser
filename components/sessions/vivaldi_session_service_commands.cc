// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "components/sessions/vivaldi_session_service_commands.h"

#include <stddef.h>

#include <limits>

#include "base/pickle.h"
#include "components/sessions/core/session_command.h"

namespace vivaldi {

namespace {

// Copied from Chromium
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(base::Pickle* pickle,
                         int* bytes_written,
                         int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString(str);
  } else {
    pickle->WriteString(std::string());
  }
}

bool ReadSessionIdFromPickle(base::PickleIterator* iterator, SessionID* id) {
  SessionID::id_type value;
  if (!iterator->ReadInt(&value)) {
    return false;
  }
  *id = SessionID::FromSerializedValue(value);
  return true;
}

}  // namespace

std::unique_ptr<SessionCommand> CreateSetTabExtDataCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& ext_data) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max for ext data.
  static const SessionCommand::size_type max_ext_data_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_ext_data_size, ext_data);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

std::unique_ptr<SessionCommand> CreateSetWindowExtDataCommand(
    SessionID::id_type command_id,
    SessionID window_id,
    const std::string& ext_data) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_id_size, ext_data);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

bool RestoreSetExtDataCommand(const SessionCommand& command,
                              SessionID* tab_id,
                              std::string* ext_data) {
  std::unique_ptr<base::Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  base::PickleIterator iterator(*pickle);
  return ReadSessionIdFromPickle(&iterator, tab_id) && iterator.ReadString(ext_data);
}

bool RestoreSetWindowExtDataCommand(const SessionCommand& command,
                                    SessionID* window_id,
                                    std::string* ext_data) {
  std::unique_ptr<base::Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  base::PickleIterator iterator(*pickle);
  return ReadSessionIdFromPickle(&iterator, window_id) && iterator.ReadString(ext_data);
}

}  // namespace vivaldi
