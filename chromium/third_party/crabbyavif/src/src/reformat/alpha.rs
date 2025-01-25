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
use super::libyuv;

use super::rgb;

use crate::decoder::Category;
use crate::image::Plane;
use crate::internal_utils::*;
use crate::reformat::rgb::Format;
use crate::*;

fn premultiply_u8(pixel: u8, alpha: u8) -> u8 {
    ((pixel as f32) * (alpha as f32) / 255.0).floor() as u8
}

fn premultiply_u16(pixel: u16, alpha: u16, max_channel_f: f32) -> u16 {
    ((pixel as f32) * (alpha as f32) / max_channel_f).floor() as u16
}

fn unpremultiply_u8(pixel: u8, alpha: u8) -> u8 {
    ((pixel as f32) * 255.0 / (alpha as f32)).floor().min(255.0) as u8
}

fn unpremultiply_u16(pixel: u16, alpha: u16, max_channel_f: f32) -> u16 {
    ((pixel as f32) * max_channel_f / (alpha as f32))
        .floor()
        .min(max_channel_f) as u16
}

impl rgb::Image {
    pub fn premultiply_alpha(&mut self) -> AvifResult<()> {
        if self.pixels().is_null() || self.row_bytes == 0 {
            return Err(AvifError::ReformatFailed);
        }
        if !self.has_alpha() {
            return Err(AvifError::InvalidArgument);
        }

        #[cfg(feature = "libyuv")]
        match libyuv::process_alpha(self, true) {
            Ok(_) => return Ok(()),
            Err(err) => {
                if err != AvifError::NotImplemented {
                    return Err(err);
                }
            }
        }

        let (alpha_offset, rgb_offsets) = match self.format {
            Format::Rgba | Format::Bgra => (3, [0, 1, 2]),
            _ => (0, [1, 2, 3]),
        };

        if self.depth > 8 {
            let max_channel = self.max_channel();
            let max_channel_f = self.max_channel_f();
            for j in 0..self.height {
                let width = self.width;
                let row = self.row16_mut(j)?;
                for i in 0..width as usize {
                    let offset = i * 4;
                    let alpha = row[offset + alpha_offset];
                    if alpha >= max_channel {
                        continue;
                    }
                    if alpha == 0 {
                        for rgb_offset in rgb_offsets {
                            row[offset + rgb_offset] = 0;
                        }
                        continue;
                    }
                    for rgb_offset in rgb_offsets {
                        row[offset + rgb_offset] =
                            premultiply_u16(row[offset + rgb_offset], alpha, max_channel_f);
                    }
                }
            }
        } else {
            for j in 0..self.height {
                let width = self.width;
                let row = self.row_mut(j)?;
                for i in 0..width as usize {
                    let offset = i * 4;
                    let alpha = row[offset + alpha_offset];
                    match alpha {
                        0 => {
                            for rgb_offset in rgb_offsets {
                                row[offset + rgb_offset] = 0;
                            }
                        }
                        255 => {}
                        _ => {
                            for rgb_offset in rgb_offsets {
                                row[offset + rgb_offset] =
                                    premultiply_u8(row[offset + rgb_offset], alpha);
                            }
                        }
                    }
                }
            }
        }
        Ok(())
    }

    pub fn unpremultiply_alpha(&mut self) -> AvifResult<()> {
        if self.pixels().is_null() || self.row_bytes == 0 {
            return Err(AvifError::ReformatFailed);
        }
        if !self.has_alpha() {
            return Err(AvifError::InvalidArgument);
        }

        #[cfg(feature = "libyuv")]
        match libyuv::process_alpha(self, false) {
            Ok(_) => return Ok(()),
            Err(err) => {
                if err != AvifError::NotImplemented {
                    return Err(err);
                }
            }
        }

        let (alpha_offset, rgb_offsets) = match self.format {
            Format::Rgba | Format::Bgra => (3, [0, 1, 2]),
            _ => (0, [1, 2, 3]),
        };

        if self.depth > 8 {
            let max_channel = self.max_channel();
            let max_channel_f = self.max_channel_f();
            for j in 0..self.height {
                let width = self.width;
                let row = self.row16_mut(j)?;
                for i in 0..width as usize {
                    let offset = i * 4;
                    let alpha = row[offset + alpha_offset];
                    if alpha >= max_channel {
                        continue;
                    }
                    if alpha == 0 {
                        for rgb_offset in rgb_offsets {
                            row[offset + rgb_offset] = 0;
                        }
                        continue;
                    }
                    for rgb_offset in rgb_offsets {
                        row[offset + rgb_offset] =
                            unpremultiply_u16(row[offset + rgb_offset], alpha, max_channel_f);
                    }
                }
            }
        } else {
            for j in 0..self.height {
                let width = self.width;
                let row = self.row_mut(j)?;
                for i in 0..width as usize {
                    let offset = i * 4;
                    let alpha = row[offset + alpha_offset];
                    match alpha {
                        0 => {
                            for rgb_offset in rgb_offsets {
                                row[offset + rgb_offset] = 0;
                            }
                        }
                        255 => {}
                        _ => {
                            for rgb_offset in rgb_offsets {
                                row[offset + rgb_offset] =
                                    unpremultiply_u8(row[offset + rgb_offset], alpha);
                            }
                        }
                    }
                }
            }
        }
        Ok(())
    }

