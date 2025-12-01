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

use super::coeffs::*;
use super::libyuv;
use super::rgb_impl;
use super::sharpyuv;

use crate::image::Plane;
use crate::image::YuvRange;
use crate::internal_utils::*;
use crate::utils::pixels::*;
use crate::*;

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq)]
pub enum Format {
    Rgb,
    #[default]
    Rgba,
    Argb,
    Bgr,
    Bgra,
    Abgr,
    Rgb565,
    Rgba1010102, // https://developer.android.com/reference/android/graphics/Bitmap.Config#RGBA_1010102
    Gray,
    GrayA,
    AGray,
}

impl Format {
    pub(crate) fn offsets(&self) -> [usize; 4] {
        match self {
            Format::Rgb => [0, 1, 2, 0],
            Format::Rgba => [0, 1, 2, 3],
            Format::Argb => [1, 2, 3, 0],
            Format::Bgr => [2, 1, 0, 0],
            Format::Bgra => [2, 1, 0, 3],
            Format::Abgr => [3, 2, 1, 0],
            Format::Rgb565 | Format::Rgba1010102 | Format::Gray => [0; 4],
            Format::GrayA => [0, 0, 0, 1],
            Format::AGray => [1, 0, 0, 0],
        }
    }

    pub fn r_offset(&self) -> usize {
        self.offsets()[0]
    }

    pub fn g_offset(&self) -> usize {
        self.offsets()[1]
    }

    pub fn b_offset(&self) -> usize {
        self.offsets()[2]
    }

    pub fn alpha_offset(&self) -> usize {
        self.offsets()[3]
    }

    pub fn has_alpha(&self) -> bool {
        !matches!(
            self,
            Format::Rgb | Format::Bgr | Format::Rgb565 | Format::Gray
        )
    }

    pub fn channel_count(&self) -> u32 {
        match self {
            Format::Rgba | Format::Bgra | Format::Argb | Format::Abgr => 4,
            Format::Rgb | Format::Bgr => 3,
            Format::Rgb565 | Format::GrayA | Format::AGray => 2,
            Format::Rgba1010102 => 0, // This is never used.
            Format::Gray => 1,
        }
    }

    pub fn pixel_size(&self, depth: u32) -> u32 {
        match self {
            Format::Rgb565 => 2,
            _ => self.channel_count() * if depth > 8 { 2 } else { 1 },
        }
    }

