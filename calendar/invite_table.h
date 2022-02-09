// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_INVITE_TABLE_H_
#define CALENDAR_INVITE_TABLE_H_

#include <stddef.h>

#include "calendar/calendar_typedefs.h"
#include "calendar/invite_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL table that holds invite info.
class InviteTable {
 public:
  // Must call CreateInviteTable() before to make sure the database is
  // initialized.
  InviteTable();
  InviteTable(const InviteTable&) = delete;
  InviteTable& operator=(const InviteTable&) = delete;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~InviteTable();

  bool CreateInviteTable();

  InviteID CreateInvite(InviteRow row);
  bool GetInvitesForEvent(EventID event_id, InviteRows* invites);
  bool GetInviteRow(InviteID invite_id, InviteRow* out_invite);
  bool UpdateInvite(const InviteRow& invite);
  bool DeleteInvite(InviteID invite_id);
  bool DeleteInvitesForCalendar(CalendarID calendar_id);
  bool DoesInviteIdExist(InviteID invite_id);
  bool MigrateCalendarToVersion5();

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillInviteRow(sql::Statement& statement, InviteRow* invite);
};

#define INVITE_ROW_FIELDS \
  " id, event_id, name, address, sent, partstat, \
  created, last_modified "

}  // namespace calendar

#endif  // CALENDAR_INVITE_TABLE_H_
