// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! A no_std-friendly wrapper around `np_adv` to allow dynamic-style
//! advertisement serialization, with correctness checked at run-time.

/// Dynamic wrappers around extended adv serialization.
pub mod extended;
/// Dynamic wrappers around legacy adv serialization.
pub mod legacy;
