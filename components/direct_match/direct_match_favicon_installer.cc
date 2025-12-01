// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "direct_match_favicon_installer.h"

#include "base/barrier_callback.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "components/favicon_base/favicon_util.h"
#include "components/history/core/browser/history_service.h"
#include "direct_match_service.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace direct_match {

void DirectMatchFaviconInstaller::Entry::OnImageDecoded(const SkBitmap& bitmap) {
  history::HistoryService* service =
    HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (service) {
    const gfx::ImageSkia icon(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    icon.EnsureRepsForSupportedScales();
    std::vector<SkBitmap> bitmaps;
    const std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
    for (const gfx::ImageSkiaRep& rep : icon.image_reps()) {
      if (base::Contains(favicon_scales, rep.scale())) {
        bitmaps.push_back(rep.GetBitmap());
      }
    }
    service->SetOnDemandFavicons(page_url_,
                                 favicon_base::IconType::kFavicon,
                                 image_url_,
                                 bitmaps,
                                 // We do not need to know if icon was installed
                                 // or not as a failure is about if it (the url)
                                 // is already present in the DB.
                                 base::BindOnce([](bool state) {}));
  }
  if (installer_) {
    installer_->Completed(this, true);
  }
}

void DirectMatchFaviconInstaller::Entry::OnDecodeImageFailed() {
  LOG(ERROR) << "Failed to decode image " << path_;
  if (installer_) {
    installer_->Completed(this, false);
  }
}

DirectMatchFaviconInstaller::DirectMatchFaviconInstaller(Profile* profile)
  : profile_(profile) {}

DirectMatchFaviconInstaller::~DirectMatchFaviconInstaller() {
  if (entries_) {
    for (auto& entry : *entries_) {
      ImageDecoder::Cancel(&entry);
    }
  }
}

void DirectMatchFaviconInstaller::Start() {
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      profile_);
  if (service) {
    Start(service);
  } else {
    LOG(ERROR) << "Failed to load direct match service";
  }
}

void DirectMatchFaviconInstaller::Start(DirectMatchService* service) {
  const auto& sites = service->GetPopularSites();
  if (sites.size() > 0) {
    base::FilePath user_data_dir = base::FilePath(profile_->GetPath().Append(
        vivaldi_image_store::kDirectMatchImageDirectory));
    std::unique_ptr<EntryList> entries =
        base::WrapUnique(new EntryList(sites.size()));
    int i = 0;
    for (const auto& site : sites) {
      Entry& entry = entries->at(i++);
      entry.profile_ = profile_;
      entry.installer_ = weak_ptr_factory_.GetWeakPtr();
      entry.title_ = site->title;
      entry.page_url_ = GURL(site->redirect_url);
      entry.image_url_ = GURL(site->image_url);
      entry.path_ = user_data_dir.Append(
          base::FilePath::FromUTF8Unsafe(entry.image_url_.ExtractFileName()));
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&DirectMatchFaviconInstaller::ReadFromDisk,
                       std::move(entries)),
        base::BindOnce(&DirectMatchFaviconInstaller::Decode,
                       weak_ptr_factory_.GetWeakPtr()));
    // This will keep ondemand icons in cache.
    timer_.Start(FROM_HERE, base::Days(7),
               this, &DirectMatchFaviconInstaller::Touch);
  }
}

// static
std::unique_ptr<DirectMatchFaviconInstaller::EntryList>
DirectMatchFaviconInstaller::ReadFromDisk(std::unique_ptr<EntryList> entries) {
  for (auto& entry : *entries) {
    std::string content;
    if (base::ReadFileToString(base::FilePath(entry.path_), &content)) {
      entry.content_ = content;
    }
  }
  return entries;
}

void DirectMatchFaviconInstaller::Touch() {
  if (profile_) {
    history::HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
    if (service) {
      for (auto& url : touch_list_) {
        service->TouchOnDemandFavicon(url);
      }
    }
  }
}

void DirectMatchFaviconInstaller::Completed(Entry* completed_entry,
                                            bool success) {
  for (auto& entry : *entries_) {
    if (&entry == completed_entry) {
      if (success) {
        touch_list_.push_back(entry.image_url_);
      }
      entry.profile_ = nullptr;
      entry.installer_ = nullptr;
      entry.title_.clear();
      entry.content_.clear();
      entry.page_url_ = GURL();
      entry.image_url_ = GURL();
      entry.path_.clear();
    }
  }
}

void DirectMatchFaviconInstaller::Decode(std::unique_ptr<EntryList> entries) {
  // Remove existing pending entries, if any.
  if (entries_) {
    for (auto& entry : *entries_) {
      ImageDecoder::Cancel(&entry);
    }
  }
  touch_list_.clear();

  entries_ = std::move(entries);
  for (auto& entry : *entries_) {
    ImageDecoder::Start(&entry, entry.content_);
  }
}

}  // direct_match
