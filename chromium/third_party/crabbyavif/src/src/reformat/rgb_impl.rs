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

use super::rgb;
use super::rgb::*;

use crate::image::Plane;
use crate::image::PlaneRow;
use crate::image::YuvRange;
use crate::internal_utils::*;
use crate::*;

use std::cmp::min;

// Copies GBR samples to YUV samples. Returns Ok(None) if not implemented.
fn identity_yuv8_to_rgb8_full_range(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    if image.yuv_format != PixelFormat::Yuv444 || rgb.format == Format::Rgb565 {
        return Ok(None); // Not implemented.
    }

    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let channel_count = rgb.channel_count() as usize;
    for i in 0..image.height {
        let y = image.row(Plane::Y, i)?;
        let u = image.row(Plane::U, i)?;
        let v = image.row(Plane::V, i)?;
        let rgb_pixels = rgb.row_mut(i)?;
        for j in 0..image.width as usize {
            rgb_pixels[(j * channel_count) + r_offset] = v[j];
            rgb_pixels[(j * channel_count) + g_offset] = y[j];
            rgb_pixels[(j * channel_count) + b_offset] = u[j];
        }
    }
    Ok(Some(()))
}

// Copies GBR samples to YUV samples. Returns Ok(None) if not implemented.
fn identity_yuv16_to_rgb16_full_range(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    if image.yuv_format != PixelFormat::Yuv444 {
        return Ok(None); // Not implemented.
    }

    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let channel_count = rgb.channel_count() as usize;
    for i in 0..image.height {
        let y = image.row16(Plane::Y, i)?;
        let u = image.row16(Plane::U, i)?;
        let v = image.row16(Plane::V, i)?;
        let rgb_pixels = rgb.row16_mut(i)?;
        for j in 0..image.width as usize {
            rgb_pixels[(j * channel_count) + r_offset] = v[j];
            rgb_pixels[(j * channel_count) + g_offset] = y[j];
            rgb_pixels[(j * channel_count) + b_offset] = u[j];
        }
    }
    Ok(Some(()))
}

// This is a macro and not a function because this is invoked per-pixel and there is a non-trivial
// performance impact if this is made into a function call.
macro_rules! store_rgb_pixel8 {
    ($dst:ident, $rgb_565: ident, $index: ident, $r: ident, $g: ident, $b: ident, $r_offset: ident,
     $g_offset: ident, $b_offset: ident, $rgb_channel_count: ident, $rgb_max_channel_f: ident) => {
        let r8 = (0.5 + ($r * $rgb_max_channel_f)) as u8;
        let g8 = (0.5 + ($g * $rgb_max_channel_f)) as u8;
        let b8 = (0.5 + ($b * $rgb_max_channel_f)) as u8;
        if $rgb_565 {
            // References for RGB565 color conversion:
            // * https://docs.microsoft.com/en-us/windows/win32/directshow/working-with-16-bit-rgb
            // * https://chromium.googlesource.com/libyuv/libyuv/+/9892d70c965678381d2a70a1c9002d1cf136ee78/source/row_common.cc#2362
            let r16 = ((r8 >> 3) as u16) << 11;
            let g16 = ((g8 >> 2) as u16) << 5;
            let b16 = (b8 >> 3) as u16;
            let rgb565 = (r16 | g16 | b16).to_le_bytes();
            $dst[($index * $rgb_channel_count) + $r_offset] = rgb565[0];
            $dst[($index * $rgb_channel_count) + $r_offset + 1] = rgb565[1];
        } else {
            $dst[($index * $rgb_channel_count) + $r_offset] = r8;
            $dst[($index * $rgb_channel_count) + $g_offset] = g8;
            $dst[($index * $rgb_channel_count) + $b_offset] = b8;
        }
    };
}

fn yuv8_to_rgb8_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row(Plane::Y, j)?;
        let u_row = image.row(Plane::U, uv_j)?;
        // If V plane is missing, then the format is NV12. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
            let y = table_y[y_row[i] as usize];
            let cb = table_uv[u_row[uv_i] as usize];
            let cr = table_uv[v_row[uv_i] as usize];
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                r,
                g,
                b,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv16_to_rgb16_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row16(Plane::Y, j)?;
        let u_row = image.row16(Plane::U, uv_j)?;
        // If V plane is missing, then the format is P010. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row16(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            let cb = table_uv[min(u_row[uv_i], yuv_max_channel) as usize];
            let cr = table_uv[min(v_row[uv_i], yuv_max_channel) as usize];
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            dst[(i * rgb_channel_count) + r_offset] = (0.5 + (r * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + g_offset] = (0.5 + (g * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + b_offset] = (0.5 + (b * rgb_max_channel_f)) as u16;
        }
    }
    Ok(())
}

