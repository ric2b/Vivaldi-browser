// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_UPDATE_BACKEND_H_
#define UPDATE_UPDATE_BACKEND_H_

#include "base/cancelable_callback.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "update_backend_notifier.h"

#if BUILDFLAG(IS_WIN)
#include "update/win/vivaldi_updater_win.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace update {

class UpdateBackend : public base::RefCountedThreadSafe<update::UpdateBackend>,
                      public UpdateBackendNotifier {
 public:
  class UpdateDelegate {
   public:
    virtual ~UpdateDelegate() {}

    virtual void NotifyUpdateProgress(const AutoUpdateStatus& status,
                                      const std::string& reason,
                                      const int progress) = 0;
  };

  explicit UpdateBackend(UpdateDelegate* delegate);

  UpdateBackend();
  UpdateBackend(UpdateDelegate* delegate,
                scoped_refptr<base::SequencedTaskRunner> task_runner);

  void Init();
  void Closing();

  void StartUpdate(bool should_install_update);
  void NotifyUpdateProgress(const AutoUpdateStatus& status,
                            const std::string& reason,
                            const int progress) override;

 protected:
  ~UpdateBackend() override;
  UpdateBackend(const UpdateBackend&) = delete;
  UpdateBackend& operator=(const UpdateBackend&) = delete;

 private:
  friend class base::RefCountedThreadSafe<UpdateBackend>;

  std::unique_ptr<UpdateDelegate> delegate_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<update::UpdaterCheckVivaldi> driver_;
#endif

  // Used to restrict to one running update task.
  int update_counter_ = 0;

  base::WeakPtrFactory<UpdateBackendNotifier> weak_ptr_factory_;
};

}  // namespace update

#endif  // UPDATE_UPDATE_BACKEND_H_
