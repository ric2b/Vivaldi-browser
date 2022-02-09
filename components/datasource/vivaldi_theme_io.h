// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_DATASOURCE_VIVALDI_THEME_IO_H_
#define COMPONENTS_DATASOURCE_VIVALDI_THEME_IO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "content/public/browser/url_data_source.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace vivaldi_theme_io {

// The maximum size of the theme archive. This matches the limit on unzipped
// data on the server, not the max upload size, to account for the worst case of
// a possible increase of the archive size when the server re-compresses it.
constexpr int64_t kMaxArchiveSize = 30 * 1024 * 1024;

// The maximum size of an individual image in the archive. The server does not
// have any individual limit, so match this to the max unzipped archive size.
constexpr size_t kMaxImageSize = 30 * 1024 * 1024;

// Check if |value| is a valid theme JSON object and normalize it if necessary
// to a canonical form. On return with errors |error| contains an error message.
// When |for_export| is true, do export-specific verification and normalization.
void VerifyAndNormalizeJson(bool for_export,
                            base::Value& value,
                            std::string& error);

// Export result callback. |dataBlob| is non-empty on success when |Export()|
// was called with empty |theme_archive| argument.
using ExportResult =
    base::OnceCallback<void(std::vector<uint8_t> dataBlob, bool success)>;

// Export the given theme to a file at |theme_archive| path or, when the latter
// is empty, to a memory blob.
void Export(content::BrowserContext* browser_context,
            base::Value theme_object,
            base::FilePath theme_archive,
            ExportResult callback);

struct ImportError {
  enum Kind {
    kIO,
    kBadArchive,
    kBadSettings,
  };
  ImportError(Kind kind, std ::string details = std::string())
      : kind(kind), details(std::move(details)) {}

  Kind kind;

  // TODO(igor@vivaldi.com): Presently this is just a low-level error message
  // not suitable to show in UI but rather to facilitate debugging of problems.
  // So use more structural info when we need to present the errors to the user.
  std::string details;
};

using ImportResult =
    base::OnceCallback<void(std::string theme_id,
                            std::unique_ptr<ImportError> error)>;
// Import a theme either from the given file path or from a memory blob. Exactly
// one of |theme_archive_path|, |theme_archive_data| must be non-empty. The
// imported theme is stored in themes.preview preference and its id is passed to
// the callback on success.
void Import(base::WeakPtr<content::BrowserContext> browser_context,
            base::FilePath theme_archive_path,
            std::vector<uint8_t> theme_archive_data,
            ImportResult callback);

// Call |callback| on each url embedded into preferences containing user themes.
void EnumerateUserThemeUrls(
    PrefService* prefs,
    base::RepeatingCallback<void(base::StringPiece url)> callback);

bool StoreImageUrl(PrefService* prefs,
                   base::StringPiece theme_id,
                   std::string url);

// Returns version or empty string for the given theme id.
double FindVersionByThemeId(PrefService* prefs, const std::string& guid);

}  // namespace vivaldi_theme_io

#endif  // COMPONENTS_DATASOURCE_VIVALDI_THEME_IO_H_
