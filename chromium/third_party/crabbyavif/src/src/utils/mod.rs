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

use crate::internal_utils::*;
use crate::*;

pub mod clap;
pub mod pixels;
pub mod reader;
pub mod writer;

// Some HEIF fractional fields can be negative, hence Fraction and UFraction.
// The denominator is always unsigned.

/// cbindgen:field-names=[n,d]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
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

// 'clap' fractions do not follow this pattern: both numerators and denominators
// are used as i32, but they are signalled as u32 according to the specification
// as of 2024. This may be fixed in later versions of the specification, see
// https://github.com/AOMediaCodec/libavif/pull/1749#discussion_r1391612932.
/// cbindgen:field-names=[n,d]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[repr(C)]
pub struct IFraction(pub i32, pub i32);

impl TryFrom<UFraction> for IFraction {
    type Error = AvifError;

    fn try_from(uf: UFraction) -> AvifResult<IFraction> {
        Ok(IFraction(uf.0 as i32, i32_from_u32(uf.1)?))
    }
}

impl IFraction {
    // This function is not used in all configurations.
    #[allow(unused)]
    pub(crate) fn is_valid(&self) -> AvifResult<()> {
        match self.1 {
            0 => Err(AvifError::InvalidArgument),
            _ => Ok(()),
        }
    }

    fn gcd(a: i32, b: i32) -> i32 {
        let mut a = if a < 0 { -a as i64 } else { a as i64 };
        let mut b = if b < 0 { -b as i64 } else { b as i64 };
        while b != 0 {
            let r = a % b;
            a = b;
            b = r;
        }
        a as i32
    }

    pub(crate) fn simplified(n: i32, d: i32) -> Self {
        let mut fraction = IFraction(n, d);
        fraction.simplify();
        fraction
    }

    pub(crate) fn simplify(&mut self) {
        let gcd = Self::gcd(self.0, self.1);
        if gcd > 1 {
            self.0 /= gcd;
            self.1 /= gcd;
        }
    }

    pub(crate) fn get_i32(&self) -> i32 {
        assert!(self.1 != 0);
        self.0 / self.1
    }

    pub(crate) fn get_u32(&self) -> AvifResult<u32> {
        u32_from_i32(self.get_i32())
    }

    pub(crate) fn is_integer(&self) -> bool {
        self.0 % self.1 == 0
    }

    fn common_denominator(&mut self, val: &mut IFraction) -> AvifResult<()> {
        self.simplify();
        if self.1 == val.1 {
            return Ok(());
        }
        let self_d = self.1;
        self.0 = self
            .0
            .checked_mul(val.1)
            .ok_or(AvifError::UnknownError("".into()))?;
        self.1 = self
            .1
            .checked_mul(val.1)
            .ok_or(AvifError::UnknownError("".into()))?;
        val.0 = val
            .0
            .checked_mul(self_d)
            .ok_or(AvifError::UnknownError("".into()))?;
        val.1 = val
            .1
            .checked_mul(self_d)
            .ok_or(AvifError::UnknownError("".into()))?;
        Ok(())
    }

    pub(crate) fn add(&mut self, val: &IFraction) -> AvifResult<()> {
        let mut val = *val;
        val.simplify();
        self.common_denominator(&mut val)?;
        self.0 = self
            .0
            .checked_add(val.0)
            .ok_or(AvifError::UnknownError("".into()))?;
        self.simplify();
        Ok(())
    }

    pub(crate) fn sub(&mut self, val: &IFraction) -> AvifResult<()> {
        let mut val = *val;
        val.simplify();
        self.common_denominator(&mut val)?;
        self.0 = self
            .0
            .checked_sub(val.0)
            .ok_or(AvifError::UnknownError("".into()))?;
        self.simplify();
        Ok(())
    }
}
