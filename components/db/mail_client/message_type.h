// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TYPE_H_
#define COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TYPE_H_

#include <string>
#include <vector>

namespace mail_client {

typedef int64_t SearchListID;

typedef std::vector<SearchListID> SearchListIDs;

// Holds all information associated with message.
class MessageRow {
 public:
  MessageRow();
  MessageRow(const MessageRow& other);
  ~MessageRow();

  SearchListID searchListId;
  std::u16string from;
  std::u16string to;
  std::u16string cc;
  std::u16string replyTo;
  std::u16string subject;
  std::u16string body;
};

typedef std::vector<MessageRow> MessageRows;

// Used for add message body and delete message result
class MessageResult {
 public:
  MessageResult() = default;
  MessageResult(const MessageResult&) = default;
  MessageResult& operator=(const MessageResult&) = default;

  bool success;
  std::string message;
};

struct Migration {
  int db_version;
  bool migration_needed;
};

}  // namespace mail_client

#endif  //  COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TYPE_H_
