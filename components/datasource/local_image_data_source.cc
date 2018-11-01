// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/local_image_data_source.h"

#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "content/public/browser/browser_thread.h"

LocalImageDataClassHandler::LocalImageDataClassHandler() {}

LocalImageDataClassHandler::~LocalImageDataClassHandler() {}

bool LocalImageDataClassHandler::GetData(
    Profile* profile,
    const std::string& data_id,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  extensions::VivaldiDataSourcesAPI* api =
      extensions::VivaldiDataSourcesAPI::GetFactoryInstance()->Get(profile);

  api->GetDataForId(data_id, callback);

  return true;
}