fn yuv16_to_rgb8_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row16(Plane::Y, j)?;
        let u_row = image.row16(Plane::U, uv_j)?;
        // If V plane is missing, then the format is P010. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row16(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            let cb = table_uv[min(u_row[uv_i], yuv_max_channel) as usize];
            let cr = table_uv[min(v_row[uv_i], yuv_max_channel) as usize];
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                r,
                g,
                b,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv8_to_rgb16_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row(Plane::Y, j)?;
        let u_row = image.row(Plane::U, uv_j)?;
        // If V plane is missing, then the format is NV12. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
            let y = table_y[y_row[i] as usize];
            let cb = table_uv[u_row[uv_i] as usize];
            let cr = table_uv[v_row[uv_i] as usize];
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            dst[(i * rgb_channel_count) + r_offset] = (0.5 + (r * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + g_offset] = (0.5 + (g * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + b_offset] = (0.5 + (b * rgb_max_channel_f)) as u16;
        }
    }
    Ok(())
}

fn yuv8_to_rgb8_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    for j in 0..image.height {
        let y_row = image.row(Plane::Y, j)?;
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[y_row[i] as usize];
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                y,
                y,
                y,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv16_to_rgb16_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    for j in 0..image.height {
        let y_row = image.row16(Plane::Y, j)?;
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            let rgb_pixel = (0.5 + (y * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + r_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + g_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + b_offset] = rgb_pixel;
        }
    }
    Ok(())
}

fn yuv16_to_rgb8_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    for j in 0..image.height {
        let y_row = image.row16(Plane::Y, j)?;
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                y,
                y,
                y,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv8_to_rgb16_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    for j in 0..image.height {
        let y_row = image.row(Plane::Y, j)?;
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[y_row[i] as usize];
            let rgb_pixel = (0.5 + (y * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + r_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + g_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + b_offset] = rgb_pixel;
        }
    }
    Ok(())
}

// Converts RGB samples to YUV samples. Returns Ok(None) if not implemented.
pub(crate) fn yuv_to_rgb_fast(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    let mode: Mode = image.into();
    Ok(match mode {
        Mode::Identity => match (image.depth, rgb.depth, image.yuv_range) {
            (8, 8, YuvRange::Full) => identity_yuv8_to_rgb8_full_range(image, rgb)?,
            (16, 16, YuvRange::Full) => identity_yuv16_to_rgb16_full_range(image, rgb)?,
            _ => None,
        },
        Mode::YuvCoefficients(kr, kg, kb) => {
            let has_color = image.yuv_format != PixelFormat::Yuv400;
            Some(match (image.depth == 8, rgb.depth == 8, has_color) {
                (true, true, true) => yuv8_to_rgb8_color(image, rgb, kr, kg, kb),
                (false, false, true) => yuv16_to_rgb16_color(image, rgb, kr, kg, kb),
                (false, true, true) => yuv16_to_rgb8_color(image, rgb, kr, kg, kb),
                (true, false, true) => yuv8_to_rgb16_color(image, rgb, kr, kg, kb),
                (true, true, false) => yuv8_to_rgb8_monochrome(image, rgb, kr, kg, kb),
                (false, false, false) => yuv16_to_rgb16_monochrome(image, rgb, kr, kg, kb),
                (false, true, false) => yuv16_to_rgb8_monochrome(image, rgb, kr, kg, kb),
                (true, false, false) => yuv8_to_rgb16_monochrome(image, rgb, kr, kg, kb),
            }?)
        }
        Mode::Ycgco | Mode::YcgcoRe | Mode::YcgcoRo => None, // Not implemented
    })
}

fn bias_and_range_y(image: &image::Image) -> (f32, f32) {
    // Formula specified in ISO/IEC 23091-2.
    if image.yuv_range == YuvRange::Limited {
        (
            (16 << (image.depth - 8)) as f32,
            (219 << (image.depth - 8)) as f32,
        )
    } else {
        (0.0, image.max_channel_f())
    }
}

