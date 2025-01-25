// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {

class IndexedDBDatabaseCallbacks;
class IndexedDBFactoryClient;
class IndexedDBTransaction;

// This struct holds data relevant to opening a new connection/database while
// IndexedDBConnectionCoordinator manages queued operations.
struct CONTENT_EXPORT IndexedDBPendingConnection {
  IndexedDBPendingConnection(
      std::unique_ptr<IndexedDBFactoryClient> factory_client,
      std::unique_ptr<IndexedDBDatabaseCallbacks> database_callbacks,
      int64_t transaction_id,
      int64_t version,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          pending_mojo_receiver);
  ~IndexedDBPendingConnection();

  std::unique_ptr<IndexedDBFactoryClient> factory_client;
  std::unique_ptr<IndexedDBDatabaseCallbacks> database_callbacks;
  int64_t transaction_id;
  int64_t version;
  IndexedDBDataLossInfo data_loss_info;
  // The versionchange operation, if any.
  base::WeakPtr<IndexedDBTransaction> transaction;
  mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
      pending_mojo_receiver;
  bool was_cold_open = false;
  mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
      client_state_checker;
  base::UnguessableToken client_token;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
