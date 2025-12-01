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

#[cfg(feature = "libyuv")]
pub mod libyuv;
#[cfg(feature = "libyuv")]
pub mod scale;
#[cfg(feature = "sharpyuv")]
pub mod sharpyuv;

pub mod alpha;
pub mod coeffs;
pub mod rgb;
pub mod rgb_impl;

// If libyuv is not present, add placeholder functions so that the library will build successfully
// without it.
#[cfg(not(feature = "libyuv"))]
pub mod libyuv {
    use crate::reformat::*;
    use crate::*;

    pub(crate) fn yuv_to_rgb(
        _image: &image::Image,
        _rgb: &mut rgb::Image,
    ) -> AvifResult<Option<bool>> {
        Ok(None) // Not implemented.
    }

    pub(crate) fn rgb_to_yuv(
        _rgb: &rgb::Image,
        _image: &mut image::Image,
    ) -> AvifResult<Option<()>> {
        Ok(None) // Not implemented.
    }

    pub(crate) fn convert_to_half_float(
        _rgb: &mut rgb::Image,
        _scale: f32,
    ) -> AvifResult<Option<()>> {
        Ok(None) // Not implemented.
    }

    impl image::Image {
        pub(crate) fn scale(
            &mut self,
            width: u32,
            height: u32,
            _category: Category,
        ) -> AvifResult<()> {
            if self.width == width && self.height == height {
                return Ok(());
            }
            AvifError::not_implemented()
        }
    }
}

// If libsharpyuv is not present, add placeholder functions so that the library will build successfully
// without it.
#[cfg(not(feature = "sharpyuv"))]
pub mod sharpyuv {
    use crate::reformat::*;
    use crate::*;

    pub(crate) fn rgb_to_yuv(_rgb: &rgb::Image, _image: &mut image::Image) -> AvifResult<()> {
        AvifError::not_implemented()
    }
}
