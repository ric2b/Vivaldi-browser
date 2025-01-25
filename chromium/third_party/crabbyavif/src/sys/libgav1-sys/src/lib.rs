// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#[allow(warnings)]
pub mod bindings {
    // Blaze does not support the `OUT_DIR` configuration used by Cargo. Instead, it specifies a
    // complete path to the generated bindings as an environment variable.
    #[cfg(google3)]
    include!(env!("CRABBYAVIF_LIBGAV1_BINDINGS_RS"));
    #[cfg(not(google3))]
    include!(concat!(env!("OUT_DIR"), "/libgav1_bindgen.rs"));
}
