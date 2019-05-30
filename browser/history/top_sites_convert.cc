// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/history/top_sites_convert.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/datasource/vivaldi_data_source_api.h"

namespace vivaldi {
void OnBookmarkThumbnailStored(int bookmark_id, std::string& image_url) {
  LOG(INFO) << "Bookmark thumbnail for bookmark " << bookmark_id << "converted.";
}

void ConvertThumbnailDataOnUIThread(
    const base::FilePath path,
    int bookmark_id,
    scoped_refptr<base::RefCountedMemory> thumbnail) {
  // Certain profile calls can't be made on the IO thread, so off load to the UI
  // thread, then bounce to the IO thread from here.
  ProfileManager* manager = g_browser_process->profile_manager();
  extensions::VivaldiDataSourcesAPI* api =
    extensions::VivaldiDataSourcesAPI::GetFactoryInstance()->Get(
      manager->GetProfile(path));
  extensions::VivaldiDataSourcesAPI::AddBookmarkImageCallback callback =
    base::Bind(&OnBookmarkThumbnailStored);

  // No error checking, it's not the end of the world if this fails.
  api->AddImageDataForBookmark(bookmark_id, thumbnail, callback);
}

}  // namespace vivaldi
