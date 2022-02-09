// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_DATABASE_H_
#define CONTACT_CONTACT_DATABASE_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

#include "contact/contact_table.h"
#include "contact/email_table.h"
#include "contact/phonenumber_table.h"
#include "contact/postaladdress_table.h"

namespace base {
class FilePath;
}

namespace contact {

// Encapsulates the SQL connection for the contact database table. This class
// holds the database connection and has methods the contact system (including
// full text search) uses for writing and retrieving information.
//
// We try to keep most logic out of the contact database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in ContactBackend.cc.
class ContactDatabase : public ContactTable,
                        public EmailTable,
                        public PhonenumberTable,
                        public PostalAddressTable {
  // public ContactTable,
  // public RecurrrenceTable {
 public:
  ContactDatabase();
  ~ContactDatabase() override;
  ContactDatabase(const ContactDatabase&) = delete;
  ContactDatabase& operator=(const ContactDatabase&) = delete;

  // Call before Init() to set the error callback to be used for the
  // underlying database connection.
  void set_error_callback(const sql::Database::ErrorCallback& error_callback) {
    db_.set_error_callback(error_callback);
  }

  // Must call this function to complete initialization. Will return
  // sql::INIT_OK on success. Otherwise, no other function should be called. You
  // may want to call BeginExclusiveMode after this when you are ready.
  sql::InitStatus Init(const base::FilePath& contact_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics(const base::FilePath& filename);

  // Call to set the mode on the database to exclusive. The default locking mode
  // is "normal" but we want to run in exclusive mode for slightly better
  // performance since we know nobody else is using the database. This is
  // separate from Init() since the in-memory database attaches to slurp the
  // data out, and this can't happen in exclusive mode.
  void BeginExclusiveMode();

  // Returns the current version that we will generate contact databases with.
  static int GetCurrentVersion();

  // Transactions on the contact database. Use the Transaction object above
  // for most work instead of these directly. We support nested transactions
  // and only commit when the outermost transaction is committed. This means
  // that it is impossible to rollback a specific transaction. We could roll
  // back the outermost transaction if any inner one is rolled back, but it
  // turns out we don't really need this type of integrity for the contact
  // database, so we just don't support it.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {  // for debugging and assertion purposes
    return db_.transaction_nesting();
  }
  void RollbackTransaction();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

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
};

}  // namespace contact

#endif  //  CONTACT_CONTACT_DATABASE_H_
