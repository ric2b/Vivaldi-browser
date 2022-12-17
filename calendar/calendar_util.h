// Copyright (c) 2020 Vivaldi. All rights reserved.

#ifndef CALENDAR_CALENDAR_UTIL_H_
#define CALENDAR_CALENDAR_UTIL_H_

#include "calendar/event_type.h"
#include "extensions/schema/calendar.h"

namespace calendar {

class EventRow;

using extensions::vivaldi::calendar::CreateDetails;
using extensions::vivaldi::calendar::CreateInviteRow;
using extensions::vivaldi::calendar::CreateNotificationRow;
using extensions::vivaldi::calendar::RecurrenceException;

EventRow GetEventRow(const CreateDetails& event);
RecurrenceExceptionRow CreateEventException(
    const RecurrenceException& exception);
bool GetIdAsInt64(const std::u16string& id_string, int64_t* id);
bool GetStdStringAsInt64(const std::string& id_string, int64_t* id);

NotificationToCreate CreateNotificationRow(
    const CreateNotificationRow& notification);

InviteToCreate CreateInviteRow(const CreateInviteRow& invite);

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_UTIL_H_
