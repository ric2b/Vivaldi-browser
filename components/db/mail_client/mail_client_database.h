// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "message_table.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

namespace base {
class FilePath;
}

namespace mail_client {

// We try to keep most logic out of the mail database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in mail_client_backend.cc.
class MailClientDatabase : public MessageTable {
 public:
  MailClientDatabase();
  ~MailClientDatabase() override;
  MailClientDatabase(const MailClientDatabase&) = delete;
  MailClientDatabase& operator=(const MailClientDatabase&) = delete;
  void Close();
  // Call before Init() to set the error callback to be used for the
  // underlying database connection.
  void set_error_callback(const sql::Database::ErrorCallback& error_callback) {
    db_.set_error_callback(error_callback);
  }

  // Must call this function to complete initialization. Will return
  // sql::INIT_OK on success. Otherwise, no other function should be called. You
  // may want to call BeginExclusiveMode after this when you are ready.
  sql::InitStatus Init(const base::FilePath& mail_client_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics(const base::FilePath& filename);

  // Returns the current version that we will generate mail databases with.
  static int GetCurrentVersion();

  // Transactions on the mail database. Use the Transaction object above
  // for most work instead of these directly. We support nested transactions
  // and only commit when the outermost transaction is committed. This means
  // that it is impossible to rollback a specific transaction. We could roll
  // back the outermost transaction if any inner one is rolled back, but it
  // turns out we don't really need this type of integrity for the mail
  // database, so we just don't support it.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {  // for debugging and assertion purposes
    return db_.transaction_nesting();
  }
  void RollbackTransaction();

  // Try to trim the cache memory used by the database.  If |aggressively| is
  // true try to trim all unused cache, otherwise trim by half.
  void TrimMemory(bool aggressively);

  // Razes the database. Returns true if successful.
  bool Raze();

  std::string GetDiagnosticInfo(int extended_error, sql::Statement* statement);

 private:
  sql::Database& GetDB() override;
  sql::InitStatus EnsureCurrentVersion();

  sql::Database db_;
  sql::MetaTable meta_table_;

  base::Time cached_early_expiration_threshold_;
};

}  // namespace mail_client

#endif  //  COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_H_
