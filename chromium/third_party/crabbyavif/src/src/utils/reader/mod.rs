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

#[cfg(feature = "jpeg")]
pub mod jpeg;
#[cfg(feature = "png")]
pub mod png;
pub mod y4m;

use crate::image::Image;
use crate::AvifResult;
use crate::MatrixCoefficients;
use crate::PixelFormat;

#[derive(Default)]
pub struct Config {
    pub yuv_format: Option<PixelFormat>,
    pub depth: Option<u8>,
    pub matrix_coefficients: Option<MatrixCoefficients>,
}

pub trait Reader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<Image>;
    fn has_more_frames(&mut self) -> bool;
}
