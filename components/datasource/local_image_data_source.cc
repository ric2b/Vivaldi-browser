// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/local_image_data_source.h"

#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "content/public/browser/browser_thread.h"

LocalImageDataClassHandler::LocalImageDataClassHandler(
    content::BrowserContext* browser_context,
    extensions::VivaldiDataSourcesAPI::UrlKind url_kind)
    : data_sources_api_(extensions::VivaldiDataSourcesAPI::FromBrowserContext(
          browser_context)),
      url_kind_(url_kind) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_sources_api_);
}

LocalImageDataClassHandler::~LocalImageDataClassHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalImageDataClassHandler::GetData(
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!data_sources_api_) {
    std::move(callback).Run(nullptr);
    return;
  }

  data_sources_api_->GetDataForId(url_kind_, data_id, std::move(callback));
}
