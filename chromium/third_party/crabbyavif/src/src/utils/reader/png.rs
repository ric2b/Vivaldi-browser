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

pub struct PngReader {
    filename: String,
}

impl PngReader {
    pub fn create(filename: &str) -> AvifResult<Self> {
        Ok(Self {
            filename: filename.into(),
        })
    }
}

impl Reader for PngReader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64)> {
        let file = File::open(self.filename.clone()).map_err(AvifError::map_unknown_error)?;
        let decoder = png::Decoder::new(file);
        let mut reader = decoder.read_info().map_err(AvifError::map_unknown_error)?;
        let mut decoded_bytes = vec![0u8; reader.output_buffer_size()];
        let info = reader
            .next_frame(&mut decoded_bytes)
            .map_err(AvifError::map_unknown_error)?;
        let rgb_bytes = &decoded_bytes[..info.buffer_size()];
        let rgb = rgb::Image {
            width: info.width,
            height: info.height,
            depth: match info.bit_depth {
                png::BitDepth::Eight => 8,
                png::BitDepth::Sixteen => 16,
                _ => {
                    return AvifError::unknown_error(format!(
                        "png bit depth is not supported: {:#?}",
                        info.bit_depth
                    ));
                }
            },
            format: match info.color_type {
                png::ColorType::Rgb => rgb::Format::Rgb,
                png::ColorType::Rgba => rgb::Format::Rgba,
                _ => {
                    return AvifError::unknown_error(format!(
                        "png color type not supported: {:#?}",
                        info.color_type
                    ));
                }
            },
            pixels: match info.bit_depth {
                png::BitDepth::Eight => Some(Pixels::Buffer(rgb_bytes.to_vec())),
                png::BitDepth::Sixteen => {
                    let mut rgb_bytes16: Vec<u16> = Vec::new();
                    for bytes in rgb_bytes.chunks_exact(2) {
                        rgb_bytes16.push(u16::from_be_bytes([bytes[0], bytes[1]]));
                    }
                    Some(Pixels::Buffer16(rgb_bytes16))
                }
                _ => {
                    return AvifError::unknown_error(format!(
                        "png bit depth is not supported: {:#?}",
                        info.bit_depth
                    ));
                }
            },
            row_bytes: info.line_size as u32,
            ..Default::default()
        };
        let mut yuv = Image {
            width: info.width,
            height: info.height,
            depth: config.depth.unwrap_or(std::cmp::min(rgb.depth, 16)),
            yuv_format: config.yuv_format.unwrap_or(PixelFormat::Yuv420),
            yuv_range: YuvRange::Full,
            matrix_coefficients: config
                .matrix_coefficients
                .unwrap_or(MatrixCoefficients::Bt601),
            ..Default::default()
        };
        rgb.convert_to_yuv(&mut yuv)?;
        Ok((yuv, 0))
    }

    fn has_more_frames(&mut self) -> bool {
        // TODO: b/403090413 - maybe support APNG?
        false
    }
}
