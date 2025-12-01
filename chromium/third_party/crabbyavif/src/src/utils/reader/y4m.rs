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
use crate::*;

use std::fs::File;
use std::io::prelude::*;

use super::Config;
use super::Reader;

use std::io::BufReader;
use std::io::Read;

#[derive(Debug, Default)]
pub struct Y4MReader {
    width: u32,
    height: u32,
    depth: u8,
    has_alpha: bool,
    format: PixelFormat,
    range: YuvRange,
    chroma_sample_position: ChromaSamplePosition,
    reader: Option<BufReader<File>>,
}

impl Y4MReader {
    fn parse_colorspace(&mut self, colorspace: &str) -> AvifResult<()> {
        (
            self.depth,
            self.format,
            self.chroma_sample_position,
            self.has_alpha,
        ) = match colorspace {
            "420jpeg" => (
                8,
                PixelFormat::Yuv420,
                ChromaSamplePosition::default(),
                false,
            ),
            "420mpeg2" => (
                8,
                PixelFormat::Yuv420,
                ChromaSamplePosition::Vertical,
                false,
            ),
            "420paldv" => (
                8,
                PixelFormat::Yuv420,
                ChromaSamplePosition::Colocated,
                false,
            ),
            "444p10" => (
                10,
                PixelFormat::Yuv444,
                ChromaSamplePosition::default(),
                false,
            ),
            "422p10" => (
                10,
                PixelFormat::Yuv422,
                ChromaSamplePosition::default(),
                false,
            ),
            "420p10" => (
                10,
                PixelFormat::Yuv420,
                ChromaSamplePosition::default(),
                false,
            ),
            "444p12" => (
                12,
                PixelFormat::Yuv444,
                ChromaSamplePosition::default(),
                false,
            ),
            "422p12" => (
                12,
                PixelFormat::Yuv422,
                ChromaSamplePosition::default(),
                false,
            ),
            "420p12" => (
                12,
                PixelFormat::Yuv420,
                ChromaSamplePosition::default(),
                false,
            ),
            "444" => (
                8,
                PixelFormat::Yuv444,
                ChromaSamplePosition::default(),
                false,
            ),
            "422" => (
                8,
                PixelFormat::Yuv422,
                ChromaSamplePosition::default(),
                false,
            ),
            "420" => (
                8,
                PixelFormat::Yuv420,
                ChromaSamplePosition::default(),
                false,
            ),
            "444alpha" => (
                8,
                PixelFormat::Yuv444,
                ChromaSamplePosition::default(),
                true,
            ),
            "mono" => (
                8,
                PixelFormat::Yuv400,
                ChromaSamplePosition::default(),
                false,
            ),
            "mono10" => (
                10,
                PixelFormat::Yuv400,
                ChromaSamplePosition::default(),
                false,
            ),
            "mono12" => (
                12,
                PixelFormat::Yuv400,
                ChromaSamplePosition::default(),
                false,
            ),
            _ => return AvifError::unknown_error("invalid colorspace string"),
        };
        Ok(())
    }

    pub fn create(filename: &str) -> AvifResult<Y4MReader> {
        let mut reader =
            BufReader::new(File::open(filename).map_err(AvifError::map_unknown_error)?);
        let mut y4m_line = String::new();
        let bytes_read = reader
            .read_line(&mut y4m_line)
            .map_err(AvifError::map_unknown_error)?;
        if bytes_read == 0 {
            return AvifError::unknown_error("no bytes in y4m line");
        }
        y4m_line.pop();
        let parts: Vec<&str> = y4m_line.split(" ").collect();
        if parts[0] != "YUV4MPEG2" {
            return AvifError::unknown_error("Not a Y4M file");
        }
        let mut y4m = Y4MReader {
            range: YuvRange::Limited,
            ..Default::default()
        };
        for part in parts[1..].iter() {
            match part.get(0..1).unwrap_or("") {
                "W" => y4m.width = part[1..].parse::<u32>().unwrap_or(0),
                "H" => y4m.height = part[1..].parse::<u32>().unwrap_or(0),
                "C" => y4m.parse_colorspace(&part[1..])?,
                "F" => {
                    // TODO: Handle frame rate.
                }
                "X" => {
                    if part[1..] == *"COLORRANGE=FULL" {
                        y4m.range = YuvRange::Full;
                    }
                }
                _ => {}
            }
        }
        if y4m.width == 0 || y4m.height == 0 || y4m.depth == 0 {
            return AvifError::invalid_argument();
        }
        y4m.reader = Some(reader);
        Ok(y4m)
    }
}

impl Reader for Y4MReader {
    fn read_frame(&mut self, _config: &Config) -> AvifResult<(Image, u64)> {
        const FRAME_MARKER: &str = "FRAME";
        let mut frame_marker = String::new();
        let bytes_read = self
            .reader
            .as_mut()
            .unwrap()
            .read_line(&mut frame_marker)
            .map_err(AvifError::map_unknown_error)?;
        if bytes_read == 0 {
            return AvifError::unknown_error("could not read frame marker");
        }
        frame_marker.pop();
        if frame_marker != FRAME_MARKER {
            return AvifError::unknown_error("could not find frame marker");
        }
        let mut image = image::Image {
            width: self.width,
            height: self.height,
            depth: self.depth,
            yuv_format: self.format,
            yuv_range: self.range,
            chroma_sample_position: self.chroma_sample_position,
            ..Default::default()
        };
        image.allocate_planes(Category::Color)?;
        if self.has_alpha {
            image.allocate_planes(Category::Alpha)?;
        }
        let reader = self.reader.as_mut().unwrap();
        for plane in ALL_PLANES {
            if !image.has_plane(plane) {
                continue;
            }
            let plane_data = image.plane_data(plane).unwrap();
            for y in 0..plane_data.height {
                if self.depth == 8 {
                    let row = image.row_exact_mut(plane, y)?;
                    reader
                        .read_exact(row)
                        .map_err(AvifError::map_unknown_error)?;
                } else {
                    let row = image.row16_exact_mut(plane, y)?;
                    let mut pixel_bytes: [u8; 2] = [0, 0];
                    for pixel in row {
                        reader
                            .read_exact(&mut pixel_bytes)
                            .map_err(AvifError::map_unknown_error)?;
                        // y4m is always little endian.
                        *pixel = u16::from_le_bytes(pixel_bytes);
                    }
                }
            }
        }
        Ok((image, 0))
    }

    fn has_more_frames(&mut self) -> bool {
        let buffer = match self.reader.as_mut().unwrap().fill_buf() {
            Ok(buffer) => buffer,
            Err(_) => return false,
        };
        !buffer.is_empty()
    }
}
