// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_
#define COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_

#include <string>
#include "components/datasource/vivaldi_data_source.h"

class DesktopWallpaperDataClassHandlerWin : public VivaldiDataClassHandler {
 public:
  DesktopWallpaperDataClassHandlerWin();
  ~DesktopWallpaperDataClassHandlerWin() override;

  bool GetData(
      Profile* profile,
      const std::string& data_id,
      const content::URLDataSource::GotDataCallback& callback) override;

 private:
   // Path to the previously served wallpaper
   std::wstring previous_path_;

   // Cache image data
   scoped_refptr<base::RefCountedMemory> cached_image_data_;
};

#endif  // COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_
