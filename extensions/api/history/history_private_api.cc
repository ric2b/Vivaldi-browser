// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/history/history_private_api.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "db/vivaldi_history_types.h"
#include "extensions/schema/history_private.h"
#include "extensions/tools/vivaldi_tools.h"

using vivaldi::GetTime;

namespace extensions {

namespace {

using vivaldi::history_private::HistoryPrivateItem;
namespace Search = vivaldi::history_private::Search;
namespace OnVisitModified = vivaldi::history_private::OnVisitModified;

using vivaldi::history_private::TopUrlItem;
using vivaldi::history_private::TransitionType;

typedef std::vector<vivaldi::history_private::HistoryPrivateItem>
    HistoryItemList;
namespace GetTopUrlsPerDay = vivaldi::history_private::GetTopUrlsPerDay;
typedef std::vector<vivaldi::history_private::HistoryPrivateItem>
    VisitsPrivateList;

namespace VisitSearch = vivaldi::history_private::VisitSearch;
typedef std::vector<vivaldi::history_private::TopUrlItem> TopSitesPerDayList;

using bookmarks::BookmarkModel;

history::HistoryService* GetFunctionCallerHistoryService(
    ExtensionFunction& fun) {
  Profile* profile = GetFunctionCallerProfile(fun);
  if (!profile)
    return nullptr;
  return HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

}  // namespace

HistoryPrivateAPI::HistoryPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnVisitModified::kEventName);
}

HistoryPrivateAPI::~HistoryPrivateAPI() {}