fn bias_and_range_uv(image: &image::Image) -> (f32, f32) {
    // Formula specified in ISO/IEC 23091-2.
    (
        (1 << (image.depth - 1)) as f32,
        if image.yuv_range == YuvRange::Limited {
            (224 << (image.depth - 8)) as f32
        } else {
            image.max_channel_f()
        },
    )
}

fn unorm_lookup_tables(
    image: &image::Image,
    mode: Mode,
) -> AvifResult<(Vec<f32>, Option<Vec<f32>>)> {
    let count = 1usize << image.depth;
    let mut table_y: Vec<f32> = create_vec_exact(count)?;
    let (bias_y, range_y) = bias_and_range_y(image);
    for cp in 0..count {
        table_y.push(((cp as f32) - bias_y) / range_y);
    }
    if mode == Mode::Identity {
        Ok((table_y, None))
    } else {
        let (bias_uv, range_uv) = bias_and_range_uv(image);
        let mut table_uv: Vec<f32> = create_vec_exact(count)?;
        for cp in 0..count {
            table_uv.push(((cp as f32) - bias_uv) / range_uv);
        }
        Ok((table_y, Some(table_uv)))
    }
}

#[allow(clippy::too_many_arguments)]
fn compute_rgb(
    y: f32,
    cb: f32,
    cr: f32,
    has_color: bool,
    mode: Mode,
    clamped_y: u16,
    yuv_max_channel: u16,
    rgb_max_channel: u16,
    rgb_max_channel_f: f32,
) -> (f32, f32, f32) {
    let r: f32;
    let g: f32;
    let b: f32;
    if has_color {
        match mode {
            Mode::Identity => {
                g = y;
                b = cb;
                r = cr;
            }
            Mode::Ycgco => {
                let t = y - cb;
                g = y + cb;
                b = t - cr;
                r = t + cr;
            }
            Mode::YcgcoRe | Mode::YcgcoRo => {
                // Equations (62) through (65) in https://www.itu.int/rec/T-REC-H.273
                let cg = (0.5 + cb * yuv_max_channel as f32).floor() as i32;
                let co = (0.5 + cr * yuv_max_channel as f32).floor() as i32;
                let t = clamped_y as i32 - (cg >> 1);
                let rgb_max_channel = rgb_max_channel as i32;
                g = clamp_i32(t + cg, 0, rgb_max_channel) as f32 / rgb_max_channel_f;
                let tmp_b = clamp_i32(t - (co >> 1), 0, rgb_max_channel) as f32;
                b = tmp_b / rgb_max_channel_f;
                r = clamp_i32(tmp_b as i32 + co, 0, rgb_max_channel) as f32 / rgb_max_channel_f;
            }
            Mode::YuvCoefficients(kr, kg, kb) => {
                r = y + (2.0 * (1.0 - kr)) * cr;
                b = y + (2.0 * (1.0 - kb)) * cb;
                g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            }
        }
    } else {
        r = y;
        g = y;
        b = y;
    }
    (
        clamp_f32(r, 0.0, 1.0),
        clamp_f32(g, 0.0, 1.0),
        clamp_f32(b, 0.0, 1.0),
    )
}

fn clamped_pixel(row: PlaneRow, index: usize, max_channel: u16) -> u16 {
    match row {
        PlaneRow::Depth8(row) => row[index] as u16,
        PlaneRow::Depth16(row) => min(max_channel, row[index]),
    }
}

fn unorm_value(row: PlaneRow, index: usize, max_channel: u16, table: &[f32]) -> f32 {
    table[clamped_pixel(row, index, max_channel) as usize]
}

