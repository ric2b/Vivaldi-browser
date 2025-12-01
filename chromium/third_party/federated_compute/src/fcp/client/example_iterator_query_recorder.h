/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_CLIENT_EXAMPLE_ITERATOR_QUERY_RECORDER_H_
#define FCP_CLIENT_EXAMPLE_ITERATOR_QUERY_RECORDER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/task_result_info.pb.h"
#include "fcp/protos/plan.pb.h"

namespace fcp::client {

// Records a single example iterator query.
class SingleExampleIteratorQueryRecorder {
 public:
  virtual ~SingleExampleIteratorQueryRecorder() = default;
  // Increment the example count.
  virtual void Increment() = 0;
  // Finish recording, and pack the results into a SingleExampleIteratorQuery
  // message.
  virtual SingleExampleIteratorQuery FinishRecordingAndGet() = 0;
};

// Records example iterator queries.
class ExampleIteratorQueryRecorder {
 public:
  virtual ~ExampleIteratorQueryRecorder() = default;
  // Create a SingleExampleIteratorQueryRecorder which records a single query.
  virtual SingleExampleIteratorQueryRecorder* RecordQuery(
      const google::internal::federated::plan::ExampleSelector& selector) = 0;
  // Stop recording for all queries and pack the results into an
  // ExampleIteratorQueries message. ExampleIteratorQueryRecorder can be reused
  // after this method is called.
  virtual ExampleIteratorQueries StopRecordingAndGetQueries() = 0;
};

// Implementation of ExampleIteratorQueryRecorder.
// This class is thread-safe.
class ExampleIteratorQueryRecorderImpl : public ExampleIteratorQueryRecorder {
 public:
  explicit ExampleIteratorQueryRecorderImpl(
      const SelectorContext& selector_context)
      : selector_context_(selector_context){};
  // Create a SingleExampleIteratorQueryRecorder which records a single query.
  // This class keeps the ownership of the created
  // SingleExampleIteratorQueryRecorder.
  SingleExampleIteratorQueryRecorder* RecordQuery(
      const google::internal::federated::plan::ExampleSelector& selector)
      override ABSL_LOCKS_EXCLUDED(mutex_);
  // Stop recording for all queries and pack the results into an
  // ExampleIteratorQueries message. All of the stored
  // SingleExampleIteratorQueryRecorder will be cleared when this method is
  // called.
  ExampleIteratorQueries StopRecordingAndGetQueries() override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  std::vector<std::unique_ptr<SingleExampleIteratorQueryRecorder>> recorders_
      ABSL_GUARDED_BY(mutex_);
  // We make a copy of the SelectorContext to avoid the original copy got moved
  // or deleted.
  SelectorContext selector_context_;
};

// Implementation of SingleExampleIteratorQueryRecorder.
// This class is thread-safe.
class SingleExampleIteratorQueryRecorderImpl
    : public SingleExampleIteratorQueryRecorder {
 public:
  explicit SingleExampleIteratorQueryRecorderImpl(
      const google::internal::federated::plan::ExampleSelector& selector)
      : selector_(selector){};
  // Increment the example count.
  void Increment() override ABSL_LOCKS_EXCLUDED(mutex_);
  // Finish recording, and pack the results into a SingleExampleIteratorQuery
  // message.  This method also reset the example count.
  SingleExampleIteratorQuery FinishRecordingAndGet() override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  // We make a copy of the ExampleSelector here to avoid the original copy gets
  // moved or deleted.
  google::internal::federated::plan::ExampleSelector selector_
      ABSL_GUARDED_BY(mutex_);
  int32_t example_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace fcp::client

#endif  // FCP_CLIENT_EXAMPLE_ITERATOR_QUERY_RECORDER_H_
