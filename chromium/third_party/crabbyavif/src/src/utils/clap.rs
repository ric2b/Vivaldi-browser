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

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CleanAperture {
    pub width: UFraction,
    pub height: UFraction,
    pub horiz_off: UFraction,
    pub vert_off: UFraction,
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct CropRect {
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
}

impl CropRect {
    fn is_valid(&self, image_width: u32, image_height: u32, pixel_format: PixelFormat) -> bool {
        let x_plus_width = checked_add!(self.x, self.width);
        let y_plus_height = checked_add!(self.y, self.height);
        if self.width == 0
            || self.height == 0
            || x_plus_width.is_err()
            || y_plus_height.is_err()
            || x_plus_width.unwrap() > image_width
            || y_plus_height.unwrap() > image_height
        {
            return false;
        }
        match pixel_format {
            PixelFormat::Yuv420 => self.x % 2 == 0 && self.y % 2 == 0,
            PixelFormat::Yuv422 => self.x % 2 == 0,
            _ => true,
        }
    }

    pub fn create_from(
        clap: &CleanAperture,
        image_width: u32,
        image_height: u32,
        pixel_format: PixelFormat,
    ) -> AvifResult<Self> {
        let width: IFraction = clap.width.try_into()?;
        let height: IFraction = clap.height.try_into()?;
        let horiz_off: IFraction = clap.horiz_off.try_into()?;
        let vert_off: IFraction = clap.vert_off.try_into()?;
        if width.1 <= 0
            || height.1 <= 0
            || horiz_off.1 <= 0
            || vert_off.1 <= 0
            || width.0 < 0
            || height.0 < 0
            || !width.is_integer()
            || !height.is_integer()
        {
            return Err(AvifError::UnknownError("invalid clap".into()));
        }
        let clap_width = width.get_i32();
        let clap_height = height.get_i32();
        let mut crop_x = IFraction::simplified(i32_from_u32(image_width)?, 2);
        crop_x.add(&horiz_off)?;
        crop_x.sub(&IFraction::simplified(clap_width, 2))?;
        let mut crop_y = IFraction::simplified(i32_from_u32(image_height)?, 2);
        crop_y.add(&vert_off)?;
        crop_y.sub(&IFraction::simplified(clap_height, 2))?;
        if !crop_x.is_integer() || !crop_y.is_integer() || crop_x.0 < 0 || crop_y.0 < 0 {
            return Err(AvifError::UnknownError("".into()));
        }
        let rect = CropRect {
            x: crop_x.get_u32()?,
            y: crop_y.get_u32()?,
            width: u32_from_i32(clap_width)?,
            height: u32_from_i32(clap_height)?,
        };
        if rect.is_valid(image_width, image_height, pixel_format) {
            Ok(rect)
        } else {
            Err(AvifError::UnknownError("".into()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestParam {
        image_width: u32,
        image_height: u32,
        pixel_format: PixelFormat,
        clap: CleanAperture,
        rect: Option<CropRect>,
    }

    macro_rules! valid {
        ($a: expr, $b: expr, $c: ident, $d: expr, $e: expr, $f: expr, $g: expr, $h: expr, $i: expr,
         $j: expr, $k: expr, $l: expr, $m: expr, $n: expr, $o: expr) => {
            TestParam {
                image_width: $a,
                image_height: $b,
                pixel_format: PixelFormat::$c,
                clap: CleanAperture {
                    width: UFraction($d, $e),
                    height: UFraction($f, $g),
                    horiz_off: UFraction($h, $i),
                    vert_off: UFraction($j, $k),
                },
                rect: Some(CropRect {
                    x: $l,
                    y: $m,
                    width: $n,
                    height: $o,
                }),
            }
        };
    }

    macro_rules! invalid {
        ($a: expr, $b: expr, $c: ident, $d: expr, $e: expr, $f: expr, $g: expr, $h: expr, $i: expr,
         $j: expr, $k: expr) => {
            TestParam {
                image_width: $a,
                image_height: $b,
                pixel_format: PixelFormat::$c,
                clap: CleanAperture {
                    width: UFraction($d, $e),
                    height: UFraction($f, $g),
                    horiz_off: UFraction($h, $i),
                    vert_off: UFraction($j, $k),
                },
                rect: None,
            }
        };
    }

    #[rustfmt::skip]
    const TEST_PARAMS: [TestParam; 20] = [
        valid!(120, 160, Yuv420, 96, 1, 132, 1, 0, 1, 0, 1, 12, 14, 96, 132),
        valid!(120, 160, Yuv420, 60, 1, 80, 1, -30i32 as u32, 1, -40i32 as u32, 1, 0, 0, 60, 80),
        valid!(100, 100, Yuv420, 99, 1, 99, 1, -1i32 as u32, 2, -1i32 as u32, 2, 0, 0, 99, 99),
        invalid!(120, 160, Yuv420, 96, 0, 132, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, -1i32 as u32, 132, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, 0, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, -1i32 as u32, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, 1, 0, 0, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, 1, 0, -1i32 as u32, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, 1, 0, 1, 0, 0),
        invalid!(120, 160, Yuv420, 96, 1, 132, 1, 0, 1, 0, -1i32 as u32),
        invalid!(120, 160, Yuv420, -96i32 as u32, 1, 132, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 0, 1, 132, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, -132i32 as u32, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 0, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 5, 132, 1, 0, 1, 0, 1),
        invalid!(120, 160, Yuv420, 96, 1, 132, 5, 0, 1, 0, 1),
        invalid!(722, 1024, Yuv420, 385, 1, 330, 1, 103, 1, -308i32 as u32, 1),
        invalid!(1024, 722, Yuv420, 330, 1, 385, 1, -308i32 as u32, 1, 103, 1),
        invalid!(99, 99, Yuv420, 99, 1, 99, 1, -1i32 as u32, 2, -1i32 as u32, 2),
    ];

    #[test_case::test_matrix(0usize..20)]
    fn valid_clap_to_rect(index: usize) {
        let param = &TEST_PARAMS[index];
        let rect = CropRect::create_from(
            &param.clap,
            param.image_width,
            param.image_height,
            param.pixel_format,
        );
        if param.rect.is_some() {
            assert!(rect.is_ok());
            let rect = rect.unwrap();
            let expected_rect = param.rect.unwrap_ref();
            assert_eq!(rect.x, expected_rect.x);
            assert_eq!(rect.y, expected_rect.y);
            assert_eq!(rect.width, expected_rect.width);
            assert_eq!(rect.height, expected_rect.height);
        } else {
            assert!(rect.is_err());
        }
    }
}
