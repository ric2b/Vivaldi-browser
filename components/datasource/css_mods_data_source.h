// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_CSS_MODS_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_CSS_MODS_DATA_SOURCE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/datasource/vivaldi_data_source.h"
#include "content/public/browser/browser_thread.h"

class CSSModsDataClassHandler : public VivaldiDataClassHandler {
 public:
  CSSModsDataClassHandler(Profile* profile);
  ~CSSModsDataClassHandler() override;

  bool GetData(
      const std::string& data_id,
      const content::URLDataSource::GotDataCallback& callback) override;

 private:
  static scoped_refptr<base::RefCountedMemory> GetDataForIdOnBlockingThread(
      base::FilePath dir_path, std::string data_id);
  static void PostResultsOnThread(
      const content::URLDataSource::GotDataCallback& callback,
      scoped_refptr<base::RefCountedMemory> data);

  Profile* const profile_;
};

#endif  // COMPONENTS_DATASOURCE_CSS_MODS_DATA_SOURCE_H_
