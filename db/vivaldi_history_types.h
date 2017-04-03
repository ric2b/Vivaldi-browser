// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef DB_VIVALDI_HISTORY_TYPES_H_
#define DB_VIVALDI_HISTORY_TYPES_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
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
  typedef base::Callback<void(const TopUrlsPerDayList&)> TopUrlsPerDayCallback;

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
        base::string16 title,
        std::string transition);
  ~Visit();

  Visit(const Visit& other);

  void set_id(std::string id) { id_ = id; }
  std::string id() { return id_; }

  void set_visit_time(base::Time visit_time) { visit_time_ = visit_time; }
  base::Time visit_time() { return visit_time_; }

  void set_url(GURL url) { url_ = url; }
  GURL url() { return url_; }

  void set_title(base::string16 title) { title_ = title; }
  base::string16 title() { return title_; }

  void set_transition(std::string transition) { transition_ = transition; }
  std::string transition() { return transition_; }

  typedef std::vector<Visit> VisitsList;
  typedef base::Callback<void(const VisitsList&)> VisitsCallback;

 private:
  std::string id_;
  base::Time visit_time_;
  GURL url_;
  base::string16 title_;
  std::string transition_;
};
}  //  namespace history

#endif  // DB_VIVALDI_HISTORY_TYPES_H_