    pub fn set_opaque(&mut self) -> AvifResult<()> {
        if !self.has_alpha() {
            return Ok(());
        }
        if self.format == rgb::Format::Rgb565 {
            return Err(AvifError::NotImplemented);
        }
        let alpha_offset = self.format.alpha_offset();
        let width = usize_from_u32(self.width)?;
        if self.depth > 8 {
            let max_channel = self.max_channel();
            for y in 0..self.height {
                let row = self.row16_mut(y)?;
                for x in 0..width {
                    row[(x * 4) + alpha_offset] = max_channel;
                }
            }
        } else {
            for y in 0..self.height {
                let row = self.row_mut(y)?;
                for x in 0..width {
                    row[(x * 4) + alpha_offset] = 255;
                }
            }
        }
        Ok(())
    }

    fn rescale_alpha_value(value: u16, src_max_channel_f: f32, dst_max_channel: u16) -> u16 {
        let alpha_f = (value as f32) / src_max_channel_f;
        let dst_max_channel_f = dst_max_channel as f32;
        let alpha = (0.5 + (alpha_f * dst_max_channel_f)) as u16;
        clamp_u16(alpha, 0, dst_max_channel)
    }

    pub fn import_alpha_from(&mut self, image: &image::Image) -> AvifResult<()> {
        if !self.has_alpha()
            || !image.has_alpha()
            || self.width != image.width
            || self.height != image.height
        {
            return Err(AvifError::InvalidArgument);
        }
        let width = usize_from_u32(self.width)?;
        let dst_alpha_offset = self.format.alpha_offset();
        if self.depth == image.depth {
            if self.depth > 8 {
                for y in 0..self.height {
                    let dst_row = self.row16_mut(y)?;
                    let src_row = image.row16(Plane::A, y)?;
                    for x in 0..width {
                        dst_row[(x * 4) + dst_alpha_offset] = src_row[x];
                    }
                }
                return Ok(());
            }
            for y in 0..self.height {
                let dst_row = self.row_mut(y)?;
                let src_row = image.row(Plane::A, y)?;
                for x in 0..width {
                    dst_row[(x * 4) + dst_alpha_offset] = src_row[x];
                }
            }
            return Ok(());
        }
        let max_channel = self.max_channel();
        if image.depth > 8 {
            if self.depth > 8 {
                // u16 to u16 depth rescaling.
                for y in 0..self.height {
                    let dst_row = self.row16_mut(y)?;
                    let src_row = image.row16(Plane::A, y)?;
                    for x in 0..width {
                        dst_row[(x * 4) + dst_alpha_offset] = Self::rescale_alpha_value(
                            src_row[x],
                            image.max_channel_f(),
                            max_channel,
                        );
                    }
                }
                return Ok(());
            }
            // u16 to u8 depth rescaling.
            for y in 0..self.height {
                let dst_row = self.row_mut(y)?;
                let src_row = image.row16(Plane::A, y)?;
                for x in 0..width {
                    dst_row[(x * 4) + dst_alpha_offset] =
                        Self::rescale_alpha_value(src_row[x], image.max_channel_f(), max_channel)
                            as u8;
                }
            }
            return Ok(());
        }
        // u8 to u16 depth rescaling.
        for y in 0..self.height {
            let dst_row = self.row16_mut(y)?;
            let src_row = image.row(Plane::A, y)?;
            for x in 0..width {
                dst_row[(x * 4) + dst_alpha_offset] = Self::rescale_alpha_value(
                    src_row[x] as u16,
                    image.max_channel_f(),
                    max_channel,
                );
            }
        }
        Ok(())
    }
}

