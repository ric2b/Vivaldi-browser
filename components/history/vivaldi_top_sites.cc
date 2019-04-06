// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/top_sites_backend.h"
#include "components/history/core/browser/top_sites_database.h"
#include "components/history/core/browser/top_sites_impl.h"

#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "prefs/vivaldi_pref_names.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace history {

namespace {
// Vivaldi: How many days between each vacuuming of the top sites database.
const int kTopSitesVacuumDays = 14;
}  // namespace

bool TopSites::HasPageThumbnail(const GURL& url) {
  return false;
}

// TopSitesImpl
bool TopSitesImpl::HasPageThumbnail(const GURL& url) {
  scoped_refptr<base::RefCountedMemory> bytes;
  // This could be relatively expensive, but the assumption is
  // that the next call to fetch the thumbnail will then use
  // the cache, making the end performance the same.
  return GetPageThumbnail(url, true, &bytes);
}

void TopSitesImpl::Vacuum() {
  base::Time time_now = base::Time::Now();
  int64_t last_vacuum =
      pref_service_->GetInt64(vivaldiprefs::kVivaldiLastTopSitesVacuumDate);
  base::Time next_vacuum =
      time_now + base::TimeDelta::FromDays(kTopSitesVacuumDays);
  if (last_vacuum == 0 || last_vacuum > next_vacuum.ToInternalValue()) {
    pref_service_->SetInt64(vivaldiprefs::kVivaldiLastTopSitesVacuumDate,
                            time_now.ToInternalValue());
    backend_->VacuumDatabase();
  }
}

void TopSitesImpl::RemoveThumbnailForUrl(const GURL& url) {
  backend_->RemoveThumbnailForUrl(url);
}

// TopSitesDatabase
bool TopSitesDatabase::DeleteDataExceptBookmarkThumbnails() {
  sql::Transaction transaction(db_.get());
  transaction.Begin();

  // NOTE(pettern@vivaldi.com): We remove all data except bookmark thumbnails.
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM thumbnails WHERE thumbnail notnull and url not like "
      "'%bookmark_thumbnail%'"));

  if (!delete_statement.Run())
    return false;

  return transaction.Commit();
}

namespace {
const int kTopSitesExpireDays = 3;
}  // namespace

bool TopSitesDatabase::TrimThumbnailData() {
  sql::Transaction transaction(db_.get());
  transaction.Begin();

  base::Time expire_before =
      base::Time::Now() - base::TimeDelta::FromDays(kTopSitesExpireDays);
  // NOTE(pettern@vivaldi.com): We remove all thumbnails that are not attached
  // to a bookmark and haven't been updated with the last 7 days.
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM thumbnails WHERE thumbnail notnull and url not like "
      "'%bookmark_thumbnail%' and last_updated < ?"));
  delete_statement.BindInt64(0, expire_before.ToInternalValue());

  if (!delete_statement.Run())
    return false;

  return transaction.Commit();
}

void TopSitesDatabase::Vacuum() {
  DCHECK(db_->transaction_nesting() == 0)
      << "Can not have a transaction when vacuuming.";
  ignore_result(db_->Execute("VACUUM"));
}

bool TopSitesDatabase::RemoveThumbnailForUrl(const GURL& url) {
  sql::Transaction transaction(db_.get());
  transaction.Begin();

  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM thumbnails WHERE thumbnail notnull and url = ?"));
  delete_statement.BindString(0, url.spec());

  if (!delete_statement.Run())
    return false;

  return transaction.Commit();
}

// TopSitesBackend
void TopSitesBackend::VacuumDatabase() {
  db_task_runner_->PostTask(
      FROM_HERE, base::Bind(&TopSitesBackend::VacuumOnDBThread, this));
}

void TopSitesBackend::VacuumOnDBThread() {
  db_->Vacuum();
}

void TopSitesBackend::RemoveThumbnailForUrl(const GURL& url) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&TopSitesBackend::RemoveThumbnailForUrlOnDBThread, this, url));
}

void TopSitesBackend::RemoveThumbnailForUrlOnDBThread(const GURL& url) {
  db_->RemoveThumbnailForUrl(url);
}

}  // namespace history