pub(crate) fn yuv_to_rgb_any(
    image: &image::Image,
    rgb: &mut rgb::Image,
    alpha_multiply_mode: AlphaMultiplyMode,
) -> AvifResult<()> {
    let mode: Mode = image.into();
    let (table_y, table_uv) = unorm_lookup_tables(image, mode)?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_depth = rgb.depth;
    let chroma_upsampling = rgb.chroma_upsampling;
    let yuv_has_color = image.has_plane(Plane::U)
        && image.has_plane(Plane::V)
        && image.yuv_format != PixelFormat::Yuv400;
    let rgb_has_color = !rgb.format.is_gray();
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel = rgb.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row_generic(Plane::Y, j)?;
        let u_row = image.row_generic(Plane::U, uv_j).ok();
        let v_row = image.row_generic(Plane::V, uv_j).ok();
        let a_row = image.row_generic(Plane::A, j).ok();
        for i in 0..image.width as usize {
            let clamped_y = clamped_pixel(y_row, i, yuv_max_channel);
            let y = table_y[clamped_y as usize];
            let mut cb = 0.5;
            let mut cr = 0.5;
            if yuv_has_color {
                let u_row = u_row.unwrap();
                let v_row = v_row.unwrap();
                let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
                if image.yuv_format == PixelFormat::Yuv444
                    || matches!(
                        chroma_upsampling,
                        ChromaUpsampling::Fastest | ChromaUpsampling::Nearest
                    )
                {
                    cb = unorm_value(u_row, uv_i, yuv_max_channel, table_uv);
                    cr = unorm_value(v_row, uv_i, yuv_max_channel, table_uv);
                } else {
                    if image.chroma_sample_position != ChromaSamplePosition::CENTER {
                        return AvifError::not_implemented();
                    }

                    // Bilinear filtering with weights. See
                    // https://github.com/AOMediaCodec/libavif/blob/0580334466d57fedb889d5ed7ae9574d6f66e00c/src/reformat.c#L657-L685.
                    let image_width_minus_1 = (image.width - 1) as usize;
                    let uv_adj_i = if i == 0 || (i == image_width_minus_1 && (i % 2) != 0) {
                        uv_i
                    } else if (i % 2) != 0 {
                        uv_i + 1
                    } else {
                        uv_i - 1
                    };
                    let uv_adj_j = if j == 0
                        || (j == image.height - 1 && (j % 2) != 0)
                        || image.yuv_format == PixelFormat::Yuv422
                    {
                        uv_j
                    } else if (j % 2) != 0 {
                        uv_j + 1
                    } else {
                        uv_j - 1
                    };
                    let u_adj_row = image.row_generic(Plane::U, uv_adj_j)?;
                    let v_adj_row = image.row_generic(Plane::V, uv_adj_j)?;
                    let mut unorm_u: [[f32; 2]; 2] = [[0.0; 2]; 2];
                    let mut unorm_v: [[f32; 2]; 2] = [[0.0; 2]; 2];
                    unorm_u[0][0] = unorm_value(u_row, uv_i, yuv_max_channel, table_uv);
                    unorm_v[0][0] = unorm_value(v_row, uv_i, yuv_max_channel, table_uv);
                    unorm_u[1][0] = unorm_value(u_row, uv_adj_i, yuv_max_channel, table_uv);
                    unorm_v[1][0] = unorm_value(v_row, uv_adj_i, yuv_max_channel, table_uv);
                    unorm_u[0][1] = unorm_value(u_adj_row, uv_i, yuv_max_channel, table_uv);
                    unorm_v[0][1] = unorm_value(v_adj_row, uv_i, yuv_max_channel, table_uv);
                    unorm_u[1][1] = unorm_value(u_adj_row, uv_adj_i, yuv_max_channel, table_uv);
                    unorm_v[1][1] = unorm_value(v_adj_row, uv_adj_i, yuv_max_channel, table_uv);
                    cb = (unorm_u[0][0] * (9.0 / 16.0))
                        + (unorm_u[1][0] * (3.0 / 16.0))
                        + (unorm_u[0][1] * (3.0 / 16.0))
                        + (unorm_u[1][1] * (1.0 / 16.0));
                    cr = (unorm_v[0][0] * (9.0 / 16.0))
                        + (unorm_v[1][0] * (3.0 / 16.0))
                        + (unorm_v[0][1] * (3.0 / 16.0))
                        + (unorm_v[1][1] * (1.0 / 16.0));
                }
            }
            let (mut rc, mut gc, mut bc) = if rgb_has_color {
                compute_rgb(
                    y,
                    cb,
                    cr,
                    yuv_has_color,
                    mode,
                    clamped_y,
                    yuv_max_channel,
                    rgb_max_channel,
                    rgb_max_channel_f,
                )
            } else {
                (clamp_f32(y, 0.0, 1.0), 0.0, 0.0)
            };
            if alpha_multiply_mode != AlphaMultiplyMode::NoOp {
                let unorm_a = clamped_pixel(a_row.unwrap(), i, yuv_max_channel);
                let ac = clamp_f32((unorm_a as f32) / (yuv_max_channel as f32), 0.0, 1.0);
                if ac == 0.0 {
                    rc = 0.0;
                    gc = 0.0;
                    bc = 0.0;
                } else if ac < 1.0 {
                    match alpha_multiply_mode {
                        AlphaMultiplyMode::Multiply => {
                            rc *= ac;
                            gc *= ac;
                            bc *= ac;
                        }
                        AlphaMultiplyMode::UnMultiply => {
                            rc = f32::min(rc / ac, 1.0);
                            gc = f32::min(gc / ac, 1.0);
                            bc = f32::min(bc / ac, 1.0);
                        }
                        _ => {} // Not reached.
                    }
                }
            }
            if rgb_depth == 8 {
                let dst = rgb.row_mut(j)?;
                dst[(i * rgb_channel_count) + r_offset] = (0.5 + (rc * rgb_max_channel_f)) as u8;
                if rgb_has_color {
                    dst[(i * rgb_channel_count) + g_offset] =
                        (0.5 + (gc * rgb_max_channel_f)) as u8;
                    dst[(i * rgb_channel_count) + b_offset] =
                        (0.5 + (bc * rgb_max_channel_f)) as u8;
                }
            } else {
                let dst16 = rgb.row16_mut(j)?;
                dst16[(i * rgb_channel_count) + r_offset] = (0.5 + (rc * rgb_max_channel_f)) as u16;
                if rgb_has_color {
                    dst16[(i * rgb_channel_count) + g_offset] =
                        (0.5 + (gc * rgb_max_channel_f)) as u16;
                    dst16[(i * rgb_channel_count) + b_offset] =
                        (0.5 + (bc * rgb_max_channel_f)) as u16;
                }
            }
        }
    }
    Ok(())
}

