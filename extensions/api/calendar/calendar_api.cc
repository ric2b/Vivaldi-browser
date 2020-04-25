// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/calendar/calendar_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/calendar.h"
#include "extensions/tools/vivaldi_tools.h"

using calendar::CalendarService;
using calendar::CalendarServiceFactory;
using calendar::RecurrenceFrequency;
using vivaldi::GetTime;
using vivaldi::MilliSecondsFromTime;

namespace {

bool GetIdAsInt64(const base::string16& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

bool GetStdStringAsInt64(const std::string& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

}  // namespace

namespace extensions {

using calendar::EventTypeRow;
using vivaldi::calendar::Calendar;
using vivaldi::calendar::CreateEventsResults;
using vivaldi::calendar::EventType;
using vivaldi::calendar::OccurrenceFrequency;
using vivaldi::calendar::RecurrencePattern;

namespace OnEventCreated = vivaldi::calendar::OnEventCreated;
namespace OnEventRemoved = vivaldi::calendar::OnEventRemoved;
namespace OnEventChanged = vivaldi::calendar::OnEventChanged;

namespace OnEventTypeCreated = vivaldi::calendar::OnEventTypeCreated;
namespace OnEventTypeRemoved = vivaldi::calendar::OnEventTypeRemoved;
namespace OnEventTypeChanged = vivaldi::calendar::OnEventTypeChanged;

namespace OnCalendarCreated = vivaldi::calendar::OnCalendarCreated;
namespace OnCalendarRemoved = vivaldi::calendar::OnCalendarRemoved;
namespace OnCalendarChanged = vivaldi::calendar::OnCalendarChanged;

typedef std::vector<vivaldi::calendar::CalendarEvent> EventList;
typedef std::vector<vivaldi::calendar::Calendar> CalendarList;
typedef std::vector<vivaldi::calendar::EventType> EventTypeList;

// static
RecurrenceFrequency UiOccurrenceToEventOccurrence(
    OccurrenceFrequency transition) {
  switch (transition) {
    case vivaldi::calendar::OccurrenceFrequency::OCCURRENCE_FREQUENCY_DAYS:
      return calendar::RecurrenceFrequency::DAILY;
    case vivaldi::calendar::OccurrenceFrequency::OCCURRENCE_FREQUENCY_WEEKS:
      return calendar::RecurrenceFrequency::WEEKLY;
    case vivaldi::calendar::OccurrenceFrequency::OCCURRENCE_FREQUENCY_MONTHS:
      return calendar::RecurrenceFrequency::MONTHLY;
    case vivaldi::calendar::OccurrenceFrequency::OCCURRENCE_FREQUENCY_YEARS:
      return calendar::RecurrenceFrequency::YEARLY;
    default:
      NOTREACHED();
  }
  return RecurrenceFrequency::NONE;
}

CalendarEvent GetEventItem(const calendar::EventRow& row) {
  CalendarEvent event_item;
  event_item.id = base::NumberToString(row.id());
  event_item.calendar_id = base::NumberToString(row.calendar_id());
  event_item.description.reset(
      new std::string(base::UTF16ToUTF8(row.description())));
  event_item.title.reset(new std::string(base::UTF16ToUTF8(row.title())));
  event_item.start.reset(new double(MilliSecondsFromTime(row.start())));
  event_item.end.reset(new double(MilliSecondsFromTime(row.end())));
  event_item.event_type_id.reset(
      new std::string(base::NumberToString(row.event_type_id())));
  return event_item;
}

calendar::EventRecurrence GetEventRecurrence(
    const RecurrencePattern& recurring_pattern) {
  calendar::EventRecurrence recurrence_event;

  if (recurring_pattern.frequency) {
    recurrence_event.frequency =
        UiOccurrenceToEventOccurrence(recurring_pattern.frequency);
    recurrence_event.updateFields |= calendar::RECURRENCE_FREQUENCY;
  }

  if (recurring_pattern.number_of_occurrences.get()) {
    recurrence_event.number_of_occurrences =
        *recurring_pattern.number_of_occurrences.get();
    recurrence_event.updateFields |= calendar::NUMBER_OF_OCCURRENCES;
  }

  if (recurring_pattern.interval.get()) {
    recurrence_event.interval = *recurring_pattern.interval.get();
    recurrence_event.updateFields |= calendar::RECURRENCE_INTERVAL;
  }

  if (recurring_pattern.day_of_week.get()) {
    recurrence_event.day_of_week = *recurring_pattern.day_of_week.get();
    recurrence_event.updateFields |= calendar::RECURRENCE_DAY_OF_WEEK;
  }

  if (recurring_pattern.week_of_month.get()) {
    recurrence_event.week_of_month = *recurring_pattern.week_of_month.get();
    recurrence_event.updateFields |= calendar::RECURRENCE_WEEK_OF_MONTH;
  }

  if (recurring_pattern.day_of_month.get()) {
    recurrence_event.day_of_month = *recurring_pattern.day_of_month.get();
    recurrence_event.updateFields |= calendar::RECURRENCE_DAY_OF_MONTH;
  }

  if (recurring_pattern.month_of_year.get()) {
    recurrence_event.month_of_year = *recurring_pattern.month_of_year.get();
    recurrence_event.updateFields |= calendar::RECURRENCE_MONTH_OF_YEAR;
  }

  return recurrence_event;
}

Calendar GetCalendarItem(const calendar::CalendarRow& row) {
  Calendar calendar;
  calendar.id = base::NumberToString(row.id());
  calendar.name.reset(new std::string(base::UTF16ToUTF8(row.name())));
  calendar.description.reset(
      new std::string(base::UTF16ToUTF8(row.description())));
  calendar.ctag.reset(new std::string(row.ctag()));
  calendar.orderindex.reset(new int(row.orderindex()));
  calendar.active.reset(new bool(row.active()));
  calendar.iconindex.reset(new int(row.iconindex()));
  calendar.color.reset(new std::string(row.color()));
  calendar.username.reset(new std::string(base::UTF16ToUTF8(row.username())));
  return calendar;
}

EventType GetEventType(const EventTypeRow& row) {
  EventType event_type;
  event_type.id = base::NumberToString(row.id());
  event_type.name = base::UTF16ToUTF8(row.name());
  event_type.color.reset(new std::string(row.color()));
  event_type.iconindex.reset(new int(row.iconindex()));

  return event_type;
}

CalendarEventRouter::CalendarEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(CalendarServiceFactory::GetForProfile(profile)) {
  model_->AddObserver(this);
}

CalendarEventRouter::~CalendarEventRouter() {
  model_->RemoveObserver(this);
}

void CalendarEventRouter::ExtensiveCalendarChangesBeginning(
    CalendarService* model) {}

void CalendarEventRouter::ExtensiveCalendarChangesEnded(
    CalendarService* model) {}

void CalendarEventRouter::OnEventCreated(CalendarService* service,
                                         const calendar::EventRow& row) {
  CalendarEvent createdEvent = GetEventItem(row);
  std::unique_ptr<base::ListValue> args = OnEventCreated::Create(createdEvent);
  DispatchEvent(OnEventCreated::kEventName, std::move(args));
}

void CalendarEventRouter::OnEventDeleted(CalendarService* service,
                                         const calendar::EventRow& row) {
  CalendarEvent deletedEvent = GetEventItem(row);
  std::unique_ptr<base::ListValue> args = OnEventRemoved::Create(deletedEvent);
  DispatchEvent(OnEventRemoved::kEventName, std::move(args));
}

void CalendarEventRouter::OnEventChanged(CalendarService* service,
                                         const calendar::EventRow& row) {
  CalendarEvent changedEvent = GetEventItem(row);
  std::unique_ptr<base::ListValue> args = OnEventChanged::Create(changedEvent);
  DispatchEvent(OnEventChanged::kEventName, std::move(args));
}

void CalendarEventRouter::OnEventTypeCreated(
    CalendarService* service,
    const calendar::EventTypeRow& row) {
  EventType createdEvent = GetEventType(row);
  std::unique_ptr<base::ListValue> args =
      OnEventTypeCreated::Create(createdEvent);
  DispatchEvent(OnEventTypeCreated::kEventName, std::move(args));
}

void CalendarEventRouter::OnEventTypeDeleted(
    CalendarService* service,
    const calendar::EventTypeRow& row) {
  EventType deletedEvent = GetEventType(row);
  std::unique_ptr<base::ListValue> args =
      OnEventTypeRemoved::Create(deletedEvent);
  DispatchEvent(OnEventTypeRemoved::kEventName, std::move(args));
}

void CalendarEventRouter::OnEventTypeChanged(
    CalendarService* service,
    const calendar::EventTypeRow& row) {
  EventType changedEvent = GetEventType(row);
  std::unique_ptr<base::ListValue> args =
      OnEventTypeChanged::Create(changedEvent);
  DispatchEvent(OnEventTypeChanged::kEventName, std::move(args));
}

void CalendarEventRouter::OnCalendarCreated(CalendarService* service,
                                            const calendar::CalendarRow& row) {
  Calendar createCalendar = GetCalendarItem(row);
  std::unique_ptr<base::ListValue> args =
      OnCalendarCreated::Create(createCalendar);
  DispatchEvent(OnCalendarCreated::kEventName, std::move(args));
}

void CalendarEventRouter::OnCalendarDeleted(CalendarService* service,
                                            const calendar::CalendarRow& row) {
  Calendar deletedCalendar = GetCalendarItem(row);
  std::unique_ptr<base::ListValue> args =
      OnCalendarChanged::Create(deletedCalendar);
  DispatchEvent(OnCalendarRemoved::kEventName, std::move(args));
}

void CalendarEventRouter::OnCalendarChanged(CalendarService* service,
                                            const calendar::CalendarRow& row) {
  Calendar changedCalendar = GetCalendarItem(row);
  std::unique_ptr<base::ListValue> args =
      OnCalendarChanged::Create(changedCalendar);
  DispatchEvent(OnCalendarChanged::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void CalendarEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

void BroadcastCalendarEvent(const std::string& eventname,
                            std::unique_ptr<base::ListValue> args,
                            content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(
      new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                            eventname, std::move(args), context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

CalendarAPI::CalendarAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnEventCreated::kEventName);
  event_router->RegisterObserver(this, OnEventRemoved::kEventName);
  event_router->RegisterObserver(this, OnEventChanged::kEventName);

  event_router->RegisterObserver(this, OnEventTypeCreated::kEventName);
  event_router->RegisterObserver(this, OnEventTypeRemoved::kEventName);
  event_router->RegisterObserver(this, OnEventTypeChanged::kEventName);

  event_router->RegisterObserver(this, OnCalendarCreated::kEventName);
  event_router->RegisterObserver(this, OnCalendarRemoved::kEventName);
  event_router->RegisterObserver(this, OnCalendarChanged::kEventName);
}

CalendarAPI::~CalendarAPI() {}

void CalendarAPI::Shutdown() {
  calendar_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CalendarAPI>>::
    DestructorAtExit g_factory_calendar = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CalendarAPI>* CalendarAPI::GetFactoryInstance() {
  return g_factory_calendar.Pointer();
}

OccurrenceFrequency RecurrenceToUiRecurrence(RecurrenceFrequency transition) {
  switch (transition) {
    case RecurrenceFrequency::NONE:
      return OccurrenceFrequency::OCCURRENCE_FREQUENCY_NONE;
    case RecurrenceFrequency::DAILY:
      return OccurrenceFrequency::OCCURRENCE_FREQUENCY_DAYS;
    case RecurrenceFrequency::WEEKLY:
      return OccurrenceFrequency::OCCURRENCE_FREQUENCY_WEEKS;
    case RecurrenceFrequency::MONTHLY:
      return OccurrenceFrequency::OCCURRENCE_FREQUENCY_MONTHS;
    case RecurrenceFrequency::YEARLY:
      return OccurrenceFrequency::OCCURRENCE_FREQUENCY_YEARS;
    default:
      NOTREACHED();
  }
  // We have to return something
  return OccurrenceFrequency::OCCURRENCE_FREQUENCY_NONE;
}

void CalendarAPI::OnListenerAdded(const EventListenerInfo& details) {
  calendar_event_router_.reset(
      new CalendarEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

std::unique_ptr<CalendarEvent> CreateVivaldiEvent(
    const calendar::EventResult& event) {
  std::unique_ptr<CalendarEvent> cal_event(new CalendarEvent());

  cal_event->id = base::NumberToString(event.id());
  cal_event->calendar_id = base::NumberToString(event.calendar_id());
  cal_event->alarm_id.reset(
      new std::string(base::NumberToString(event.alarm_id())));

  cal_event->title.reset(new std::string(base::UTF16ToUTF8(event.title())));
  cal_event->description.reset(
      new std::string(base::UTF16ToUTF8(event.description())));
  cal_event->start.reset(new double(MilliSecondsFromTime(event.start())));
  cal_event->end.reset(new double(MilliSecondsFromTime(event.end())));
  cal_event->all_day.reset(new bool(event.all_day()));
  cal_event->is_recurring.reset(new bool(event.is_recurring()));
  cal_event->start_recurring.reset(
      new double(MilliSecondsFromTime(event.start_recurring())));
  cal_event->end_recurring.reset(
      new double(MilliSecondsFromTime(event.end_recurring())));
  cal_event->location.reset(
      new std::string(base::UTF16ToUTF8(event.location())));
  cal_event->url.reset(new std::string(base::UTF16ToUTF8(event.url())));
  cal_event->etag.reset(new std::string(event.etag()));
  cal_event->href.reset(new std::string(event.href()));
  cal_event->uid.reset(new std::string(event.uid()));
  cal_event->event_type_id.reset(
      new std::string(base::NumberToString(event.event_type_id())));
  cal_event->task.reset(new bool(event.task()));
  cal_event->complete.reset(new bool(event.complete()));
  cal_event->trash.reset(new bool(event.trash()));
  cal_event->trash_time.reset(
    new double(MilliSecondsFromTime(event.trash_time())));

  RecurrencePattern* pattern = new RecurrencePattern();
  pattern->frequency = RecurrenceToUiRecurrence(event.recurrence().frequency);
  pattern->number_of_occurrences.reset(
      new int(event.recurrence().number_of_occurrences));
  pattern->interval.reset(new int(event.recurrence().interval));
  pattern->day_of_week.reset(new int(event.recurrence().day_of_week));
  pattern->week_of_month.reset(new int(event.recurrence().week_of_month));
  pattern->day_of_month.reset(new int(event.recurrence().day_of_month));
  pattern->month_of_year.reset(new int(event.recurrence().month_of_year));

  cal_event->recurrence.reset(pattern);

  return cal_event;
}

CalendarService* CalendarAsyncFunction::GetCalendarService() {
  return CalendarServiceFactory::GetForProfile(GetProfile());
}

CalendarGetAllEventsFunction::~CalendarGetAllEventsFunction() {}

ExtensionFunction::ResponseAction CalendarGetAllEventsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEvents(
      base::Bind(&CalendarGetAllEventsFunction::GetAllEventsComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllEventsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllEventsFunction::GetAllEventsComplete(
    std::shared_ptr<calendar::EventQueryResults> results) {
  EventList eventList;

  if (results && !results->empty()) {
    for (calendar::EventQueryResults::EventResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      eventList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiEvent(**iterator).release()))));
    }
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEvents::Results::Create(eventList)));
}

Profile* CalendarAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

calendar::EventRow GetEventRow(const vivaldi::calendar::CreateDetails& event) {
  calendar::EventRow row;
  row.set_title(base::UTF8ToUTF16(event.title));

  if (event.description.get()) {
    row.set_description(base::UTF8ToUTF16(*event.description));
  }

  if (event.start.get()) {
    row.set_start(GetTime(*event.start.get()));
  }

  if (event.end.get()) {
    row.set_end(GetTime(*event.end.get()));
  }

  if (event.all_day.get()) {
    row.set_all_day(*event.all_day.get());
  }

  if (event.is_recurring.get()) {
    row.set_is_recurring(*event.is_recurring.get());
  }

  if (event.start_recurring.get()) {
    row.set_start_recurring(GetTime(*event.start_recurring.get()));
  }

  if (event.end_recurring.get()) {
    row.set_end_recurring(GetTime(*event.end_recurring.get()));
  }

  if (event.location.get()) {
    row.set_location(base::UTF8ToUTF16(*event.location));
  }

  if (event.url.get()) {
    row.set_url(base::UTF8ToUTF16(*event.url));
  }

  if (event.etag.get()) {
    row.set_etag(*event.etag);
  }

  if (event.href.get()) {
    row.set_href(*event.href);
  }

  if (event.uid.get()) {
    row.set_uid(*event.uid);
  }

  if (event.recurrence.get()) {
    row.set_recurrence(GetEventRecurrence(*event.recurrence));
  }

  calendar::CalendarID calendar_id;
  if (GetStdStringAsInt64(event.calendar_id, &calendar_id)) {
    row.set_calendar_id(calendar_id);
  }

  if (event.task.get()) {
    row.set_task(*event.task);
  }

  if (event.complete.get()) {
    row.set_complete(*event.complete);
  }

  return row;
}

ExtensionFunction::ResponseAction CalendarEventCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventCreate::Params> params(
      vivaldi::calendar::EventCreate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  calendar::EventRow createEvent = GetEventRow(params->event);

  model->CreateCalendarEvent(
      createEvent,
      base::Bind(&CalendarEventCreateFunction::CreateEventComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventCreateFunction::CreateEventComplete(
    std::shared_ptr<calendar::CreateEventResult> results) {
  if (!results->success) {
    Respond(Error("Error creating event. " + results->message));
  } else {
    CalendarEvent ev = GetEventItem(results->createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::EventCreate::Results::Create(ev)));
  }
}

ExtensionFunction::ResponseAction CalendarEventsCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventsCreate::Params> params(
      vivaldi::calendar::EventsCreate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  std::vector<vivaldi::calendar::CreateDetails>& events = params->events_list;
  size_t count = events.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  std::vector<calendar::EventRow> event_rows;

  for (size_t i = 0; i < count; ++i) {
    vivaldi::calendar::CreateDetails& create_details = events[i];
    calendar::EventRow createEvent = GetEventRow(create_details);
    event_rows.push_back(createEvent);
  }

  model->CreateCalendarEvents(
      event_rows,
      base::Bind(&CalendarEventsCreateFunction::CreateEventsComplete, this),
      &task_tracker_);

  return RespondLater();
}

CreateEventsResults GetCreateEventsItem(
    const calendar::CreateEventsResult& res) {
  CreateEventsResults event_item;
  event_item.created_count = res.number_success;
  event_item.failed_count = res.number_failed;
  return event_item;
}

void CalendarEventsCreateFunction::CreateEventsComplete(
    std::shared_ptr<calendar::CreateEventsResult> results) {
  CreateEventsResults return_results = GetCreateEventsItem(*results);
  Respond(
      ArgumentList(extensions::vivaldi::calendar::EventsCreate::Results::Create(
          return_results)));
}

ExtensionFunction::ResponseAction CalendarUpdateEventFunction::Run() {
  std::unique_ptr<vivaldi::calendar::UpdateEvent::Params> params(
      vivaldi::calendar::UpdateEvent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::CalendarEvent updatedEvent;

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  if (params->changes.calendar_id.get()) {
    calendar::CalendarID calendarId;
    if (!GetStdStringAsInt64(*params->changes.calendar_id, &calendarId)) {
      return RespondNow(Error("Error. Invalid calendar_id"));
    }
    updatedEvent.calendar_id = calendarId;
    updatedEvent.updateFields |= calendar::CALENDAR_ID;
  }

  if (params->changes.alarm_id.get()) {
    calendar::AlarmID alarmId;
    if (!GetStdStringAsInt64(*params->changes.calendar_id, &alarmId)) {
      return RespondNow(Error("Error. Invalid alarm"));
    }

    updatedEvent.alarm_id = alarmId;
    updatedEvent.updateFields |= calendar::ALARM_ID;
  }

  if (params->changes.description.get()) {
    updatedEvent.description = base::UTF8ToUTF16(*params->changes.description);
    updatedEvent.updateFields |= calendar::DESCRIPTION;
  }

  if (params->changes.title.get()) {
    updatedEvent.title = base::UTF8ToUTF16(*params->changes.title);
    updatedEvent.updateFields |= calendar::TITLE;
  }

  if (params->changes.start.get()) {
    double start = *params->changes.start;
    updatedEvent.start = GetTime(start);
    updatedEvent.updateFields |= calendar::START;
  }

  if (params->changes.end.get()) {
    double end = *params->changes.end;
    updatedEvent.end = GetTime(end);
    updatedEvent.updateFields |= calendar::END;
  }

  if (params->changes.all_day.get()) {
    updatedEvent.all_day = *params->changes.all_day;
    updatedEvent.updateFields |= calendar::ALLDAY;
  }

  if (params->changes.is_recurring.get()) {
    updatedEvent.is_recurring = *params->changes.is_recurring;
    updatedEvent.updateFields |= calendar::ISRECURRING;
  }

  if (params->changes.start_recurring.get()) {
    updatedEvent.start_recurring = GetTime(*params->changes.start_recurring);
    updatedEvent.updateFields |= calendar::STARTRECURRING;
  }

  if (params->changes.end_recurring.get()) {
    updatedEvent.end_recurring = GetTime(*params->changes.end_recurring);
    updatedEvent.updateFields |= calendar::ENDRECURRING;
  }

  if (params->changes.location.get()) {
    updatedEvent.location = base::UTF8ToUTF16(*params->changes.location);
    updatedEvent.updateFields |= calendar::LOCATION;
  }

  if (params->changes.url.get()) {
    updatedEvent.url = base::UTF8ToUTF16(*params->changes.url);
    updatedEvent.updateFields |= calendar::URL;
  }

  if (params->changes.recurrence.get()) {
    updatedEvent.recurrence = GetEventRecurrence(*params->changes.recurrence);
    updatedEvent.updateFields |= calendar::RECURRENCE;
  }

  if (params->changes.etag.get()) {
    updatedEvent.etag = *params->changes.etag;
    updatedEvent.updateFields |= calendar::ETAG;
  }

  if (params->changes.href.get()) {
    updatedEvent.href = *params->changes.href;
    updatedEvent.updateFields |= calendar::HREF;
  }

  if (params->changes.uid.get()) {
    updatedEvent.uid = *params->changes.uid;
    updatedEvent.updateFields |= calendar::UID;
  }

  if (params->changes.task.get()) {
    updatedEvent.task = *params->changes.task;
    updatedEvent.updateFields |= calendar::TASK;
  }

  if (params->changes.complete.get()) {
    updatedEvent.complete = *params->changes.complete;
    updatedEvent.updateFields |= calendar::COMPLETE;
  }

  if (params->changes.trash.get()) {
    updatedEvent.trash = *params->changes.trash;
    updatedEvent.updateFields |= calendar::TRASH;
  }

  if (params->changes.event_type_id.get()) {
    calendar::EventTypeID event_type_id;
    if (!GetStdStringAsInt64(*params->changes.event_type_id, &event_type_id)) {
      return RespondNow(Error("Error. Invalid event_type_id"));
    }

    updatedEvent.event_type_id = event_type_id;
    updatedEvent.updateFields |= calendar::EVENT_TYPE_ID;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendarEvent(
      eventId, updatedEvent,
      base::Bind(&CalendarUpdateEventFunction::UpdateEventComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateEventComplete() will be called
                          // asynchronously.
}

void CalendarUpdateEventFunction::UpdateEventComplete(
    std::shared_ptr<calendar::UpdateEventResult> results) {
  if (!results->success) {
    Respond(Error("Error updating event"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteEvent::Params> params(
      vivaldi::calendar::DeleteEvent::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendarEvent(
      eventId,
      base::Bind(&CalendarDeleteEventFunction::DeleteEventComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteEventComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventFunction::DeleteEventComplete(
    std::shared_ptr<calendar::DeleteEventResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting event"));
  } else {
    Respond(NoArguments());
  }
}

std::unique_ptr<vivaldi::calendar::Calendar> CreateVivaldiCalendar(
    const calendar::CalendarResult& result) {
  std::unique_ptr<vivaldi::calendar::Calendar> calendar(
      new vivaldi::calendar::Calendar());

  calendar->id = base::NumberToString(result.id());
  calendar->name.reset(new std::string(base::UTF16ToUTF8(result.name())));

  calendar->description.reset(
      new std::string(base::UTF16ToUTF8(result.description())));
  calendar->url.reset(new std::string(result.url().spec()));
  calendar->orderindex.reset(new int(result.orderindex()));
  calendar->color.reset(new std::string(result.color()));
  calendar->hidden.reset(new bool(result.hidden()));
  calendar->ctag.reset(new std::string(result.ctag()));
  calendar->active.reset(new bool(result.active()));
  calendar->iconindex.reset(new int(result.iconindex()));
  calendar->username.reset(
      new std::string(base::UTF16ToUTF8(result.username())));
  return calendar;
}

ExtensionFunction::ResponseAction CalendarCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Create::Params> params(
      vivaldi::calendar::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::CalendarRow createCalendar;

  base::string16 name;
  name = base::UTF8ToUTF16(params->calendar.name);

  createCalendar.set_name(name);

  base::string16 description;
  if (params->calendar.description.get()) {
    description = base::UTF8ToUTF16(*params->calendar.description);
    createCalendar.set_description(description);
  }

  std::string url;
  if (params->calendar.url.get()) {
    url = *params->calendar.url.get();
    createCalendar.set_url(GURL(url));
  }

  int orderindex;
  if (params->calendar.orderindex.get()) {
    orderindex = *params->calendar.orderindex.get();
    createCalendar.set_orderindex(orderindex);
  }

  std::string color;
  if (params->calendar.color.get()) {
    color = *params->calendar.color.get();
    createCalendar.set_color(color);
  }

  bool hidden;
  if (params->calendar.hidden.get()) {
    hidden = *params->calendar.hidden.get();
    createCalendar.set_hidden(hidden);
  }

  bool active;
  if (params->calendar.active.get()) {
    active = *params->calendar.active.get();
    createCalendar.set_active(active);
  }

  base::string16 username;
  if (params->calendar.username.get()) {
    username = base::UTF8ToUTF16(*params->calendar.username.get());
    createCalendar.set_username(username);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateCalendar(
      createCalendar, base::Bind(&CalendarCreateFunction::CreateComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarCreateFunction::CreateComplete(
    std::shared_ptr<calendar::CreateCalendarResult> results) {
  if (!results->success) {
    Respond(Error("Error creating calendar"));
  } else {
    Calendar ev = GetCalendarItem(results->createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::Create::Results::Create(ev)));
  }
}

CalendarGetAllFunction::~CalendarGetAllFunction() {}

ExtensionFunction::ResponseAction CalendarGetAllFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllCalendars(
      base::Bind(&CalendarGetAllFunction::GetAllComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllComplete() will be called
                          // asynchronously.
}

void CalendarGetAllFunction::GetAllComplete(
    std::shared_ptr<calendar::CalendarQueryResults> results) {
  CalendarList calendarList;

  if (results && !results->empty()) {
    for (calendar::CalendarQueryResults::CalendarResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      calendarList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiCalendar(*iterator).release()))));
    }
  }

  Respond(
      ArgumentList(vivaldi::calendar::GetAll::Results::Create(calendarList)));
}

ExtensionFunction::ResponseAction CalendarUpdateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Update::Params> params(
      vivaldi::calendar::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::Calendar updatedCalendar;

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;
  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  if (params->changes.name.get()) {
    updatedCalendar.name = base::UTF8ToUTF16(*params->changes.name);
    updatedCalendar.updateFields |= calendar::CALENDAR_NAME;
  }

  if (params->changes.description.get()) {
    updatedCalendar.description =
        base::UTF8ToUTF16(*params->changes.description);
    updatedCalendar.updateFields |= calendar::CALENDAR_DESCRIPTION;
  }

  if (params->changes.url.get()) {
    updatedCalendar.url = GURL(*params->changes.url);
    updatedCalendar.updateFields |= calendar::CALENDAR_URL;
  }

  if (params->changes.orderindex.get()) {
    updatedCalendar.orderindex = *params->changes.orderindex;
    updatedCalendar.updateFields |= calendar::CALENDAR_ORDERINDEX;
  }

  if (params->changes.color.get()) {
    updatedCalendar.color = *params->changes.color;
    updatedCalendar.updateFields |= calendar::CALENDAR_COLOR;
  }

  if (params->changes.hidden.get()) {
    updatedCalendar.hidden = *params->changes.hidden;
    updatedCalendar.updateFields |= calendar::CALENDAR_HIDDEN;
  }

  if (params->changes.active.get()) {
    updatedCalendar.active = *params->changes.active;
    updatedCalendar.updateFields |= calendar::CALENDAR_ACTIVE;
  }

  if (params->changes.iconindex.get()) {
    updatedCalendar.iconindex = *params->changes.iconindex;
    updatedCalendar.updateFields |= calendar::CALENDAR_ICONINDEX;
  }

  if (params->changes.username.get()) {
    updatedCalendar.username = base::UTF8ToUTF16(*params->changes.username);
    updatedCalendar.updateFields |= calendar::CALENDAR_USERNAME;
  }

  if (params->changes.ctag.get()) {
    updatedCalendar.ctag = *params->changes.ctag;
    updatedCalendar.updateFields |= calendar::CALENDAR_CTAG;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendar(
      calendarId, updatedCalendar,
      base::Bind(&CalendarUpdateFunction::UpdateCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateCalendarComplete() will be called
                          // asynchronously.
}

void CalendarUpdateFunction::UpdateCalendarComplete(
    std::shared_ptr<calendar::UpdateCalendarResult> results) {
  if (!results->success) {
    Respond(Error("Error updating calendar"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Delete::Params> params(
      vivaldi::calendar::Delete::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;

  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendar(
      calendarId,
      base::Bind(&CalendarDeleteFunction::DeleteCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteCalendarComplete() will be called
                          // asynchronously.
}

void CalendarDeleteFunction::DeleteCalendarComplete(
    std::shared_ptr<calendar::DeleteCalendarResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting calendar"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarGetAllEventTypesFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEventTypes(
      base::Bind(&CalendarGetAllEventTypesFunction::GetAllEventTypesComplete,
                 this),
      &task_tracker_);

  return RespondLater();  // CalendarGetAllEventTypesFunction() will be called
                          // asynchronously.
}

void CalendarGetAllEventTypesFunction::GetAllEventTypesComplete(
    std::shared_ptr<calendar::EventTypeRows> results) {
  EventTypeList event_type_list;
  for (EventTypeRow event_type : *results) {
    event_type_list.push_back(GetEventType(std::move(event_type)));
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEventTypes::Results::Create(event_type_list)));
}

ExtensionFunction::ResponseAction CalendarEventTypeCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Create::Params> params(
      vivaldi::calendar::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::EventTypeRow create_event_type;

  base::string16 name;
  name = base::UTF8ToUTF16(params->calendar.name);
  create_event_type.set_name(name);

  if (params->calendar.color.get()) {
    std::string color = *params->calendar.color;
    create_event_type.set_color(color);
  }

  if (params->calendar.iconindex.get()) {
    int iconindex = *params->calendar.iconindex;
    create_event_type.set_iconindex(iconindex);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateEventType(
      create_event_type,
      base::Bind(&CalendarEventTypeCreateFunction::CreateEventTypeComplete,
                 this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeCreateFunction::CreateEventTypeComplete(
    std::shared_ptr<calendar::CreateEventTypeResult> results) {
  if (!results->success) {
    Respond(Error("Error creating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarEventTypeUpdateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventTypeUpdate::Params> params(
      vivaldi::calendar::EventTypeUpdate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  calendar::EventType update_event_type;

  if (params->changes.name.get()) {
    base::string16 name;
    name = base::UTF8ToUTF16(*params->changes.name);
    update_event_type.name = name;
    update_event_type.updateFields |= calendar::NAME;
  }

  if (params->changes.color.get()) {
    std::string color = *params->changes.color;
    update_event_type.color = color;
    update_event_type.updateFields |= calendar::COLOR;
  }

  if (params->changes.iconindex.get()) {
    int iconindex = *params->changes.iconindex;
    update_event_type.iconindex = iconindex;
    update_event_type.updateFields |= calendar::ICONINDEX;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateEventType(
      event_type_id, update_event_type,
      base::Bind(&CalendarEventTypeUpdateFunction::UpdateEventTypeComplete,
                 this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeUpdateFunction::UpdateEventTypeComplete(
    std::shared_ptr<calendar::UpdateEventTypeResult> results) {
  if (!results->success) {
    Respond(Error("Error updating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventTypeFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteEventType::Params> params(
      vivaldi::calendar::DeleteEventType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventType(
      event_type_id,
      base::Bind(&CalendarDeleteEventTypeFunction::DeleteEventTypeComplete,
                 this),
      &task_tracker_);
  return RespondLater();  // DeleteEventTypeComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventTypeFunction::DeleteEventTypeComplete(
    std::shared_ptr<calendar::DeleteEventTypeResult> result) {
  if (!result->success) {
    Respond(Error("Error deleting event type"));
  } else {
    Respond(NoArguments());
  }
}

}  //  namespace extensions
