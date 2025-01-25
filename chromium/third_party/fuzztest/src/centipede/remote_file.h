// Copyright 2022 The Centipede Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A very simple abstract API for working with potentially remote files.
// The implementation may use any file API, including the plain C FILE API, C++
// streams, or an actual API for dealing with remote files. The abstractions are
// the same as in the C FILE API.

// TODO(ussuri): Add unit tests (currently tested via .sh integration tests).

#ifndef THIRD_PARTY_CENTIPEDE_REMOTE_FILE_H_
#define THIRD_PARTY_CENTIPEDE_REMOTE_FILE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "./centipede/defs.h"
#ifndef CENTIPEDE_DISABLE_RIEGELI
#include "riegeli/bytes/reader.h"
#include "riegeli/bytes/writer.h"
#endif  // CENTIPEDE_DISABLE_RIEGELI

namespace centipede {

// An opaque file handle.
struct RemoteFile {};

// Opens a (potentially remote) file 'file_path' and returns a handle to it.
// Supported modes: "r", "a", "w", same as in C FILE API.
absl::Nullable<RemoteFile *> RemoteFileOpen(std::string_view file_path,
                                            const char *mode);

// Closes the file previously opened by RemoteFileOpen.
void RemoteFileClose(absl::Nonnull<RemoteFile *> f);

// Adjusts the buffered I/O capacity for a file opened for writing. By default,
// the internal buffer of size `BUFSIZ` is used. May only be used after opening
// a file, but before performing any other operations on it. Violating this
// requirement in general can cause undefined behavior.
void RemoteFileSetWriteBufferSize(absl::Nonnull<RemoteFile *> f, size_t size);

// Appends bytes from 'ba' to 'f'.
void RemoteFileAppend(absl::Nonnull<RemoteFile *> f, const ByteArray &ba);

// Appends characters from 'contents' to 'f'.
void RemoteFileAppend(absl::Nonnull<RemoteFile *> f,
                      const std::string &contents);

// Flushes the file's internal buffer. Some dynamic results of a running
// pipeline are consumed by itself (e.g. shard cross-pollination) and can be
// consumed by external processes (e.g. monitoring): for such files, call this
// API after every write to ensure that they are in a valid state.
void RemoteFileFlush(absl::Nonnull<RemoteFile *> f);

// Reads all current contents of 'f' into 'ba'.
void RemoteFileRead(absl::Nonnull<RemoteFile *> f, ByteArray &ba);

// Reads all current contents of 'f' into 'contents'.
void RemoteFileRead(absl::Nonnull<RemoteFile *> f, std::string &contents);

// Creates a (potentially remote) directory 'dir_path', as well as any missing
// parent directories. No-op if the directory already exists.
void RemoteMkdir(std::string_view dir_path);

// Sets the contents of the file at 'path' to 'contents'.
void RemoteFileSetContents(const std::filesystem::path &path,
                           const ByteArray &contents);

// Sets the contents of the file at 'path' to 'contents'.
void RemoteFileSetContents(const std::filesystem::path &path,
                           const std::string &contents);

// Reads the contents of the file at 'path' into 'contents'.
void RemoteFileGetContents(const std::filesystem::path &path,
                           ByteArray &contents);

// Reads the contents of the file at 'path' into 'contents'.
void RemoteFileGetContents(const std::filesystem::path &path,
                           std::string &contents);

// Returns true if `path` exists.
bool RemotePathExists(std::string_view path);

// Returns the size of the file at `path` in bytes. The file must exist.
int64_t RemoteFileGetSize(std::string_view path);

// Finds all files matching `glob` and appends them to `matches`.
void RemoteGlobMatch(std::string_view glob, std::vector<std::string> &matches);

// Lists all files within `path`, recursively expanding subdirectories if
// `recursively` is true. Does not return any directories. Returns an empty
// vector if `path` is an empty directory, or `path` does not exist. Returns
// `{path}` if `path` is a non-directory.
std::vector<std::string> RemoteListFiles(std::string_view path,
                                         bool recursively);

// Renames `from` to `to`.
void RemotePathRename(std::string_view from, std::string_view to);

// Deletes `path`. If `path` is a directory and `recursively` is true,
// recursively deletes all files and subdirectories within `path`.
void RemotePathDelete(std::string_view path, bool recursively);

#ifndef CENTIPEDE_DISABLE_RIEGELI
// Returns a reader for the file at `file_path`.
std::unique_ptr<riegeli::Reader> CreateRiegeliFileReader(
    std::string_view file_path);

// Returns a writer for the file at `file_path`.
// If `append` is `true`, writes will append to the end of the file if it
// exists. If `false, the file will be truncated to empty if it exists.
std::unique_ptr<riegeli::Writer> CreateRiegeliFileWriter(
    std::string_view file_path, bool append);
#endif  // CENTIPEDE_DISABLE_RIEGELI

}  // namespace centipede

#endif  // THIRD_PARTY_CENTIPEDE_REMOTE_FILE_H_