#[derive(Debug, Default, Copy, Clone)]
struct YUVBlock(f32, f32, f32);

pub(crate) fn rgb_gray_to_yuv(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let rgb_channel_count = rgb.channel_count() as usize;
    let gray_offset = rgb.format.r_offset();
    let rgb_max_channel_f = rgb.max_channel_f();
    let (bias_y, range_y) = bias_and_range_y(image);
    let yuv_max_channel = image.max_channel();
    for j in 0..image.height {
        for i in 0..image.width as usize {
            let gray_pixel = if rgb.depth == 8 {
                let src = rgb.row(j)?;
                src[(i * rgb_channel_count) + gray_offset] as f32 / rgb_max_channel_f
            } else {
                let src = rgb.row16(j)?;
                src[(i * rgb_channel_count) + gray_offset] as f32 / rgb_max_channel_f
            };
            // TODO: b/410088660 - handle alpha multiply/unmultiply.
            let gray_pixel = to_unorm(bias_y, range_y, yuv_max_channel, gray_pixel);
            if image.depth == 8 {
                let dst_y = image.row_mut(Plane::Y, j)?;
                dst_y[i] = gray_pixel as u8;
            } else {
                let dst_y = image.row16_mut(Plane::Y, j)?;
                dst_y[i] = gray_pixel;
            }
        }
    }
    let chroma_value = (image.max_channel() / 2) + 1;
    image.fill_plane_with_value(Plane::U, chroma_value)?;
    image.fill_plane_with_value(Plane::V, chroma_value)?;
    Ok(())
}

