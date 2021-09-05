// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/invite_type.h"

#include "calendar_type.h"

namespace calendar {

InviteRow::InviteRow() : sent(false) {}

InviteRow::~InviteRow() {}

InviteRow::InviteRow(const InviteRow& invite)
    : id(invite.id),
      event_id(invite.event_id),
      name(invite.name),
      address(invite.address),
      sent(invite.sent),
      partstat(invite.partstat) {}

}  // namespace calendar
