// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/local_image_data_source.h"

#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_image_store.h"
#include "content/public/browser/browser_thread.h"

LocalImageDataClassHandler::LocalImageDataClassHandler(
    VivaldiImageStore::UrlKind url_kind)
    : url_kind_(url_kind) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

LocalImageDataClassHandler::~LocalImageDataClassHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalImageDataClassHandler::GetData(
    Profile* profile,
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VivaldiImageStore::GetDataForId(profile, url_kind_, data_id,
                                  std::move(callback));
}
