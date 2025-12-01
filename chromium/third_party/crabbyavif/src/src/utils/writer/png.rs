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
use crate::PixelFormat;

use std::fs::File;

use super::Writer;

#[derive(Default)]
pub struct PngWriter {
    pub depth: Option<u8>,
}

fn scale_to_8bit(pixel: u16, max_channel: u16) -> u8 {
    (pixel as u32 * 255 / max_channel as u32) as u8
}

fn scale_to_16bit(pixel: u16, max_channel: u16) -> u16 {
    ((pixel as u32 * 65535) / max_channel as u32) as u16
}

impl Writer for PngWriter {
    fn write_frame(&mut self, file: &mut File, image: &Image) -> AvifResult<()> {
        let is_monochrome = image.yuv_format == PixelFormat::Yuv400;
        let png_color_type = match (is_monochrome, image.alpha_present) {
            (true, _) => png::ColorType::Grayscale,
            (_, false) => png::ColorType::Rgb,
            (_, true) => png::ColorType::Rgba,
        };
        let depth = self.depth.unwrap_or(if image.depth == 8 { 8 } else { 16 });
        let mut rgb = rgb::Image::create_from_yuv(image);
        if !is_monochrome {
            rgb.depth = depth;
            rgb.format = if image.alpha_present { rgb::Format::Rgba } else { rgb::Format::Rgb };
            rgb.allocate()?;
            rgb.convert_from_yuv(image)?;
        }

        let mut encoder = png::Encoder::new(file, image.width, image.height);
        encoder.set_color(png_color_type);
        encoder.set_depth(if depth == 8 { png::BitDepth::Eight } else { png::BitDepth::Sixteen });
        if !image.xmp.is_empty() {
            if let Ok(text) = String::from_utf8(image.xmp.clone()) {
                if encoder
                    .add_itxt_chunk("XML:com.adobe.xmp".to_string(), text)
                    .is_err()
                {
                    eprintln!("Warning: Ignoring XMP data");
                }
            } else {
                eprintln!("Warning: Ignoring XMP data because it is not a valid UTF-8 string");
            }
        }
        let mut writer = encoder.write_header().or(Err(AvifError::UnknownError(
            "Could not write the PNG header".into(),
        )))?;
        let mut rgba_pixel_buffer: Vec<u8> = Vec::new();
        let rgba_slice = if is_monochrome {
            for y in 0..image.height {
                match (image.depth == 8, depth == 8) {
                    (true, true) => {
                        let y_row = image.row(Plane::Y, y)?;
                        rgba_pixel_buffer.extend_from_slice(&y_row[..image.width as usize]);
                    }
                    (false, false) => {
                        let y_row = image.row16(Plane::Y, y)?;
                        for pixel in &y_row[..image.width as usize] {
                            let pixel16 = scale_to_16bit(*pixel, image.max_channel());
                            rgba_pixel_buffer.extend_from_slice(&pixel16.to_be_bytes());
                        }
                    }
                    (true, false) => {
                        let y_row = image.row(Plane::Y, y)?;
                        for pixel in &y_row[..image.width as usize] {
                            let pixel16 = scale_to_16bit(*pixel as u16, image.max_channel());
                            rgba_pixel_buffer.extend_from_slice(&pixel16.to_be_bytes());
                        }
                    }
                    (false, true) => {
                        let y_row = image.row16(Plane::Y, y)?;
                        for pixel in &y_row[..image.width as usize] {
                            rgba_pixel_buffer.push(scale_to_8bit(*pixel, image.max_channel()));
                        }
                    }
                }
            }
            &rgba_pixel_buffer[..]
        } else if depth == 8 {
            let rgba_pixels = rgb.pixels.as_ref().unwrap();
            rgba_pixels.slice(0, rgba_pixels.size() as u32)?
        } else {
            let rgba_pixels = rgb.pixels.as_ref().unwrap();
            let rgba_slice16 = rgba_pixels.slice16(0, rgba_pixels.size() as u32).unwrap();
            for pixel in rgba_slice16 {
                rgba_pixel_buffer.extend_from_slice(&pixel.to_be_bytes());
            }
            &rgba_pixel_buffer[..]
        };
        writer
            .write_image_data(rgba_slice)
            .or(Err(AvifError::UnknownError(
                "Could not write PNG image data".into(),
            )))?;
        writer.finish().or(Err(AvifError::UnknownError(
            "Could not finalize the PNG encoder".into(),
        )))?;
        Ok(())
    }
}
