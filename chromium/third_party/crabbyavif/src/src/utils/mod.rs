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

use crate::*;

pub mod clap;

// Some HEIF fractional fields can be negative, hence Fraction and UFraction.
// The denominator is always unsigned.

/// cbindgen:field-names=[n,d]
#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct Fraction(pub i32, pub u32);

/// cbindgen:field-names=[n,d]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[repr(C)]
pub struct UFraction(pub u32, pub u32);

impl Fraction {
    pub(crate) fn is_valid(&self) -> AvifResult<()> {
        match self.1 {
            0 => Err(AvifError::InvalidArgument),
            _ => Ok(()),
        }
    }

    pub(crate) fn as_f64(&self) -> AvifResult<f64> {
        self.is_valid()?;
        Ok(self.0 as f64 / self.1 as f64)
    }
}

impl UFraction {
    pub(crate) fn is_valid(&self) -> AvifResult<()> {
        match self.1 {
            0 => Err(AvifError::InvalidArgument),
            _ => Ok(()),
        }
    }
}
