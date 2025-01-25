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
use crate::decoder::Category;
use crate::decoder::ProgressiveState;
use crate::internal_utils::pixels::*;
use crate::internal_utils::*;
use crate::parser::mp4box::*;
use crate::utils::clap::CleanAperture;
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
    pub fn to_usize(&self) -> usize {
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
    pub fn depth_valid(&self) -> bool {
        matches!(self.depth, 8 | 10 | 12 | 16)
    }

    pub fn max_channel(&self) -> u16 {
        if !self.depth_valid() {
            0
        } else {
            ((1i32 << self.depth) - 1) as u16
        }
    }

    pub fn max_channel_f(&self) -> f32 {
        self.max_channel() as f32
    }

    pub fn has_plane(&self, plane: Plane) -> bool {
        let plane_index = plane.to_usize();
        if self.planes[plane_index].is_none() || self.row_bytes[plane_index] == 0 {
            return false;
        }
        self.planes[plane_index].unwrap_ref().has_data()
    }

    pub fn has_alpha(&self) -> bool {
        self.has_plane(Plane::A)
    }

    pub fn has_same_properties(&self, other: &Image) -> bool {
        self.width == other.width && self.height == other.height && self.depth == other.depth
    }

    pub fn width(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.width as usize,
            Plane::U | Plane::V => match self.yuv_format {
                PixelFormat::Yuv444
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => self.width as usize,
                PixelFormat::Yuv420 | PixelFormat::Yuv422 => (self.width as usize + 1) / 2,
                PixelFormat::None | PixelFormat::Yuv400 => 0,
            },
        }
    }

    pub fn height(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.height as usize,
            Plane::U | Plane::V => match self.yuv_format {
                PixelFormat::Yuv444 | PixelFormat::Yuv422 => self.height as usize,
                PixelFormat::Yuv420
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => (self.height as usize + 1) / 2,
                PixelFormat::None | PixelFormat::Yuv400 => 0,
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
            row_bytes: self.row_bytes[plane.to_usize()],
            pixel_size: if self.depth == 8 { 1 } else { 2 },
        })
    }

    pub fn row(&self, plane: Plane, row: u32) -> AvifResult<&[u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let start = checked_mul!(row, plane_data.row_bytes)?;
        self.planes[plane.to_usize()]
            .unwrap_ref()
            .slice(start, plane_data.row_bytes)
    }

    pub fn row_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.to_usize()]
            .unwrap_mut()
            .slice_mut(start, row_bytes)
    }

    pub fn row16(&self, plane: Plane, row: u32) -> AvifResult<&[u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.to_usize()]
            .unwrap_ref()
            .slice16(start, row_bytes)
    }

    pub fn row16_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.to_usize()]
            .unwrap_mut()
            .slice16_mut(start, row_bytes)
    }

    pub fn row_generic(&self, plane: Plane, row: u32) -> AvifResult<PlaneRow> {
        Ok(if self.depth == 8 {
            PlaneRow::Depth8(self.row(plane, row)?)
        } else {
            PlaneRow::Depth16(self.row16(plane, row)?)
        })
    }

    pub fn clear_chroma_planes(&mut self) {
        for plane in [Plane::U, Plane::V] {
            let plane = plane.to_usize();
            self.planes[plane] = None;
            self.row_bytes[plane] = 0;
            self.image_owns_planes[plane] = false;
        }
    }

    pub fn allocate_planes(&mut self, category: Category) -> AvifResult<()> {
        let pixel_size: usize = if self.depth == 8 { 1 } else { 2 };
        for plane in category.planes() {
            let plane = *plane;
            let plane_index = plane.to_usize();
            let width = self.width(plane);
            let plane_size = checked_mul!(width, self.height(plane))?;
            let default_value = if plane == Plane::A { self.max_channel() } else { 0 };
            if self.planes[plane_index].is_some()
                && self.planes[plane_index].unwrap_ref().size() == plane_size
                && (self.planes[plane_index].unwrap_ref().pixel_bit_size() == 0
                    || self.planes[plane_index].unwrap_ref().pixel_bit_size() == pixel_size * 8)
            {
                // TODO: need to memset to 0 maybe?
                continue;
            }
            self.planes[plane_index] = Some(if self.depth == 8 {
                Pixels::Buffer(Vec::new())
            } else {
                Pixels::Buffer16(Vec::new())
            });
            let pixels = self.planes[plane_index].unwrap_mut();
            pixels.resize(plane_size, default_value)?;
            self.row_bytes[plane_index] = u32_from_usize(checked_mul!(width, pixel_size)?)?;
            self.image_owns_planes[plane_index] = true;
        }
        Ok(())
    }

    // If src contains pointers, this function will simply make a copy of the pointer without
    // copying the actual pixels (stealing). If src contains buffer, this function will clone the
    // buffers (copying).
    pub fn steal_or_copy_from(&mut self, src: &Image, category: Category) -> AvifResult<()> {
        for plane in category.planes() {
            let plane = plane.to_usize();
            (self.planes[plane], self.row_bytes[plane]) = match &src.planes[plane] {
                Some(src_plane) => (Some(src_plane.clone()), src.row_bytes[plane]),
                None => (None, 0),
            }
        }
        Ok(())
    }

    pub fn copy_from_tile(
        &mut self,
        tile: &Image,
        tile_info: &TileInfo,
        tile_index: u32,
        category: Category,
    ) -> AvifResult<()> {
        // This function is used only when |tile| contains pointers and self contains buffers.
        let row_index = u64::from(tile_index / tile_info.grid.columns);
        let column_index = u64::from(tile_index % tile_info.grid.columns);
        for plane in category.planes() {
            let plane = *plane;
            let src_plane = tile.plane_data(plane);
            if src_plane.is_none() {
                continue;
            }
            let src_plane = src_plane.unwrap();
            // If this is the last tile column, clamp to left over width.
            let src_width_to_copy = if column_index == (tile_info.grid.columns - 1).into() {
                let width_so_far = u64::from(src_plane.width)
                    .checked_mul(column_index)
                    .ok_or(AvifError::BmffParseFailed("".into()))?;
                u64_from_usize(self.width(plane))?
                    .checked_sub(width_so_far)
                    .ok_or(AvifError::BmffParseFailed("".into()))?
            } else {
                u64::from(src_plane.width)
            };
            let src_width_to_copy = usize_from_u64(src_width_to_copy)?;

            // If this is the last tile row, clamp to left over height.
            let src_height_to_copy = if row_index == (tile_info.grid.rows - 1).into() {
                let height_so_far = u64::from(src_plane.height)
                    .checked_mul(row_index)
                    .ok_or(AvifError::BmffParseFailed("".into()))?;
                u64_from_usize(self.height(plane))?
                    .checked_sub(height_so_far)
                    .ok_or(AvifError::BmffParseFailed("".into()))?
            } else {
                u64::from(src_plane.height)
            };

            let dst_y_start = checked_mul!(row_index, u64::from(src_plane.height))?;
            let dst_x_offset =
                usize_from_u64(checked_mul!(column_index, u64::from(src_plane.width))?)?;
            let dst_x_offset_end = checked_add!(dst_x_offset, src_width_to_copy)?;
            // TODO: src_height_to_copy can just be u32?
            if self.depth == 8 {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row(plane, u32_from_u64(y)?)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row =
                        self.row_mut(plane, u32_from_u64(checked_add!(dst_y_start, y)?)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            } else {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row16(plane, u32_from_u64(y)?)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row =
                        self.row16_mut(plane, u32_from_u64(checked_add!(dst_y_start, y)?)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            }
        }
        Ok(())
    }
}
