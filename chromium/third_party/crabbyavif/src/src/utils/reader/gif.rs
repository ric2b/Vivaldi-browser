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

use crate::reformat::*;
use crate::utils::pixels::Pixels;
use crate::AvifError;
use crate::AvifResult;
use crate::*;

use super::Config;
use super::Reader;

use std::fs::File;

pub struct GifReader {
    decoder: gif::Decoder<File>,
    screen: Option<gif_dispose::Screen>,
    frame: Option<gif::Frame<'static>>,
}

impl GifReader {
    pub fn create(filename: &str) -> AvifResult<Self> {
        let input = File::open(filename).map_err(AvifError::map_unknown_error)?;
        let mut options = gif::DecodeOptions::new();
        options.set_color_output(gif::ColorOutput::Indexed);
        Ok(Self {
            decoder: options
                .read_info(input)
                .map_err(AvifError::map_unknown_error)?,
            frame: None,
            screen: None,
        })
    }
}

impl Reader for GifReader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64)> {
        if self.frame.is_none() {
            self.frame = Some(match self.decoder.read_next_frame() {
                Ok(Some(frame)) => frame.clone(),
                _ => return AvifError::unknown_error("error reading gif frame"),
            });
        }
        if self.screen.is_none() {
            self.screen = Some(gif_dispose::Screen::new_decoder(&self.decoder));
        }
        self.screen
            .unwrap_mut()
            .blit_frame(self.frame.unwrap_ref())
            .map_err(AvifError::map_unknown_error)?;
        let (rgba_pixels, width, height) =
            self.screen.unwrap_mut().pixels_rgba().to_contiguous_buf();
        if width != self.decoder.width() as usize || height != self.decoder.height() as usize {
            return AvifError::unknown_error(
                "width/height mismatch between gif decoder and screen",
            );
        }
        let mut rgba_buffer: Vec<u8> = Vec::new();
        for rgba in rgba_pixels.iter() {
            rgba_buffer.extend_from_slice(&[rgba.r, rgba.g, rgba.b, rgba.a]);
        }
        let rgb = rgb::Image {
            width: self.decoder.width() as u32,
            height: self.decoder.height() as u32,
            depth: 8,
            format: rgb::Format::Rgba,
            pixels: Some(Pixels::Buffer(rgba_buffer)),
            row_bytes: (width * 4) as u32,
            ..Default::default()
        };
        let mut yuv = Image {
            width: self.decoder.width() as u32,
            height: self.decoder.height() as u32,
            depth: config.depth.unwrap_or(8),
            yuv_format: config.yuv_format.unwrap_or(PixelFormat::Yuv420),
            yuv_range: YuvRange::Full,
            matrix_coefficients: config
                .matrix_coefficients
                .unwrap_or(MatrixCoefficients::Bt601),
            ..Default::default()
        };
        rgb.convert_to_yuv(&mut yuv)?;
        // GIF delay is in centi-seconds.
        let duration_ms = self.frame.unwrap_ref().delay as u64 * 10;
        self.frame = None;
        Ok((yuv, duration_ms))
    }

    fn has_more_frames(&mut self) -> bool {
        if self.frame.is_some() {
            return true;
        }
        self.frame = Some(match self.decoder.read_next_frame() {
            Ok(Some(frame)) => frame.clone(),
            _ => return false,
        });
        true
    }
}
