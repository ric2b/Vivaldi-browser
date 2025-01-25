// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_

#include <vector>

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;
class PrefRegistrySimple;

namespace content {
class WebContents;
}  // namespace content

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class URLVisitAggregateDataType {
  kTab = 0,
  kHistory = 1,
  kMaxValue = kHistory,
};

// The handler for communication between the WebUI.
class MostRelevantTabResumptionPageHandler
    : public ntp::most_relevant_tab_resumption::mojom::PageHandler {
 public:
  MostRelevantTabResumptionPageHandler(
      mojo::PendingReceiver<
          ntp::most_relevant_tab_resumption::mojom::PageHandler>
          pending_page_handler,
      content::WebContents* web_contents);

  MostRelevantTabResumptionPageHandler(
      const MostRelevantTabResumptionPageHandler&) = delete;
  MostRelevantTabResumptionPageHandler& operator=(
      const MostRelevantTabResumptionPageHandler&) = delete;

  ~MostRelevantTabResumptionPageHandler() override;

  // most_relevant_tab_resumption::mojom::PageHandler:
  void GetTabs(GetTabsCallback callback) override;
  void DismissModule(const std::vector<history::mojom::TabPtr> tabs) override;
  void DismissTab(const history::mojom::TabPtr tab) override;
  void RestoreModule(const std::vector<history::mojom::TabPtr> tabs) override;
  void RestoreTab(history::mojom::TabPtr tab) override;
  void RecordAction(
      ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction action,
      const std::string& url_key,
      int64_t visit_request_id) override;

  // Invoked when the URL visit aggregates have been fetched.
  void OnURLVisitAggregatesFetched(
      GetTabsCallback callback,
      visited_url_ranking::ResultStatus status,
      std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates);

  // Invoked when the URL visit aggregates have been ranked.
  void OnGotRankedURLVisitAggregates(
      GetTabsCallback callback,
      visited_url_ranking::ResultStatus status,
      std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Method to determine if a url is in the list of previously dismissed urls.
  bool IsNewURL(history::mojom::TabPtr& tab);

  // Method to clear dismissed tabs that are older than a certain amount of
  // time.
  void RemoveOldDismissedTabs();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  // The result types to request for when fetching URL visit aggregate data.
  visited_url_ranking::FetchOptions::URLTypeSet result_url_types_;

  mojo::Receiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
      page_handler_;

  base::WeakPtrFactory<MostRelevantTabResumptionPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_
