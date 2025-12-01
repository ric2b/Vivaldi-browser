// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef DIRECT_MATCH_FAVICON_INSTALLER_H_
#define DIRECT_MATCH_FAVICON_INSTALLER_H_

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace direct_match {
class DirectMatchService;

class DirectMatchFaviconInstaller {
 public:
  struct Entry: public ImageDecoder::ImageRequest {
    Entry() = default;
    ~Entry() = default;
    std::string title_;
    GURL page_url_;
    GURL image_url_;
    base::FilePath path_;
    std::string content_;
    Profile* profile_;
    base::WeakPtr<DirectMatchFaviconInstaller> installer_;
    void OnImageDecoded(const SkBitmap& decoded_image) override;
    void OnDecodeImageFailed() override;
  };
  typedef std::vector<Entry> EntryList;
  typedef std::vector<GURL> UrlList;

  DirectMatchFaviconInstaller(Profile* profile);
  ~DirectMatchFaviconInstaller();

  void Start();
  void Start(DirectMatchService *service);
  void Completed(Entry* entry, bool success);

 private:
  static std::unique_ptr<EntryList> ReadFromDisk(
      std::unique_ptr<EntryList> entries);
  void Touch();
  void Decode(std::unique_ptr<EntryList> entries);

  Profile* profile_ = nullptr;
  std::unique_ptr<EntryList> entries_;
  UrlList touch_list_;
  base::RepeatingTimer timer_;
  base::WeakPtrFactory<DirectMatchFaviconInstaller> weak_ptr_factory_{this};
};

}  // direct_match


#endif //DIRECT_MATCH_FAVICON_INSTALLER_H_