impl image::Image {
    pub fn alpha_to_full_range(&mut self) -> AvifResult<()> {
        if self.planes[3].is_none() {
            return Ok(());
        }
        let width = self.width as usize;
        let depth = self.depth;
        if self.planes[3].unwrap_ref().is_pointer() {
            let src = image::Image {
                width: self.width,
                height: self.height,
                depth: self.depth,
                yuv_format: self.yuv_format,
                planes: [
                    None,
                    None,
                    None,
                    self.planes[3].unwrap_ref().clone_pointer(),
                ],
                row_bytes: [0, 0, 0, self.row_bytes[3]],
                ..image::Image::default()
            };
            self.allocate_planes(Category::Alpha)?;
            if depth > 8 {
                for y in 0..self.height {
                    let src_row = src.row16(Plane::A, y)?;
                    let dst_row = self.row16_mut(Plane::A, y)?;
                    for x in 0..width {
                        dst_row[x] = limited_to_full_y(depth, src_row[x]);
                    }
                }
            } else {
                for y in 0..self.height {
                    let src_row = src.row(Plane::A, y)?;
                    let dst_row = self.row_mut(Plane::A, y)?;
                    for x in 0..width {
                        dst_row[x] = limited_to_full_y(8, src_row[x] as u16) as u8;
                    }
                }
            }
        } else if depth > 8 {
            for y in 0..self.height {
                let row = self.row16_mut(Plane::A, y)?;
                for pixel in row.iter_mut().take(width) {
                    *pixel = limited_to_full_y(depth, *pixel);
                }
            }
        } else {
            for y in 0..self.height {
                let row = self.row_mut(Plane::A, y)?;
                for pixel in row.iter_mut().take(width) {
                    *pixel = limited_to_full_y(8, *pixel as u16) as u8;
                }
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::internal_utils::pixels::*;

    use rand::Rng;
    use test_case::test_matrix;

    const ALPHA_RGB_FORMATS: [rgb::Format; 4] = [
        rgb::Format::Rgba,
        rgb::Format::Argb,
        rgb::Format::Bgra,
        rgb::Format::Abgr,
    ];

    fn rgb_image(
        width: u32,
        height: u32,
        depth: u8,
        format: rgb::Format,
        use_pointer: bool,
        buffer: &mut Vec<u8>,
    ) -> AvifResult<rgb::Image> {
        let mut rgb = rgb::Image {
            width,
            height,
            depth,
            format,
            ..rgb::Image::default()
        };
        if use_pointer {
            let pixel_size = if depth == 8 { 1 } else { 2 };
            let buffer_size = (width * height * 4 * pixel_size) as usize;
            buffer.reserve_exact(buffer_size);
            buffer.resize(buffer_size, 0);
            rgb.row_bytes = width * 4 * pixel_size;
            // Use a pointer to mimic C API calls.
            rgb.pixels = Some(Pixels::from_raw_pointer(
                buffer.as_mut_ptr(),
                rgb.depth as u32,
                height,
                rgb.row_bytes,
            )?);
        } else {
            rgb.allocate()?;
        }
        Ok(rgb)
    }

    #[test_matrix(20, 10, [8, 10, 12, 16], 0..4, [true, false])]
    fn fill_alpha(
        width: u32,
        height: u32,
        depth: u8,
        format_index: usize,
        use_pointer: bool,
    ) -> AvifResult<()> {
        let format = ALPHA_RGB_FORMATS[format_index];
        let mut buffer: Vec<u8> = vec![];
        let mut rgb = rgb_image(width, height, depth, format, use_pointer, &mut buffer)?;

        rgb.set_opaque()?;

        let alpha_offset = rgb.format.alpha_offset();
        if depth == 8 {
            for y in 0..height {
                let row = rgb.row(y)?;
                assert_eq!(row.len(), (width * 4) as usize);
                for x in 0..width as usize {
                    for idx in 0..4usize {
                        let expected_value = if idx == alpha_offset { 255 } else { 0 };
                        assert_eq!(row[x * 4 + idx], expected_value);
                    }
                }
            }
        } else {
            let max_channel = ((1 << depth) - 1) as u16;
            for y in 0..height {
                let row = rgb.row16(y)?;
                assert_eq!(row.len(), (width * 4) as usize);
                for x in 0..width as usize {
                    for idx in 0..4usize {
                        let expected_value = if idx == alpha_offset { max_channel } else { 0 };
                        assert_eq!(row[x * 4 + idx], expected_value);
                    }
                }
            }
        }
        Ok(())
    }

    #[test]
    fn rescale_alpha_value() {
        // 8bit to 10bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 255.0, 1023), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(100, 255.0, 1023), 401);
        assert_eq!(rgb::Image::rescale_alpha_value(128, 255.0, 1023), 514);
        assert_eq!(rgb::Image::rescale_alpha_value(255, 255.0, 1023), 1023);
        // 10bit to 8bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 1023.0, 255), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(401, 1023.0, 255), 100);
        assert_eq!(rgb::Image::rescale_alpha_value(514, 1023.0, 255), 128);
        assert_eq!(rgb::Image::rescale_alpha_value(1023, 1023.0, 255), 255);
        // 8bit to 12bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 255.0, 4095), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(100, 255.0, 4095), 1606);
        assert_eq!(rgb::Image::rescale_alpha_value(128, 255.0, 4095), 2056);
        assert_eq!(rgb::Image::rescale_alpha_value(255, 255.0, 4095), 4095);
        // 12bit to 8bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 4095.0, 255), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(1606, 4095.0, 255), 100);
        assert_eq!(rgb::Image::rescale_alpha_value(2056, 4095.0, 255), 128);
        assert_eq!(rgb::Image::rescale_alpha_value(4095, 4095.0, 255), 255);
        // 10bit to 12bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 1023.0, 4095), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(401, 1023.0, 4095), 1605);
        assert_eq!(rgb::Image::rescale_alpha_value(514, 1023.0, 4095), 2058);
        assert_eq!(rgb::Image::rescale_alpha_value(1023, 1023.0, 4095), 4095);
        // 12bit to 10bit.
        assert_eq!(rgb::Image::rescale_alpha_value(0, 4095.0, 1023), 0);
        assert_eq!(rgb::Image::rescale_alpha_value(1606, 4095.0, 1023), 401);
        assert_eq!(rgb::Image::rescale_alpha_value(2056, 4095.0, 1023), 514);
        assert_eq!(rgb::Image::rescale_alpha_value(4095, 4095.0, 1023), 1023);
    }

    #[test_matrix(20, 10, [8, 10, 12, 16], 0..4, [8, 10, 12], [true, false])]
    fn reformat_alpha(
        width: u32,
        height: u32,
        rgb_depth: u8,
        format_index: usize,
        yuv_depth: u8,
        use_pointer: bool,
    ) -> AvifResult<()> {
        // Note: This test simply makes sure reformat_alpha puts the alpha pixels in the right
        // place in the rgb image (with scaling). It does not check for the actual validity of the
        // scaled pixels.
        let format = ALPHA_RGB_FORMATS[format_index];
        let mut buffer: Vec<u8> = vec![];
        let mut rgb = rgb_image(width, height, rgb_depth, format, use_pointer, &mut buffer)?;

        let mut image = image::Image::default();
        image.width = width;
        image.height = height;
        image.depth = yuv_depth;
        image.allocate_planes(Category::Alpha)?;

        let mut rng = rand::thread_rng();
        let mut expected_values: Vec<u16> = Vec::new();
        let image_max_channel_f = image.max_channel_f();
        if yuv_depth == 8 {
            for y in 0..height {
                let row = image.row_mut(Plane::A, y)?;
                for x in 0..width as usize {
                    let value = rng.gen_range(0..256) as u8;
                    if rgb.depth == 8 {
                        expected_values.push(value as u16);
                    } else {
                        expected_values.push(rgb::Image::rescale_alpha_value(
                            value as u16,
                            image_max_channel_f,
                            rgb.max_channel(),
                        ));
                    }
                    row[x] = value;
                }
            }
        } else {
            for y in 0..height {
                let row = image.row16_mut(Plane::A, y)?;
                for x in 0..width as usize {
                    let value = rng.gen_range(0..(1i32 << yuv_depth)) as u16;
                    if rgb.depth == yuv_depth {
                        expected_values.push(value);
                    } else {
                        expected_values.push(rgb::Image::rescale_alpha_value(
                            value as u16,
                            image_max_channel_f,
                            rgb.max_channel(),
                        ));
                    }
                    row[x] = value;
                }
            }
        }

        rgb.import_alpha_from(&image)?;

        let alpha_offset = rgb.format.alpha_offset();
        let mut expected_values = expected_values.into_iter();
        if rgb_depth == 8 {
            for y in 0..height {
                let rgb_row = rgb.row(y)?;
                assert_eq!(rgb_row.len(), (width * 4) as usize);
                for x in 0..width as usize {
                    for idx in 0..4usize {
                        let expected_value =
                            if idx == alpha_offset { expected_values.next().unwrap() } else { 0 };
                        assert_eq!(rgb_row[x * 4 + idx], expected_value as u8);
                    }
                }
            }
        } else {
            for y in 0..height {
                let rgb_row = rgb.row16(y)?;
                assert_eq!(rgb_row.len(), (width * 4) as usize);
                for x in 0..width as usize {
                    for idx in 0..4usize {
                        let expected_value =
                            if idx == alpha_offset { expected_values.next().unwrap() } else { 0 };
                        assert_eq!(rgb_row[x * 4 + idx], expected_value);
                    }
                }
            }
        }
        Ok(())
    }
}