pub(crate) fn rgb_to_yuv(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_max_channel_f = rgb.max_channel_f();
    let mode = (image as &image::Image).into();
    let (bias_y, range_y) = bias_and_range_y(image);
    let (bias_uv, range_uv) = if mode == Mode::Identity {
        (bias_y, range_y)
    } else {
        bias_and_range_uv(image)
    };
    let yuv_max_channel = image.max_channel();

    for outer_j in (0..image.height).step_by(2) {
        let block_h = if (outer_j + 1) >= image.height { 1 } else { 2 };
        for outer_i in (0..image.width).step_by(2) {
            let mut yuv_block: [[YUVBlock; 3]; 3] = Default::default();
            let block_w = if (outer_i + 1) >= image.width { 1 } else { 2 };
            for block_j in 0..block_h as usize {
                #[allow(clippy::needless_range_loop)]
                for block_i in 0..block_w as usize {
                    let j = outer_j + block_j as u32;
                    let i = outer_i as usize + block_i;

                    let rgb_pixel = if rgb.depth == 8 {
                        let src = rgb.row(j)?;
                        [
                            src[(i * rgb_channel_count) + r_offset] as f32 / rgb_max_channel_f,
                            src[(i * rgb_channel_count) + g_offset] as f32 / rgb_max_channel_f,
                            src[(i * rgb_channel_count) + b_offset] as f32 / rgb_max_channel_f,
                        ]
                    } else {
                        let src = rgb.row16(j)?;
                        [
                            src[(i * rgb_channel_count) + r_offset] as f32 / rgb_max_channel_f,
                            src[(i * rgb_channel_count) + g_offset] as f32 / rgb_max_channel_f,
                            src[(i * rgb_channel_count) + b_offset] as f32 / rgb_max_channel_f,
                        ]
                    };
                    // TODO: b/410088660 - handle alpha multiply/unmultiply.
                    yuv_block[block_i][block_j] = match mode {
                        Mode::YuvCoefficients(kr, kg, kb) => {
                            let y = (kr * rgb_pixel[0]) + (kg * rgb_pixel[1]) + (kb * rgb_pixel[2]);
                            YUVBlock(
                                y,
                                (rgb_pixel[2] - y) / (2.0 * (1.0 - kb)),
                                (rgb_pixel[0] - y) / (2.0 * (1.0 - kr)),
                            )
                        }
                        Mode::Identity => {
                            // Formulas 41,42,43 from https://www.itu.int/rec/T-REC-H.273-201612-S.
                            YUVBlock(rgb_pixel[1], rgb_pixel[2], rgb_pixel[0])
                        }
                        Mode::YcgcoRe | Mode::YcgcoRo => {
                            // Formulas 58,59,60,61 from https://www.itu.int/rec/T-REC-H.273-202407-P.
                            let r = ((rgb_pixel[0] * rgb_max_channel_f)
                                .clamp(0.0, rgb_max_channel_f)
                                + 0.5)
                                .floor() as i32;
                            let g = ((rgb_pixel[1] * rgb_max_channel_f)
                                .clamp(0.0, rgb_max_channel_f)
                                + 0.5)
                                .floor() as i32;
                            let b = ((rgb_pixel[2] * rgb_max_channel_f)
                                .clamp(0.0, rgb_max_channel_f)
                                + 0.5)
                                .floor() as i32;
                            let co = r - b;
                            let t = b + (co >> 1);
                            let cg = g - t;
                            YUVBlock(
                                (t + (cg >> 1)) as f32 / range_y,
                                cg as f32 / range_uv,
                                co as f32 / range_uv,
                            )
                        }
                        Mode::Ycgco => {
                            // Formulas 44,45,46 from https://www.itu.int/rec/T-REC-H.273-201612-S.
                            YUVBlock(
                                0.5 * rgb_pixel[1] + 0.25 * (rgb_pixel[0] + rgb_pixel[2]),
                                0.5 * rgb_pixel[1] - 0.25 * (rgb_pixel[0] + rgb_pixel[2]),
                                0.5 * (rgb_pixel[0] - rgb_pixel[2]),
                            )
                        }
                    };
                    if image.depth == 8 {
                        let dst_y = image.row_mut(Plane::Y, j)?;
                        dst_y[i] = to_unorm(
                            bias_y,
                            range_y,
                            yuv_max_channel,
                            yuv_block[block_i][block_j].0,
                        ) as u8;
                        if image.yuv_format == PixelFormat::Yuv444 {
                            let dst_u = image.row_mut(Plane::U, j)?;
                            dst_u[i] = to_unorm(
                                bias_uv,
                                range_uv,
                                yuv_max_channel,
                                yuv_block[block_i][block_j].1,
                            ) as u8;
                            let dst_v = image.row_mut(Plane::V, j)?;
                            dst_v[i] = to_unorm(
                                bias_uv,
                                range_uv,
                                yuv_max_channel,
                                yuv_block[block_i][block_j].2,
                            ) as u8;
                        }
                    } else {
                        let dst_y = image.row16_mut(Plane::Y, j)?;
                        dst_y[i] = to_unorm(
                            bias_y,
                            range_y,
                            yuv_max_channel,
                            yuv_block[block_i][block_j].0,
                        );
                        if image.yuv_format == PixelFormat::Yuv444 {
                            let dst_u = image.row16_mut(Plane::U, j)?;
                            dst_u[i] = to_unorm(
                                bias_uv,
                                range_uv,
                                yuv_max_channel,
                                yuv_block[block_i][block_j].1,
                            );
                            let dst_v = image.row16_mut(Plane::V, j)?;
                            dst_v[i] = to_unorm(
                                bias_uv,
                                range_uv,
                                yuv_max_channel,
                                yuv_block[block_i][block_j].2,
                            );
                        }
                    }
                }
            }

            // Populate subsampled channels with average values of the 2x2 block.
            match image.yuv_format {
                PixelFormat::Yuv420 => {
                    let (avg_u, avg_v) = average_2x2(&yuv_block, block_w * block_h);
                    let uv_j = outer_j >> 1;
                    let uv_i = outer_i as usize >> 1;
                    if image.depth == 8 {
                        let dst_u = image.row_mut(Plane::U, uv_j)?;
                        dst_u[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_u) as u8;
                        let dst_v = image.row_mut(Plane::V, uv_j)?;
                        dst_v[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_v) as u8;
                    } else {
                        let dst_u = image.row16_mut(Plane::U, uv_j)?;
                        dst_u[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_u);
                        let dst_v = image.row16_mut(Plane::V, uv_j)?;
                        dst_v[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_v);
                    }
                }
                PixelFormat::Yuv422 => {
                    for block_j in 0..block_h {
                        let (avg_u, avg_v) = average_1x2(&yuv_block, block_j, block_w);
                        let uv_j = outer_j + block_j;
                        let uv_i = outer_i as usize >> 1;
                        if image.depth == 8 {
                            let dst_u = image.row_mut(Plane::U, uv_j)?;
                            dst_u[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_u) as u8;
                            let dst_v = image.row_mut(Plane::V, uv_j)?;
                            dst_v[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_v) as u8;
                        } else {
                            let dst_u = image.row16_mut(Plane::U, uv_j)?;
                            dst_u[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_u);
                            let dst_v = image.row16_mut(Plane::V, uv_j)?;
                            dst_v[uv_i] = to_unorm(bias_uv, range_uv, yuv_max_channel, avg_v);
                        }
                    }
                }
                _ => {}
            }
        }
    }
    Ok(())
}

