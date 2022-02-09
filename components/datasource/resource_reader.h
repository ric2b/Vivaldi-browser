// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_RESOURCE_READER_H_
#define COMPONENTS_DATASOURCE_RESOURCE_READER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Helper to access Vivaldi resources
class ResourceReader {
 public:
  ResourceReader();
  ResourceReader(const ResourceReader&) = delete;
  ~ResourceReader();
  ResourceReader operator=(const ResourceReader&) = delete;

  // Check if the url points to internal Vivaldi resources. If |subpath| is not
  // null on return |*subpath| holds the resource path.
  static bool IsResourceURL(base::StringPiece url,
                            std::string* subpath = nullptr);

  // Convenience method to read a resource as JSON from the given resource
  // directory and resource. |resource_directory| should not start or end with a
  // slash. If |error| is given, it will contain an error message on return in
  // case of failure. When not given, the error is logged.
  static absl::optional<base::Value> ReadJSON(
      base::StringPiece resource_directory,
      base::StringPiece resource_name,
      std::string* error = nullptr);

  // Try to open the resource. On failure |error()| contains the error text.
  bool Open(std::string resource_path);

  const uint8_t* data() const { return mapped_file_.data(); }
  size_t size() const { return mapped_file_.length(); }

  base::StringPiece as_string_view() const {
    return base::StringPiece(reinterpret_cast<const char*>(data()), size());
  }

  // Parse the asset as JSON.
  absl::optional<base::Value> ParseJSON();

  const std::string& error() const { return error_; }

 private:
  base::MemoryMappedFile mapped_file_;
  std::string resource_path_;
  std::string error_;
};

#endif  // COMPONENTS_RESOURCES_RESOURCE_READER_H_
