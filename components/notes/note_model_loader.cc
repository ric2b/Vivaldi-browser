// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/note_model_loader.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/notes/note_constants.h"
#include "components/notes/note_load_details.h"
#include "components/notes/notes_codec.h"

namespace vivaldi {

namespace {
void LoadNotes(const base::FilePath& profile_path, NoteLoadDetails* details) {
  base::FilePath path = profile_path.Append(kNotesFileName);
  bool notes_file_exists = base::PathExists(path);
  if (notes_file_exists) {
    JSONFileValueDeserializer deserializer(path);
    std::unique_ptr<base::Value> root(
        deserializer.Deserialize(nullptr, nullptr));

    if (root) {
      // Building the index can take a while, so we do it on the background
      // thread.
      int64_t max_node_id = 0;
      std::string sync_metadata_str;
      NotesCodec codec;

      codec.Decode(details->main_notes_node(), details->other_notes_node(),
                   details->trash_notes_node(), &max_node_id, *root,
                   &sync_metadata_str);
      details->set_sync_metadata_str(std::move(sync_metadata_str));
      details->set_max_id(std::max(max_node_id, details->max_id()));
      details->set_computed_checksum(codec.computed_checksum());
      details->set_stored_checksum(codec.stored_checksum());
      details->set_ids_reassigned(codec.ids_reassigned());
      details->set_uuids_reassigned(codec.uuids_reassigned());
      details->set_has_deprecated_attachments(
          codec.has_deprecated_attachments());
    }
  }
}
}  // namespace

// static
scoped_refptr<NoteModelLoader> NoteModelLoader::Create(
    const base::FilePath& profile_path,
    std::unique_ptr<NoteLoadDetails> details,
    LoadCallback callback) {
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new NoteModelLoader());
  model_loader->backend_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // We plumb the value for kEmitExperimentalBookmarkLoadUma as retrieved on
  // the UI thread to avoid issues with TSAN bots (in case there are tests that
  // override feature toggles -not necessarily this one- while bookmark loading
  // is ongoing, which is problematic due to how feature overriding for tests is
  // implemented).
  model_loader->backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NoteModelLoader::DoLoadOnBackgroundThread, model_loader,
                     profile_path, std::move(details)),
      std::move(callback));
  return model_loader;
}

void NoteModelLoader::BlockTillLoaded() {
  loaded_signal_.Wait();
}

NoteModelLoader::NoteModelLoader()
    : loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

NoteModelLoader::~NoteModelLoader() = default;

std::unique_ptr<NoteLoadDetails> NoteModelLoader::DoLoadOnBackgroundThread(
    const base::FilePath& profile_path,
    std::unique_ptr<NoteLoadDetails> details) {
  LoadNotes(profile_path, details.get());
  loaded_signal_.Signal();
  return details;
}

}  // namespace vivaldi
