// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

#include <memory>
#include <string>
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

class Profile;

namespace base {
class RefCountedMemory;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcher;
}

// Handlers for Vivaldi data will inherit and implement this class
// for providing data to the data source.
class VivaldiDataClassHandler {
 public:
  VivaldiDataClassHandler() = default;
  virtual ~VivaldiDataClassHandler() = default;

  // The callback must be called on all code paths including any
  // failures.
  virtual void GetData(
      const std::string& data_id,
      content::URLDataSource::GotDataCallback callback) = 0;
};

class VivaldiDataSource : public content::URLDataSource {
 public:
  explicit VivaldiDataSource(Profile* profile);
  ~VivaldiDataSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& path,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;

 protected:
  bool IsCSSRequest(const std::string& path) const;

  // Use flat_map, not std::map, as it allows to find a value by StringPiece
  // without allocating a temporary std::string.
  base::flat_map<std::string, std::unique_ptr<VivaldiDataClassHandler>>
      data_class_handlers_;

 private:
  void ExtractRequestTypeAndData(base::StringPiece path,
                                 base::StringPiece& type,
                                 base::StringPiece& data) const;

  DISALLOW_COPY_AND_ASSIGN(VivaldiDataSource);
};

class VivaldiThumbDataSource : public VivaldiDataSource {
 public:
  explicit VivaldiThumbDataSource(Profile* profile);
  ~VivaldiThumbDataSource() override;

    // content::URLDataSource implementation.
  std::string GetSource() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiThumbDataSource);
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_