void HistoryPrivateAPI::Shutdown() {
  history_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// static
ui::PageTransition HistoryPrivateAPI::PrivateHistoryTransitionToUiTransition(
    vivaldi::history_private::TransitionType transition) {
  switch (transition) {
    case vivaldi::history_private::TransitionType::kLink:
      return ui::PAGE_TRANSITION_LINK;
    case vivaldi::history_private::TransitionType::kTyped:
      return ui::PAGE_TRANSITION_TYPED;
    case vivaldi::history_private::TransitionType::kAutoBookmark:
      return ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    case vivaldi::history_private::TransitionType::kAutoSubframe:
      return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    case vivaldi::history_private::TransitionType::kManualSubframe:
      return ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    case vivaldi::history_private::TransitionType::kGenerated:
      return ui::PAGE_TRANSITION_GENERATED;
    case vivaldi::history_private::TransitionType::kAutoToplevel:
      return ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    case vivaldi::history_private::TransitionType::kFormSubmit:
      return ui::PAGE_TRANSITION_FORM_SUBMIT;
    case vivaldi::history_private::TransitionType::kReload:
      return ui::PAGE_TRANSITION_RELOAD;
    case vivaldi::history_private::TransitionType::kKeyword:
      return ui::PAGE_TRANSITION_KEYWORD;
    case vivaldi::history_private::TransitionType::kKeywordGenerated:
      return ui::PAGE_TRANSITION_KEYWORD_GENERATED;
    default:
      NOTREACHED();
  }
  // We have to return something
  return ui::PAGE_TRANSITION_LINK;
}
// static
vivaldi::history_private::TransitionType
HistoryPrivateAPI::UiTransitionToPrivateHistoryTransition(
    ui::PageTransition transition) {
  switch (transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      return vivaldi::history_private::TransitionType::kLink;
    case ui::PAGE_TRANSITION_TYPED:
      return vivaldi::history_private::TransitionType::kTyped;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return vivaldi::history_private::TransitionType::kAutoBookmark;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      return vivaldi::history_private::TransitionType::kAutoSubframe;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      return vivaldi::history_private::TransitionType::kManualSubframe;
    case ui::PAGE_TRANSITION_GENERATED:
      return vivaldi::history_private::TransitionType::kGenerated;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      return vivaldi::history_private::TransitionType::kAutoToplevel;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return vivaldi::history_private::TransitionType::kFormSubmit;
    case ui::PAGE_TRANSITION_RELOAD:
      return vivaldi::history_private::TransitionType::kReload;
    case ui::PAGE_TRANSITION_KEYWORD:
      return vivaldi::history_private::TransitionType::kKeyword;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return vivaldi::history_private::TransitionType::kKeywordGenerated;
    default:
      NOTREACHED();
  }
  // We have to return something
  return vivaldi::history_private::TransitionType::kLink;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<HistoryPrivateAPI>>::
    DestructorAtExit g_factory_history = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<HistoryPrivateAPI>*
HistoryPrivateAPI::GetFactoryInstance() {
  return g_factory_history.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    HistoryPrivateAPI>::DeclareFactoryDependencies() {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

void HistoryPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  history_event_router_.reset(new HistoryPrivateEventRouter(
      profile, HistoryServiceFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

HistoryPrivateEventRouter::HistoryPrivateEventRouter(
    Profile* profile,
    history::HistoryService* history_service)
    : profile_(profile), history_service_observer_(this) {
  DCHECK(profile);
  history_service_observer_.Observe(history_service);
}

HistoryPrivateEventRouter::~HistoryPrivateEventRouter() {}

void HistoryPrivateEventRouter::OnURLsModified(
    history::HistoryService* history_service,
    const history::URLRows& changed_urls) {
  OnVisitModified::Modified modified;
  std::vector<std::string> urls;
  for (const auto& row : changed_urls) {
    urls.push_back(row.url().spec());
  }
  modified.urls = std::move(urls);
  base::Value::List args = OnVisitModified::Create(modified);

  DispatchEvent(profile_, OnVisitModified::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void HistoryPrivateEventRouter::DispatchEvent(Profile* profile,
                                              const std::string& event_name,
                                              base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter::Get(profile)->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

ExtensionFunction::ResponseAction HistoryPrivateSearchFunction::Run() {
  std::optional<Search::Params> params(Search::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string search_text = base::UTF8ToUTF16(params->query.text);

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (params->query.start_time.has_value())
    options.begin_time = GetTime(params->query.start_time.value());
  if (params->query.end_time.has_value())
    options.end_time = GetTime(params->query.end_time.value());
  if (params->query.max_results.has_value())
    options.max_count = params->query.max_results.value();

  if (params->query.result_grouping !=
      vivaldi::history_private::HistoryResultSetGrouping::kNone) {
    if (params->query.result_grouping ==
        vivaldi::history_private::HistoryResultSetGrouping::
            kKeepAllDuplicates) {
      options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
    } else if (params->query.result_grouping ==
               vivaldi::history_private::HistoryResultSetGrouping::
                   kRemoveDuplicatesPerDay) {
      options.duplicate_policy =
          history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
    }
  }

  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  hs->QueryHistory(
      search_text, options,
      base::BindOnce(&HistoryPrivateSearchFunction::SearchComplete, this),
      &task_tracker_);
  return RespondLater();
}

HistoryPrivateItem GetHistoryAndVisitItem(const history::URLResult& row,
                                          BookmarkModel* bookmark_model) {
  HistoryPrivateItem history_item;

  if (row.visit_time().is_null()) {
    // If the row has visitTime, id is not unique. Caused by grouping.
    history_item.id = row.id();
  } else {
    history_item.id =
        std::to_string(row.visit_time().InMillisecondsFSinceUnixEpoch());
  }
  history_item.is_bookmarked = bookmark_model->IsBookmarked(row.url());
  history_item.visit_time = row.visit_time().InMillisecondsFSinceUnixEpoch();
  history_item.url = row.url().spec();
  history_item.title = base::UTF16ToUTF8(row.title());
  history_item.last_visit_time =
      row.last_visit().InMillisecondsFSinceUnixEpoch();
  history_item.typed_count = row.typed_count();
  history_item.visit_count = row.visit_count();

  return history_item;
}

void HistoryPrivateSearchFunction::SearchComplete(
    history::QueryResults results) {
  Profile* profile = GetFunctionCallerProfile(*this);
  if (!profile)
    return;
  HistoryItemList history_item_vec;
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (!results.empty()) {
    for (const auto& item : results)
      history_item_vec.push_back(GetHistoryAndVisitItem(item, model));
  }
  Respond(ArgumentList(Search::Results::Create(history_item_vec)));
}

ExtensionFunction::ResponseAction HistoryPrivateDeleteVisitsFunction::Run() {
  std::optional<vivaldi::history_private::DeleteVisits::Params> params(
      vivaldi::history_private::DeleteVisits::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error))
    return RespondNow(Error(error));

  base::Time time = GetTime(params->details.time);

  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  // This implementation is copied from BrowsingHistoryService::RemoveVisits
  std::vector<history::ExpireHistoryArgs> expire_list(1);
  expire_list.back().urls.insert(url);
  expire_list.back().SetTimeRangeForOneDay(time);

  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
  sync_pb::GlobalIdDirective* global_id_directive =
      delete_directive.mutable_global_id_directive();
  global_id_directive->add_global_id(time.ToInternalValue());
  global_id_directive->set_start_time_usec(
      (expire_list.back().begin_time - base::Time::UnixEpoch())
          .InMicroseconds());
  base::Time end_time =
      std::min(expire_list.back().end_time, base::Time::Now());
  global_id_directive->set_end_time_usec(
      (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

  hs->ProcessLocalDeleteDirective(delete_directive);
  hs->ExpireHistory(
      expire_list,
      base::BindOnce(&HistoryPrivateDeleteVisitsFunction::DeleteVisitComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

void HistoryPrivateDeleteVisitsFunction::DeleteVisitComplete() {
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction
HistoryPrivateGetTopUrlsPerDayFunction::Run() {
  std::optional<GetTopUrlsPerDay::Params> params(
      GetTopUrlsPerDay::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  double number_of_days = kMaxResultsWithinDay;
  if (params->max_top_url_results)
    number_of_days = params->max_top_url_results;

  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  hs->TopUrlsPerDay(
      number_of_days,
      base::BindOnce(&HistoryPrivateGetTopUrlsPerDayFunction::TopUrlsComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

std::unique_ptr<TopUrlItem> GetTopUrlPerDay(history::UrlVisitCount visit) {
  std::unique_ptr<TopUrlItem> history_item(new TopUrlItem());

  history_item->date = visit.date();
  history_item->url = visit.url().spec();
  history_item->number_of_visit = visit.count();

  return history_item;
}

void HistoryPrivateGetTopUrlsPerDayFunction::TopUrlsComplete(
    const history::UrlVisitCount::TopUrlsPerDayList& results) {
  TopSitesPerDayList history_item_vec;

  for (auto& item : results) {
    history_item_vec.push_back(std::move(*(GetTopUrlPerDay(item))));
  }
  Respond(ArgumentList(GetTopUrlsPerDay::Results::Create(history_item_vec)));
}

std::unique_ptr<HistoryPrivateItem> GetVisitsItem(
    history::Visit visit,
    BookmarkModel* bookmark_model) {
  std::unique_ptr<HistoryPrivateItem> history_item(new HistoryPrivateItem());

  base::Time visitTime = visit.visit_time;
  base::Time::Exploded exploded;
  visitTime.LocalExplode(&exploded);

  history_item->id = visit.id;
  history_item->url = visit.url.spec();
  history_item->protocol = visit.url.spec();
  history_item->address = visit.url.host();
  history_item->title = base::UTF16ToUTF8(visit.title);
  history_item->visit_time = visit.visit_time.InMillisecondsFSinceUnixEpoch();
  history_item->is_bookmarked = bookmark_model->IsBookmarked(visit.url);
  history_item->date_key = base::StringPrintf(
      "%04d-%02d-%02d", exploded.year, exploded.month, exploded.day_of_month);
  // history_item->date_key.reset(new std::string(base::StringPrintf(
  //    "%04d-%02d-%02d", exploded.year, exploded.month,
  //    exploded.day_of_month)));
  history_item->hour = int(exploded.hour);
  history_item->visit_count = int(visit.visit_count);

  history_item->transition_type =
      HistoryPrivateAPI::UiTransitionToPrivateHistoryTransition(
          visit.transition);
  return history_item;
}

ExtensionFunction::ResponseAction HistoryPrivateVisitSearchFunction::Run() {
  std::optional<VisitSearch::Params> params(
      VisitSearch::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  history::QueryOptions options;

  if (params->query.start_time.has_value())
    options.begin_time = GetTime(params->query.start_time.value());
  if (params->query.end_time.has_value())
    options.end_time = GetTime(params->query.end_time.value());

  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  hs->VisitSearch(
      options,
      base::BindOnce(&HistoryPrivateVisitSearchFunction::VisitsComplete, this),
      &task_tracker_);
  return RespondLater();
}

// Callback for the history service to acknowledge visits search complete.
void HistoryPrivateVisitSearchFunction::VisitsComplete(
    const history::Visit::VisitsList& visit_list) {
  Profile* profile = GetFunctionCallerProfile(*this);
  if (!profile)
    return;
  VisitsPrivateList history_item_vec;
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);

  for (auto& item : visit_list) {
    history_item_vec.push_back(std::move(*(GetVisitsItem(item, model))));
  }
  Respond(ArgumentList(VisitSearch::Results::Create(history_item_vec)));
}

ExtensionFunction::ResponseAction HistoryPrivateGetTypedHistoryFunction::Run() {
  std::optional<vivaldi::history_private::GetTypedHistory::Params> params(
      vivaldi::history_private::GetTypedHistory::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  history::TypedUrlResults results;
  history::KeywordID prefix_keyword_id;
  base::StringToInt64(params->prefix_keyword_id, &prefix_keyword_id);

  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  hs->GetVivaldiTypedHistory(
      params->query, prefix_keyword_id, params->max_results,
      base::BindOnce(
          &HistoryPrivateGetTypedHistoryFunction::TypedHistorySearchComplete,
          this),

      &task_tracker_);

  return RespondLater();
}

void HistoryPrivateGetTypedHistoryFunction::TypedHistorySearchComplete(
    const history::TypedUrlResults& results) {
  std::vector<vivaldi::history_private::TypedHistoryItem> response;
  for (const auto& result : results) {
    vivaldi::history_private::TypedHistoryItem item;
    item.url.assign(result.url.spec());
    item.title.assign(result.title);
    item.keyword_id = base::NumberToString(result.keyword_id);
    item.terms.assign(result.terms);
    item.typed_count = result.typed_count;
    response.push_back(std::move(item));
  }

  Respond(ArgumentList(
      vivaldi::history_private::GetTypedHistory::Results::Create(response)));
}

ExtensionFunction::ResponseAction
HistoryPrivateGetDetailedHistoryFunction::Run() {
  std::optional<vivaldi::history_private::GetDetailedHistory::Params> params(
      vivaldi::history_private::GetDetailedHistory::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  history::HistoryService* hs = GetFunctionCallerHistoryService(*this);
  if (!hs) {
    NOTREACHED();
    return RespondNow(NoArguments());
  }

  hs->GetVivaldiDetailedHistory(
      params->query, params->max_results,
      base::BindOnce(&HistoryPrivateGetDetailedHistoryFunction::SearchComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}
void HistoryPrivateGetDetailedHistoryFunction::SearchComplete(
    const history::DetailedUrlResults& results) {
  std::vector<vivaldi::history_private::DetailedHistoryItem> response;
  Profile* profile = GetFunctionCallerProfile(*this);
  HistoryItemList history_item_vec;
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  for (const auto& result : results) {
    vivaldi::history_private::DetailedHistoryItem item;
    bool has_chain_start =
        ui::PAGE_TRANSITION_CHAIN_START &
        ui::PageTransitionGetQualifier(result.transition_type);
    bool has_chain_end = ui::PAGE_TRANSITION_CHAIN_END &
                         ui::PageTransitionGetQualifier(result.transition_type);

    item.id.assign(result.id);
    item.url.assign(result.url.spec());
    item.title.assign(result.title);
    item.last_visit_time =
        result.last_visit_time.InMillisecondsFSinceUnixEpoch();
    item.visit_count = result.visit_count;
    item.typed_count = result.typed_count;
    item.is_bookmarked = bookmark_model->IsBookmarked(result.url);
    item.transition_type =
        HistoryPrivateAPI::UiTransitionToPrivateHistoryTransition(
            result.transition_type);
    item.is_redirect = ui::PageTransitionIsRedirect(result.transition_type) &&
                       !(has_chain_start || has_chain_end);

    response.push_back(std::move(item));
  }
  return Respond(ArgumentList(
      vivaldi::history_private::GetDetailedHistory::Results::Create(response)));
}

}  // namespace extensions
