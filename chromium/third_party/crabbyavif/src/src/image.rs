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

use crate::decoder::tile::TileInfo;
use crate::decoder::ProgressiveState;
use crate::internal_utils::*;
use crate::parser::mp4box::CodecConfiguration;
use crate::reformat::coeffs::*;
use crate::utils::clap::CleanAperture;
use crate::utils::pixels::*;
use crate::*;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Plane {
    Y = 0,
    U = 1,
    V = 2,
    A = 3,
}

impl From<usize> for Plane {
    fn from(plane: usize) -> Self {
        match plane {
            1 => Plane::U,
            2 => Plane::V,
            3 => Plane::A,
            _ => Plane::Y,
        }
    }
}

impl Plane {
    pub(crate) fn as_usize(&self) -> usize {
        match self {
            Plane::Y => 0,
            Plane::U => 1,
            Plane::V => 2,
            Plane::A => 3,
        }
    }
}

/// cbindgen:ignore
pub const MAX_PLANE_COUNT: usize = 4;
pub const YUV_PLANES: [Plane; 3] = [Plane::Y, Plane::U, Plane::V];
pub const A_PLANE: [Plane; 1] = [Plane::A];
pub const ALL_PLANES: [Plane; MAX_PLANE_COUNT] = [Plane::Y, Plane::U, Plane::V, Plane::A];

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
// VideoFullRangeFlag as specified in ISO/IEC 23091-2/ITU-T H.273.
pub enum YuvRange {
    Limited = 0,
    #[default]
    Full = 1,
}

#[derive(Default)]
pub struct Image {
    pub width: u32,
    pub height: u32,
    pub depth: u8,

    pub yuv_format: PixelFormat,
    pub yuv_range: YuvRange,
    pub chroma_sample_position: ChromaSamplePosition,

    pub alpha_present: bool,
    pub alpha_premultiplied: bool,

    pub row_bytes: [u32; MAX_PLANE_COUNT],
    pub image_owns_planes: [bool; MAX_PLANE_COUNT],

    pub planes: [Option<Pixels>; MAX_PLANE_COUNT],

    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,

    pub clli: Option<ContentLightLevelInformation>,
    pub pasp: Option<PixelAspectRatio>,
    pub clap: Option<CleanAperture>,
    pub irot_angle: Option<u8>,
    pub imir_axis: Option<u8>,

    pub exif: Vec<u8>,
    pub icc: Vec<u8>,
    pub xmp: Vec<u8>,

    pub image_sequence_track_present: bool,
    pub progressive_state: ProgressiveState,
}

pub struct PlaneData {
    pub width: u32,
    pub height: u32,
    pub row_bytes: u32,
    pub pixel_size: u32,
}

