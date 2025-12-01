// Copyright 2025 Google LLC
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

// Not all sub-modules are used by all targets. Ignore dead code warnings.
#![allow(dead_code)]

pub(crate) mod jpeg;
pub(crate) mod png;
pub(crate) mod y4m;

use crabby_avif::image::Image;
use crabby_avif::AvifResult;

use std::fs::File;

pub trait Writer {
    fn write_frame(&mut self, file: &mut File, image: &Image) -> AvifResult<()>;
}
