// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tasks_provider.h"

#include <optional>
#include <vector>

#include "ash/api/tasks/tasks_controller.h"
#include "ash/api/tasks/tasks_delegate.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/barrier_closure.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The tasks UI has limited space, so we restrict to showing N tasks.
constexpr size_t kTasksToFetch = 5;

// In order to get these tasks, we first query the API for task lists. We then
// query the task lists until we have received at least N tasks. To reduce
// latency, we query up to `kListFetchBatchSize` task lists in parallel.
constexpr size_t kListFetchBatchSize = 8;

// Controls the amount of time we'll serve a cached version of the task list.
constexpr base::TimeDelta kCacheLifetime = base::Seconds(30);

// Used to sort tasks for the carousel.
struct TaskComparator {
  // Tasks are classified into these groups and within each group sorted by
  // their update time. Tasks that have been created by the user in the focus
  // mode UI appear first, followed by past due tasks and so on.
  enum class TaskGroupOrdering {
    kCreatedInSession,
    kPastDue,
    kDueSoon,
    kDueLater,
  };

  bool operator()(const FocusModeTask& lhs, const FocusModeTask& rhs) const {
    auto lhs_group = GetOrdering(lhs);
    auto rhs_group = GetOrdering(rhs);
    if (lhs_group != rhs_group) {
      return lhs_group < rhs_group;
    }

    return lhs.updated > rhs.updated;
  }

  TaskGroupOrdering GetOrdering(const FocusModeTask& entry) const {
    if (created_task_ids->contains(entry.task_id)) {
      return TaskGroupOrdering::kCreatedInSession;
    }

    auto remaining = entry.due.value_or(base::Time::Max()) - now;
    if (remaining < base::Hours(0)) {
      return TaskGroupOrdering::kPastDue;
    } else if (remaining < base::Hours(24)) {
      return TaskGroupOrdering::kDueSoon;
    }
    return TaskGroupOrdering::kDueLater;
  }

  base::Time now;
  raw_ref<base::flat_set<TaskId>> created_task_ids;
};

}  // namespace

std::strong_ordering TaskId::operator<=>(const TaskId& other) const {
  if (pending && other.pending) {
    // Two pending ids are always equivalent.
    return std::strong_ordering::equivalent;
  }
  if (pending != other.pending) {
    // If pending does not match, use the ordering for bools.
    return pending <=> other.pending;
  }

  if (list_id < other.list_id || (list_id == other.list_id && id < other.id)) {
    return std::strong_ordering::less;
  }
  if (list_id > other.list_id || (list_id == other.list_id && id > other.id)) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equivalent;
}

FocusModeTask::FocusModeTask() = default;
FocusModeTask::~FocusModeTask() = default;
FocusModeTask::FocusModeTask(const FocusModeTask&) = default;
FocusModeTask::FocusModeTask(FocusModeTask&&) = default;
FocusModeTask& FocusModeTask::operator=(const FocusModeTask&) = default;
FocusModeTask& FocusModeTask::operator=(FocusModeTask&&) = default;

// Helper used to fetch tasks from the API. It starts by querying for task
// lists, and then queries tasks from each list.
class TaskFetcher {
 public:
  void Start(base::OnceClosure done) {
    done_ = std::move(done);
    api::TasksController::Get()->tasks_delegate()->GetTaskLists(
        /*force_fetch=*/true, base::BindOnce(&TaskFetcher::OnGetTaskLists,
                                             weak_factory_.GetWeakPtr()));
  }

  std::string GetMostRecentlyUpdatedTaskList() const {
    return task_lists_.empty() ? "" : task_lists_[0].first;
  }

  std::vector<FocusModeTask> GetTasks() && { return std::move(tasks_); }

  bool error() const { return error_; }

 private:
  void OnGetTaskLists(bool sucess,
                      const ui::ListModel<api::TaskList>* api_task_lists) {
    if (!api_task_lists) {
      error_ = true;
      std::move(done_).Run();
      return;
    }
    if (api_task_lists->item_count() == 0) {
      std::move(done_).Run();
      return;
    }

    // Collect the task lists and sort them so that the greatest one is first.
    task_lists_.reserve(api_task_lists->item_count());
    for (const auto& list : *api_task_lists) {
      task_lists_.emplace_back(list->id, list->updated);
    }
    base::ranges::sort(task_lists_, std::greater{},
                       &std::pair<std::string, base::Time>::second);

    MaybeFetchMoreTasks();
  }

