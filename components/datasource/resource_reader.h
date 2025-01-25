// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_RESOURCE_READER_H_
#define COMPONENTS_DATASOURCE_RESOURCE_READER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

// Helper to access Vivaldi resources
class ResourceReader {
 public:
  // Try to open the resource. If this does not succeed, `IsValid()` will return
  // false and `GetError()` will return error details.
  ResourceReader(std::string resource_path);
  ResourceReader(const ResourceReader&) = delete;
  ~ResourceReader();
  ResourceReader operator=(const ResourceReader&) = delete;

#if !BUILDFLAG(IS_ANDROID)
  // Get directory holding Vivaldi resource files. To simplify development in
  // non-official builds this may return source directory of vivapp/src, not the
  // directory from the build or installation. This way the changes to it can be
  // reflected without a rebuild.
  static const base::FilePath& GetResourceDirectory();
#endif

  // Convenience method to read a resource as JSON from the given resource
  // directory and resource. `resource_directory`, when not empty, should not
  // start or end with a slash. All errors are logged.
  static std::optional<base::Value> ReadJSON(
      std::string_view resource_directory,
      std::string_view resource_name);

  static gfx::Image ReadPngImage(std::string_view resource_url);

  bool IsValid() const { return mapped_file_.IsValid(); }

  const uint8_t* data() const { return mapped_file_.data(); }
  size_t size() const { return mapped_file_.length(); }

  base::span<const uint8_t> bytes() const { return mapped_file_.bytes(); }

  std::string_view as_string_view() const {
    return std::string_view(reinterpret_cast<const char*>(data()), size());
  }

  // Parse the asset as JSON.
  std::optional<base::Value> ParseJSON();

  std::string GetError() const;

  // Return true if the open error was due to missing resource.
  bool IsNotFoundError() const {
    DCHECK(!IsValid());
    return not_found_error_;
  }

 private:
  base::MemoryMappedFile mapped_file_;
  std::string resource_path_;
  std::string error_;
  bool not_found_error_ = false;
};

#endif  // COMPONENTS_RESOURCES_RESOURCE_READER_H_
