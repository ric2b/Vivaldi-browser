// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef DB_VIVALDI_HISTORY_TYPES_H_
#define DB_VIVALDI_HISTORY_TYPES_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace history {

class UrlVisitCount {
 public:
  UrlVisitCount(std::string date, GURL url, int count);
  ~UrlVisitCount();
  UrlVisitCount(const UrlVisitCount& other);

  void set_date(std::string date) { date_ = date; }
  std::string date() { return date_; }

  void set_url(GURL url) { url_ = url; }
  GURL url() { return url_; }

  void set_count(int count) { count_ = count; }
  int count() { return count_; }

  typedef std::vector<UrlVisitCount> TopUrlsPerDayList;
  // Callback for value asynchronously returned by TopUrlsPerDay().
  typedef base::OnceCallback<void(const TopUrlsPerDayList&)>
      TopUrlsPerDayCallback;

 private:
  std::string date_;
  GURL url_;
  int count_;
};

class Visit {
 public:
  Visit(std::string id,
        base::Time visit_time,
        GURL url,
        std::u16string title,
        ui::PageTransition transition,
        int visit_count);
  ~Visit();

  Visit(const Visit& other);

  typedef std::vector<Visit> VisitsList;
  typedef base::OnceCallback<void(const VisitsList&)> VisitsCallback;

  std::string id;
  base::Time visit_time;
  GURL url;
  std::u16string title;
  ui::PageTransition transition;
  int visit_count;
};

class DetailedHistory {
  public:
   DetailedHistory();
   DetailedHistory(const DetailedHistory& other);
   ~DetailedHistory();

   typedef std::vector<DetailedHistory> DetailedHistoryList;

  // The unique identifier for the item.
  std::string id;

  // The URL navigated to by a user.
  GURL url;

  // The title of the page when it was last loaded.
  std::string title;

  // When this page was last loaded, represented in milliseconds since the epoch.
  base::Time last_visit_time;

  // The number of times the user has navigated to this page.
  int visit_count;

  // The number of times the user has navigated to this page by typing in the
  // address.
  int typed_count;

  // States if the URL is bookmarked.
  bool is_bookmarked;

  // The transition type for this visit from its referrer.
  ui::PageTransition transition_type;

};
}  //  namespace history

#endif  // DB_VIVALDI_HISTORY_TYPES_H_
