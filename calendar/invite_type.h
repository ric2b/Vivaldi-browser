// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_INVITE_TYPE_H_
#define CALENDAR_INVITE_TYPE_H_

#include <string>
#include <vector>

#include "calendar/calendar_typedefs.h"

namespace calendar {

// Bit flags determing which fields should be updated in the
// UpdateInvite API method
enum UpdateInviteFields {
  INVITE_NAME = 1 << 0,
  INVITE_ADDRESS = 1 << 1,
  INVITE_SENT = 1 << 2,
  INVITE_PARTSTAT = 1 << 3,
};

// Simplified invite. Used when invite has to be created during
// event create
struct InviteToCreate {
  std::u16string name;
  std::string partstat;
  std::u16string address;
};

// Holds all information associated with event invite.
class InviteRow {
 public:
  InviteRow();
  InviteRow(const InviteRow& other);
  ~InviteRow();

  InviteID id;
  EventID event_id;
  std::u16string name;
  std::u16string address;
  bool sent;
  std::string partstat;
};

class UpdateInviteRow {
 public:
  UpdateInviteRow() = default;
  ~UpdateInviteRow() = default;
  UpdateInviteRow(const UpdateInviteRow& invite) = default;
  UpdateInviteRow& operator=(UpdateInviteRow& invite) = default;

  InviteRow invite_row;
  int updateFields = 0;
};

typedef std::vector<InviteRow> InviteRows;
typedef std::vector<InviteToCreate> InvitesToCreate;

class InviteResult {
 public:
  InviteResult() = default;
  InviteResult(const InviteResult& invite) = default;
  InviteResult& operator=(InviteResult& invite) = default;

  bool success;
  std::string message;
  InviteRow inviteRow;
};
}  // namespace calendar

#endif  //  CALENDAR_INVITE_TYPE_H_
