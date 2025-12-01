/*
 * Copyright 2022 Google LLC
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

#include "fcp/tensorflow/serve_slices_registry.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "fcp/base/random_token.h"
#include "fcp/tensorflow/host_object.h"
#include "pybind11_abseil/absl_casters.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor.pb.h"

namespace pybind11::detail {

// Type caster converting a Tensor from C++ to Python.
template <>
struct type_caster<tensorflow::Tensor> {
  PYBIND11_TYPE_CASTER(tensorflow::Tensor, const_name("Tensor"));

  static handle cast(const tensorflow::Tensor& tensor, return_value_policy,
                     handle) {
    // We'd ideally use tensorflow::TensorToNdarray, but that function isn't
    // available to code running in custom ops. Instead, we pass the Tensor
    // as a serialized proto and convert to an ndarray in Python.
    tensorflow::TensorProto proto;
    if (tensor.dtype() == tensorflow::DT_STRING) {
      // Strings encoded using AsProtoTensorContent are incompatible with
      // tf.make_ndarray.
      tensor.AsProtoField(&proto);
    } else {
      tensor.AsProtoTensorContent(&proto);
    }
    std::string serialized = proto.SerializeAsString();
    return PyBytes_FromStringAndSize(serialized.data(), serialized.size());
  }
};

}  // namespace pybind11::detail

namespace {

namespace py = ::pybind11;

// A variant of fcp::ServeSlicesCallback with Python-friendly types.
using ServeSlicesCallback = std::function<std::string(
    /*callback_token=*/py::bytes,
    /*server_val=*/std::vector<tensorflow::Tensor>,
    /*max_key=*/int32_t,
    /*select_fn_initialize_op=*/std::string,
    /*select_fn_server_val_input_tensor_names=*/std::vector<std::string>,
    /*select_fn_key_input_tensor_name=*/absl::string_view,
    /*select_fn_filename_input_tensor_name=*/absl::string_view,
    /*select_fn_target_tensor_name=*/absl::string_view)>;

// A fcp::HostObjectRegistration wrapper allowing use as a context manager.
class ServeSlicesCallbackRegistration {
 public:
  explicit ServeSlicesCallbackRegistration(ServeSlicesCallback callback)
      : callback_(std::move(callback)) {}

  py::bytes enter() {
    registration_ = fcp::register_serve_slices_callback(
        [this](fcp::RandomToken callback_token,
               std::vector<tensorflow::Tensor> server_val, int32_t max_key,
               std::string select_fn_initialize_op,
               std::vector<std::string> select_fn_server_val_input_tensor_names,
               absl::string_view select_fn_key_input_tensor_name,
               absl::string_view select_fn_filename_input_tensor_name,
               absl::string_view select_fn_target_tensor_name) {
          // The GIL isn't normally held in the context of ServeSlicesCallbacks,
          // which are typically invoked from the ServeSlices TensorFlow op.
          py::gil_scoped_acquire acquire;
          return callback_(callback_token.ToString(), std::move(server_val),
                           max_key, std::move(select_fn_initialize_op),
                           std::move(select_fn_server_val_input_tensor_names),
                           select_fn_key_input_tensor_name,
                           select_fn_filename_input_tensor_name,
                           select_fn_target_tensor_name);
        });
    return registration_->token().ToString();
  }

  void exit(py::object, py::object, py::object) { registration_.reset(); }

 private:
  ServeSlicesCallback callback_;
  std::optional<fcp::HostObjectRegistration> registration_;
};

PYBIND11_MODULE(_serve_slices_op, m) {
  py::class_<ServeSlicesCallbackRegistration>(m,
                                              "ServeSlicesCallbackRegistration")
      .def("__enter__", &ServeSlicesCallbackRegistration::enter)
      .def("__exit__", &ServeSlicesCallbackRegistration::exit);

  m.def(
      "register_serve_slices_callback",
      [](ServeSlicesCallback callback) {
        return ServeSlicesCallbackRegistration(std::move(callback));
      },
      py::return_value_policy::move);
}

}  // namespace
