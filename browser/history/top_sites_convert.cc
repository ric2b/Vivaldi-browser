// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/history/top_sites_convert.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/datasource/vivaldi_data_source_api.h"

namespace vivaldi {

void OnBookmarkThumbnailStored(int64_t bookmark_id, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to convert bookmark thumbnail for bookmark "
               << bookmark_id;
  } else {
    LOG(INFO) << "Bookmark thumbnail for bookmark " << bookmark_id
              << "converted.";
  }
}

void ConvertThumbnailDataOnUIThread(
    const base::FilePath& path,
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> thumbnail) {
  // Certain profile calls can't be made on the IO thread, so off load to the UI
  // thread, then bounce to the IO thread from here.
  ProfileManager* manager = g_browser_process->profile_manager();
  Profile* profile = manager->GetProfile(path);

  extensions::VivaldiDataSourcesAPI::AddImageDataForBookmark(
      profile, bookmark_id, thumbnail,
      base::BindOnce(&OnBookmarkThumbnailStored, bookmark_id));
}

}  // namespace vivaldi