  // If we haven't yet fetched enough tasks to show *and* there are lists that
  // haven't yet been queried, then try to fetch more tasks. In any other case,
  // we invoke the done callback.
  void MaybeFetchMoreTasks() {
    const auto lists_left = task_lists_.size() - task_list_fetch_index_;
    if (lists_left == 0 || tasks_.size() >= kTasksToFetch) {
      // We are done.
      std::move(done_).Run();
      return;
    }

    const auto batch_size = std::min(lists_left, kListFetchBatchSize);
    auto barrier = base::BarrierClosure(
        batch_size, base::BindOnce(&TaskFetcher::MaybeFetchMoreTasks,
                                   weak_factory_.GetWeakPtr()));

    // The code here is structured so that we don't modify any members after
    // calling `GetTasks`. This is done so that the code still works if
    // `GetTasks` invokes the callback synchronously (which happens in tests).
    auto next_task_list_index = task_list_fetch_index_;
    task_list_fetch_index_ += batch_size;

    auto* delegate = api::TasksController::Get()->tasks_delegate();
    for (size_t i = 0; i != batch_size; ++i) {
      const std::string& list_id = task_lists_[next_task_list_index++].first;
      delegate->GetTasks(
          list_id,
          /*force_fetch=*/true,
          base::BindOnce(&TaskFetcher::OnGetTasks, weak_factory_.GetWeakPtr(),
                         list_id, barrier));
    }
  }

  void OnGetTasks(const std::string& list_id,
                  base::RepeatingClosure barrier,
                  bool success,
                  const ui::ListModel<api::Task>* api_tasks) {
    // NOTE: Completed tasks will not show up in `api_tasks`.
    if (success && api_tasks) {
      for (const auto& api_task : *api_tasks) {
        // Skip tasks with empty titles.
        if (api_task->title.empty()) {
          continue;
        }
        FocusModeTask& task = tasks_.emplace_back();
        task.task_id = {.list_id = list_id, .id = api_task->id};
        task.title = api_task->title;
        task.updated = api_task->updated;
        task.due = api_task->due;
      }
    }

    // Do not do anything with `this` after this line since the fetcher will be
    // deleted after the last list has been queried.
    std::move(barrier).Run();
  }

  bool error_ = false;

  // Task list IDs, sorted by creation time.
  std::vector<std::pair<std::string, base::Time>> task_lists_;

  // The index of the next task list to fetch tasks for.
  std::size_t task_list_fetch_index_ = 0;

  // Tasks fetched.
  std::vector<FocusModeTask> tasks_;

  // Invoked when the fetcher is complete.
  base::OnceClosure done_;

  base::WeakPtrFactory<TaskFetcher> weak_factory_{this};
};

FocusModeTasksProvider::FocusModeTasksProvider() = default;
FocusModeTasksProvider::~FocusModeTasksProvider() = default;

void FocusModeTasksProvider::ScheduleTaskListUpdate() {
  if (!task_fetcher_) {
    // We don't start a new fetch if a fetch is already running.
    task_fetcher_ = std::make_unique<TaskFetcher>();
    task_fetcher_->Start(base::BindOnce(&FocusModeTasksProvider::OnTasksFetched,
                                        weak_factory_.GetWeakPtr()));
  }
}

void FocusModeTasksProvider::GetSortedTaskList(OnGetTasksCallback callback) {
  if ((base::Time::Now() - task_fetch_time_) < kCacheLifetime) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetSortedTasksImpl()));
    return;
  }

  get_tasks_requests_.push_back(std::move(callback));
  ScheduleTaskListUpdate();
}

void FocusModeTasksProvider::GetTask(const std::string& task_list_id,
                                     const std::string& task_id,
                                     OnGetTaskCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());

  api::TasksController::Get()->tasks_delegate()->GetTasks(
      task_list_id, /*force_fetch=*/true,
      base::BindOnce(&FocusModeTasksProvider::OnTasksFetchedForTask,
                     weak_factory_.GetWeakPtr(), task_list_id, task_id,
                     std::move(callback)));
}

