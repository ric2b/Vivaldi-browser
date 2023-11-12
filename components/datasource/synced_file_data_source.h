// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_

#include <string>

#include "components/datasource/vivaldi_data_source.h"

class SyncedFileDataClassHandler : public VivaldiDataClassHandler {
 public:
  void GetData(Profile* profile,
               const std::string& data_id,
               content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimetype(Profile* profile,
                          const std::string& data_id) override;
};

#endif  // COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_
