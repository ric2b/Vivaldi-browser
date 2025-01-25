// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_IMAGE_STORE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_IMAGE_STORE_H_

#include <map>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace bookmarks {
class BookmarkModel;
}

namespace content {
class BrowserContext;
}

namespace vivaldi {
class ThumbnailCaptureContents;
}
class VivaldiImageStoreHolder;

class VivaldiImageStore;

namespace vivaldi_image_store {
// preserve order!
enum class ImageFormat {
  kBMP = 1,
  kGIF = 2,
  kJPEG = 3,
  kPNG = 4,
  kWEBP = 5,
  kSVG = 6,
  kTIFF = 7
};

enum class BatchItemState {
  kPending,
  kOk,
  kError,
};

const base::FilePath::StringPieceType kDirectMatchImageDirectory =
    FILE_PATH_LITERAL("VivaldiDirectMatchIcons");

struct BatchItem {
  using State = vivaldi_image_store::BatchItemState;

  explicit BatchItem(std::string url);
  BatchItem();
  ~BatchItem();
  BatchItem(BatchItem&&);

  BatchItem(const BatchItem&) = delete;
  BatchItem& operator=(const BatchItem&) = delete;

  State state;
  ImageFormat format;
  std::string url;
  std::vector<unsigned char> data;
};

using Batch = std::vector<BatchItem>;

class ImageStoreDataProvider {
 public:
  virtual scoped_refptr<base::RefCountedMemory> GetData() = 0;
  virtual ImageFormat GetImageType() = 0;
  virtual BatchItemState GetState() = 0;
};

// Prevents thumbnails GC from running twice at the same time
class GCGuard {
 public:
  ~GCGuard();
  static std::unique_ptr<GCGuard> Create(VivaldiImageStore* api);

 private:
  explicit GCGuard(scoped_refptr<VivaldiImageStore> api);
  scoped_refptr<VivaldiImageStore> api_;
};

}  // namespace vivaldi_image_store

// This is used to setup and control the mapping between local images and the
// images exposed to the UI using the chrome://vivaldi-data/ protocol.
class VivaldiImageStore : public base::RefCountedThreadSafe<VivaldiImageStore> {
 public:
  explicit VivaldiImageStore(Profile* profile);
  VivaldiImageStore(const VivaldiImageStore&) = delete;
  VivaldiImageStore& operator=(const VivaldiImageStore&) = delete;

  // Those paths are related to vivaldi_data_url_utils::kTypeNames
  enum UrlKind {
    // /local-image/id
    kPathMappingUrl = 0,
    // /thumbnail/id
    kImageUrl = 1,
    // /direct-match/id
    kDirectMatchImageUrl = 2,
  };

  static constexpr int kUrlKindCount = kDirectMatchImageUrl + 1;

  using ImageFormat = vivaldi_image_store::ImageFormat;
  using BatchItem = vivaldi_image_store::BatchItem;
  using Batch = vivaldi_image_store::Batch;

  friend class vivaldi_image_store::GCGuard;

  // Location where to store or update the image.
  class ImagePlace {
   public:
    ImagePlace() = default;
    ~ImagePlace() = default;
    ImagePlace(ImagePlace&&) = default;
    ImagePlace& operator=(ImagePlace&&) = default;

    bool IsEmpty() const {
      return !IsBookmarkId() && !IsBackgroundUserImage() && !IsThemeId();
    }

    bool IsBookmarkId() const { return bookmark_id_ > 0; }

    bool IsBackgroundUserImage() const { return background_user_image_; }

    bool IsThemeId() const { return !theme_id_.empty(); }

    int64_t GetBookmarkId() const {
      DCHECK(IsBookmarkId());
      return bookmark_id_;
    }

    const std::string& GetThemeId() const {
      DCHECK(IsThemeId());
      return theme_id_;
    }

    void SetBookmarkId(int64_t bookmark_id) {
      DCHECK_GT(bookmark_id, 0);
      DCHECK(IsEmpty());
      bookmark_id_ = bookmark_id;
    }

    void SetBackgroundUserImage() {
      DCHECK(IsEmpty());
      background_user_image_ = true;
    }

