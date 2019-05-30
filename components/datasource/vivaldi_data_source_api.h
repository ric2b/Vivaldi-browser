// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/synchronization/lock.h"
#include "components/prefs/json_pref_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VivaldiDataSourceItem {
 public:
  VivaldiDataSourceItem(const std::string& id, const base::FilePath& path);
  VivaldiDataSourceItem(int bookmark_id, const base::FilePath& path);
  explicit VivaldiDataSourceItem(const std::string& id);
  ~VivaldiDataSourceItem();

  bool HasCachedData();

  scoped_refptr<base::RefCountedMemory> cached_data() {
    return cached_image_data_;
  }

  void SetPath(const base::FilePath& path) {
    file_path_ = path;
  }

  base::FilePath GetPath() {
    return file_path_;
  }

  std::string GetPathString() {
    return file_path_.AsUTF8Unsafe();
  }

  int bookmark_id() {
    return bookmark_id_;
  }

  void SetCachedData(scoped_refptr<base::RefCountedMemory> data);

 private:
  // The file on disk.
  base::FilePath file_path_;

  // The id used to request this file from the protocol side.
  std::string mapping_id_;

  // If this is a bookmark, this is the bookmark id.
  int bookmark_id_ = 0;

  // The cached image data.
  scoped_refptr<base::RefCountedMemory> cached_image_data_;
};

/*
This is used to setup and control the mapping between local images and the
images exposed to the UI using the chrome://vivaldi-data/ protocol.
*/
class VivaldiDataSourcesAPI : public BrowserContextKeyedAPI {
 public:
  explicit VivaldiDataSourcesAPI(content::BrowserContext* context);
  ~VivaldiDataSourcesAPI() override;

  typedef base::Callback<void(int bookmark_id, std::string& image_url)>
    AddBookmarkImageCallback;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPI>*
    GetFactoryInstance();

  // The version with an existing id already and creates a straight mapping
  // between an absolute path and an id.
  bool AddMapping(const std::string& id, const base::FilePath& file_path);

  // This version connects the bookmark id with the given path.
  bool AddMapping(int bookmark_id, const base::FilePath& file_path);

  bool RemoveMapping(const std::string& id);
  bool RemoveMapping(int bookmark_id);

  // This method should be called from the UI thread.
  void GetDataForId(const std::string& id,
                    const content::URLDataSource::GotDataCallback& callback);

  // Add image data to disk and set up a mapping so it can be requested
  // using the usual image data protocol.
  void AddImageDataForBookmark(int bookmark_id,
                               std::unique_ptr<SkBitmap> bitmap,
                               AddBookmarkImageCallback callback);

  void AddImageDataForBookmark(int bookmark_id,
                               scoped_refptr<base::RefCountedMemory> png_data,
                               AddBookmarkImageCallback callback);

  // If bookmark_id is provided, use that, otherwise lookup based on url.
  bool HasBookmarkThumbnail(int bookmark_id);

 private:
  void GetDataForIdOnFileThread(
      const std::string& id,
      const content::URLDataSource::GotDataCallback& callback,
      content::BrowserThread::ID thread_id);

  void PostResultsOnThread(
      const content::URLDataSource::GotDataCallback& callback,
      scoped_refptr<base::RefCountedMemory> data);

  void AddRawImageDataForBookmarkOnFileThread(
      int bookmark_id,
      scoped_refptr<base::RefCountedMemory> png_data,
      AddBookmarkImageCallback callback,
      content::BrowserThread::ID thread_id);
  void AddImageDataForBookmarkOnFileThread(
      int bookmark_id,
      std::unique_ptr<SkBitmap> bitmap,
      AddBookmarkImageCallback callback,
      content::BrowserThread::ID thread_id);

  void PostAddBookmarkImageResultsOnThread(AddBookmarkImageCallback callback,
                                           base::FilePath image_path,
                                           int bookmark_id);

  bool LoadMappings();
  bool GetMappings();
  void SaveMappings();

  friend class BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPI>;

  content::BrowserContext* browser_context_;

  // This is a map between the exposed id and the file name
  std::map<std::string, VivaldiDataSourceItem*> id_to_file_map_;

  // Lock access to the map for one thread at a time.
  base::Lock map_lock_;

  scoped_refptr<JsonPrefStore> store_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "VivaldiDataSourcesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = false;
  static const bool kServiceRedirectedInIncognito = true;
};

}  // namespace extensions

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
