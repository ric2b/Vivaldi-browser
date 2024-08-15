// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTE_MODEL_LOADER_H_
#define COMPONENTS_NOTES_NOTE_MODEL_LOADER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace vivaldi {

class NoteLoadDetails;

// NoteModelLoader is created by NotesModel to track loading of NotesModel.
// NoteModelLoader may be used on multiple threads. NoteModelLoader may outlive
// NotesModel.
class NoteModelLoader : public base::RefCountedThreadSafe<NoteModelLoader> {
 public:
  using LoadCallback =
      base::OnceCallback<void(std::unique_ptr<NoteLoadDetails>)>;
  // Creates the ModelLoader, and schedules loading on a backend task runner.
  // |callback| is run once loading completes (on the main thread).
  static scoped_refptr<NoteModelLoader> Create(
      const base::FilePath& profile_path,
      std::unique_ptr<NoteLoadDetails> details,
      LoadCallback callback);

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  void BlockTillLoaded();

  // Returns null until the model has loaded. Use BlockTillLoaded() to ensure
  // this returns non-null.
 private:
  friend base::RefCountedThreadSafe<NoteModelLoader>;
  NoteModelLoader();
  ~NoteModelLoader();
  NoteModelLoader(const NoteModelLoader&) = delete;
  NoteModelLoader& operator=(const NoteModelLoader&) = delete;

  // Performs the load on a background thread.
  std::unique_ptr<NoteLoadDetails> DoLoadOnBackgroundThread(
      const base::FilePath& profile_path,
      std::unique_ptr<NoteLoadDetails> details);

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Signaled once loading completes.
  base::WaitableEvent loaded_signal_;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTE_MODEL_LOADER_H_