void FocusModeTasksProvider::AddTask(const std::string& title,
                                     OnTaskSavedCallback callback) {
  if (task_list_for_new_task_.empty()) {
    // TODO(b/339667327): Instead of failing the request, consider queueing it.
    std::move(callback).Run(FocusModeTask{});
    return;
  }

  // Clear the cache. This is done so that the backend is queried the next time
  // a task list is requested. This in turn is done so that we can get the
  // actual ID of the newly created task.
  task_fetch_time_ = {};

  auto* delegate = api::TasksController::Get()->tasks_delegate();
  delegate->AddTask(
      task_list_for_new_task_, title,
      base::BindOnce(&FocusModeTasksProvider::OnTaskSaved,
                     weak_factory_.GetWeakPtr(), task_list_for_new_task_, "",
                     false, std::move(callback)));
}

void FocusModeTasksProvider::UpdateTask(const std::string& task_list_id,
                                        const std::string& task_id,
                                        const std::string& title,
                                        bool completed,
                                        OnTaskSavedCallback callback) {
  CHECK(!task_id.empty());
  CHECK(!task_list_id.empty());

  if (completed) {
    deleted_task_ids_.insert({.list_id = task_list_id, .id = task_id});
  }

  api::TasksController::Get()->tasks_delegate()->UpdateTask(
      task_list_id, task_id, title, completed,
      base::BindOnce(&FocusModeTasksProvider::OnTaskSaved,
                     weak_factory_.GetWeakPtr(), task_list_id, task_id,
                     completed, std::move(callback)));
}

void FocusModeTasksProvider::OnTasksFetched() {
  CHECK(task_fetcher_);

  if (!task_fetcher_->error()) {
    task_fetch_time_ = base::Time::Now();
    task_list_for_new_task_ = task_fetcher_->GetMostRecentlyUpdatedTaskList();
    tasks_ = std::move(*task_fetcher_).GetTasks();
  } else {
    tasks_ = {};
    task_list_for_new_task_ = {};
  }
  task_fetcher_ = nullptr;

  auto pending = std::move(get_tasks_requests_);
  auto tasks = GetSortedTasksImpl();
  for (auto& callback : pending) {
    std::move(callback).Run(tasks);
  }
}

void FocusModeTasksProvider::OnTasksFetchedForTask(
    const std::string& task_list_id,
    const std::string& task_id,
    OnGetTaskCallback callback,
    bool success,
    const ui::ListModel<api::Task>* api_tasks) {
  FocusModeTask task;

  if (success) {
    // NOTE: Completed tasks will not show up in `api_tasks`, so we first assume
    // it's completed and update the state if the task is found in `api_tasks`.
    // TODO: Can we actually verify that the task is complete instead of making
    // this assumption?
    task.task_id = {.list_id = task_list_id, .id = task_id};
    task.completed = true;

    for (const auto& api_task : *api_tasks) {
      if (api_task->id == task_id) {
        task.title = api_task->title;
        task.updated = api_task->updated;
        task.completed = api_task->completed;
        break;
      }
    }
  }

  std::move(callback).Run(task);
}

void FocusModeTasksProvider::OnTaskSaved(const std::string& task_list_id,
                                         const std::string& task_id,
                                         bool completed,
                                         OnTaskSavedCallback callback,
                                         const api::Task* api_task) {
  if (!api_task || api_task->title.empty()) {
    // If there's an error, we clear the cache.
    task_fetch_time_ = {};
    if (completed) {
      deleted_task_ids_.erase({.list_id = task_list_id, .id = task_id});
    }
    std::move(callback).Run(FocusModeTask());
    return;
  }

  TaskId created_id = {.list_id = task_list_id, .id = api_task->id};
  created_task_ids_.insert(created_id);

  // Try to find the task in the cache or insert it.
  auto iter = base::ranges::find(tasks_, created_id, &FocusModeTask::task_id);

  FocusModeTask& task = (iter != tasks_.end()) ? *iter : tasks_.emplace_back();
  task.task_id = created_id;
  task.title = api_task->title;
  task.updated = api_task->updated;

  std::move(callback).Run(task);
}

std::vector<FocusModeTask> FocusModeTasksProvider::GetSortedTasksImpl() {
  std::vector<FocusModeTask> result;
  for (const FocusModeTask& task : tasks_) {
    if (!deleted_task_ids_.contains(task.task_id)) {
      result.push_back(task);
    }
  }

  base::ranges::sort(
      result, TaskComparator{base::Time::Now(), raw_ref<base::flat_set<TaskId>>(
                                                    created_task_ids_)});

  return result;
}

}  // namespace ash
