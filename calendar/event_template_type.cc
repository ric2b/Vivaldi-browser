// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "event_template_type.h"

namespace calendar {

EventTemplateRow::EventTemplateRow() {}

EventTemplateRow::~EventTemplateRow() {}

EventTemplateRow::EventTemplateRow(const EventTemplateRow& event_template)
    : id(event_template.id),
      name(event_template.name),
      ical(event_template.ical),
      updateFields(event_template.updateFields) {}

}  // namespace calendar