    void SetThemeId(std::string theme_id) {
      DCHECK(!theme_id.empty());
      DCHECK(IsEmpty());
      theme_id_ = std::move(theme_id);
    }

    // Extract the id of the theme. After the call |IsEmpty()| returns true.
    std::string ExtractThemeId() {
      DCHECK(IsThemeId());
      return std::move(theme_id_);
    }

   private:
    int64_t bookmark_id_ = 0;
    bool background_user_image_ = false;
    std::string theme_id_;
  };

  static std::vector<base::FilePath::StringType> GetAllowedImageExtensions();

  static std::optional<ImageFormat> FindFormatForMimeType(
      std::string_view mime_type);

  // Find the format for the given file_extension. The extension may
  // start with a dot.
  static std::optional<ImageFormat> FindFormatForExtension(
      std::string_view file_extension);

  static std::optional<ImageFormat> FindFormatForPath(
      const base::FilePath& path);

  static void InitFactory();

  static VivaldiImageStore* FromBrowserContext(
      content::BrowserContext* browser_context);

  // If the number of images stored since the last GC run < limit, do
  // nothing.
  static void ScheduleRemovalOfUnusedUrlData(
      content::BrowserContext* browser_context,
      int leeway);

  void ScheduleThumbnalSanitizer();

  static bool ParseDataUrl(std::string_view url,
                           VivaldiImageStore::UrlKind& url_kind,
                           std::string& id);

  // Callback to inform about the url of a successful image store operation.
  // Empty string means operation failed.
  using StoreImageCallback = base::OnceCallback<void(std::string data_url)>;

  using StoreImageBatchReadCallback = base::OnceCallback<void(Batch)>;

  // The following methods taking the BrowserContext* argument are static to
  // spare the caller from calling FromBrowserContext and checking the result.

  // Update data mapping URL for the given image place to point to the given
  // local path.
  static void UpdateMapping(content::BrowserContext* browser_context,
                            ImagePlace place,
                            ImageFormat format,
                            base::FilePath file_path,
                            StoreImageCallback callback);

  // Add image data to disk and set up a mapping so it can be requested
  // using the usual image data protocol.
  static void StoreImage(content::BrowserContext* browser_context,
                         ImagePlace place,
                         ImageFormat format,
                         scoped_refptr<base::RefCountedMemory> image_data,
                         StoreImageCallback callback);

  // Capture the url and store the resulting image as a thumbnail for the given
  // bookmark.
  static ::vivaldi::ThumbnailCaptureContents* CaptureBookmarkThumbnail(
      content::BrowserContext* browser_context,
      int64_t bookmark_id,
      const GURL& url,
      StoreImageCallback ui_thread_callback);

  static void GetDataForId(content::BrowserContext* browser_context,
                           UrlKind url_kind,
                           std::string id,
                           content::URLDataSource::GotDataCallback callback);

  static void BatchRead(content::BrowserContext* browser_context,
                        const std::vector<std::string>& ids,
                        StoreImageBatchReadCallback);
  // Read data for the given UrlKind. This can be called from any thread.
  void GetDataForId(UrlKind url_kind,
                    std::string id,
                    content::URLDataSource::GotDataCallback callback);

  void Start();

  // Store the image data persistently and return the url to refer to the stored
  // data. The caller must call ForgetNewbornUrl() after storing the url or on
  // any errors. This can be called from any thread.
  using StoreImageDataResult = base::OnceCallback<void(std::string image_url)>;
  void StoreImageData(ImageFormat format,
                      scoped_refptr<base::RefCountedMemory> image_data,
                      StoreImageDataResult callback);

  // Call this after storing the newborn data_url for stored image data into a
  // persistent storage like bookmark or preferences or on errors. This can be
  // called from any thread.
  void ForgetNewbornUrl(std::string data_url);

 private:
  void BatchRead(const std::vector<std::string>& ids,
                 StoreImageBatchReadCallback);

  friend class base::RefCountedThreadSafe<VivaldiImageStore>;
  friend class VivaldiImageStoreHolder;

  ~VivaldiImageStore();

