// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_

#include <string>

#include "components/datasource/vivaldi_data_source.h"
#include "components/datasource/vivaldi_data_source_api.h"

class NotesAttachmentDataClassHandler : public VivaldiDataClassHandler {
 public:
  void GetData(Profile* profile,
               const std::string& data_id,
               content::URLDataSource::GotDataCallback callback) override;
};

#endif  // COMPONENTS_DATASOURCE_NOTES_ATTACHMENT_DATA_SOURCE_H_
