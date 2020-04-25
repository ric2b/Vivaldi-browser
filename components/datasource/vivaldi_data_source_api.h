// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "components/prefs/json_pref_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VivaldiDataSourcesAPIHolder;

/*
This is used to setup and control the mapping between local images and the
images exposed to the UI using the chrome://vivaldi-data/ protocol.
*/
class VivaldiDataSourcesAPI
    : public base::RefCountedThreadSafe<VivaldiDataSourcesAPI> {
 public:
  explicit VivaldiDataSourcesAPI(Profile* profile);

  // number and preferences containing data mapping urls.
  static constexpr int kDataMappingPrefsCount = 2;
  static const char* kDataMappingPrefs[kDataMappingPrefsCount];

  static void InitFactory();

  static VivaldiDataSourcesAPI* FromBrowserContext(
      content::BrowserContext* browser_context);

  // Read the given file into the vector. Return false when file does not exit
  // or is empty or on errors. The errors are logged. This should only be used
  // on threads that are allowed to block.
  static bool ReadFileOnBlockingThread(const base::FilePath& file_path,
                                       std::vector<unsigned char>* data);

  static scoped_refptr<base::RefCountedMemory> ReadFileOnBlockingThread(
      const base::FilePath& file_path);

  static bool IsBookmarkCapureUrl(const std::string& url);
  static std::string GetBookmarkThumbnailUrl(int64_t bookmarkId);

  // The following methods taking the BrowserContext* argument are static to
  // spare the caller from calling FromBrowserContext and checking the result.

  // Creates a straight mapping between an absolute path and an id.
  // In AddMappingCallback the profile argument is the original "recording"
  // profile which is different from browser_context for incognito windows.
  // profile is null during the shutdown.
  using AddMappingCallback = base::OnceCallback<
      void(Profile* profile, bool success, std::string image_url)>;
  static void AddMapping(content::BrowserContext* browser_context,
                         base::FilePath file_path,
                         AddMappingCallback callback);

  static void OnUrlChange(content::BrowserContext* browser_context,
                          const std::string& old_url,
                          const std::string& new_url);

  void OnUrlChange(const std::string& old_url, const std::string& new_url);

  // Add image data to disk and set up a mapping so it can be requested
  // using the usual image data protocol.
  using AddBookmarkImageCallback = base::OnceCallback<void(bool success)>;
  static void AddImageDataForBookmark(
      content::BrowserContext* browser_context,
      int64_t bookmark_id,
      scoped_refptr<base::RefCountedMemory> png_data,
      AddBookmarkImageCallback callback);

  // This can be called from any thread. The callback will be called from the UI
  // thread.
  void AddImageDataForBookmark(int64_t bookmark_id,
                               scoped_refptr<base::RefCountedMemory> png_data,
                               AddBookmarkImageCallback ui_thread_callback);

  // This method must be called from the IO thread.
  void GetDataForId(const std::string& id,
                    content::URLDataSource::GotDataCallback callback);

  // During bulk changes the file mapping is not saved after each mutation
  // operation.
  static void SetBulkChangesMode(content::BrowserContext* browser_context,
                                 bool enable);

  void LoadMappings();

 private:
  friend class base::RefCountedThreadSafe<VivaldiDataSourcesAPI>;
  friend class VivaldiDataSourcesAPIHolder;

  class DataSourceItem {
   public:
    DataSourceItem();
    explicit DataSourceItem(base::FilePath file_path);
    DataSourceItem(DataSourceItem&&);
    DataSourceItem& operator=(DataSourceItem&&);
    ~DataSourceItem();

    const base::FilePath& GetFilePath() const { return file_path_; }

   private:
    // The file on disk.
    base::FilePath file_path_;

    DISALLOW_COPY_AND_ASSIGN(DataSourceItem);
  };

  ~VivaldiDataSourcesAPI();

  static scoped_refptr<base::RefCountedMemory> ReadFileOnFileThread(
      const base::FilePath& file_path);

  static bool GetDataMappingId(const std::string& url, std::string* id);
  static std::string GetDataMappingUrl(const std::string& id);

  void AddMappingOnFileThread(base::FilePath file_path,
                              AddMappingCallback callback);
  void FinishAddMappingOnUIThread(std::string id, AddMappingCallback callback);

  void RemoveMappingOnFileThread(std::string id);

  void SetCacheOnIOThread(std::string id,
                          scoped_refptr<base::RefCountedMemory> data);
  void ClearCacheOnIOThread(std::string id);
  void GetDataForIdOnFileThread(
      std::string id,
      content::URLDataSource::GotDataCallback callback);
  void FinishGetDataForIdOnIOThread(
      std::string id,
      scoped_refptr<base::RefCountedMemory> data,
      content::URLDataSource::GotDataCallback callback);

  void AddImageDataForBookmarkOnFileThread(
      int64_t bookmark_id,
      scoped_refptr<base::RefCountedMemory> png_data,
      AddBookmarkImageCallback callback);

  void LoadMappingsOnFileThread();
  void InitMappingsOnFileThread(const base::DictionaryValue* dict);
  void SetBulkChangesModeOnFileThread(bool enable);

  std::string GetMappingJSONOnFileThread();
  void SaveMappingsOnFileThread();

  base::FilePath GetFileMappingFilePath();
  base::FilePath GetBookmarkThumbnailPath(int64_t bookmark_id);

  // This must be accessed only on UI thread. It is reset to null on shutdown.
  Profile* profile_;

  const base::FilePath user_data_dir_;

  // Runner to ensure that tasks to manipulate the data mapping runs in sequence
  // with the proper order.
  scoped_refptr<base::SequencedTaskRunner> sequence_task_runner_;

  // Outside constructor or destructor this must be accessed only from the
  // sequence_task_runner_.
  std::map<std::string, DataSourceItem> id_to_file_map_;

  // Flags to prevent frequent writes on bulk changes. They must be accessed
  // only from sequence_task_runner_.
  bool bulk_changes_ = false;
  bool unsaved_changes_ = false;

  // Outside constructor or destructor this must be accessed only from the IO
  // thread.
  std::map<std::string, scoped_refptr<base::RefCountedMemory>>
      io_thread_data_cache_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiDataSourcesAPI);
};

}  // namespace extensions

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