    pub(crate) fn is_gray(&self) -> bool {
        matches!(self, Format::Gray | Format::GrayA | Format::AGray)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub enum ChromaUpsampling {
    #[default]
    Automatic,
    Fastest,
    BestQuality,
    Nearest,
    Bilinear,
}

impl ChromaUpsampling {
    #[cfg(feature = "libyuv")]
    pub(crate) fn nearest_neighbor_filter_allowed(&self) -> bool {
        !matches!(self, Self::Bilinear | Self::BestQuality)
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn bilinear_or_better_filter_allowed(&self) -> bool {
        !matches!(self, Self::Nearest | Self::Fastest)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq)]
pub enum ChromaDownsampling {
    #[default]
    Automatic,
    Fastest,
    BestQuality,
    Average,
    SharpYuv,
}

#[derive(Default)]
pub struct Image {
    pub width: u32,
    pub height: u32,
    pub depth: u8,
    pub format: Format,
    pub chroma_upsampling: ChromaUpsampling,
    pub chroma_downsampling: ChromaDownsampling,
    pub premultiply_alpha: bool,
    pub is_float: bool,
    pub max_threads: i32,
    pub pixels: Option<Pixels>,
    pub row_bytes: u32,
}

#[derive(Debug, Default, PartialEq)]
pub enum AlphaMultiplyMode {
    #[default]
    NoOp,
    Multiply,
    UnMultiply,
}

#[derive(Clone, Copy, PartialEq)]
pub(crate) enum Mode {
    YuvCoefficients(f32, f32, f32),
    Identity,
    Ycgco,
    YcgcoRe,
    YcgcoRo,
}

impl From<&image::Image> for Mode {
    fn from(image: &image::Image) -> Self {
        match image.matrix_coefficients {
            MatrixCoefficients::Identity => Mode::Identity,
            MatrixCoefficients::Ycgco => Mode::Ycgco,
            MatrixCoefficients::YcgcoRe => Mode::YcgcoRe,
            MatrixCoefficients::YcgcoRo => Mode::YcgcoRo,
            _ => {
                let coeffs =
                    calculate_yuv_coefficients(image.color_primaries, image.matrix_coefficients);
                Mode::YuvCoefficients(coeffs[0], coeffs[1], coeffs[2])
            }
        }
    }
}

impl Image {
    pub fn max_channel(&self) -> u16 {
        ((1i32 << self.depth) - 1) as u16
    }

    pub(crate) fn max_channel_f(&self) -> f32 {
        self.max_channel() as f32
    }

    pub fn create_from_yuv(image: &image::Image) -> Self {
        Self {
            width: image.width,
            height: image.height,
            depth: image.depth,
            format: Format::Rgba,
            chroma_upsampling: ChromaUpsampling::Automatic,
            chroma_downsampling: ChromaDownsampling::Automatic,
            premultiply_alpha: false,
            is_float: false,
            max_threads: 1,
            pixels: None,
            row_bytes: 0,
        }
    }

    // This function may not be used in some configurations.
    #[allow(dead_code)]
    pub(crate) fn pixels(&self) -> *const u8 {
        match &self.pixels {
            Some(pixels) => pixels.ptr_generic(),
            None => std::ptr::null(),
        }
    }

    // This function may not be used in some configurations.
    #[allow(dead_code)]
    pub(crate) fn pixels16(&self) -> *const u16 {
        match &self.pixels {
            Some(pixels) => pixels.ptr16(),
            None => std::ptr::null(),
        }
    }

    pub(crate) fn pixels_mut(&mut self) -> *mut u8 {
        match &mut self.pixels {
            Some(pixels) => pixels.ptr_mut_generic(),
            None => std::ptr::null_mut(),
        }
    }

    pub fn row(&self, row: u32) -> AvifResult<&[u8]> {
        self.pixels
            .as_ref()
            .ok_or(AvifError::NoContent)?
            .slice(checked_mul!(row, self.row_bytes)?, self.row_bytes)
    }

    pub fn row_mut(&mut self, row: u32) -> AvifResult<&mut [u8]> {
        self.pixels
            .as_mut()
            .ok_or(AvifError::NoContent)?
            .slice_mut(checked_mul!(row, self.row_bytes)?, self.row_bytes)
    }

    pub fn row16(&self, row: u32) -> AvifResult<&[u16]> {
        self.pixels
            .as_ref()
            .ok_or(AvifError::NoContent)?
            .slice16(checked_mul!(row, self.row_bytes / 2)?, self.row_bytes / 2)
    }

    pub fn row16_mut(&mut self, row: u32) -> AvifResult<&mut [u16]> {
        self.pixels
            .as_mut()
            .ok_or(AvifError::NoContent)?
            .slice16_mut(checked_mul!(row, self.row_bytes / 2)?, self.row_bytes / 2)
    }

    pub fn allocate(&mut self) -> AvifResult<()> {
        let row_bytes = checked_mul!(self.width, self.pixel_size())?;
        if self.channel_size() == 1 {
            let buffer_size: usize = usize_from_u32(checked_mul!(row_bytes, self.height)?)?;
            let buffer: Vec<u8> = vec![0; buffer_size];
            self.pixels = Some(Pixels::Buffer(buffer));
        } else {
            let buffer_size: usize = usize_from_u32(checked_mul!(row_bytes / 2, self.height)?)?;
            let buffer: Vec<u16> = vec![0; buffer_size];
            self.pixels = Some(Pixels::Buffer16(buffer));
        }
        self.row_bytes = row_bytes;
        Ok(())
    }

    pub(crate) fn depth_valid(&self) -> bool {
        match (self.format, self.is_float, self.depth) {
            (Format::Rgb565, false, 8) => true,
            (Format::Rgb565, _, _) => false,
            (_, true, 16) => true, // IEEE 754 half-precision binary16
            (_, false, 8 | 10 | 12 | 16) => true,
            _ => false,
        }
    }

    pub fn has_alpha(&self) -> bool {
        match self.format {
            Format::Rgba
            | Format::Bgra
            | Format::Argb
            | Format::Abgr
            | Format::Rgba1010102
            | Format::GrayA
            | Format::AGray => true,
            Format::Rgb | Format::Bgr | Format::Rgb565 | Format::Gray => false,
        }
    }

    pub(crate) fn channel_size(&self) -> u32 {
        match self.depth {
            8 => 1,
            10 | 12 | 16 => 2,
            _ => panic!(),
        }
    }

    pub fn channel_count(&self) -> u32 {
        self.format.channel_count()
    }

    pub(crate) fn pixel_size(&self) -> u32 {
        match self.format {
            Format::Rgba | Format::Bgra | Format::Argb | Format::Abgr => self.channel_size() * 4,
            Format::Rgb | Format::Bgr => self.channel_size() * 3,
            Format::Rgb565 => 2,
            Format::Rgba1010102 => 4,
            Format::Gray => self.channel_size(),
            Format::GrayA | Format::AGray => self.channel_size() * 2,
        }
    }

    fn convert_to_half_float(&mut self) -> AvifResult<()> {
        let scale = 1.0 / self.max_channel_f();
        if libyuv::convert_to_half_float(self, scale)?.is_some() {
            return Ok(());
        }
        // This constant comes from libyuv. For details, see here:
        // https://chromium.googlesource.com/libyuv/libyuv/+/2f87e9a7/source/row_common.cc#3537
        let reinterpret_f32_as_u32 = |f: f32| u32::from_le_bytes(f.to_le_bytes());
        let multiplier = 1.925_93e-34 * scale;
        for y in 0..self.height {
            let row = self.row16_mut(y)?;
            for pixel in row {
                *pixel = (reinterpret_f32_as_u32((*pixel as f32) * multiplier) >> 13) as u16;
            }
        }
        Ok(())
    }

    pub fn convert_from_yuv(&mut self, image: &image::Image) -> AvifResult<()> {
        if !image.has_plane(Plane::Y) || !image.depth_valid() || !self.depth_valid() {
            return AvifError::reformat_failed();
        }
        if matches!(
            image.matrix_coefficients,
            MatrixCoefficients::Reserved
                | MatrixCoefficients::Bt2020Cl
                | MatrixCoefficients::Smpte2085
                | MatrixCoefficients::ChromaDerivedCl
                | MatrixCoefficients::Ictcp
        ) {
            return AvifError::not_implemented();
        }
        if image.matrix_coefficients == MatrixCoefficients::Ycgco
            && image.yuv_range == YuvRange::Limited
        {
            return AvifError::not_implemented();
        }
        if matches!(
            image.matrix_coefficients,
            MatrixCoefficients::YcgcoRe | MatrixCoefficients::YcgcoRo
        ) {
            if image.yuv_range == YuvRange::Limited {
                return AvifError::not_implemented();
            }
            let bit_offset =
                if image.matrix_coefficients == MatrixCoefficients::YcgcoRe { 2 } else { 1 };
            if image.depth - bit_offset != self.depth {
                return AvifError::not_implemented();
            }
        }
        // Android MediaCodec maps all underlying YUV formats to PixelFormat::Yuv420. So do not
        // perform this validation for Android MediaCodec. The libyuv wrapper will simply use Bt601
        // coefficients for this color conversion.
        #[cfg(not(feature = "android_mediacodec"))]
        if image.matrix_coefficients == MatrixCoefficients::Identity
            && !matches!(image.yuv_format, PixelFormat::Yuv444 | PixelFormat::Yuv400)
        {
            return AvifError::not_implemented();
        }

        let mut alpha_multiply_mode = if image.has_alpha() {
            if !self.has_alpha() && !image.alpha_premultiplied {
                // If we are converting an image with alpha into a format without alpha, we should
                // premultiply the alpha value before discarding the alpha plane. This has the same
                // effect of rendering this image on a black background.
                AlphaMultiplyMode::Multiply
            } else if !image.alpha_premultiplied && self.premultiply_alpha {
                AlphaMultiplyMode::Multiply
            } else if image.alpha_premultiplied && !self.premultiply_alpha {
                AlphaMultiplyMode::UnMultiply
            } else {
                AlphaMultiplyMode::NoOp
            }
        } else {
            AlphaMultiplyMode::NoOp
        };

        let mut converted_with_libyuv: bool = false;
        let mut alpha_reformatted_with_libyuv = false;
        if alpha_multiply_mode == AlphaMultiplyMode::NoOp || self.has_alpha() {
            if let Some(alpha_reformatted) = libyuv::yuv_to_rgb(image, self)? {
                alpha_reformatted_with_libyuv = alpha_reformatted;
                converted_with_libyuv = true;
            }
        }
        if image.yuv_format == PixelFormat::AndroidNv21 || self.format == Format::Rgba1010102 {
            // These conversions are only supported via libyuv.
            if converted_with_libyuv {
                if image.has_alpha() && matches!(self.format, Format::Rgba1010102) {
                    // If the source image has an alpha channel, scale them to 2 bits and fill it
                    // into the rgb image. Otherwise, libyuv writes them as opaque by default.
                    self.import_alpha_from(image)?;
                }
                return Ok(());
            } else {
                return AvifError::not_implemented();
            }
        }
        if self.has_alpha() && !alpha_reformatted_with_libyuv {
            if image.has_alpha() {
                self.import_alpha_from(image)?;
            } else {
                self.set_opaque()?;
            }
        }
        if !converted_with_libyuv {
            let mut converted_by_fast_path = false;
            if !self.format.is_gray()
                && (matches!(
                    self.chroma_upsampling,
                    ChromaUpsampling::Nearest | ChromaUpsampling::Fastest
                ) || matches!(image.yuv_format, PixelFormat::Yuv444 | PixelFormat::Yuv400))
                && (alpha_multiply_mode == AlphaMultiplyMode::NoOp || self.format.has_alpha())
            {
                converted_by_fast_path = rgb_impl::yuv_to_rgb_fast(image, self)?.is_some();
            }
            if !converted_by_fast_path {
                rgb_impl::yuv_to_rgb_any(image, self, alpha_multiply_mode)?;
                alpha_multiply_mode = AlphaMultiplyMode::NoOp;
            }
        }
        match alpha_multiply_mode {
            AlphaMultiplyMode::Multiply => self.premultiply_alpha()?,
            AlphaMultiplyMode::UnMultiply => self.unpremultiply_alpha()?,
            AlphaMultiplyMode::NoOp => {}
        }
        if self.is_float {
            self.convert_to_half_float()?;
        }
        Ok(())
    }

    pub fn convert_to_yuv(&self, image: &mut image::Image) -> AvifResult<()> {
        if self.format == Format::Rgb565 || self.is_float {
            return AvifError::not_implemented();
        }
        image.allocate_planes(Category::Color)?;
        // TODO: b/410088660 - add a setting to ignore alpha channel.
        let has_alpha = self.has_alpha();
        if has_alpha {
            image.allocate_planes(Category::Alpha)?;
        }
        let alpha_multiply_mode =
            match (has_alpha, self.premultiply_alpha, image.alpha_premultiplied) {
                (true, false, true) => AlphaMultiplyMode::Multiply,
                (true, true, false) => AlphaMultiplyMode::UnMultiply,
                _ => AlphaMultiplyMode::NoOp,
            };
        if self.format.is_gray() {
            rgb_impl::rgb_gray_to_yuv(self, image)?;
        } else {
            let mut conversion_complete = false;
            if self.chroma_downsampling == ChromaDownsampling::SharpYuv {
                sharpyuv::rgb_to_yuv(self, image)?;
                conversion_complete = true;
            } else if alpha_multiply_mode == AlphaMultiplyMode::NoOp {
                conversion_complete = libyuv::rgb_to_yuv(self, image)?.is_some();
            }
            if !conversion_complete {
                rgb_impl::rgb_to_yuv(self, image)?;
            }
        }
        if image.has_plane(Plane::A) {
            if has_alpha {
                image.import_alpha_from(self)?;
            } else {
                image.set_opaque()?;
            }
        }
        Ok(())
    }

    pub fn shuffle_channels_to(self, format: Format) -> AvifResult<Image> {
        if self.format == format {
            return Ok(self);
        }
        if self.format == Format::Rgb565 || format == Format::Rgb565 {
            return AvifError::not_implemented();
        }

        let mut dst = Image {
            format,
            pixels: None,
            row_bytes: 0,
            ..self
        };
        dst.allocate()?;

        let src_channel_count = self.channel_count();
        let dst_channel_count = dst.channel_count();
        let src_offsets = self.format.offsets();
        let dst_offsets = dst.format.offsets();
        let src_has_alpha = self.has_alpha();
        let dst_has_alpha = dst.has_alpha();
        let dst_max_channel = dst.max_channel();
        for y in 0..self.height {
            if self.depth == 8 {
                let src_row = self.row(y)?;
                let dst_row = &mut dst.row_mut(y)?;
                for x in 0..self.width {
                    let src_pixel_i = (src_channel_count * x) as usize;
                    let dst_pixel_i = (dst_channel_count * x) as usize;
                    for c in 0..3 {
                        dst_row[dst_pixel_i + dst_offsets[c]] =
                            src_row[src_pixel_i + src_offsets[c]];
                    }
                    if dst_has_alpha {
                        dst_row[dst_pixel_i + dst_offsets[3]] = if src_has_alpha {
                            src_row[src_pixel_i + src_offsets[3]]
                        } else {
                            dst_max_channel as u8
                        };
                    }
                }
            } else {
                let src_row = self.row16(y)?;
                let dst_row = &mut dst.row16_mut(y)?;
                for x in 0..self.width {
                    let src_pixel_i = (src_channel_count * x) as usize;
                    let dst_pixel_i = (dst_channel_count * x) as usize;
                    for c in 0..3 {
                        dst_row[dst_pixel_i + dst_offsets[c]] =
                            src_row[src_pixel_i + src_offsets[c]];
                    }
                    if dst_has_alpha {
                        dst_row[dst_pixel_i + dst_offsets[3]] = if src_has_alpha {
                            src_row[src_pixel_i + src_offsets[3]]
                        } else {
                            dst_max_channel
                        };
                    }
                }
            }
        }
        Ok(dst)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::image::YuvRange;
    use crate::image::ALL_PLANES;
    use crate::image::MAX_PLANE_COUNT;
    use crate::Category;

    use test_case::test_case;
    use test_case::test_matrix;

    const WIDTH: usize = 3;
    const HEIGHT: usize = 3;
    struct YuvParams {
        width: u32,
        height: u32,
        depth: u8,
        format: PixelFormat,
        yuv_range: YuvRange,
        color_primaries: ColorPrimaries,
        matrix_coefficients: MatrixCoefficients,
        planes: [[&'static [u16]; HEIGHT]; MAX_PLANE_COUNT],
    }

    const YUV_PARAMS: [YuvParams; 1] = [YuvParams {
        width: WIDTH as u32,
        height: HEIGHT as u32,
        depth: 12,
        format: PixelFormat::Yuv420,
        yuv_range: YuvRange::Limited,
        color_primaries: ColorPrimaries::Srgb,
        matrix_coefficients: MatrixCoefficients::Bt709,
        planes: [
            [
                &[1001, 1001, 1001],
                &[1001, 1001, 1001],
                &[1001, 1001, 1001],
            ],
            [&[1637, 1637], &[1637, 1637], &[1637, 1637]],
            [&[3840, 3840], &[3840, 3840], &[3840, 3840]],
            [&[0, 0, 2039], &[0, 2039, 4095], &[2039, 4095, 4095]],
        ],
    }];

    struct RgbParams {
        params: (
            /*yuv_param_index:*/ usize,
            /*format:*/ Format,
            /*depth:*/ u8,
            /*premultiply_alpha:*/ bool,
            /*is_float:*/ bool,
        ),
        expected_rgba: [&'static [u16]; HEIGHT],
    }

    const RGB_PARAMS: [RgbParams; 5] = [
        RgbParams {
            params: (0, Format::Rgba, 16, true, false),
            expected_rgba: [
                &[0, 0, 0, 0, 0, 0, 0, 0, 32631, 1, 0, 32631],
                &[0, 0, 0, 0, 32631, 1, 0, 32631, 65535, 2, 0, 65535],
                &[32631, 1, 0, 32631, 65535, 2, 0, 65535, 65535, 2, 0, 65535],
            ],
        },
        RgbParams {
            params: (0, Format::Rgba, 16, true, true),
            expected_rgba: [
                &[0, 0, 0, 0, 0, 0, 0, 0, 14327, 256, 0, 14327],
                &[0, 0, 0, 0, 14327, 256, 0, 14327, 15360, 512, 0, 15360],
                &[
                    14327, 256, 0, 14327, 15360, 512, 0, 15360, 15360, 512, 0, 15360,
                ],
            ],
        },
        RgbParams {
            params: (0, Format::Rgba, 16, false, true),
            expected_rgba: [
                &[15360, 512, 0, 0, 15360, 512, 0, 0, 15360, 512, 0, 14327],
                &[15360, 512, 0, 0, 15360, 512, 0, 14327, 15360, 512, 0, 15360],
                &[
                    15360, 512, 0, 14327, 15360, 512, 0, 15360, 15360, 512, 0, 15360,
                ],
            ],
        },
        RgbParams {
            params: (0, Format::Rgba, 16, false, false),
            expected_rgba: [
                &[65535, 2, 0, 0, 65535, 2, 0, 0, 65535, 2, 0, 32631],
                &[65535, 2, 0, 0, 65535, 2, 0, 32631, 65535, 2, 0, 65535],
                &[65535, 2, 0, 32631, 65535, 2, 0, 65535, 65535, 2, 0, 65535],
            ],
        },
        RgbParams {
            params: (0, Format::Bgra, 16, true, false),
            expected_rgba: [
                &[0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 32631, 32631],
                &[0, 0, 0, 0, 0, 1, 32631, 32631, 0, 2, 65535, 65535],
                &[0, 1, 32631, 32631, 0, 2, 65535, 65535, 0, 2, 65535, 65535],
            ],
        },
    ];

    #[allow(clippy::zero_prefixed_literal)]
    #[test_matrix(0usize..5)]
    fn rgb_conversion(rgb_param_index: usize) -> AvifResult<()> {
        let rgb_params = &RGB_PARAMS[rgb_param_index];
        let yuv_params = &YUV_PARAMS[rgb_params.params.0];
        let mut image = image::Image {
            width: yuv_params.width,
            height: yuv_params.height,
            depth: yuv_params.depth,
            yuv_format: yuv_params.format,
            color_primaries: yuv_params.color_primaries,
            matrix_coefficients: yuv_params.matrix_coefficients,
            yuv_range: yuv_params.yuv_range,
            ..image::Image::default()
        };
        image.allocate_planes(Category::Color)?;
        image.allocate_planes(Category::Alpha)?;
        let yuva_planes = &yuv_params.planes;
        for plane in ALL_PLANES {
            let plane_index = plane.as_usize();
            if yuva_planes[plane_index].is_empty() {
                continue;
            }
            let plane_width = image.width(plane);
            for y in 0..image.height(plane) {
                let row16 = image.row16_mut(plane, y as u32)?;
                let dst = &mut row16[..plane_width];
                assert_eq!(dst.len(), yuva_planes[plane_index][y].len());
                dst.copy_from_slice(yuva_planes[plane_index][y]);
            }
        }

        let mut rgb = Image::create_from_yuv(&image);
        assert_eq!(rgb.width, image.width);
        assert_eq!(rgb.height, image.height);
        assert_eq!(rgb.depth, image.depth);

        rgb.format = rgb_params.params.1;
        rgb.depth = rgb_params.params.2;
        rgb.premultiply_alpha = rgb_params.params.3;
        rgb.is_float = rgb_params.params.4;

        rgb.allocate()?;
        rgb.convert_from_yuv(&image)?;

        for y in 0..rgb.height as usize {
            let row16 = rgb.row16(y as u32)?;
            assert_eq!(row16, rgb_params.expected_rgba[y]);
        }
        Ok(())
    }

    #[test_case(Format::Rgba, &[0, 1, 2, 3])]
    #[test_case(Format::Abgr, &[3, 2, 1, 0])]
    #[test_case(Format::Rgb, &[0, 1, 2])]
    fn shuffle_channels_to(format: Format, expected: &[u8]) {
        let image = Image {
            width: 1,
            height: 1,
            depth: 8,
            format: Format::Rgba,
            pixels: Some(Pixels::Buffer(vec![0u8, 1, 2, 3])),
            row_bytes: 4,
            ..Default::default()
        };
        assert_eq!(
            image.shuffle_channels_to(format).unwrap().row(0).unwrap(),
            expected
        );
    }
}
