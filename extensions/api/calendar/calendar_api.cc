// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/calendar/calendar_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "browser/vivaldi_internal_handlers.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_service_factory.h"
#include "calendar/calendar_util.h"
#include "calendar/invite_type.h"
#include "calendar/notification_type.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/calendar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using calendar::CalendarService;
using calendar::CalendarServiceFactory;
using vivaldi::GetTime;

namespace extensions {

using calendar::AccountRow;
using calendar::CalendarRow;
using calendar::EventTemplateResultCB;
using calendar::EventTemplateRow;
using calendar::EventTemplateRows;
using calendar::EventTypeRow;
using calendar::GetIdAsInt64;
using calendar::GetStdStringAsInt64;
using calendar::InviteRow;
using calendar::InviteRows;
using calendar::InviteToCreate;
using calendar::NotificationRow;
using calendar::NotificationRows;
using calendar::NotificationToCreate;
using calendar::RecurrenceExceptionRow;
using calendar::RecurrenceExceptionRows;
using vivaldi::calendar::Account;
using vivaldi::calendar::AccountType;
using vivaldi::calendar::Calendar;
using vivaldi::calendar::CreateEventsResults;
using vivaldi::calendar::CreateInviteRow;
using vivaldi::calendar::CreateNotificationRow;
using vivaldi::calendar::EventTemplate;
using vivaldi::calendar::EventType;
using vivaldi::calendar::Invite;
using vivaldi::calendar::Notification;
using vivaldi::calendar::RecurrenceException;
using vivaldi::calendar::SupportedCalendarComponents;

namespace OnIcsFileOpened = vivaldi::calendar::OnIcsFileOpened;
namespace OnWebcalUrlOpened = vivaldi::calendar::OnWebcalUrlOpened;
namespace OnMailtoOpened = vivaldi::calendar::OnMailtoOpened;

namespace OnEventCreated = vivaldi::calendar::OnEventCreated;
namespace OnNotificationChanged = vivaldi::calendar::OnNotificationChanged;
namespace OnCalendarDataChanged = vivaldi::calendar::OnCalendarDataChanged;

typedef std::vector<vivaldi::calendar::CalendarEvent> EventList;
typedef std::vector<EventTemplate> EventTemplateList;
typedef std::vector<vivaldi::calendar::Account> AccountList;
typedef std::vector<vivaldi::calendar::Notification> NotificaionList;
typedef std::vector<vivaldi::calendar::Calendar> CalendarList;
typedef std::vector<vivaldi::calendar::EventType> EventTypeList;
typedef std::vector<vivaldi::calendar::RecurrenceException>
    RecurrenceExceptions;

// static
RecurrenceException CreateException(const RecurrenceExceptionRow& row) {
  RecurrenceException exception;
  exception.exception_id = base::NumberToString(row.id);
  exception.cancelled = row.cancelled;
  exception.date = row.exception_day.InMillisecondsFSinceUnixEpoch();
  exception.exception_event_id = base::NumberToString(row.exception_event_id);
  exception.parent_event_id = base::NumberToString(row.parent_event_id);

  return exception;
}

std::vector<RecurrenceException> CreateRecurrenceException(
    const RecurrenceExceptionRows& exceptions) {
  std::vector<RecurrenceException> new_exceptions;
  for (const auto& exception : exceptions) {
    new_exceptions.push_back(CreateException(exception));
  }

  return new_exceptions;
}

int MapAccountType(const AccountType& account_type) {
  int type = 0;
  switch (account_type) {
    case vivaldi::calendar::AccountType::kLocal:
      type = 0;
      break;
    case vivaldi::calendar::AccountType::kVivaldi:
      type = 1;
      break;
    case vivaldi::calendar::AccountType::kGoogle:
      type = 2;
      break;
    case vivaldi::calendar::AccountType::kCaldav:
      type = 3;
      break;
    case vivaldi::calendar::AccountType::kIcal:
      type = 4;
      break;
    case vivaldi::calendar::AccountType::kFastmail:
      type = 5;
      break;
    case vivaldi::calendar::AccountType::kNone:
      type = 0;
      break;
  }
  return type;
}

AccountType MapAccountTypeFromDb(int type) {
  AccountType account_type = vivaldi::calendar::AccountType::kLocal;
  switch (type) {
    case calendar::ACCOUNT_TYPE_LOCAL:
      account_type = vivaldi::calendar::AccountType::kLocal;
      break;
    case calendar::ACCOUNT_TYPE_VIVALDINET:
      account_type = vivaldi::calendar::AccountType::kVivaldi;
      break;
    case calendar::ACCOUNT_TYPE_GOOGLE:
      account_type = vivaldi::calendar::AccountType::kGoogle;
      break;
    case calendar::ACCOUNT_TYPE_CALDAV:
      account_type = vivaldi::calendar::AccountType::kCaldav;
      break;
    case calendar::ACCOUNT_TYPE_FASTMAIL:
      account_type = vivaldi::calendar::AccountType::kFastmail;
      break;
    case calendar::ACCOUNT_TYPE_ICAL:
      account_type = vivaldi::calendar::AccountType::kIcal;
      break;
  }
  return account_type;
}

Notification CreateNotification(const NotificationRow& row) {
  Notification notification;
  notification.id = base::NumberToString(row.id);
  notification.event_id = base::NumberToString(row.event_id);
  notification.name = base::UTF16ToUTF8(row.name);
  notification.description = base::UTF16ToUTF8(row.description);
  notification.when = row.when.InMillisecondsFSinceUnixEpoch();
  notification.delay = row.delay;
  notification.period = row.period.InMillisecondsFSinceUnixEpoch();

  return notification;
}

std::vector<Notification> CreateNotifications(
    const NotificationRows& notifications) {
  std::vector<Notification> new_exceptions;
  for (const auto& notification : notifications) {
    new_exceptions.push_back(CreateNotification(notification));
  }

  return new_exceptions;
}

Invite CreateInviteItem(const InviteRow& row) {
  Invite invite;
  invite.id = base::NumberToString(row.id);
  invite.event_id = base::NumberToString(row.event_id);
  invite.name = base::UTF16ToUTF8(row.name);
  invite.address = base::UTF16ToUTF8(row.address);
  invite.partstat = row.partstat;
  invite.sent = row.sent;

  return invite;
}

std::vector<Invite> CreateInvites(const InviteRows& invites) {
  std::vector<Invite> new_invites;
  for (const auto& invite : invites) {
    new_invites.push_back(CreateInviteItem(invite));
  }

  return new_invites;
}

SupportedCalendarComponents GetSupportedComponents(
    int supported_component_set) {
  bool vevent = (supported_component_set & calendar::CALENDAR_VEVENT);
  bool vtodo = (supported_component_set & calendar::CALENDAR_VTODO);
  bool vjournal = (supported_component_set & calendar::CALENDAR_VJOURNAL);
  SupportedCalendarComponents supported_components_set;
  supported_components_set.vevent = vevent;
  supported_components_set.vtodo = vtodo;
  supported_components_set.vjournal = vjournal;

  return supported_components_set;
}

Calendar GetCalendarItem(const CalendarRow& row) {
  Calendar calendar;
  calendar.id = base::NumberToString(row.id());
  calendar.account_id = base::NumberToString(row.account_id());
  calendar.name = base::UTF16ToUTF8(row.name());
  calendar.description = base::UTF16ToUTF8(row.description());
  calendar.ctag = row.ctag();
  calendar.orderindex = row.orderindex();
  calendar.active = row.active();
  calendar.iconindex = row.iconindex();
  calendar.color = row.color();
  calendar.last_checked = row.last_checked().InMillisecondsFSinceUnixEpoch();
  calendar.timezone = row.timezone();
  calendar.supported_calendar_component =
      GetSupportedComponents(row.supported_component_set());

  return calendar;
}

EventType GetEventType(const EventTypeRow& row) {
  EventType event_type;
  event_type.id = base::NumberToString(row.id());
  event_type.name = base::UTF16ToUTF8(row.name());
  event_type.color = row.color();
  event_type.iconindex = row.iconindex();

  return event_type;
}

Account GetAccountType(const AccountRow& row) {
  Account account;
  account.id = base::NumberToString(row.id);
  account.name = base::UTF16ToUTF8(row.name);
  account.username = base::UTF16ToUTF8(row.username);
  account.account_type = MapAccountTypeFromDb(row.account_type);
  account.url = row.url.spec();
  account.interval = row.interval;
  return account;
}

CalendarEventRouter::CalendarEventRouter(Profile* profile,
                                         CalendarService* calendar_service)
    : profile_(profile) {
  DCHECK(profile);
  calendar_service_observation_.Observe(calendar_service);
}

CalendarEventRouter::~CalendarEventRouter() {}

void CalendarEventRouter::ExtensiveCalendarChangesBeginning(
    CalendarService* model) {}

void CalendarEventRouter::ExtensiveCalendarChangesEnded(
    CalendarService* model) {}

EventTemplate CreateEventTemplate(EventTemplateRow event_template) {
  extensions::vivaldi::calendar::EventTemplate template_event;
  template_event.id = base::NumberToString(event_template.id);
  template_event.ical = base::UTF16ToUTF8(event_template.ical);
  template_event.name = base::UTF16ToUTF8(event_template.name);
  return template_event;
}

CalendarEvent CreateVivaldiEvent(calendar::EventRow event) {
  CalendarEvent cal_event;

  cal_event.id = base::NumberToString(event.id);
  cal_event.calendar_id = base::NumberToString(event.calendar_id);
  cal_event.alarm_id = base::NumberToString(event.alarm_id);
  cal_event.title = base::UTF16ToUTF8(event.title);
  cal_event.description = base::UTF16ToUTF8(event.description);
  cal_event.start = event.start.InMillisecondsFSinceUnixEpoch();
  cal_event.end = event.end.InMillisecondsFSinceUnixEpoch();
  cal_event.all_day = event.all_day;
  cal_event.is_recurring = event.is_recurring;
  cal_event.location = base::UTF16ToUTF8(event.location);
  cal_event.url = base::UTF16ToUTF8(event.url);
  cal_event.etag = event.etag;
  cal_event.href = event.href;
  cal_event.uid = event.uid;
  cal_event.event_type_id = base::NumberToString(event.event_type_id);
  cal_event.task = event.task;
  cal_event.complete = event.complete;
  cal_event.trash = event.trash;
  cal_event.trash_time = event.trash_time.InMillisecondsFSinceUnixEpoch();
  cal_event.sequence = event.sequence;
  cal_event.ical = base::UTF16ToUTF8(event.ical);
  cal_event.rrule = event.rrule;
  cal_event.recurrence_exceptions =
      CreateRecurrenceException(event.recurrence_exceptions);

  cal_event.notifications = CreateNotifications(event.notifications);
  cal_event.invites = CreateInvites(event.invites);
  cal_event.organizer = event.organizer;
  cal_event.timezone = event.timezone;
  cal_event.priority = event.priority;
  cal_event.status = event.status;
  cal_event.percentage_complete = event.percentage_complete;
  cal_event.categories = base::UTF16ToUTF8(event.categories);
  cal_event.component_class = base::UTF16ToUTF8(event.component_class);
  cal_event.attachment = base::UTF16ToUTF8(event.attachment);
  cal_event.completed = event.completed.InMillisecondsFSinceUnixEpoch();
  cal_event.sync_pending = event.sync_pending;
  cal_event.delete_pending = event.delete_pending;
  cal_event.end_recurring = event.end_recurring.InMillisecondsFSinceUnixEpoch();
  return cal_event;
}
void CalendarEventRouter::OnEventCreated(CalendarService* service,
                                         const calendar::EventResult& event) {
  CalendarEvent createdEvent = CreateVivaldiEvent(event);

  base::Value::List args = OnEventCreated::Create(createdEvent);
  DispatchEvent(profile_, OnEventCreated::kEventName, std::move(args));
}

void CalendarEventRouter::OnIcsFileOpened(std::string path) {
  DispatchEvent(profile_, OnIcsFileOpened::kEventName,
                OnIcsFileOpened::Create(path));
}

void CalendarEventRouter::OnWebcalUrlOpened(GURL url) {
  DispatchEvent(profile_, OnWebcalUrlOpened::kEventName,
                OnWebcalUrlOpened::Create(url.spec()));
}

void CalendarEventRouter::OnMailtoOpened(GURL mailto) {
  DispatchEvent(profile_, OnMailtoOpened::kEventName,
                OnMailtoOpened::Create(mailto.spec()));
}

void CalendarEventRouter::OnNotificationChanged(
    CalendarService* service,
    const calendar::NotificationRow& row) {
  Notification changedNotification = CreateNotification(row);
  base::Value::List args = OnNotificationChanged::Create(changedNotification);
  DispatchEvent(profile_, OnNotificationChanged::kEventName, std::move(args));
}

void CalendarEventRouter::OnCalendarModified(CalendarService* service) {
  base::Value::List args;
  DispatchEvent(profile_, OnCalendarDataChanged::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void CalendarEventRouter::DispatchEvent(Profile* profile,
                                        const std::string& event_name,
                                        base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter* event_router = EventRouter::Get(profile);
    if (event_router) {
      event_router->BroadcastEvent(base::WrapUnique(
          new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                                event_name, std::move(event_args))));
    }
  }
}

void BroadcastCalendarEvent(const std::string& eventname,
                            base::Value::List args,
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
  event_router->RegisterObserver(this, OnCalendarDataChanged::kEventName);
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
  RegisterInternalHandlers();
  return g_factory_calendar.Pointer();
}

/*static*/
void CalendarAPI::RegisterInternalHandlers() {
  constexpr base::FilePath::CharType kIcsExtension[] =
      FILE_PATH_LITERAL(".ics");
  constexpr char kWebcalProtocol[] = "webcal";
  constexpr char kWMailtoProtocol[] = "mailto";

  static bool internal_handlers_registered = false;
  if (internal_handlers_registered)
    return;
  internal_handlers_registered = true;
  ::vivaldi::RegisterDownloadHandler(
      kIcsExtension, base::BindRepeating([](Profile* profile,
                                            download::DownloadItem* download) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kCalendarHandleIcsDownloads))
          return false;
        calendar_api->calendar_event_router_->OnIcsFileOpened(
            download->GetTargetFilePath().AsUTF8Unsafe());
        return true;
      }));
  ::vivaldi::RegisterProtocolHandler(
      kWebcalProtocol, base::BindRepeating([](Profile* profile, GURL url) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kCalendarHandleWebcalLinks))
          return false;
        calendar_api->calendar_event_router_->OnWebcalUrlOpened(url);
        return true;
      }));

  ::vivaldi::RegisterProtocolHandler(
      kWMailtoProtocol, base::BindRepeating([](Profile* profile, GURL mailto) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kMailMailtoInVivaldi))
          return false;
        calendar_api->calendar_event_router_->OnMailtoOpened(mailto);
        return true;
      }));
}

void CalendarAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  calendar_event_router_ = std::make_unique<CalendarEventRouter>(
      profile, CalendarServiceFactory::GetForProfile(profile));

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

CalendarService* CalendarAsyncFunction::GetCalendarService() {
  return CalendarServiceFactory::GetForProfile(GetProfile());
}

ExtensionFunction::ResponseAction CalendarGetAllEventsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEvents(
      base::BindOnce(&CalendarGetAllEventsFunction::GetAllEventsComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllEventsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllEventsFunction::GetAllEventsComplete(
    std::vector<calendar::EventRow> results) {
  EventList eventList;

  if (!results.empty()) {
    for (const auto& result : results) {
      eventList.push_back(CreateVivaldiEvent(result));
    }
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEvents::Results::Create(eventList)));
}

Profile* CalendarAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

ExtensionFunction::ResponseAction CalendarEventCreateFunction::Run() {
  std::optional<vivaldi::calendar::EventCreate::Params> params(
      vivaldi::calendar::EventCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  calendar::EventRow createEvent = calendar::GetEventRow(params->event);

  model->CreateCalendarEvent(
      createEvent,
      base::BindOnce(&CalendarEventCreateFunction::CreateEventComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventCreateFunction::CreateEventComplete(
    calendar::EventResultCB results) {
  if (!results.success) {
    Respond(Error("Error creating event. " + results.message));
  } else {
    CalendarEvent event = CreateVivaldiEvent(results.event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::EventCreate::Results::Create(event)));
  }
}

ExtensionFunction::ResponseAction CalendarEventsCreateFunction::Run() {
  std::optional<vivaldi::calendar::EventsCreate::Params> params(
      vivaldi::calendar::EventsCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  std::vector<vivaldi::calendar::CreateDetails>& events = params->events_list;
  size_t count = events.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  std::vector<calendar::EventRow> event_rows;

  for (size_t i = 0; i < count; ++i) {
    vivaldi::calendar::CreateDetails& create_details = events[i];
    calendar::EventRow createEvent = calendar::GetEventRow(create_details);
    event_rows.push_back(createEvent);
  }

  model->CreateCalendarEvents(
      event_rows,
      base::BindOnce(&CalendarEventsCreateFunction::CreateEventsComplete, this),
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
    calendar::CreateEventsResult results) {
  CreateEventsResults return_results = GetCreateEventsItem(results);
  Respond(
      ArgumentList(extensions::vivaldi::calendar::EventsCreate::Results::Create(
          return_results)));
}

ExtensionFunction::ResponseAction CalendarUpdateEventFunction::Run() {
  std::optional<vivaldi::calendar::UpdateEvent::Params> params(
      vivaldi::calendar::UpdateEvent::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::EventRow updatedEvent;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  if (params->changes.calendar_id.has_value()) {
    calendar::CalendarID calendarId;
    if (!GetStdStringAsInt64(params->changes.calendar_id.value(),
                             &calendarId)) {
      return RespondNow(Error("Error. Invalid calendar_id"));
    }
    updatedEvent.calendar_id = calendarId;
    updatedEvent.updateFields |= calendar::CALENDAR_ID;
  }

  if (params->changes.alarm_id.has_value()) {
    calendar::AlarmID alarmId;
    if (!GetStdStringAsInt64(params->changes.calendar_id.value(), &alarmId)) {
      return RespondNow(Error("Error. Invalid alarm"));
    }

    updatedEvent.alarm_id = alarmId;
    updatedEvent.updateFields |= calendar::ALARM_ID;
  }

  if (params->changes.description.has_value()) {
    updatedEvent.description =
        base::UTF8ToUTF16(params->changes.description.value());
    updatedEvent.updateFields |= calendar::DESCRIPTION;
  }

  if (params->changes.title.has_value()) {
    updatedEvent.title = base::UTF8ToUTF16(params->changes.title.value());
    updatedEvent.updateFields |= calendar::TITLE;
  }

  if (params->changes.start.has_value()) {
    double start = params->changes.start.value();
    updatedEvent.start = GetTime(start);
    updatedEvent.updateFields |= calendar::START;
  }

  if (params->changes.end.has_value()) {
    double end = params->changes.end.value();
    updatedEvent.end = GetTime(end);
    updatedEvent.updateFields |= calendar::END;
  }

  if (params->changes.all_day.has_value()) {
    updatedEvent.all_day = params->changes.all_day.value();
    updatedEvent.updateFields |= calendar::ALLDAY;
  }

  if (params->changes.is_recurring.has_value()) {
    updatedEvent.is_recurring = params->changes.is_recurring.value();
    updatedEvent.updateFields |= calendar::ISRECURRING;
  }

  if (params->changes.location.has_value()) {
    updatedEvent.location = base::UTF8ToUTF16(params->changes.location.value());
    updatedEvent.updateFields |= calendar::LOCATION;
  }

  if (params->changes.url.has_value()) {
    updatedEvent.url = base::UTF8ToUTF16(params->changes.url.value());
    updatedEvent.updateFields |= calendar::URL;
  }

  if (params->changes.etag.has_value()) {
    updatedEvent.etag = params->changes.etag.value();
    updatedEvent.updateFields |= calendar::ETAG;
  }

  if (params->changes.href.has_value()) {
    updatedEvent.href = params->changes.href.value();
    updatedEvent.updateFields |= calendar::HREF;
  }

  if (params->changes.uid.has_value()) {
    updatedEvent.uid = params->changes.uid.value();
    updatedEvent.updateFields |= calendar::UID;
  }

  if (params->changes.task.has_value()) {
    updatedEvent.task = params->changes.task.value();
    updatedEvent.updateFields |= calendar::TASK;
  }

  if (params->changes.complete.has_value()) {
    updatedEvent.complete = params->changes.complete.value();
    updatedEvent.updateFields |= calendar::COMPLETE;
  }

  if (params->changes.trash.has_value()) {
    updatedEvent.trash = params->changes.trash.value();
    updatedEvent.updateFields |= calendar::TRASH;
  }

  if (params->changes.sequence.has_value()) {
    updatedEvent.sequence = params->changes.sequence.value();
    updatedEvent.updateFields |= calendar::SEQUENCE;
  }

  if (params->changes.ical.has_value()) {
    updatedEvent.ical = base::UTF8ToUTF16(params->changes.ical.value());
    updatedEvent.updateFields |= calendar::ICAL;
  }

  if (params->changes.rrule.has_value()) {
    updatedEvent.rrule = params->changes.rrule.value();
    updatedEvent.updateFields |= calendar::RRULE;
  }

  if (params->changes.organizer.has_value()) {
    updatedEvent.organizer = params->changes.organizer.value();
    updatedEvent.updateFields |= calendar::ORGANIZER;
  }

  if (params->changes.timezone.has_value()) {
    updatedEvent.timezone = params->changes.timezone.value();
    updatedEvent.updateFields |= calendar::TIMEZONE;
  }

  if (params->changes.event_type_id.has_value()) {
    calendar::EventTypeID event_type_id;
    if (!GetStdStringAsInt64(params->changes.event_type_id.value(),
                             &event_type_id)) {
      return RespondNow(Error("Error. Invalid event_type_id"));
    }

    updatedEvent.event_type_id = event_type_id;
    updatedEvent.updateFields |= calendar::EVENT_TYPE_ID;
  }

  if (params->changes.priority.has_value()) {
    updatedEvent.priority = params->changes.priority.value();
    updatedEvent.updateFields |= calendar::PRIORITY;
  }

  if (params->changes.status.has_value()) {
    updatedEvent.status = params->changes.status.value();
    updatedEvent.updateFields |= calendar::STATUS;
  }

  if (params->changes.percentage_complete.has_value()) {
    updatedEvent.percentage_complete =
        params->changes.percentage_complete.value();
    updatedEvent.updateFields |= calendar::PERCENTAGE_COMPLETE;
  }

  if (params->changes.categories.has_value()) {
    updatedEvent.categories =
        base::UTF8ToUTF16(params->changes.categories.value());
    updatedEvent.updateFields |= calendar::CATEGORIES;
  }

  if (params->changes.component_class.has_value()) {
    updatedEvent.component_class =
        base::UTF8ToUTF16(params->changes.component_class.value());
    updatedEvent.updateFields |= calendar::COMPONENT_CLASS;
  }

  if (params->changes.attachment.has_value()) {
    updatedEvent.attachment =
        base::UTF8ToUTF16(params->changes.attachment.value());
    updatedEvent.updateFields |= calendar::ATTACHMENT;
  }

  if (params->changes.completed.has_value()) {
    updatedEvent.completed = GetTime(params->changes.completed.value());
    updatedEvent.updateFields |= calendar::COMPLETED;
  }

  if (params->changes.sync_pending.has_value()) {
    updatedEvent.sync_pending = params->changes.sync_pending.value();
    updatedEvent.updateFields |= calendar::SYNC_PENDING;
  }

  if (params->changes.delete_pending.has_value()) {
    updatedEvent.delete_pending = params->changes.delete_pending.value();
    updatedEvent.updateFields |= calendar::DELETE_PENDING;
  }

  if (params->changes.end_recurring.has_value()) {
    double end_recurring = params->changes.end_recurring.value();
    updatedEvent.end_recurring = GetTime(end_recurring);
    updatedEvent.updateFields |= calendar::END_RECURRING;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendarEvent(
      eventId, updatedEvent,
      base::BindOnce(&CalendarUpdateEventFunction::UpdateEventComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateEventComplete() will be called
                          // asynchronously.
}

void CalendarUpdateEventFunction::UpdateEventComplete(
    calendar::EventResultCB results) {
  if (!results.success) {
    Respond(Error("Error updating event"));
  } else {
    CalendarEvent event = CreateVivaldiEvent(results.event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateEvent::Results::Create(event)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventFunction::Run() {
  std::optional<vivaldi::calendar::DeleteEvent::Params> params(
      vivaldi::calendar::DeleteEvent::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendarEvent(
      eventId,
      base::BindOnce(&CalendarDeleteEventFunction::DeleteEventComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteEventComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventFunction::DeleteEventComplete(bool results) {
  if (!results) {
    Respond(Error("Error deleting event"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction
CalendarUpdateRecurrenceExceptionFunction::Run() {
  std::optional<vivaldi::calendar::UpdateRecurrenceException::Params> params(
      vivaldi::calendar::UpdateRecurrenceException::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->recurrence_id);
  calendar::RecurrenceExceptionID recurrence_id;

  calendar::RecurrenceExceptionRow recurrence_row;

  if (!GetIdAsInt64(id, &recurrence_id)) {
    return RespondNow(Error("Error. Invalid exception id"));
  }

  if (params->changes.cancelled.has_value()) {
    recurrence_row.cancelled = params->changes.cancelled.value();
    recurrence_row.updateFields |= calendar::CANCELLED;
  }

  if (params->changes.date.has_value()) {
    double date = params->changes.date.value();
    recurrence_row.exception_day = GetTime(date);
    recurrence_row.updateFields |= calendar::EXCEPTION_DAY;
  }

  if (params->changes.parent_event_id.has_value()) {
    std::u16string parent_id;
    parent_id = base::UTF8ToUTF16(params->changes.parent_event_id.value());
    calendar::EventID parent_event_id;
    if (!GetIdAsInt64(parent_id, &parent_event_id)) {
      return RespondNow(Error("Error. Invalid parent event id"));
    }
    recurrence_row.parent_event_id = parent_event_id;
    recurrence_row.updateFields |= calendar::PARENT_EVENT_ID;
  }

  if (params->changes.exception_event_id.has_value()) {
    std::u16string exceptionId;
    exceptionId = base::UTF8ToUTF16(params->changes.exception_event_id.value());
    calendar::EventID exception_event_id;
    if (!GetIdAsInt64(exceptionId, &exception_event_id)) {
      return RespondNow(Error("Error. Invalid parent event id"));
    }
    recurrence_row.exception_event_id = exception_event_id;
    recurrence_row.updateFields |= calendar::EXCEPTION_EVENT_ID;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateRecurrenceException(
      recurrence_id, recurrence_row,
      base::BindOnce(&CalendarUpdateRecurrenceExceptionFunction::
                         UpdateRecurrenceExceptionComplete,
                     this),
      &task_tracker_);
  return RespondLater();  // CalendarUpdateRecurrenceExceptionFunction() will be
                          // called asynchronously.
}

void CalendarUpdateRecurrenceExceptionFunction::
    UpdateRecurrenceExceptionComplete(calendar::EventResultCB results) {
  if (!results.success) {
    Respond(Error("Error updating recurrence exception"));
  } else {
    CalendarEvent event = CreateVivaldiEvent(results.event);
    Respond(
        ArgumentList(extensions::vivaldi::calendar::UpdateRecurrenceException::
                         Results::Create(event)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventExceptionFunction::Run() {
  std::optional<vivaldi::calendar::DeleteEventException::Params> params(
      vivaldi::calendar::DeleteEventException::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->exception_id);
  calendar::RecurrenceExceptionID exception_id;

  if (!GetIdAsInt64(id, &exception_id)) {
    return RespondNow(Error("Error. Invalid exception id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventRecurrenceException(
      exception_id,
      base::BindOnce(
          &CalendarDeleteEventExceptionFunction::DeleteEventExceptionComplete,
          this),
      &task_tracker_);
  return RespondLater();  // DeleteEventExceptionComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventExceptionFunction::DeleteEventExceptionComplete(
    calendar::EventResultCB results) {
  if (!results.success) {
    Respond(Error("Error deleting event exception"));
  } else {
    CalendarEvent event = CreateVivaldiEvent(results.event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::DeleteEventException::Results::Create(
            event)));
  }
}

vivaldi::calendar::Calendar CreateVivaldiCalendar(
    const calendar::CalendarRow& result) {
  vivaldi::calendar::Calendar calendar;

  calendar.id = base::NumberToString(result.id());
  calendar.account_id = base::NumberToString(result.account_id());
  calendar.name = base::UTF16ToUTF8(result.name());

  calendar.description = base::UTF16ToUTF8(result.description());
  calendar.orderindex = result.orderindex();
  calendar.color = result.color();
  calendar.hidden = result.hidden();
  calendar.ctag = result.ctag();
  calendar.active = result.active();
  calendar.iconindex = result.iconindex();
  calendar.last_checked = result.last_checked().InMillisecondsFSinceUnixEpoch();
  calendar.timezone = result.timezone();
  calendar.supported_calendar_component =
      GetSupportedComponents(result.supported_component_set());

  return calendar;
}

ExtensionFunction::ResponseAction CalendarCreateFunction::Run() {
  std::optional<vivaldi::calendar::Create::Params> params(
      vivaldi::calendar::Create::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::CalendarRow createCalendar;

  std::u16string name;
  name = base::UTF8ToUTF16(params->calendar.name);
  createCalendar.set_name(name);

  std::u16string account_id = base::UTF8ToUTF16(params->calendar.account_id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(account_id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  createCalendar.set_account_id(accountId);

  if (params->calendar.description.has_value()) {
    std::u16string description =
        base::UTF8ToUTF16(params->calendar.description.value());
    createCalendar.set_description(description);
  }

  if (params->calendar.orderindex.has_value()) {
    int orderindex = params->calendar.orderindex.value();
    createCalendar.set_orderindex(orderindex);
  }

  if (params->calendar.color.has_value()) {
    std::string color = params->calendar.color.value();
    createCalendar.set_color(color);
  }

  if (params->calendar.hidden.has_value()) {
    bool hidden = params->calendar.hidden.value();
    createCalendar.set_hidden(hidden);
  }

  if (params->calendar.active.has_value()) {
    bool active = params->calendar.active.value();
    createCalendar.set_active(active);
  }

  if (params->calendar.last_checked.has_value()) {
    int last_checked = params->calendar.last_checked.value();
    createCalendar.set_last_checked(GetTime(last_checked));
  }

  if (params->calendar.timezone.has_value()) {
    std::string timezone = params->calendar.timezone.value();
    createCalendar.set_timezone(timezone);
  }

  if (params->calendar.ctag.has_value()) {
    std::string timezone = params->calendar.ctag.value();
    createCalendar.set_ctag(timezone);
  }

  int supported_components = calendar::NONE;
  if (params->calendar.supported_calendar_component.vevent)
    supported_components |= calendar::CALENDAR_VEVENT;

  if (params->calendar.supported_calendar_component.vtodo)
    supported_components |= calendar::CALENDAR_VTODO;

  if (params->calendar.supported_calendar_component.vjournal)
    supported_components |= calendar::CALENDAR_VJOURNAL;

  createCalendar.set_supported_component_set(supported_components);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateCalendar(
      createCalendar,
      base::BindOnce(&CalendarCreateFunction::CreateComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarCreateFunction::CreateComplete(
    calendar::CreateCalendarResult results) {
  if (!results.success) {
    Respond(Error("Error creating calendar"));
  } else {
    Calendar ev = GetCalendarItem(results.createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::Create::Results::Create(ev)));
  }
}

CalendarGetAllFunction::~CalendarGetAllFunction() {}

ExtensionFunction::ResponseAction CalendarGetAllFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllCalendars(
      base::BindOnce(&CalendarGetAllFunction::GetAllComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllComplete() will be called
                          // asynchronously.
}

void CalendarGetAllFunction::GetAllComplete(calendar::CalendarRows results) {
  CalendarList calendarList;

  if (!results.empty()) {
    if (!results.empty()) {
      for (const auto& result : results) {
        calendarList.push_back(CreateVivaldiCalendar(result));
      }
    }
  }

  Respond(
      ArgumentList(vivaldi::calendar::GetAll::Results::Create(calendarList)));
}

ExtensionFunction::ResponseAction CalendarUpdateFunction::Run() {
  std::optional<vivaldi::calendar::Update::Params> params(
      vivaldi::calendar::Update::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::Calendar updatedCalendar;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;
  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  if (params->changes.name.has_value()) {
    updatedCalendar.name = base::UTF8ToUTF16(params->changes.name.value());
    updatedCalendar.updateFields |= calendar::CALENDAR_NAME;
  }

  if (params->changes.description.has_value()) {
    updatedCalendar.description =
        base::UTF8ToUTF16(params->changes.description.value());
    updatedCalendar.updateFields |= calendar::CALENDAR_DESCRIPTION;
  }

  if (params->changes.orderindex.has_value()) {
    updatedCalendar.orderindex = params->changes.orderindex.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_ORDERINDEX;
  }

  if (params->changes.color.has_value()) {
    updatedCalendar.color = params->changes.color.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_COLOR;
  }

  if (params->changes.hidden.has_value()) {
    updatedCalendar.hidden = params->changes.hidden.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_HIDDEN;
  }

  if (params->changes.active.has_value()) {
    updatedCalendar.active = params->changes.active.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_ACTIVE;
  }

  if (params->changes.iconindex.has_value()) {
    updatedCalendar.iconindex = params->changes.iconindex.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_ICONINDEX;
  }

  if (params->changes.ctag.has_value()) {
    updatedCalendar.ctag = params->changes.ctag.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_CTAG;
  }

  if (params->changes.last_checked.has_value()) {
    updatedCalendar.last_checked =
        GetTime(params->changes.last_checked.value());
    updatedCalendar.updateFields |= calendar::CALENDAR_LAST_CHECKED;
  }

  if (params->changes.timezone.has_value()) {
    updatedCalendar.timezone = params->changes.timezone.value();
    updatedCalendar.updateFields |= calendar::CALENDAR_TIMEZONE;
  }

  if (params->changes.supported_calendar_component.has_value()) {
    int supported_components = calendar::NONE;
    if (params->changes.supported_calendar_component->vevent)
      supported_components |= calendar::CALENDAR_VEVENT;

    if (params->changes.supported_calendar_component->vtodo)
      supported_components |= calendar::CALENDAR_VTODO;

    if (params->changes.supported_calendar_component->vjournal)
      supported_components |= calendar::CALENDAR_VJOURNAL;

    updatedCalendar.supported_component_set = supported_components;
    updatedCalendar.updateFields |= calendar::CALENDAR_SUPPORTED_COMPONENT_SET;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendar(
      calendarId, updatedCalendar,
      base::BindOnce(&CalendarUpdateFunction::UpdateCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateCalendarComplete() will be called
                          // asynchronously.
}

void CalendarUpdateFunction::UpdateCalendarComplete(calendar::StatusCB cb) {
  if (!cb.success) {
    Respond(ErrorWithArguments(
        vivaldi::calendar::Update::Results::Create(false), cb.message));
  } else {
    Respond(ArgumentList(vivaldi::calendar::Update::Results::Create(true)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteFunction::Run() {
  std::optional<vivaldi::calendar::Delete::Params> params(
      vivaldi::calendar::Delete::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;

  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendar(
      calendarId,
      base::BindOnce(&CalendarDeleteFunction::DeleteCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteCalendarComplete() will be called
                          // asynchronously.
}

void CalendarDeleteFunction::DeleteCalendarComplete(bool results) {
  if (!results) {
    Respond(Error("Error deleting calendar"));
  } else {
    Respond(ArgumentList(vivaldi::calendar::Delete::Results::Create(true)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllEventTypesFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEventTypes(
      base::BindOnce(
          &CalendarGetAllEventTypesFunction::GetAllEventTypesComplete, this),
      &task_tracker_);

  return RespondLater();  // CalendarGetAllEventTypesFunction() will be called
                          // asynchronously.
}

void CalendarGetAllEventTypesFunction::GetAllEventTypesComplete(
    calendar::EventTypeRows results) {
  EventTypeList event_type_list;
  for (EventTypeRow event_type : results) {
    event_type_list.push_back(GetEventType(std::move(event_type)));
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEventTypes::Results::Create(event_type_list)));
}

ExtensionFunction::ResponseAction CalendarEventTypeCreateFunction::Run() {
  std::optional<vivaldi::calendar::EventTypeCreate::Params> params(
      vivaldi::calendar::EventTypeCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::EventTypeRow create_event_type;

  std::u16string name;
  name = base::UTF8ToUTF16(params->event_type.name);
  create_event_type.set_name(name);

  if (params->event_type.color.has_value()) {
    std::string color = params->event_type.color.value();
    create_event_type.set_color(color);
  }

  if (params->event_type.iconindex.has_value()) {
    int iconindex = params->event_type.iconindex.value();
    create_event_type.set_iconindex(iconindex);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateEventType(
      create_event_type,
      base::BindOnce(&CalendarEventTypeCreateFunction::CreateEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeCreateFunction::CreateEventTypeComplete(bool results) {
  if (!results) {
    Respond(Error("Error creating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarEventTypeUpdateFunction::Run() {
  std::optional<vivaldi::calendar::EventTypeUpdate::Params> params(
      vivaldi::calendar::EventTypeUpdate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  calendar::EventType update_event_type;

  if (params->changes.name.has_value()) {
    std::u16string name;
    name = base::UTF8ToUTF16(params->changes.name.value());
    update_event_type.name = name;
    update_event_type.updateFields |= calendar::NAME;
  }

  if (params->changes.color.has_value()) {
    std::string color = params->changes.color.value();
    update_event_type.color = color;
    update_event_type.updateFields |= calendar::COLOR;
  }

  if (params->changes.iconindex.has_value()) {
    int iconindex = params->changes.iconindex.value();
    update_event_type.iconindex = iconindex;
    update_event_type.updateFields |= calendar::ICONINDEX;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateEventType(
      event_type_id, update_event_type,
      base::BindOnce(&CalendarEventTypeUpdateFunction::UpdateEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeUpdateFunction::UpdateEventTypeComplete(bool results) {
  if (!results) {
    Respond(Error("Error updating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventTypeFunction::Run() {
  std::optional<vivaldi::calendar::DeleteEventType::Params> params(
      vivaldi::calendar::DeleteEventType::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventType(
      event_type_id,
      base::BindOnce(&CalendarDeleteEventTypeFunction::DeleteEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();  // DeleteEventTypeComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventTypeFunction::DeleteEventTypeComplete(bool result) {
  if (!result) {
    Respond(Error("Error deleting event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarCreateEventExceptionFunction::Run() {
  std::optional<vivaldi::calendar::CreateEventException::Params> params(
      vivaldi::calendar::CreateEventException::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->parent_event_id);
  calendar::EventID parent_event_id;

  if (!GetIdAsInt64(id, &parent_event_id)) {
    return RespondNow(Error("Error. Invalid parent event id"));
  }

  RecurrenceExceptionRow row;
  row.parent_event_id = parent_event_id;
  row.exception_day = GetTime(params->date.value_or(0));
  row.cancelled = params->cancelled;

  if (params->exception_event_id.has_value()) {
    std::string ex_id = params->exception_event_id.value();
    if (ex_id.length() > 0) {
      calendar::EventID exception_event_id;
      if (!GetStdStringAsInt64(ex_id, &exception_event_id)) {
        return RespondNow(Error("Error. Invalid exception event id"));
      }
      row.exception_event_id = exception_event_id;
    }
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateRecurrenceException(
      row,
      base::BindOnce(
          &CalendarCreateEventExceptionFunction::CreateEventExceptionComplete,
          this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateEventExceptionFunction::CreateEventExceptionComplete(
    calendar::EventResultCB results) {
  if (!results.success) {
    Respond(Error("Error creating event exception"));
  } else {
    CalendarEvent event = CreateVivaldiEvent(results.event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateEventException::Results::Create(
            event)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllNotificationsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllNotifications(
      base::BindOnce(
          &CalendarGetAllNotificationsFunction::GetAllNotificationsComplete,
          this),
      &task_tracker_);

  return RespondLater();  // GetAllNotificationsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllNotificationsFunction::GetAllNotificationsComplete(
    calendar::GetAllNotificationResult result) {
  NotificaionList notification_list;

  for (const auto& notification : result.notifications) {
    notification_list.push_back(CreateNotification(notification));
  }

  Respond(ArgumentList(vivaldi::calendar::GetAllNotifications::Results::Create(
      notification_list)));
}

ExtensionFunction::ResponseAction CalendarCreateNotificationFunction::Run() {
  std::optional<vivaldi::calendar::CreateNotification::Params> params(
      vivaldi::calendar::CreateNotification::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  NotificationRow row;
  if (params->create_notification.event_id.has_value()) {
    std::u16string id;
    id = base::UTF8ToUTF16(params->create_notification.event_id.value());
    calendar::EventID event_id;

    if (!GetIdAsInt64(id, &event_id)) {
      return RespondNow(Error("Error. Invalid event id"));
    }
    row.event_id = event_id;
  }

  row.name = base::UTF8ToUTF16(params->create_notification.name);
  row.when = GetTime(params->create_notification.when);
  if (params->create_notification.description.has_value()) {
    row.description =
        base::UTF8ToUTF16(params->create_notification.description.value());
  }

  if (params->create_notification.delay.has_value()) {
    row.delay = params->create_notification.delay.value();
  }

  if (params->create_notification.period.has_value()) {
    row.period = GetTime(params->create_notification.period.value());
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateNotification(
      row,
      base::BindOnce(
          &CalendarCreateNotificationFunction::CreateNotificationComplete,
          this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateNotificationFunction::CreateNotificationComplete(
    calendar::NotificationResult results) {
  if (!results.success) {
    Respond(Error("Error creating notification"));
  } else {
    Notification notification = CreateNotification(results.notification_row);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateNotification::Results::Create(
            notification)));
  }
}

ExtensionFunction::ResponseAction CalendarUpdateNotificationFunction::Run() {
  std::optional<vivaldi::calendar::UpdateNotification::Params> params(
      vivaldi::calendar::UpdateNotification::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::UpdateNotificationRow update_notification;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid notification id"));
  }

  update_notification.notification_row.id = eventId;

  if (params->changes.name.has_value()) {
    update_notification.notification_row.name =
        base::UTF8ToUTF16(params->changes.name.value());
    update_notification.updateFields |= calendar::NOTIFICATION_NAME;
  }

  if (params->changes.description.has_value()) {
    update_notification.notification_row.description =
        base::UTF8ToUTF16(params->changes.description.value());
    update_notification.updateFields |= calendar::NOTIFICATION_DESCRIPTION;
  }

  if (params->changes.when.has_value()) {
    double when = params->changes.when.value();

    update_notification.notification_row.when = GetTime(when);
    update_notification.updateFields |= calendar::NOTIFICATION_WHEN;
  }

  if (params->changes.period.has_value()) {
    update_notification.notification_row.period =
        GetTime(params->changes.period.value());
    update_notification.updateFields |= calendar::NOTIFICATION_PERIOD;
  }

  if (params->changes.delay.has_value()) {
    update_notification.notification_row.delay = params->changes.delay.value();
    update_notification.updateFields |= calendar::NOTIFICATION_DELAY;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateNotification(
      eventId, update_notification,
      base::BindOnce(
          &CalendarUpdateNotificationFunction::UpdateNotificationComplete,
          this),
      &task_tracker_);
  return RespondLater();  // UpdateNotificationComplete() will be called
                          // asynchronously.
}

void CalendarUpdateNotificationFunction::UpdateNotificationComplete(
    calendar::NotificationResult results) {
  if (!results.success) {
    Respond(Error(results.message));
  } else {
    Notification notification = CreateNotification(results.notification_row);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateNotification::Results::Create(
            notification)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteNotificationFunction::Run() {
  std::optional<vivaldi::calendar::DeleteNotification::Params> params(
      vivaldi::calendar::DeleteNotification::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::NotificationID notification_id;

  if (!GetIdAsInt64(id, &notification_id)) {
    return RespondNow(Error("Error. Invalid notification id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteNotification(
      notification_id,
      base::BindOnce(
          &CalendarDeleteNotificationFunction::DeleteNotificationComplete,
          this),
      &task_tracker_);
  return RespondLater();  // DeleteNotificationComplete() will be called
                          // asynchronously.
}

void CalendarDeleteNotificationFunction::DeleteNotificationComplete(
    bool results) {
  if (!results) {
    Respond(Error("Error deleting event"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarCreateInviteFunction::Run() {
  std::optional<vivaldi::calendar::CreateInvite::Params> params(
      vivaldi::calendar::CreateInvite::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  InviteRow row;

  std::u16string id;

  id = base::UTF8ToUTF16(params->create_invite.event_id);
  calendar::EventID event_id;

  if (!GetIdAsInt64(id, &event_id)) {
    return RespondNow(Error("Error. Invalid event id"));
  }
  row.event_id = event_id;
  row.name = base::UTF8ToUTF16(params->create_invite.name);
  row.address = base::UTF8ToUTF16(params->create_invite.address);

  if (params->create_invite.sent.has_value()) {
    row.sent = params->create_invite.sent.value();
  }

  if (params->create_invite.partstat.has_value()) {
    row.partstat = params->create_invite.partstat.value();
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateInvite(
      row,
      base::BindOnce(&CalendarCreateInviteFunction::CreateInviteComplete, this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateInviteFunction::CreateInviteComplete(
    calendar::InviteResult results) {
  if (!results.success) {
    Respond(Error("Error creating invite"));
  } else {
    vivaldi::calendar::Invite invite = CreateInviteItem(results.inviteRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateInvite::Results::Create(invite)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteInviteFunction::Run() {
  std::optional<vivaldi::calendar::DeleteNotification::Params> params(
      vivaldi::calendar::DeleteNotification::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::InviteID invite_id;

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteInvite(
      invite_id,
      base::BindOnce(&CalendarDeleteInviteFunction::DeleteInviteComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteInviteComplete() will be called
                          // asynchronously.
}

void CalendarDeleteInviteFunction::DeleteInviteComplete(bool results) {
  if (!results) {
    Respond(Error("Error deleting invite"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarUpdateInviteFunction::Run() {
  std::optional<vivaldi::calendar::UpdateInvite::Params> params(
      vivaldi::calendar::UpdateInvite::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->update_invite.id);
  calendar::InviteID invite_id;

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  calendar::UpdateInviteRow updateInvite;

  updateInvite.invite_row.id = invite_id;

  if (params->update_invite.address.has_value()) {
    updateInvite.invite_row.address =
        base::UTF8ToUTF16(params->update_invite.address.value());
    updateInvite.updateFields |= calendar::INVITE_ADDRESS;
  }

  if (params->update_invite.name.has_value()) {
    updateInvite.invite_row.name =
        base::UTF8ToUTF16(params->update_invite.name.value());
    updateInvite.updateFields |= calendar::INVITE_NAME;
  }

  if (params->update_invite.partstat.has_value()) {
    updateInvite.invite_row.partstat = params->update_invite.partstat.value();
    updateInvite.updateFields |= calendar::INVITE_PARTSTAT;
  }

  if (params->update_invite.sent.has_value()) {
    updateInvite.invite_row.sent = params->update_invite.sent.value();
    updateInvite.updateFields |= calendar::INVITE_SENT;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateInvite(
      updateInvite,
      base::BindOnce(&CalendarUpdateInviteFunction::UpdateInviteComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateInviteComplete() will be called
                          // asynchronously.
}

void CalendarUpdateInviteFunction::UpdateInviteComplete(
    calendar::InviteResult results) {
  if (!results.success) {
    Respond(Error("Error updating invite"));
  } else {
    vivaldi::calendar::Invite invite = CreateInviteItem(results.inviteRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateInvite::Results::Create(invite)));
  }
}

ExtensionFunction::ResponseAction CalendarCreateAccountFunction::Run() {
  std::optional<vivaldi::calendar::CreateAccount::Params> params(
      vivaldi::calendar::CreateAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  AccountRow row;
  row.name = base::UTF8ToUTF16(params->to_create.name);
  row.url = GURL(params->to_create.url);
  row.account_type = MapAccountType(params->to_create.account_type);

  if (params->to_create.interval) {
    row.interval = *params->to_create.interval;
  }
  row.username = base::UTF8ToUTF16(params->to_create.username);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateAccount(
      row,
      base::BindOnce(&CalendarCreateAccountFunction::CreateAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateAccountFunction::CreateAccountComplete(
    calendar::CreateAccountResult results) {
  if (!results.success) {
    Respond(Error("Error creating account"));
  } else {
    vivaldi::calendar::Account account = GetAccountType(results.createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateAccount::Results::Create(
            account)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteAccountFunction::Run() {
  std::optional<vivaldi::calendar::DeleteAccount::Params> params(
      vivaldi::calendar::DeleteAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->DeleteAccount(
      accountId,
      base::BindOnce(&CalendarDeleteAccountFunction::DeleteAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarDeleteAccountFunction::DeleteAccountComplete(
    calendar::DeleteAccountResult results) {
  if (!results.success) {
    Respond(Error("Error deleting account"));
  } else {
    Respond(ArgumentList(
        extensions::vivaldi::calendar::DeleteAccount::Results::Create(
            results.success)));
  }
}

ExtensionFunction::ResponseAction CalendarUpdateAccountFunction::Run() {
  std::optional<vivaldi::calendar::UpdateAccount::Params> params(
      vivaldi::calendar::UpdateAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  AccountRow row;
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  row.id = accountId;

  if (params->changes.name.has_value()) {
    std::u16string name;
    name = base::UTF8ToUTF16(params->changes.name.value());
    row.name = name;
    row.updateFields |= calendar::ACCOUNT_NAME;
  }

  if (params->changes.username.has_value()) {
    std::u16string username;
    username = base::UTF8ToUTF16(params->changes.username.value());
    row.username = username;
    row.updateFields |= calendar::ACCOUNT_USERNAME;
  }

  if (params->changes.url.has_value()) {
    row.url = GURL(params->changes.url.value());
    row.updateFields |= calendar::ACCOUNT_URL;
  }

  if (params->changes.account_type != vivaldi::calendar::AccountType::kNone) {
    row.account_type = MapAccountType(params->changes.account_type);
    row.updateFields |= calendar::ACCOUNT_TYPE;
  }

  if (params->changes.interval.has_value()) {
    row.interval = params->changes.interval.value();
    row.updateFields |= calendar::ACCOUNT_INTERVAL;
  }

  model->UpdateAccount(
      row,
      base::BindOnce(&CalendarUpdateAccountFunction::UpdateAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarUpdateAccountFunction::UpdateAccountComplete(
    calendar::UpdateAccountResult results) {
  if (!results.success) {
    Respond(Error("Error updating account"));
  } else {
    vivaldi::calendar::Account account = GetAccountType(results.updatedRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateAccount::Results::Create(
            account)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllAccountsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllAccounts(
      base::BindOnce(&CalendarGetAllAccountsFunction::GetAllAccountsComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarGetAllAccountsFunction::GetAllAccountsComplete(
    calendar::AccountRows accounts) {
  AccountList accountList;

  for (calendar::AccountRow account : accounts) {
    accountList.push_back(GetAccountType(account));
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllAccounts::Results::Create(accountList)));
}
ExtensionFunction::ResponseAction CalendarCreateEventTemplateFunction::Run() {
  std::optional<vivaldi::calendar::CreateEventTemplate::Params> params(
      vivaldi::calendar::CreateEventTemplate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  EventTemplateRow event_template;
  event_template.name = base::UTF8ToUTF16(params->name);
  event_template.ical = base::UTF8ToUTF16(params->ical);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateEventTemplate(
      event_template,
      base::BindOnce(
          &CalendarCreateEventTemplateFunction::CreateEventTemplateComplete,
          this),
      &task_tracker_);
  return RespondLater();
}

void CalendarCreateEventTemplateFunction::CreateEventTemplateComplete(
    EventTemplateResultCB result) {
  if (!result.success) {
    Respond(Error("Error creating event template. " + result.message));
  } else {
    vivaldi::calendar::EventTemplate event_template =
        CreateEventTemplate(result.event_template);
    Respond(
        ArgumentList(vivaldi::calendar::CreateEventTemplate::Results::Create(
            event_template)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllEventTemplatesFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEventTemplates(
      base::BindOnce(
          &CalendarGetAllEventTemplatesFunction::GetAllEventTemplatesComplete,
          this),
      &task_tracker_);

  return RespondLater();  // GetAllEventTemplatesComplete() will be called
                          // asynchronously.
}

void CalendarGetAllEventTemplatesFunction::GetAllEventTemplatesComplete(
    EventTemplateRows results) {
  EventTemplateList template_list;

  if (!results.empty()) {
    for (const auto& result : results) {
      template_list.push_back(CreateEventTemplate(result));
    }
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEventTemplates::Results::Create(template_list)));
}

ExtensionFunction::ResponseAction CalendarUpdateEventTemplateFunction::Run() {
  std::optional<vivaldi::calendar::UpdateEventTemplate::Params> params(
      vivaldi::calendar::UpdateEventTemplate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  calendar::EventTemplateRow update_event_row;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTemplateID event_template_id;

  if (!GetIdAsInt64(id, &event_template_id)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  if (params->changes.name.has_value()) {
    update_event_row.name = base::UTF8ToUTF16(params->changes.name.value());
    update_event_row.updateFields |= calendar::TEMPLATE_NAME;
  }

  if (params->changes.ical.has_value()) {
    update_event_row.ical = base::UTF8ToUTF16(params->changes.ical.value());
    update_event_row.updateFields |= calendar::TEMPLATE_ICAL;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateEventTemplate(
      event_template_id, update_event_row,
      base::BindOnce(
          &CalendarUpdateEventTemplateFunction::UpdateEventTemplateComplete,
          this),
      &task_tracker_);
  return RespondLater();
}

void CalendarUpdateEventTemplateFunction::UpdateEventTemplateComplete(
    EventTemplateResultCB result) {
  if (!result.success) {
    Respond(Error("Error updating event template"));
  } else {
    EventTemplate event = CreateEventTemplate(result.event_template);
    Respond(ArgumentList(
        vivaldi::calendar::UpdateEventTemplate::Results::Create(event)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventTemplateFunction::Run() {
  std::optional<vivaldi::calendar::DeleteEventTemplate::Params> params(
      vivaldi::calendar::DeleteEventTemplate::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTemplateID event_template_id;

  if (!GetIdAsInt64(id, &event_template_id)) {
    return RespondNow(Error("Error. Invalid event template id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventTemplate(
      event_template_id,
      base::BindOnce(
          &CalendarDeleteEventTemplateFunction::DeleteEventTemplateComplete,
          this),
      &task_tracker_);
  return RespondLater();
}

void CalendarDeleteEventTemplateFunction::DeleteEventTemplateComplete(
    bool result) {
  if (!result) {
    Respond(Error("Error deleting event template"));
  } else {
    Respond(ArgumentList(
        vivaldi::calendar::DeleteEventTemplate::Results::Create(result)));
  }
}

ExtensionFunction::ResponseAction CalendarGetParentExceptionIdFunction::Run() {
  std::optional<vivaldi::calendar::GetParentExceptionId::Params> params(
      vivaldi::calendar::GetParentExceptionId::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string id;
  id = base::UTF8ToUTF16(params->exception_event_id);
  calendar::EventTemplateID event_exception_id;

  if (!GetIdAsInt64(id, &event_exception_id)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetParentExceptionEventId(
      event_exception_id,
      base::BindOnce(
          &CalendarGetParentExceptionIdFunction::GetParentExceptionIdComplete,
          this),
      &task_tracker_);
  return RespondLater();
}

void CalendarGetParentExceptionIdFunction::GetParentExceptionIdComplete(
    calendar::EventID parent_event_id) {
  if (!parent_event_id) {
    Respond(ArgumentList(
        vivaldi::calendar::GetParentExceptionId::Results::Create("")));
  } else {
    std::string id = std::to_string(parent_event_id);
    Respond(ArgumentList(
        vivaldi::calendar::GetParentExceptionId::Results::Create(id)));
  }
}

}  //  namespace extensions
