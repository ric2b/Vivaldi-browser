// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_

#include <string>
#include "components/datasource/vivaldi_data_source.h"
#include "components/datasource/vivaldi_image_store.h"

class LocalImageDataClassHandler : public VivaldiDataClassHandler {
 public:
  LocalImageDataClassHandler(VivaldiImageStore::UrlKind url_kind);

  ~LocalImageDataClassHandler() override;

  void GetData(Profile* profile,
               const std::string& data_id,
               content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimetype(Profile* profile,
                          const std::string& data_id) override;

 private:
  const VivaldiImageStore::UrlKind url_kind_;
};

#endif  // COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
