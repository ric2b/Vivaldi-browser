// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_
#define COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_

#include <string>
#include "base/win/scoped_co_mem.h"
#include "components/datasource/vivaldi_data_source.h"

class DesktopWallpaperDataClassHandlerWin : public VivaldiDataClassHandler {
 public:
  DesktopWallpaperDataClassHandlerWin();
  ~DesktopWallpaperDataClassHandlerWin() override;

  void GetData(Profile* profile,
               const std::string& data_id,
               content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimetype(Profile* profile,
                          const std::string& data_id) override;

 private:
  void GetDataOnFileThread(std::wstring file_path,
                           content::URLDataSource::GotDataCallback callback);

  void SendDataResultsOnUiThread(
      scoped_refptr<base::RefCountedMemory> image_data,
      std::wstring path,
      content::URLDataSource::GotDataCallback callback);

  // Path to the previously served wallpaper
  std::wstring previous_path_;

  // Cache image data
  scoped_refptr<base::RefCountedMemory> cached_image_data_;
};

#endif  // COMPONENTS_DATASOURCE_DESKTOP_DATA_SOURCE_WIN_H_
