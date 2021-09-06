// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "message_type.h"

namespace mail_client {

MessageRow::MessageRow() {}

MessageRow::~MessageRow() {}

MessageRow::MessageRow(const MessageRow& emails)
    : searchListId(emails.searchListId),
      from(emails.from),
      to(emails.to),
      cc(emails.cc),
      replyTo(emails.replyTo),
      subject(emails.subject),
      body(emails.body) {}

}  // namespace mail_client
