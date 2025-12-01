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

#[allow(unused_imports)]
use crate::image::*;
use crate::*;

use std::fs::File;
use std::io::prelude::*;

use super::Writer;

#[derive(Default)]
pub(crate) struct Y4MWriter {
    header_written: bool,
    write_alpha: bool,
    skip_headers: bool,
}

impl Y4MWriter {
    #[allow(unused)]
    pub(crate) fn create(skip_headers: bool) -> Self {
        Self {
            skip_headers,
            ..Default::default()
        }
    }

    fn write_header(&mut self, file: &mut File, image: &Image) -> AvifResult<()> {
        if self.header_written {
            return Ok(());
        }
        self.write_alpha = false;

        if image.alpha_present && (image.depth != 8 || image.yuv_format != PixelFormat::Yuv444) {
            println!("WARNING: writing alpha is currently only supported in 8bpc YUV444, ignoring alpha channel");
        }

        let y4m_format = match image.depth {
            8 => match image.yuv_format {
                PixelFormat::None
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => "",
                PixelFormat::Yuv444 => {
                    if image.alpha_present {
                        self.write_alpha = true;
                        "C444alpha XYSCSS=444"
                    } else {
                        "C444 XYSCSS=444"
                    }
                }
                PixelFormat::Yuv422 => "C422 XYSCSS=422",
                PixelFormat::Yuv420 => "C420jpeg XYSCSS=420JPEG",
                PixelFormat::Yuv400 => "Cmono XYSCSS=400",
            },
            10 => match image.yuv_format {
                PixelFormat::None
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => "",
                PixelFormat::Yuv444 => "C444p10 XYSCSS=444P10",
                PixelFormat::Yuv422 => "C422p10 XYSCSS=422P10",
                PixelFormat::Yuv420 => "C420p10 XYSCSS=420P10",
                PixelFormat::Yuv400 => "Cmono10 XYSCSS=400",
            },
            12 => match image.yuv_format {
                PixelFormat::None
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => "",
                PixelFormat::Yuv444 => "C444p12 XYSCSS=444P12",
                PixelFormat::Yuv422 => "C422p12 XYSCSS=422P12",
                PixelFormat::Yuv420 => "C420p12 XYSCSS=420P12",
                PixelFormat::Yuv400 => "Cmono12 XYSCSS=400",
            },
            _ => {
                return Err(AvifError::NotImplemented);
            }
        };
        let y4m_color_range = if image.yuv_range == YuvRange::Limited {
            "XCOLORRANGE=LIMITED"
        } else {
            "XCOLORRANGE=FULL"
        };
        let header = format!(
            "YUV4MPEG2 W{} H{} F25:1 Ip A0:0 {y4m_format} {y4m_color_range}\n",
            image.width, image.height
        );
        file.write_all(header.as_bytes())
            .or(Err(AvifError::IoError))?;
        self.header_written = true;
        Ok(())
    }
}

impl Writer for Y4MWriter {
    fn write_frame(&mut self, file: &mut File, image: &Image) -> AvifResult<()> {
        if !self.skip_headers {
            self.write_header(file, image)?;
            let frame_marker = "FRAME\n";
            file.write_all(frame_marker.as_bytes())
                .or(Err(AvifError::IoError))?;
        }
        let planes: &[Plane] = if self.write_alpha { &ALL_PLANES } else { &YUV_PLANES };
        for plane in planes {
            let plane = *plane;
            if !image.has_plane(plane) {
                continue;
            }
            if image.depth == 8 {
                for y in 0..image.height(plane) {
                    let row = image.row(plane, y as u32)?;
                    let pixels = &row[..image.width(plane)];
                    file.write_all(pixels).or(Err(AvifError::IoError))?;
                }
            } else {
                for y in 0..image.height(plane) {
                    let row16 = image.row16(plane, y as u32)?;
                    let pixels16 = &row16[..image.width(plane)];
                    let mut pixels: Vec<u8> = Vec::new();
                    // y4m is always little endian.
                    for &pixel16 in pixels16 {
                        pixels.extend_from_slice(&pixel16.to_le_bytes());
                    }
                    file.write_all(&pixels[..]).or(Err(AvifError::IoError))?;
                }
            }
        }
        Ok(())
    }
}
