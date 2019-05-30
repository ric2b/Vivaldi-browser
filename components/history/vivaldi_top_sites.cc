// Copyright (c) 2018-2019 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/history/core/browser/top_sites_backend.h"
#include "components/history/core/browser/top_sites_database.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "sql/statement.h"

namespace history {
bool TopSitesDatabase::ConvertThumbnailData(
    const base::FilePath db_path,
    ConvertThumbnailDataCallback callback) {
  static const char kThumbSql[] =
      "SELECT thumbnail, url FROM thumbnails WHERE thumbnail notnull and url "
      "like '%bookmark_thumbnail%'";

  base::FilePath path = db_path.DirName();
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kThumbSql));

  while (statement.Step()) {
    std::vector<unsigned char> data;
    std::string url = statement.ColumnString(1);

    scoped_refptr<base::RefCountedMemory> thumbnail;
    statement.ColumnBlobAsVector(0, &data);
    if (!data.empty() && !url.empty()) {
      thumbnail = base::RefCountedBytes::TakeVector(&data);

      // Bookmark url is in this format: http://bookmark_thumbnail/6650
      size_t pos = url.find_last_of('/');
      if (pos != std::string::npos) {
        url = url.substr(pos + 1);
      }
      int bookmark_id = url.empty() ? 0 : std::atoi(url.c_str());
      if (bookmark_id) {
        base::PostTaskWithTraits(
            FROM_HERE, {content::BrowserThread::UI},
            base::Bind(callback, path, bookmark_id, std::move(thumbnail)));
      } else {
        LOG(ERROR) << "Did not find valid thumbnail id in url: "
                         << statement.ColumnString(1);
      }
    }
  }
  return true;
}

void TopSitesImpl::SetThumbnailConvertCallback(
    ConvertThumbnailDataCallback callback) {
  backend_->SetThumbnailConvertCallback(std::move(callback));
}

}  // namespace history
