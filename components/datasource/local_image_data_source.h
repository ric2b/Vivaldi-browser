// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_

#include <string>
#include "components/datasource/vivaldi_data_source.h"

class LocalImageDataClassHandler : public VivaldiDataClassHandler {
 public:
  LocalImageDataClassHandler();
  ~LocalImageDataClassHandler() override;

  bool GetData(
      Profile* profile,
      const std::string& data_id,
      const content::URLDataSource::GotDataCallback& callback) override;
};

#endif  // COMPONENTS_DATASOURCE_LOCAL_IMAGE_DATA_SOURCE_H_