  scoped_refptr<base::RefCountedMemory> GetDataForNoteAttachment(
      const std::string& path);

  static scoped_refptr<base::RefCountedMemory> ReadFileOnFileThread(
      const base::FilePath& file_path);

  void UpdateMappingOnFileThread(ImagePlace place,
                                 ImageFormat format,
                                 base::FilePath file_path,
                                 StoreImageCallback callback);

  void FindUsedUrlsOnUIThread();

  void FindUsedUrlsOnUIThreadWithLoadedBookmarks(
      std::vector<base::FilePath> ids,
      std::unique_ptr<vivaldi_image_store::GCGuard> guard,
      bookmarks::BookmarkModel* bookmark_model);

  void SanitizeUrlsOnUIThreadWithLoadedBookmarks(
      bookmarks::BookmarkModel* bookmark_model);

  // Use std::array, not plain C array to get proper move semantics.
  using UsedIds = std::array<std::vector<std::string>, kUrlKindCount>;
  void RemoveUnusedUrlDataOnFileThread(
      UsedIds used_ids,
      std::unique_ptr<vivaldi_image_store::GCGuard> guard,
      std::vector<base::FilePath> ids);

  // Custom bookmark thumbnails have to be moved to the synced file store,
  // so that they can be synced.
  void MigrateCustomBookmarkThumbnailsOnFileThread(
      std::vector<std::pair<base::Uuid, std::string>> ids_to_migrate);

  void FinishCustomBookmarkThumbnailMigrationOnUIThread(
      base::Uuid bookmark_uuid,
      std::vector<uint8_t> content);

  scoped_refptr<base::RefCountedMemory> GetDataForIdOnFileThread(
      UrlKind url_kind,
      std::string id);

  bool GetDataForIdToVectorOnFileThread(UrlKind url_kind,
                                        std::string id,
                                        std::vector<unsigned char>& buffer);

  void ReadBatchOnFileThread(Batch& batch);

  void StoreImageUIThread(ImagePlace place,
                          StoreImageCallback ui_thread_callback,
                          ImageFormat format,
                          scoped_refptr<base::RefCountedMemory> image_data);

  std::string StoreImageDataOnFileThread(
      ImageFormat format,
      scoped_refptr<base::RefCountedMemory> image_data);

  void FinishStoreImageOnUIThread(StoreImageCallback callback,
                                  ImagePlace place,
                                  std::string image_url);

  void LoadMappingsOnFileThread();
  void InitMappingsOnFileThread(base::Value::Dict& mappings);

  std::string GetMappingJSONOnFileThread();
  void SaveMappingsOnFileThread();

  base::FilePath GetFileMappingFilePath();
  base::FilePath GetImagePath(std::string_view thumbnail_id);
  base::FilePath GetDirectMatchImagePath(std::string_view thumbnail_id);

  void AddNewbornUrlOnFileThread(std::string_view data_url);
  void ForgetNewbornUrlOnFileThread(std::string data_url);

  // Helper to get bookmark model. It must be called from UI thread.
  bookmarks::BookmarkModel* GetBookmarkModel();

  // This must be accessed only on UI thread. It is reset to null on shutdown.
  raw_ptr<Profile> profile_;

  const base::FilePath user_data_dir_;

  // Runner to ensure that tasks to manipulate the data mapping runs in sequence
  // with the proper order.
  const scoped_refptr<base::SequencedTaskRunner> sequence_task_runner_;

  // Map path ids into their paths. Outside constructor or destructor this must
  // be accessed only from the sequence_task_runner_.
  std::map<std::string, base::FilePath> path_id_map_;

  // urls that have data stored but that are not stored themselves. This
  // prevents their removal in RemoveUnusedUrlData. This must be accessed only
  // from sequence_task_runner_.
  std::vector<std::string> file_thread_newborn_urls_;

  // Protect GC from running twice at the same time
  std::atomic_flag gc_in_progress_ = ATOMIC_FLAG_INIT;

  // Number of images created sinc the last GC run.
  std::atomic<int> images_stored_since_last_gc_ = 0;
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_IMAGE_STORE_H_
