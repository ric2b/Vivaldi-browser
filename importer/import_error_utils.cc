// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "importer/import_error_utils.h"

#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "sql/statement.h"

// ImportFileOperations implementation
ImportValue<std::string> ImportFileOperations::ReadFileToString(
    const base::FilePath& file_path,
    int not_found_error_id,
    int read_failed_error_id) {
  if (!base::PathExists(file_path)) {
    return import_result::Error<std::string>(not_found_error_id);
  }

  std::string content;

  if (!base::ReadFileToString(file_path, &content)) {
    return import_result::Error<std::string>(read_failed_error_id);
  }

  return import_result::Success<std::string>(std::move(content));
}

ImportResult ImportFileOperations::CheckFileExists(
    const base::FilePath& file_path,
    int not_found_error_id) {
  if (!base::PathExists(file_path)) {
    return import_result::Error(not_found_error_id);
  }

  return import_result::Success();
}

ImportResult ImportFileOperations::ParseJsonFile(
    const base::FilePath& file_path,
    base::Value* result,
    int not_found_error_id,
    int read_failed_error_id,
    int parse_error_id) {
  std::string content;
  auto read_result = ReadFileToString(file_path, not_found_error_id, read_failed_error_id);
  if (!read_result.has_value()) {
    return import_result::Error(read_result.error());
  }

  std::optional<base::Value> parsed = base::JSONReader::Read(read_result.value());
  if (!parsed) {
    return import_result::Error(parse_error_id);
  }

  *result = std::move(*parsed);
  return import_result::Success();
}

// ImportDatabaseOperations implementation
ImportResult ImportDatabaseOperations::OpenDatabase(
    const base::FilePath& db_path,
    sql::Database* database,
    int open_error_id) {
  if (!base::PathExists(db_path)) {
    return import_result::Error(open_error_id);
  }

  if (!database->Open(db_path)) {
    return import_result::Error(open_error_id);
  }

  return import_result::Success();
}
