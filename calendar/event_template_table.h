// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_TEMPLATE_TABLE_H_
#define CALENDAR_EVENT_TEMPLATE_TABLE_H_

#include <stddef.h>

#include "calendar/event_template_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL table that holds Event templates.
class EventTemplateTable {
 public:
  EventTemplateTable() = default;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~EventTemplateTable() = default;

  EventTemplateTable(const EventTemplateTable&) = delete;
  EventTemplateTable& operator=(const EventTemplateTable&) = delete;

  EventTemplateID CreateEventTemplate(
      calendar::EventTemplateRow event_template);

  bool CreateEventTemplateTable();
  bool GetAllEventTemplates(EventTemplateRows* events);
  bool GetRowForEventTemplate(EventTemplateID event_id,
                              EventTemplateRow* out_event);
  bool UpdateEventTemplate(const EventTemplateRow& event);
  bool DeleteEventTemplate(EventTemplateID event_template_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillEventTemplate(sql::Statement& statement, EventTemplateRow* event);
};

#define CALENDAR_EVENT_TEMPLATE_ROW_FIELDS " id, name, ical "

}  // namespace calendar

#endif  // CALENDAR_EVENT_TEMPLATE_TABLE_H_
