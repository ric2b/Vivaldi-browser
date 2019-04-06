// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;

class MessageLoop;
}

namespace vivaldi_update_notifier {

class UpdateNotifierWindow;

class UpdateNotifierManager {
 public:
  static UpdateNotifierManager* GetInstance();
  ~UpdateNotifierManager();

  bool RunNotifier(HINSTANCE instance);

  static bool OnUpdateAvailable(const char* version);
  static void OnShutdownRequested();
  void TriggerUpdate(bool with_ui = false);

  void Disable();

  base::string16 current_version() const { return current_version_; }

 private:
  friend struct base::DefaultSingletonTraits<UpdateNotifierManager>;
  UpdateNotifierManager();

  void OnEventTriggered(base::WaitableEvent* waitable_event);

  bool IsNotifierAlreadyRunning();
  bool IsNotifierEnabled();

  void CheckForUpdates() const;
  void Quit() const;

  HINSTANCE instance_ = NULL;
  HANDLE already_exists_ = NULL;
  base::string16 current_version_;

  bool notification_accepted_ = false;
  std::unique_ptr<UpdateNotifierWindow> update_notifier_window_;
  base::MessageLoop* ui_thread_loop_ = nullptr;
  base::RunLoop run_loop_;

  std::unique_ptr<base::WaitableEvent> global_restart_event_;
  base::WaitableEventWatcher global_restart_event_watch_;

  std::unique_ptr<base::WaitableEvent> global_quit_event_;
  base::WaitableEventWatcher global_quit_event_watch_;

  std::unique_ptr<base::WaitableEvent> quit_event_;
  base::WaitableEventWatcher quit_event_watch_;

  std::unique_ptr<base::WaitableEvent> check_for_updates_event_;
  base::WaitableEventWatcher check_for_updates_event_watch_;

  DISALLOW_COPY_AND_ASSIGN(UpdateNotifierManager);
};
}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_