#[derive(Clone, Copy)]
pub enum PlaneRow<'a> {
    Depth8(&'a [u8]),
    Depth16(&'a [u16]),
}

impl Image {
    pub(crate) fn shallow_clone(&self) -> Self {
        Self {
            width: self.width,
            height: self.height,
            depth: self.depth,
            yuv_format: self.yuv_format,
            yuv_range: self.yuv_range,
            chroma_sample_position: self.chroma_sample_position,
            alpha_present: self.alpha_present,
            alpha_premultiplied: self.alpha_premultiplied,
            color_primaries: self.color_primaries,
            transfer_characteristics: self.transfer_characteristics,
            matrix_coefficients: self.matrix_coefficients,
            clli: self.clli,
            pasp: self.pasp,
            clap: self.clap,
            irot_angle: self.irot_angle,
            imir_axis: self.imir_axis,
            exif: self.exif.clone(),
            icc: self.icc.clone(),
            xmp: self.xmp.clone(),
            image_sequence_track_present: self.image_sequence_track_present,
            progressive_state: self.progressive_state,
            ..Default::default()
        }
    }

    pub(crate) fn is_supported_depth(depth: u8) -> bool {
        matches!(depth, 8 | 10 | 12 | 16)
    }

    pub(crate) fn depth_valid(&self) -> bool {
        Self::is_supported_depth(self.depth)
    }

    pub fn max_channel(&self) -> u16 {
        if !self.depth_valid() {
            0
        } else {
            ((1i32 << self.depth) - 1) as u16
        }
    }

    pub(crate) fn max_channel_f(&self) -> f32 {
        self.max_channel() as f32
    }

    pub fn has_plane(&self, plane: Plane) -> bool {
        let plane_index = plane.as_usize();
        if self.planes[plane_index].is_none() || self.row_bytes[plane_index] == 0 {
            return false;
        }
        self.planes[plane_index].unwrap_ref().has_data()
    }

    pub fn has_alpha(&self) -> bool {
        self.has_plane(Plane::A)
    }

    pub(crate) fn has_same_properties(&self, other: &Image) -> bool {
        self.width == other.width && self.height == other.height && self.depth == other.depth
    }

    // TODO: b/392112497 - remove this annotation once encoder feature is enabled by default.
    #[allow(dead_code)]
    pub(crate) fn has_same_cicp(&self, other: &Image) -> bool {
        self.depth == other.depth
            && self.yuv_format == other.yuv_format
            && self.yuv_range == other.yuv_range
            && self.chroma_sample_position == other.chroma_sample_position
            && self.color_primaries == other.color_primaries
            && self.transfer_characteristics == other.transfer_characteristics
            && self.matrix_coefficients == other.matrix_coefficients
    }

    pub fn has_same_properties_and_cicp(&self, other: &Image) -> bool {
        self.has_same_properties(other) && self.has_same_cicp(other)
    }

    pub fn width(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.width as usize,
            Plane::U => match self.yuv_format {
                PixelFormat::Yuv444
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => self.width as usize,
                PixelFormat::Yuv420 | PixelFormat::Yuv422 => (self.width as usize).div_ceil(2),
                PixelFormat::None | PixelFormat::Yuv400 => 0,
            },
            Plane::V => match self.yuv_format {
                PixelFormat::Yuv444 => self.width as usize,
                PixelFormat::Yuv420 | PixelFormat::Yuv422 => (self.width as usize).div_ceil(2),
                PixelFormat::None
                | PixelFormat::Yuv400
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => 0,
            },
        }
    }

    pub fn height(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.height as usize,
            Plane::U => match self.yuv_format {
                PixelFormat::Yuv444 | PixelFormat::Yuv422 => self.height as usize,
                PixelFormat::Yuv420
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => (self.height as usize).div_ceil(2),
                PixelFormat::None | PixelFormat::Yuv400 => 0,
            },
            Plane::V => match self.yuv_format {
                PixelFormat::Yuv444 | PixelFormat::Yuv422 => self.height as usize,
                PixelFormat::Yuv420 => (self.height as usize).div_ceil(2),
                PixelFormat::None
                | PixelFormat::Yuv400
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => 0,
            },
        }
    }

    pub fn plane_data(&self, plane: Plane) -> Option<PlaneData> {
        if !self.has_plane(plane) {
            return None;
        }
        Some(PlaneData {
            width: self.width(plane) as u32,
            height: self.height(plane) as u32,
            row_bytes: self.row_bytes[plane.as_usize()],
            pixel_size: if self.depth == 8 { 1 } else { 2 },
        })
    }

    pub fn row(&self, plane: Plane, row: u32) -> AvifResult<&[u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_ref()
            .slice(start, row_bytes)
    }

    // Same as row() but only returns `width` pixels (extra row padding is excluded).
    pub fn row_exact(&self, plane: Plane, row: u32) -> AvifResult<&[u8]> {
        let width = self.width(plane);
        Ok(&self.row(plane, row)?[0..width])
    }

    pub fn row_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_mut()
            .slice_mut(start, row_bytes)
    }

    // Same as row_mut() but only returns `width` pixels (extra row padding is excluded).
    pub fn row_exact_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u8]> {
        let width = self.width(plane);
        Ok(&mut self.row_mut(plane, row)?[0..width])
    }

    pub fn row16(&self, plane: Plane, row: u32) -> AvifResult<&[u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_ref()
            .slice16(start, row_bytes)
    }

    // Same as row16() but only returns `width` pixels (extra row padding is excluded).
    pub fn row16_exact(&self, plane: Plane, row: u32) -> AvifResult<&[u16]> {
        let width = self.width(plane);
        Ok(&self.row16(plane, row)?[0..width])
    }

    pub fn row16_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_mut()
            .slice16_mut(start, row_bytes)
    }

    // Same as row16_mut() but only returns `width` pixels (extra row padding is excluded).
    pub fn row16_exact_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u16]> {
        let width = self.width(plane);
        Ok(&mut self.row16_mut(plane, row)?[0..width])
    }

    pub(crate) fn row_generic<'a>(&'a self, plane: Plane, row: u32) -> AvifResult<PlaneRow<'a>> {
        Ok(if self.depth == 8 {
            PlaneRow::Depth8(self.row(plane, row)?)
        } else {
            PlaneRow::Depth16(self.row16(plane, row)?)
        })
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn plane_ptrs(&self) -> [*const u8; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_ref().ptr()
            } else {
                std::ptr::null()
            }
        })
    }

    #[cfg(any(feature = "libyuv", feature = "sharpyuv"))]
    pub(crate) fn plane_ptrs_mut(&mut self) -> [*mut u8; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_mut().ptr_mut_generic()
            } else {
                std::ptr::null_mut()
            }
        })
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn plane16_ptrs(&self) -> [*const u16; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_ref().ptr16()
            } else {
                std::ptr::null()
            }
        })
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn plane_row_bytes(&self) -> AvifResult<[i32; 4]> {
        Ok(ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                i32_from_u32(self.plane_data(x).unwrap().row_bytes).unwrap()
            } else {
                0
            }
        }))
    }

    #[cfg(any(feature = "dav1d", feature = "libgav1"))]
    pub(crate) fn clear_chroma_planes(&mut self) {
        for plane in [Plane::U, Plane::V] {
            let plane = plane.as_usize();
            self.planes[plane] = None;
            self.row_bytes[plane] = 0;
            self.image_owns_planes[plane] = false;
        }
    }

    pub(crate) fn allocate_planes_with_default_values(
        &mut self,
        category: Category,
        default_values: [u16; 4],
    ) -> AvifResult<()> {
        let pixel_size: usize = if self.depth == 8 { 1 } else { 2 };
        for plane in category.planes() {
            let plane = *plane;
            let plane_index = plane.as_usize();
            let width = round2_usize(self.width(plane));
            let plane_size = checked_mul!(width, round2_usize(self.height(plane)))?;
            if self.planes[plane_index].is_some()
                && self.planes[plane_index].unwrap_ref().size() == plane_size
                && (self.planes[plane_index].unwrap_ref().pixel_bit_size() == 0
                    || self.planes[plane_index].unwrap_ref().pixel_bit_size() == pixel_size * 8)
            {
                continue;
            }
            self.planes[plane_index] = Some(if self.depth == 8 {
                Pixels::Buffer(Vec::new())
            } else {
                Pixels::Buffer16(Vec::new())
            });
            let pixels = self.planes[plane_index].unwrap_mut();
            pixels.resize(plane_size, default_values[plane_index])?;
            self.row_bytes[plane_index] = u32_from_usize(checked_mul!(width, pixel_size)?)?;
            self.image_owns_planes[plane_index] = true;
        }
        Ok(())
    }

    pub fn allocate_planes(&mut self, category: Category) -> AvifResult<()> {
        self.allocate_planes_with_default_values(category, [0, 0, 0, self.max_channel()])
    }

    pub(crate) fn copy_properties_from(
        &mut self,
        image: &Image,
        codec_config: &CodecConfiguration,
    ) {
        self.yuv_format = image.yuv_format;
        self.depth = image.depth;
        if cfg!(feature = "heic") && codec_config.is_heic() {
            // For AVIF, the information in the `colr` box takes precedence over what is reported
            // by the decoder. For HEIC, we always honor what is reported by the decoder.
            self.yuv_range = image.yuv_range;
            self.color_primaries = image.color_primaries;
            self.transfer_characteristics = image.transfer_characteristics;
            self.matrix_coefficients = image.matrix_coefficients;
        }
    }

    // If src contains pointers, this function will simply make a copy of the pointer without
    // copying the actual pixels (stealing). If src contains buffer, this function will clone the
    // buffers (copying).
    pub(crate) fn steal_or_copy_planes_from(
        &mut self,
        src: &Image,
        category: Category,
    ) -> AvifResult<()> {
        for plane in category.planes() {
            let plane = plane.as_usize();
            (self.planes[plane], self.row_bytes[plane]) = match &src.planes[plane] {
                Some(src_plane) => (Some(src_plane.try_clone()?), src.row_bytes[plane]),
                None => (None, 0),
            }
        }
        Ok(())
    }

    #[cfg(feature = "encoder")]
    pub(crate) fn copy_and_pad(&mut self, image: &Image) -> AvifResult<()> {
        if image.width > self.width || image.height > self.height {
            return Err(AvifError::InvalidArgument);
        }
        self.allocate_planes(Category::Color)?;
        if image.has_alpha() {
            self.allocate_planes(Category::Alpha)?;
        }
        for plane in ALL_PLANES {
            let src_plane = match image.plane_data(plane) {
                Some(pd) => pd,
                None => continue,
            };
            if self.depth == 8 {
                for y in 0..src_plane.height {
                    let src_row = image.row_exact(plane, y)?;
                    let dst_row = self.row_mut(plane, y)?;
                    let dst_slice = &mut dst_row[0..src_row.len()];
                    dst_slice.copy_from_slice(src_row);
                    let dst_slice = &mut dst_row[src_row.len()..];
                    dst_slice.fill(*src_row.last().unwrap());
                }
            } else {
                for y in 0..src_plane.height {
                    let src_row = image.row16_exact(plane, y)?;
                    let dst_row = self.row16_mut(plane, y)?;
                    let dst_slice = &mut dst_row[0..src_row.len()];
                    dst_slice.copy_from_slice(src_row);
                    let dst_slice = &mut dst_row[src_row.len()..];
                    dst_slice.fill(*src_row.last().unwrap());
                }
            }
        }
        Ok(())
    }

    pub(crate) fn copy_from_tile(
        &mut self,
        tile: &Image,
        grid: &Grid,
        tile_index: u32,
        category: Category,
    ) -> AvifResult<()> {
        // This function is used only when |tile| contains pointers and self contains buffers.
        let row_index = tile_index / grid.columns;
        let column_index = tile_index % grid.columns;
        for plane in category.planes() {
            let plane = *plane;
            let src_plane = tile.plane_data(plane);
            if src_plane.is_none() {
                continue;
            }
            let src_plane = src_plane.unwrap();
            // If this is the last tile column, clamp to left over width.
            let src_width_to_copy = if column_index == grid.columns - 1 {
                let width_so_far = checked_mul!(src_plane.width, column_index)?;
                checked_sub!(self.width(plane), usize_from_u32(width_so_far)?)?
            } else {
                usize_from_u32(src_plane.width)?
            };

            // If this is the last tile row, clamp to left over height.
            let src_height_to_copy = if row_index == grid.rows - 1 {
                let height_so_far = checked_mul!(src_plane.height, row_index)?;
                checked_sub!(u32_from_usize(self.height(plane))?, height_so_far)?
            } else {
                src_plane.height
            };

            let dst_y_start = checked_mul!(row_index, src_plane.height)?;
            let dst_x_offset = usize_from_u32(checked_mul!(column_index, src_plane.width)?)?;
            let dst_x_offset_end = checked_add!(dst_x_offset, src_width_to_copy)?;
            if self.depth == 8 {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row(plane, y)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row = self.row_mut(plane, checked_add!(dst_y_start, y)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            } else {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row16(plane, y)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row = self.row16_mut(plane, checked_add!(dst_y_start, y)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            }
        }
        Ok(())
    }

    pub(crate) fn copy_and_overlay_from_tile(
        &mut self,
        tile: &Image,
        tile_info: &TileInfo,
        tile_index: u32,
        category: Category,
    ) -> AvifResult<()> {
        // This function is used only when |tile| contains pointers and self contains buffers.
        for plane in category.planes() {
            let plane = *plane;
            let src_plane = tile.plane_data(plane);
            let dst_plane = self.plane_data(plane);
            if src_plane.is_none() || dst_plane.is_none() {
                continue;
            }
            let dst_plane = dst_plane.unwrap();
            let tile_index = usize_from_u32(tile_index)?;

            let vertical_offset = tile_info.overlay.vertical_offsets[tile_index] as i128;
            let horizontal_offset = tile_info.overlay.horizontal_offsets[tile_index] as i128;
            let src_height = tile.height as i128;
            let src_width = tile.width as i128;
            let dst_height = dst_plane.height as i128;
            let dst_width = dst_plane.width as i128;

            if matches!(plane, Plane::Y | Plane::A)
                && (vertical_offset + src_height < 0
                    || horizontal_offset + src_width < 0
                    || vertical_offset >= dst_height
                    || horizontal_offset >= dst_width)
            {
                // Entire tile outside of the canvas. It is sufficient to perform this check only
                // for Y and A plane since they are never sub-sampled.
                return Ok(());
            }

            let mut src_y_start: u32;
            let mut src_height_to_copy: u32;
            let mut dst_y_start: u32;
            if vertical_offset >= 0 {
                src_y_start = 0;
                src_height_to_copy = src_height as u32;
                dst_y_start = vertical_offset as u32;
            } else {
                src_y_start = vertical_offset.unsigned_abs() as u32;
                src_height_to_copy = (src_height - vertical_offset.abs()) as u32;
                dst_y_start = 0;
            }

            let mut src_x_start: u32;
            let mut src_width_to_copy: u32;
            let mut dst_x_start: u32;
            if horizontal_offset >= 0 {
                src_x_start = 0;
                src_width_to_copy = src_width as u32;
                dst_x_start = horizontal_offset as u32;
            } else {
                src_x_start = horizontal_offset.unsigned_abs() as u32;
                src_width_to_copy = (src_width - horizontal_offset.abs()) as u32;
                dst_x_start = 0;
            }

            // Clamp width to the canvas width.
            if self.width - dst_x_start < src_width_to_copy {
                src_width_to_copy = self.width - dst_x_start;
            }

            // Clamp height to the canvas height.
            if self.height - dst_y_start < src_height_to_copy {
                src_height_to_copy = self.height - dst_y_start;
            }

            // Apply chroma subsampling to the offsets.
            if plane == Plane::U || plane == Plane::V {
                src_y_start = tile.yuv_format.apply_chroma_shift_y(src_y_start);
                src_height_to_copy = tile.yuv_format.apply_chroma_shift_y(src_height_to_copy);
                dst_y_start = tile.yuv_format.apply_chroma_shift_y(dst_y_start);
                src_x_start = tile.yuv_format.apply_chroma_shift_x(src_x_start);
                src_width_to_copy = tile.yuv_format.apply_chroma_shift_x(src_width_to_copy);
                dst_x_start = tile.yuv_format.apply_chroma_shift_x(dst_x_start);
            }

            let src_y_range = src_y_start..checked_add!(src_y_start, src_height_to_copy)?;
            let dst_x_range = usize_from_u32(dst_x_start)?
                ..usize_from_u32(checked_add!(dst_x_start, src_width_to_copy)?)?;
            let src_x_range = usize_from_u32(src_x_start)?
                ..checked_add!(usize_from_u32(src_x_start)?, dst_x_range.len())?;
            let mut dst_y = dst_y_start;
            if self.depth == 8 {
                for src_y in src_y_range {
                    let src_row = tile.row(plane, src_y)?;
                    let src_slice = &src_row[src_x_range.clone()];
                    let dst_row = self.row_mut(plane, dst_y)?;
                    let dst_slice = &mut dst_row[dst_x_range.clone()];
                    dst_slice.copy_from_slice(src_slice);
                    checked_incr!(dst_y, 1);
                }
            } else {
                for src_y in src_y_range {
                    let src_row = tile.row16(plane, src_y)?;
                    let src_slice = &src_row[src_x_range.clone()];
                    let dst_row = self.row16_mut(plane, dst_y)?;
                    let dst_slice = &mut dst_row[dst_x_range.clone()];
                    dst_slice.copy_from_slice(src_slice);
                    checked_incr!(dst_y, 1);
                }
            }
        }
        Ok(())
    }

    pub(crate) fn convert_rgba16_to_yuva(&self, rgba: [u16; 4]) -> [u16; 4] {
        let r = rgba[0] as f32 / 65535.0;
        let g = rgba[1] as f32 / 65535.0;
        let b = rgba[2] as f32 / 65535.0;
        let coeffs = calculate_yuv_coefficients(self.color_primaries, self.matrix_coefficients);
        let y = coeffs[0] * r + coeffs[1] * g + coeffs[2] * b;
        let u = (b - y) / (2.0 * (1.0 - coeffs[2]));
        let v = (r - y) / (2.0 * (1.0 - coeffs[0]));
        let uv_bias = (1 << (self.depth - 1)) as f32;
        let max_channel = self.max_channel_f();
        [
            (y * max_channel).clamp(0.0, max_channel) as u16,
            (u * max_channel + uv_bias).clamp(0.0, max_channel) as u16,
            (v * max_channel + uv_bias).clamp(0.0, max_channel) as u16,
            ((rgba[3] as f32) / 65535.0 * max_channel).round() as u16,
        ]
    }

    #[cfg(feature = "encoder")]
    pub(crate) fn is_opaque(&self) -> bool {
        if let Some(plane_data) = self.plane_data(Plane::A) {
            let opaque_value = self.max_channel();
            if self.depth == 8 {
                for y in 0..plane_data.height {
                    let row = self.row_exact(Plane::A, y).unwrap();
                    if !row.iter().all(|pixel| *pixel == opaque_value as u8) {
                        return false;
                    }
                }
            } else {
                for y in 0..plane_data.height {
                    let row = self.row16_exact(Plane::A, y).unwrap();
                    if !row.iter().all(|pixel| *pixel == opaque_value) {
                        return false;
                    }
                }
            }
        }
        true
    }
}
