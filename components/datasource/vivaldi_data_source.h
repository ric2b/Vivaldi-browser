// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

#include <memory>
#include <string>
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

#include "components/datasource/vivaldi_data_url_utils.h"

class Profile;

// Handlers for Vivaldi data will inherit and implement this class
// for providing data to the data source.
class VivaldiDataClassHandler {
 public:
  VivaldiDataClassHandler() = default;
  virtual ~VivaldiDataClassHandler() = default;

  // The callback must be called on all code paths including any
  // failures.
  virtual void GetData(Profile* profile,
                       const std::string& data_id,
                       content::URLDataSource::GotDataCallback callback) = 0;

  virtual std::string GetMimetype(Profile* profile,
                                  const std::string& data_id) = 0;
};

class VivaldiDataSource : public content::URLDataSource {
 public:
  explicit VivaldiDataSource(Profile* profile);
  ~VivaldiDataSource() override;
  VivaldiDataSource(const VivaldiDataSource&) = delete;
  VivaldiDataSource& operator=(const VivaldiDataSource&) = delete;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& path,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching(const GURL& url) override;

 private:
  using PathType = vivaldi_data_url_utils::PathType;

  const raw_ptr<Profile> profile_;
  base::flat_map<vivaldi_data_url_utils::PathType,
                 std::unique_ptr<VivaldiDataClassHandler>>
      data_class_handlers_;
};

class VivaldiThumbDataSource : public VivaldiDataSource {
 public:
  explicit VivaldiThumbDataSource(Profile* profile);
  ~VivaldiThumbDataSource() override;
  VivaldiThumbDataSource(const VivaldiThumbDataSource&) = delete;
  VivaldiThumbDataSource& operator=(const VivaldiThumbDataSource&) = delete;

  // content::URLDataSource implementation.
  std::string GetSource() override;
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_
