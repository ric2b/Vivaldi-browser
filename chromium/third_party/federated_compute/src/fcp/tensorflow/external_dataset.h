/*
 * Copyright 2019 Google LLC
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

#ifndef FCP_TENSORFLOW_EXTERNAL_DATASET_H_
#define FCP_TENSORFLOW_EXTERNAL_DATASET_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/base/bounds.h"
#include "fcp/tensorflow/host_object.h"

namespace fcp {

/**
 * Interface for an iterator, created from a particular dataset. A single
 * dataset may be used to create multiple iterators.
 */
class ExternalDatasetIterator {
 public:
  virtual ~ExternalDatasetIterator() = default;

  /**
   * Returns the next element, if possible. Indicates end-of-stream with
   * OUT_OF_RANGE, even when repeatedly called. Corresponds to
   * tensorflow::data::IteratorBase::GetNext.
   *
   * Implementations must be thread-safe.
   */
  virtual absl::StatusOr<std::string> GetNext() = 0;
};

namespace external_dataset_internal {

template <typename FuncType>
class DatasetFromFunction;

}  // namespace external_dataset_internal

/**
 * Interface for a particular dataset - created from an ExternalDatasetProvider
 * (during dataset op execution), for a particular selector. A dataset may be
 * used zero or more times to create an ExternalDatasetIterator.
 *
 * Dataset implementations are often trivial, just needing to capture some
 * values (like the selector) for the iterator constructor. Consider using
 * ExternalDataset::FromFunction.
 */
class ExternalDataset {
 public:
  virtual ~ExternalDataset() = default;

  /**
   * Creates a new iterator. Corresponds to
   * tensorflow::data::DatasetBase::MakeIterator.
   */
  virtual std::unique_ptr<ExternalDatasetIterator> MakeIterator() = 0;

  /**
   * Creates an ExternalDataset that wraps a callable object 'f', implementing
   * MakeIterator(). The lifetime of 'f' is that of the dataset (so,
   * by-reference lambda captures are almost always unsafe here).
   */
  template <typename F>
  static std::unique_ptr<ExternalDataset> FromFunction(F f) {
    return std::make_unique<external_dataset_internal::DatasetFromFunction<F>>(
        std::move(f));
  }
};

/**
 * Interface for an ExternalDataset op's host object.
 *
 * An ExternalDatasetProvider is a function from Selector -> ExternalDataset.
 * Here, 'Selector' is a string provided to the dataset op (typically, an
 * encoded proto). The returned ExternalDataset may be used (perhaps multiple
 * times) to create an iterator.
 *
 * When implementing a dataset provider and the selector is a proto message,
 * consider inheritng from ExternalDatasetProvider::UsingProtoSelector<T> (for
 * some message type T).
 */
class ExternalDatasetProvider {
 public:
  virtual ~ExternalDatasetProvider() = default;

  /**
   * Creates a dataset for a given selector.
   *
   * This function can usually be implemented succinctly, using
   * ExternalDataset::FromFunction.
   *
   * Corresponds to tensorflow::data::DatasetOpKernel::MakeDataset.
   */
  virtual absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      absl::string_view selector) = 0;

  /**
   * Base class for dataset providers that expect a selector of a particular
   * proto message type. If inheriting from UsingProtoSelector<T>, then one
   * implements MakeDataset(T) instead of MakeDataset(absl::string_view).
   */
  template <typename T>
  class UsingProtoSelector;
};

/**
 * HostObjectRegistry for the ExternalDataset interface.
 */
using ExternalDatasetProviderRegistry =
    HostObjectRegistry<ExternalDatasetProvider>;

namespace external_dataset_internal {

template <typename T>
absl::StatusOr<T> TryParseProtoSelector(absl::string_view selector) {
  T msg;
  if (!msg.ParseFromArray(selector.data(),
                          CastIntegerChecked<int>(selector.size()))) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to parse selector proto of type ", msg.GetTypeName()));
  }

  return msg;
}

template <typename FuncType>
class DatasetFromFunction : public ExternalDataset {
 public:
  explicit DatasetFromFunction(FuncType func) : func_(std::move(func)) {}

  std::unique_ptr<ExternalDatasetIterator> MakeIterator() final {
    return func_();
  }

 private:
  FuncType func_;
};

}  // namespace external_dataset_internal

template <typename T>
class ExternalDatasetProvider::UsingProtoSelector
    : public ExternalDatasetProvider {
 public:
  absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      absl::string_view selector) final {
    auto maybe_msg =
        external_dataset_internal::TryParseProtoSelector<T>(selector);
    if (!maybe_msg.ok()) {
      return maybe_msg.status();
    }

    return MakeDataset(std::move(maybe_msg).value());
  }

  virtual absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      T selector) = 0;
};

}  // namespace fcp

#endif  // FCP_TENSORFLOW_EXTERNAL_DATASET_H_
