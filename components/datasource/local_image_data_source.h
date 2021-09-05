// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_

#include <string>
#include "components/datasource/vivaldi_data_source.h"
#include "components/datasource/vivaldi_data_source_api.h"

class LocalImageDataClassHandler : public VivaldiDataClassHandler {
 public:
  LocalImageDataClassHandler(
      content::BrowserContext* browser_context,
      extensions::VivaldiDataSourcesAPI::UrlKind url_kind);

  ~LocalImageDataClassHandler() override;

  void GetData(
      const std::string& data_id,
      content::URLDataSource::GotDataCallback callback) override;

 private:
  scoped_refptr<extensions::VivaldiDataSourcesAPI> data_sources_api_;
  const extensions::VivaldiDataSourcesAPI::UrlKind url_kind_;
};

#endif  // COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
