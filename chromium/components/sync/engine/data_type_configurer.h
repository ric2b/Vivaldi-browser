// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONFIGURER_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONFIGURER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/configure_reason.h"

namespace syncer {

struct DataTypeActivationResponse;

// The DataTypeConfigurer interface abstracts out the action of configuring a
// set of new data types and cleaning up after a set of removed data types.
// Lives on the UI thread.
class DataTypeConfigurer {
 public:
  // Utility struct for holding ConfigureDataTypes options.
  struct ConfigureParams {
    ConfigureParams();

    ConfigureParams(const ConfigureParams&) = delete;
    ConfigureParams& operator=(const ConfigureParams&) = delete;

    ConfigureParams(ConfigureParams&& other);
    ConfigureParams& operator=(ConfigureParams&& other);

    ~ConfigureParams();

    ConfigureReason reason = CONFIGURE_REASON_UNKNOWN;
    DataTypeSet to_download;
    DataTypeSet to_purge;

    base::OnceCallback<void(DataTypeSet succeeded, DataTypeSet failed)>
        ready_task;

    // Whether full sync (or sync the feature) is enabled;
    bool is_sync_feature_enabled = false;
  };

  DataTypeConfigurer();
  virtual ~DataTypeConfigurer();

  // Changes the set of data types that are currently being synced.
  virtual void ConfigureDataTypes(ConfigureParams params) = 0;

  // Connects the datatype |type|, which means the sync engine will propagate
  // changes between the server and datatype's processor, as provided in
  // |activation_response|. This must be called before requesting the initial
  // download of a datatype via ConfigureDataTypes().
  virtual void ConnectDataType(
      DataType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) = 0;

  // Opposite of the above: stops treating |type| as a datatype that is
  // propagating changes between the server and the processor. No-op if the
  // type is not connected.
  virtual void DisconnectDataType(DataType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_CONFIGURER_H_
