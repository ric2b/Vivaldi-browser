// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef DB_VIVALDI_HISTORY_TYPES_H_
#define DB_VIVALDI_HISTORY_TYPES_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace history {

struct TypedUrlResult {
  TypedUrlResult();
  TypedUrlResult& operator=(const TypedUrlResult&);
  TypedUrlResult(const TypedUrlResult&);

  GURL url;
  std::string title;
  KeywordID keyword_id = -1;
  std::string terms;
  int visit_count;
};

typedef std::vector<TypedUrlResult> TypedUrlResults;

struct DetailedUrlResult {
  ~DetailedUrlResult();
  DetailedUrlResult();
  DetailedUrlResult& operator=(const DetailedUrlResult&);
  DetailedUrlResult(const DetailedUrlResult&);

  std::string id;
  GURL url;
  std::string title;
  base::Time last_visit_time;
  int visit_count;
  int typed_count;
  bool is_bookmarked;
  ui::PageTransition transition_type;
  bool is_redirect;
  int score;
};

typedef std::vector<DetailedUrlResult> DetailedUrlResults;

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
        ui::PageTransition transition);
  ~Visit();

  Visit(const Visit& other);

  typedef std::vector<Visit> VisitsList;
  typedef base::OnceCallback<void(const VisitsList&)> VisitsCallback;

  std::string id;
  base::Time visit_time;
  GURL url;
  std::u16string title;
  ui::PageTransition transition;
};

}  //  namespace history

#endif  // DB_VIVALDI_HISTORY_TYPES_H_
