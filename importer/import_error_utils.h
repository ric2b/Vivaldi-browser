// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_IMPORT_ERROR_UTILS_H_
#define IMPORTER_IMPORT_ERROR_UTILS_H_

#include "base/files/file_path.h"
#include "base/values.h"
#include "importer/viv_import_result.h"
#include "sql/database.h"

// Centralized file operations with consistent error handling
class ImportFileOperations {
 public:
  // Safe file reading with proper error mapping
  static ImportValue<std::string> ReadFileToString(
      const base::FilePath& file_path,
      int not_found_error_id,
      int read_failed_error_id);

  // Check file existence with appropriate error message
  static ImportResult CheckFileExists(const base::FilePath& file_path,
                                      int not_found_error_id);

  // Parse JSON with consistent error handling
  static ImportResult ParseJsonFile(const base::FilePath& file_path,
                                    base::Value* result,
                                    int not_found_error_id,
                                    int read_failed_error_id,
                                    int parse_error_id);
};

// Centralized database operations with consistent error handling
class ImportDatabaseOperations {
 public:
  // Open database with error mapping
  static ImportResult OpenDatabase(const base::FilePath& db_path,
                                   sql::Database* database,
                                   int open_error_id);
};

#endif  // IMPORTER_IMPORT_ERROR_UTILS_H_