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

use crate::image::*;
use crate::reformat::rgb;
use crate::AvifError;
use crate::AvifResult;

use super::Writer;

use image::codecs::jpeg;
use std::fs::File;

#[derive(Default)]
pub struct JpegWriter {
    pub quality: Option<u8>,
}

impl Writer for JpegWriter {
    fn write_frame(&mut self, file: &mut File, image: &Image) -> AvifResult<()> {
        let mut rgb = rgb::Image::create_from_yuv(image);
        rgb.depth = 8;
        rgb.format = rgb::Format::Rgb;
        rgb.allocate()?;
        rgb.convert_from_yuv(image)?;

        let rgba_pixels = rgb.pixels.as_ref().unwrap();
        let mut encoder = jpeg::JpegEncoder::new_with_quality(file, self.quality.unwrap_or(90));
        encoder
            .encode(
                rgba_pixels.slice(0, rgba_pixels.size() as u32)?,
                image.width,
                image.height,
                image::ExtendedColorType::Rgb8,
            )
            .map_err(AvifError::map_unknown_error)?;
        Ok(())
    }
}
