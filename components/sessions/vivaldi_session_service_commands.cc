// Copyright (c) 2015-2019 Vivaldi Technologies AS. All rights reserved

#include "components/sessions/vivaldi_session_service_commands.h"

#include <stddef.h>
#include <limits>
#include <map>

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

std::unique_ptr<SessionCommand> CreateSetTabVivExtDataCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& viv_ext_data) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max for ext data.
  static const SessionCommand::size_type max_ext_data_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_ext_data_size, viv_ext_data);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

std::unique_ptr<SessionCommand> CreateVivPageActionOverrideCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& script_path,
    bool is_enabled_override) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max path length
  static const SessionCommand::size_type max_path_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_path_size, script_path);
  pickle.WriteBool(is_enabled_override);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

std::unique_ptr<SessionCommand> CreateVivCreateThumbnailCommand(
    SessionID::id_type command_id,
    int image_format,
    const char * data,
    size_t size) {
  base::Pickle pickle;
  pickle.WriteInt(image_format);
  pickle.WriteData(data, size);
  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateRemoveVivPageActionOverrideCommand(
    SessionID::id_type command_id,
    SessionID tab_id,
    const std::string& script_path) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max path length
  static const SessionCommand::size_type max_path_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_path_size, script_path);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

std::unique_ptr<SessionCommand> CreateSetWindowVivExtDataCommand(
    SessionID::id_type command_id,
    SessionID window_id,
    const std::string& viv_ext_data) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(&pickle, &bytes_written, max_id_size, viv_ext_data);

  return std::unique_ptr<SessionCommand>(
      new SessionCommand(command_id, pickle));
}

bool RestoreSetVivExtDataCommand(const SessionCommand& command,
                              SessionID* tab_id,
                                 std::string* viv_ext_data) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         iterator.ReadString(viv_ext_data);
}

bool RestoreVivPageActionOverrideCommand(const SessionCommand& command,
                                         SessionID* tab_id,
                                         std::string* script_path,
                                         bool* is_enabled_override) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         iterator.ReadString(script_path) &&
         iterator.ReadBool(is_enabled_override);
}

bool RestoreRemoveVivPageActionOverrideCommand(const SessionCommand& command,
                                            SessionID* tab_id,
                                            std::string* script_path) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         iterator.ReadString(script_path);
}

bool RestoreSetWindowVivExtDataCommand(const SessionCommand& command,
                                    SessionID* window_id,
                                    std::string* viv_ext_data) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, window_id) &&
         iterator.ReadString(viv_ext_data);
}

}  // namespace vivaldi
