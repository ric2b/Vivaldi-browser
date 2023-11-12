// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_TEMPLATE_TYPE_H_
#define CALENDAR_EVENT_TEMPLATE_TYPE_H_

#include <string>
#include <vector>

namespace calendar {

enum UpdateEventTemplateFields {
  TEMPLATE_NAME = 1 << 0,
  TEMPLATE_ICAL = 1 << 1
};

typedef int64_t EventTemplateID;

// Holds all information associated with event template.
class EventTemplateRow {
 public:
  EventTemplateRow();
  EventTemplateRow(const EventTemplateRow& other);
  ~EventTemplateRow();

  EventTemplateID id;
  std::u16string name;
  std::u16string ical;
  int updateFields;
};

class EventTemplateResultCB {
 public:
  EventTemplateResultCB() = default;

  bool success;
  std::string message;
  EventTemplateRow event_template;
};

typedef std::vector<EventTemplateRow> EventTemplateRows;

}  // namespace calendar

#endif  //   CALENDAR_EVENT_TEMPLATE_TYPE_H_
