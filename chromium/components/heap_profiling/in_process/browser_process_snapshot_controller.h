// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class SequencedTaskRunner;
}

namespace heap_profiling {

namespace mojom {
class SnapshotController;
}

// The probability of including a child process in each snapshot that's taken
// when kHeapProfilerCentralControl is enabled, as a percentage from 0 to 100.
// Defaults to 100, but can be set lower to sub-sample process types that are
// very common (mainly renderers) to keep data volume low. Samples from child
// processes are weighted in inverse proportion to the snapshot probability to
// normalize the aggregated results.
extern const base::FeatureParam<int> kGpuSnapshotProbability;
extern const base::FeatureParam<int> kNetworkSnapshotProbability;
extern const base::FeatureParam<int> kRendererSnapshotProbability;
extern const base::FeatureParam<int> kUtilitySnapshotProbability;

// Sends notifications to ChildProcessSnapshotController endpoints in child
// processes to trigger snapshots on demand from the HeapProfilerController in
// the current browser process.
//
// Unless otherwise noted all methods must be called on the main thread.
class BrowserProcessSnapshotController {
 public:
  // Returns the BrowserProcessSnapshotController for this process or nullptr if
  // none exists.
  static BrowserProcessSnapshotController* GetInstance();

  // Outside of tests, BrowserProcessSnapshotController is created and owned by
  // the HeapProfilerController. `snapshot_task_runner` will be used to take
  // heap snapshots off of the main thread.
  explicit BrowserProcessSnapshotController(
      scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner);

  // The destructor must be called on `snapshot_task_runner` so that
  // WeakPtr's are invalidated on the correct sequence.
  ~BrowserProcessSnapshotController();

  BrowserProcessSnapshotController(const BrowserProcessSnapshotController&) =
      delete;
  BrowserProcessSnapshotController& operator=(
      const BrowserProcessSnapshotController&) = delete;

  // Returns a pointer that must be dereferenced on `snapshot_task_runner`.
  // This can be called from any thread.
  base::WeakPtr<BrowserProcessSnapshotController> GetWeakPtr();

  // Sets a callback that registers a remote endpoint to send commands to a
  // child process. `callback` will be invoked from BindRemoteForChildProcess()
  // with a `child_process_id` and a `pending_receiver`. The callback should
  // bind the `pending_receiver` to a mojo::Receiver in the child process with
  // ID `child_process_id`. The BrowserProcessSnapshotController will hold the
  // mojo::Remote end of the connection.
  using BindRemoteCallback = base::RepeatingCallback<void(
      int /*child_process_id*/,
      mojo::PendingReceiver<mojom::SnapshotController> /*pending_receiver*/)>;
  void SetBindRemoteForChildProcessCallback(BindRemoteCallback callback);

  // Binds a remote endpoint to communicate with `child_process_id`.
  void BindRemoteForChildProcess(
      int child_process_id,
      metrics::CallStackProfileParams::Process child_process_type);

  // Triggers snapshots in all known child processes. For each process type, a
  // random sample of processes of will be snapshotted based on the
  // `k(ProcessType)SnapshotProbability` feature params. This must be called on
  // `snapshot_task_runner`.
  void TakeSnapshotsOnSnapshotSequence();

  // Causes TakeSnapshotsOnSnapshotSequence() to choose processes to snapshot
  // deterministically: the first processes found of each type will always be
  // snapshotted, instead of a random sample.
  void SuppressRandomnessForTesting();

 private:
  // Saves `remote` in the RemoteSet for `process_type`. Must be called on
  // `snapshot_task_runner`.
  void StoreRemoteOnSnapshotSequence(
      mojo::PendingRemote<mojom::SnapshotController> remote,
      metrics::CallStackProfileParams::Process process_type);

  SEQUENCE_CHECKER(main_sequence_checker_);
  SEQUENCE_CHECKER(snapshot_sequence_checker_);

  // A task runner to trigger snapshots off of the main thread.
  scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner_;

  // Callback used to bind a mojo remote to a child process.
  BindRemoteCallback bind_remote_callback_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // Remotes for controlling child processes, split up by process type. Must be
  // accessed on `snapshot_task_runner_`. Note that RemoteSet isn't movable so
  // flat_map needs to hold a pointer to it.
  base::flat_map<metrics::CallStackProfileParams::Process,
                 std::unique_ptr<mojo::RemoteSet<mojom::SnapshotController>>>
      remotes_by_process_type_ GUARDED_BY_CONTEXT(snapshot_sequence_checker_);

  // If true, processes to snapshot are chosen deterministically.
  bool suppress_randomness_for_testing_ = false;

  base::WeakPtrFactory<BrowserProcessSnapshotController> weak_factory_{this};
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_
