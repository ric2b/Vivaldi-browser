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

class Profile;

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
  static constexpr int kThemeWindowBackgroundImageUrl_Index = 0;
  static constexpr int kStartpageImagePathCustom_Index = 1;
  static const char* kDataMappingPrefs[kDataMappingPrefsCount];

  // /local-image/id and /thumbnail/id urls
  enum UrlKind {
    PATH_MAPPING_URL = 0,
    THUMBNAIL_URL = 1,
  };

  static constexpr int kUrlKindCount = 2;

  // Return an index of data mapping preference or -1 if the preference
  // does not hold a data mapping.
  static int FindMappingPreference(base::StringPiece preference);

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

  static void ScheduleRemovalOfUnusedUrlData(
      content::BrowserContext* browser_context, base::TimeDelta when);

  // The following methods taking the BrowserContext* argument are static to
  // spare the caller from calling FromBrowserContext and checking the result.

  // Update data mapping for a bookmark or preference to point to the given
  // local path.
  using UpdateMappingCallback = base::OnceCallback<void(bool success)>;
  static void UpdateMapping(content::BrowserContext* browser_context,
                            int64_t bookmark_id,
                            int preference_index,
                            base::FilePath file_path,
                            UpdateMappingCallback callback);

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
  void GetDataForId(UrlKind url_kind,
                    std::string id,
                    content::URLDataSource::GotDataCallback callback);

  void LoadMappings();

 private:
  friend class base::RefCountedThreadSafe<VivaldiDataSourcesAPI>;
  friend class VivaldiDataSourcesAPIHolder;

  ~VivaldiDataSourcesAPI();

  static scoped_refptr<base::RefCountedMemory> ReadFileOnFileThread(
      const base::FilePath& file_path);

  static bool ParseDataUrl(base::StringPiece url,
                           UrlKind* url_kind,
                           std::string* id);

  static std::string MakeDataUrl(UrlKind url_kind, base::StringPiece id);

  // Check if path mapping id is really old-format thumbanil, not a path
  // mapping.
  static bool isOldFormatThumbnailId(base::StringPiece id);

  void UpdateMappingOnFileThread(int64_t bookmark_id,
                                 int preference_index,
                                 base::FilePath file_path,
                                 UpdateMappingCallback callback);
  void FinishUpdateMappingOnUIThread(int64_t bookmark_id,
                                     int preference_index,
                                     std::string path_id,
                                     UpdateMappingCallback callback);

  // Use std::array, not plain C array to get proper move semantics.
  using UsedIds = std::array<std::vector<std::string>, kUrlKindCount>;
  void CollectUsedUrlsOnUIThread();
  void RemoveUnusedUrlDataOnFileThread(UsedIds used_ids);

  void SetCacheOnIOThread(UrlKind url_kind, std::string id,
                          scoped_refptr<base::RefCountedMemory> data);
  void ClearCacheOnIOThread(UrlKind url_kind, std::string id);
  void GetDataForIdOnFileThread(
      UrlKind url_kind,
      std::string id,
      content::URLDataSource::GotDataCallback callback);
  void FinishGetDataForIdOnIOThread(
      UrlKind url_kind,
      std::string id,
      scoped_refptr<base::RefCountedMemory> data,
      content::URLDataSource::GotDataCallback callback);

  void AddImageDataForBookmarkOnFileThread(
      int64_t bookmark_id,
      scoped_refptr<base::RefCountedMemory> png_data,
      AddBookmarkImageCallback callback);
  void FinishAddImageDataForBookmarkOnUIThread(
      AddBookmarkImageCallback callback,
      bool success,
      int64_t bookmark_id,
      std::string thumbnail_id);

  void LoadMappingsOnFileThread();
  void InitMappingsOnFileThread(const base::DictionaryValue* dict);

  std::string GetMappingJSONOnFileThread();
  void SaveMappingsOnFileThread();

  base::FilePath GetFileMappingFilePath();
  base::FilePath GetThumbnailPath(base::StringPiece thumbnail_id);

  void AddNewbornIdOnFileThread(UrlKind url_kind, const std::string& id);
  void RemoveNewbornIdOnFileThread(UrlKind url_kind, const std::string& id);

  // This must be accessed only on UI thread. It is reset to null on shutdown.
  Profile* profile_;

  const base::FilePath user_data_dir_;

  // Runner to ensure that tasks to manipulate the data mapping runs in sequence
  // with the proper order.
  scoped_refptr<base::SequencedTaskRunner> sequence_task_runner_;

  // Map path ids into their paths. Outside constructor or destructor this must
  // be accessed only from the sequence_task_runner_.
  std::map<std::string, base::FilePath> path_id_map_;

  // ids that have been newly allocated but not yet been stored as URL is
  // bookmark nodes or preferences. This prevents their removal in
  // RemoveUnusedUrlData. This must be accessed only from sequence_task_runner_.
  std::vector<std::string> file_thread_newborn_ids_[kUrlKindCount];

  // Outside constructor or destructor this must be accessed only from the IO
  // thread.
  std::map<std::string, scoped_refptr<base::RefCountedMemory>>
      io_thread_data_cache_[kUrlKindCount];

  DISALLOW_COPY_AND_ASSIGN(VivaldiDataSourcesAPI);
};

}  // namespace extensions

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_API_H_