// TODO - b/410088660: this can be a macro since it's per pixel?
fn to_unorm(bias_y: f32, range_y: f32, max_channel: u16, v: f32) -> u16 {
    clamp_i32(
        (0.5 + (v * range_y + bias_y)).floor() as i32,
        0,
        max_channel as i32,
    ) as u16
}

fn average_2x2(yuv_block: &[[YUVBlock; 3]; 3], sample_count: u32) -> (f32, f32) {
    let sum_u: f32 = yuv_block.iter().flatten().map(|pixel| pixel.1).sum();
    let sum_v: f32 = yuv_block.iter().flatten().map(|pixel| pixel.2).sum();
    (sum_u / sample_count as f32, sum_v / sample_count as f32)
}

fn average_1x2(yuv_block: &[[YUVBlock; 3]; 3], block_j: u32, block_w: u32) -> (f32, f32) {
    let mut sum_u = 0.0;
    let mut sum_v = 0.0;
    for row in yuv_block.iter().take(block_w as usize) {
        sum_u += row[block_j as usize].1;
        sum_v += row[block_j as usize].2;
    }
    (sum_u / block_w as f32, sum_v / block_w as f32)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn yuv_to_rgb() {
        fn create_420(
            matrix_coefficients: MatrixCoefficients,
            y: &[&[u8]],
            u: &[&[u8]],
            v: &[&[u8]],
        ) -> image::Image {
            let mut yuv = image::Image {
                width: y[0].len() as u32,
                height: y.len() as u32,
                depth: 8,
                yuv_format: PixelFormat::Yuv420,
                matrix_coefficients,
                yuv_range: YuvRange::Limited,
                ..Default::default()
            };
            assert!(yuv.allocate_planes(Category::Color).is_ok());
            for plane in image::YUV_PLANES {
                let samples = if plane == Plane::Y {
                    &y
                } else if plane == Plane::U {
                    &u
                } else {
                    &v
                };
                assert_eq!(yuv.height(plane), samples.len());
                for y in 0..yuv.height(plane) {
                    assert_eq!(yuv.width(plane), samples[y].len());
                    for x in 0..yuv.width(plane) {
                        yuv.row_mut(plane, y as u32).unwrap()[x] = samples[y][x];
                    }
                }
            }
            yuv
        }
        fn assert_near(yuv: &image::Image, r: &[&[u8]], g: &[&[u8]], b: &[&[u8]]) {
            let mut dst = rgb::Image::create_from_yuv(yuv);
            dst.format = rgb::Format::Rgb;
            dst.chroma_upsampling = ChromaUpsampling::Bilinear;
            assert!(dst.allocate().is_ok());
            assert!(yuv_to_rgb_any(yuv, &mut dst, AlphaMultiplyMode::NoOp).is_ok());
            assert_eq!(dst.height, r.len() as u32);
            assert_eq!(dst.height, g.len() as u32);
            assert_eq!(dst.height, b.len() as u32);
            for y in 0..dst.height {
                assert_eq!(dst.width, r[y as usize].len() as u32);
                assert_eq!(dst.width, g[y as usize].len() as u32);
                assert_eq!(dst.width, b[y as usize].len() as u32);
                for x in 0..dst.width {
                    let i = (x * dst.pixel_size()) as usize;
                    let pixel = &dst.row(y).unwrap()[i..i + 3];
                    assert_eq!(pixel[0], r[y as usize][x as usize]);
                    assert_eq!(pixel[1], g[y as usize][x as usize]);
                    assert_eq!(pixel[2], b[y as usize][x as usize]);
                }
            }
        }

        // Testing identity 4:2:0 -> RGB would be simpler to check upsampling
        // but this is not allowed (not a real use case).
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 100, 200],  //
                    &[10, 110, 210], //
                    &[50, 150, 250],
                ],
                /*u=*/
                &[
                    &[0, 100], //
                    &[10, 110],
                ],
                /*v=*/
                &[
                    &[57, 57], //
                    &[57, 57],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 101], //
                &[0, 0, 113], //
                &[0, 43, 159],
            ],
            /*g=*/
            &[
                &[89, 196, 255],  //
                &[100, 207, 255], //
                &[145, 251, 255],
            ],
            /*b=*/
            &[
                &[0, 0, 107], //
                &[0, 0, 124], //
                &[0, 0, 181],
            ],
        );

        // Extreme values.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/ &[&[0]],
                /*u=*/ &[&[0]],
                /*v=*/ &[&[0]],
            ),
            /*r=*/ &[&[0]],
            /*g=*/ &[&[136]],
            /*b=*/ &[&[0]],
        );
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/ &[&[255]],
                /*u=*/ &[&[255]],
                /*v=*/ &[&[255]],
            ),
            /*r=*/ &[&[255]],
            /*g=*/ &[&[125]],
            /*b=*/ &[&[255]],
        );

        // Top-right square "bleeds" into other samples during upsampling.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 0, 255, 255],
                    &[0, 0, 255, 255],
                    &[0, 0, 0, 0],
                    &[0, 0, 0, 0],
                ],
                /*u=*/
                &[
                    &[0, 255], //
                    &[0, 0],
                ],
                /*v=*/
                &[
                    &[0, 255], //
                    &[0, 0],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 255, 255],
                &[0, 0, 255, 255],
                &[0, 0, 0, 0],
                &[0, 0, 0, 0],
            ],
            /*g=*/
            &[
                &[136, 59, 202, 125],
                &[136, 78, 255, 202],
                &[136, 116, 78, 59],
                &[136, 136, 136, 136],
            ],
            /*b=*/
            &[
                &[0, 0, 255, 255],
                &[0, 0, 255, 255],
                &[0, 0, 0, 0],
                &[0, 0, 0, 0],
            ],
        );

        // Middle square does not "bleed" into other samples during upsampling.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 0, 0, 0],
                    &[0, 255, 255, 0],
                    &[0, 255, 255, 0],
                    &[0, 0, 0, 0],
                ],
                /*u=*/
                &[
                    &[0, 0], //
                    &[0, 0],
                ],
                /*v=*/
                &[
                    &[0, 0], //
                    &[0, 0],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 0, 0],
                &[0, 74, 74, 0],
                &[0, 74, 74, 0],
                &[0, 0, 0, 0],
            ],
            /*g=*/
            &[
                &[136, 136, 136, 136],
                &[136, 255, 255, 136],
                &[136, 255, 255, 136],
                &[136, 136, 136, 136],
            ],
            /*b=*/
            &[
                &[0, 0, 0, 0],
                &[0, 20, 20, 0],
                &[0, 20, 20, 0],
                &[0, 0, 0, 0],
            ],
        );
    }
}
