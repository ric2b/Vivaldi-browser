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
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct VivaldiDataSourceItem;

/*
This is used to setup and control the mapping between local images and the
images exposed to the UI using the chrome://vivaldi-data/ protocol.
*/
class VivaldiDataSourcesAPI
    : public base::RefCountedThreadSafe<VivaldiDataSourcesAPI> {
 public:
  using AddBookmarkImageCallback =
      base::OnceCallback<void(int bookmark_id, std::string& image_url)>;

  explicit VivaldiDataSourcesAPI(base::FilePath user_data_dir);

  static void InitFactory();

  static VivaldiDataSourcesAPI* FromBrowserContext(
      content::BrowserContext* browser_context);

  // Read the given file. This should only be used on threads that are allowed
  // to block. On errors or when the file doesn't exist or is empty return a
  // null reference.
  static scoped_refptr<base::RefCountedMemory> ReadFileOnBlockingThread(
      const base::FilePath& file_path);

  // Most methods are static taking the context argument to spare the caller
  // from calling FromBrowserContext and checking the result.

  // Creates a straight mapping between an absolute path and an id.
  static bool AddMapping(content::BrowserContext* browser_context,
                         const std::string& id,
                         const base::FilePath& file_path);

  static bool RemoveMapping(content::BrowserContext* browser_context,
                            const std::string& id);

  // Add image data to disk and set up a mapping so it can be requested
  // using the usual image data protocol.
  static void AddImageDataForBookmark(content::BrowserContext* browser_context,
                                      int bookmark_id,
                                      std::unique_ptr<SkBitmap> bitmap,
                                      AddBookmarkImageCallback callback);

  static void AddImageDataForBookmark(
      content::BrowserContext* browser_context,
      int bookmark_id,
      scoped_refptr<base::RefCountedMemory> png_data,
      AddBookmarkImageCallback callback);

  static bool HasBookmarkThumbnail(content::BrowserContext* browser_context,
                                   int bookmark_id);

  // This method can be called from any thread.
  void GetDataForId(const std::string& id,
                    const content::URLDataSource::GotDataCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<VivaldiDataSourcesAPI>;

  using IdFileMap =
      std::map<std::string, std::unique_ptr<VivaldiDataSourceItem>>;

  ~VivaldiDataSourcesAPI();

  static scoped_refptr<base::RefCountedMemory> ReadFileOnFileThread(
      const base::FilePath& file_path);

  void FinishGetDataForId(
      const std::string& id,
      const base::FilePath& file_path,
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

  static void PostAddBookmarkImageResultsOnThread(
      AddBookmarkImageCallback callback,
      int bookmark_id);

  static IdFileMap LoadMappings(const base::FilePath& user_data_dir);
  static IdFileMap GetMappings(const base::DictionaryValue* dict);

  void ScheduleSaveMappings();
  void SaveMappingsOnFileThread();

  const base::FilePath user_data_dir_;

  // Access to any mutable field must be protected by this lock.
  // TODO(igor@vivaldi.com): consider eliminating the lock and accessing
  // id_to_file_map_ only from UI thread.
  base::Lock lock_;

  // This is a map between the exposed id and the file name
  IdFileMap id_to_file_map_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiDataSourcesAPI);
};

}  // namespace extensions

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
