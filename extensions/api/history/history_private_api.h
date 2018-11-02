// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_
#define EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "db/vivaldi_history_database.h"
#include "extensions/schema/history_private.h"
#include "ui/base/page_transition_types.h"

namespace history {
class QueryResults;
}

namespace extensions {

// Observes History service and routes the notifications as events to the
// extension system.
class HistoryPrivateEventRouter : public history::HistoryServiceObserver {
 public:
  HistoryPrivateEventRouter(Profile* profile,
                            history::HistoryService* history_service);
  ~HistoryPrivateEventRouter() override;

 private:
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;

  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);
  Profile* profile_;
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(HistoryPrivateEventRouter);
};

class HistoryPrivateAPI : public BrowserContextKeyedAPI,
                          public EventRouter::Observer {
 public:
  explicit HistoryPrivateAPI(content::BrowserContext* context);
  ~HistoryPrivateAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HistoryPrivateAPI>* GetFactoryInstance();

  static ui::PageTransition PrivateHistoryTransitionToUiTransition(
      vivaldi::history_private::TransitionType transition);
  static vivaldi::history_private::TransitionType
  UiTransitionToPrivateHistoryTransition(ui::PageTransition transition);

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<HistoryPrivateAPI>;
  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "HistoryPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<HistoryPrivateEventRouter> history_event_router_;
};

class HistoryPrivateDbSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.dbSearch", HISTORYPRIVATE_DBSEARCH)

 protected:
  ~HistoryPrivateDbSearchFunction() override {}

  // HistoryFunctionWithCallback:
  ExtensionFunction::ResponseAction Run() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

class HistoryPrivateSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.search", HISTORYPRIVATE_SEARCH)

 protected:
  ~HistoryPrivateSearchFunction() override {}

  // HistoryFunctionWithCallback:
  ExtensionFunction::ResponseAction Run() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

class HistoryPrivateDeleteVisitsFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.deleteVisits",
                             HISTORYPRIVATE_DELETEVISITS)

 protected:
  ~HistoryPrivateDeleteVisitsFunction() override {}

  // HistoryFunctionWithCallback:
  ExtensionFunction::ResponseAction Run() override;

  // Callback for the history service to acknowledge visit deletion.
  void DeleteVisitComplete();
};

class HistoryPrivateGetTopUrlsPerDayFunction
    : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.getTopUrlsPerDay",
                             HISTORYPRIVATE_GETTOPURLSPERDAY)

 protected:
  ~HistoryPrivateGetTopUrlsPerDayFunction() override {}

  // HistoryFunctionWithCallback:
  ExtensionFunction::ResponseAction Run() override;

  // Callback for the history service to acknowledge top url search complete.
  void TopUrlsComplete(
      const history::UrlVisitCount::TopUrlsPerDayList& host_counts);

  // Default number of visit results within the day
  const int kMaxResultsWithinDay = 4;
};

class HistoryPrivateVisitSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.visitSearch",
                             HISTORYPRIVATE_VISITSEARCH)

 protected:
  ~HistoryPrivateVisitSearchFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
  // Callback for the history service to acknowledge visits search complete.
  void VisitsComplete(const history::Visit::VisitsList& visit_list);
};

class HistoryPrivateSetKeywordSearchTermsForURLFunction
    : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.setKeywordSearchTermsForURL",
                             HISTORYPRIVATE_SETKEYWORDSEARCHTERMSFORURL)

 protected:
  ~HistoryPrivateSetKeywordSearchTermsForURLFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class HistoryPrivateDeleteAllSearchTermsForKeywordFunction
    : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.deleteAllSearchTermsForKeyword",
                             HISTORYPRIVATE_DELETEALLSEARCHTERMSFORKEYWORD)

 protected:
  ~HistoryPrivateDeleteAllSearchTermsForKeywordFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class HistoryPrivateGetTypedHistoryFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.getTypedHistory",
                             HISTORYPRIVATE_GETTYPEDURLSANDSEARCHES)

 protected:
  ~HistoryPrivateGetTypedHistoryFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

class HistoryPrivateMigrateOldTypedUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.migrateOldTypedUrl",
                             HISTORYPRIVATE_MIGRATEOLDTYPEDURL)

 protected:
  ~HistoryPrivateMigrateOldTypedUrlFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_
