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

use crate::decoder::Category;
use crate::image::*;
use crate::internal_utils::*;
use crate::*;

use libyuv_sys::bindings::*;

use std::os::raw::c_int;

fn find_constants(image: &image::Image) -> Option<(&YuvConstants, &YuvConstants)> {
    let matrix_coefficients = if image.yuv_format == PixelFormat::Yuv400
        && image.matrix_coefficients == MatrixCoefficients::Identity
    {
        MatrixCoefficients::Bt601
    } else {
        image.matrix_coefficients
    };
    // Android MediaCodec always uses Yuv420. So use Bt601 instead of Identity in that case.
    #[cfg(feature = "android_mediacodec")]
    let matrix_coefficients = if matrix_coefficients == MatrixCoefficients::Identity {
        MatrixCoefficients::Bt601
    } else {
        matrix_coefficients
    };
    unsafe {
        match image.yuv_range {
            YuvRange::Full => match matrix_coefficients {
                MatrixCoefficients::Bt709 => Some((&kYuvF709Constants, &kYvuF709Constants)),
                MatrixCoefficients::Bt470bg
                | MatrixCoefficients::Bt601
                | MatrixCoefficients::Unspecified => Some((&kYuvJPEGConstants, &kYvuJPEGConstants)),
                MatrixCoefficients::Bt2020Ncl => Some((&kYuvV2020Constants, &kYvuV2020Constants)),
                MatrixCoefficients::ChromaDerivedNcl => match image.color_primaries {
                    ColorPrimaries::Srgb | ColorPrimaries::Unspecified => {
                        Some((&kYuvF709Constants, &kYvuF709Constants))
                    }
                    ColorPrimaries::Bt470bg | ColorPrimaries::Bt601 => {
                        Some((&kYuvJPEGConstants, &kYvuJPEGConstants))
                    }
                    ColorPrimaries::Bt2020 => Some((&kYuvV2020Constants, &kYvuV2020Constants)),
                    _ => None,
                },
                _ => None,
            },
            YuvRange::Limited => match matrix_coefficients {
                MatrixCoefficients::Bt709 => Some((&kYuvH709Constants, &kYvuH709Constants)),
                MatrixCoefficients::Bt470bg
                | MatrixCoefficients::Bt601
                | MatrixCoefficients::Unspecified => Some((&kYuvI601Constants, &kYvuI601Constants)),
                MatrixCoefficients::Bt2020Ncl => Some((&kYuv2020Constants, &kYvu2020Constants)),
                MatrixCoefficients::ChromaDerivedNcl => match image.color_primaries {
                    ColorPrimaries::Srgb | ColorPrimaries::Unspecified => {
                        Some((&kYuvH709Constants, &kYvuH709Constants))
                    }
                    ColorPrimaries::Bt470bg | ColorPrimaries::Bt601 => {
                        Some((&kYuvI601Constants, &kYvuI601Constants))
                    }
                    ColorPrimaries::Bt2020 => Some((&kYuv2020Constants, &kYvu2020Constants)),
                    _ => None,
                },
                _ => None,
            },
        }
    }
}

#[rustfmt::skip]
type YUV400ToRGBMatrix = unsafe extern "C" fn(
    *const u8, c_int, *mut u8, c_int, *const YuvConstants, c_int, c_int) -> c_int;
#[rustfmt::skip]
type YUVToRGBMatrixFilter = unsafe extern "C" fn(
    *const u8, c_int, *const u8, c_int, *const u8, c_int, *mut u8, c_int, *const YuvConstants,
    c_int, c_int, FilterMode) -> c_int;
#[rustfmt::skip]
type YUVAToRGBMatrixFilter = unsafe extern "C" fn(
    *const u8, c_int, *const u8, c_int, *const u8, c_int, *const u8, c_int, *mut u8, c_int,
    *const YuvConstants, c_int, c_int, c_int, FilterMode) -> c_int;
#[rustfmt::skip]
type YUVToRGBMatrix = unsafe extern "C" fn(
    *const u8, c_int, *const u8, c_int, *const u8, c_int, *mut u8, c_int, *const YuvConstants,
    c_int, c_int) -> c_int;
#[rustfmt::skip]
type YUVAToRGBMatrix = unsafe extern "C" fn(
    *const u8, c_int, *const u8, c_int, *const u8, c_int, *const u8, c_int, *mut u8, c_int,
    *const YuvConstants, c_int, c_int, c_int) -> c_int;
#[rustfmt::skip]
type YUVToRGBMatrixFilterHighBitDepth = unsafe extern "C" fn(
    *const u16, c_int, *const u16, c_int, *const u16, c_int, *mut u8, c_int, *const YuvConstants,
    c_int, c_int, FilterMode) -> c_int;
#[rustfmt::skip]
type YUVAToRGBMatrixFilterHighBitDepth = unsafe extern "C" fn(
    *const u16, c_int, *const u16, c_int, *const u16, c_int, *const u16, c_int, *mut u8, c_int,
    *const YuvConstants, c_int, c_int, c_int, FilterMode) -> c_int;
#[rustfmt::skip]
type YUVToRGBMatrixHighBitDepth = unsafe extern "C" fn(
    *const u16, c_int, *const u16, c_int, *const u16, c_int, *mut u8, c_int, *const YuvConstants,
    c_int, c_int) -> c_int;
#[rustfmt::skip]
type YUVAToRGBMatrixHighBitDepth = unsafe extern "C" fn(
    *const u16, c_int, *const u16, c_int, *const u16, c_int, *const u16, c_int, *mut u8, c_int,
    *const YuvConstants, c_int, c_int, c_int) -> c_int;
#[rustfmt::skip]
type P010ToRGBMatrix = unsafe extern "C" fn(
    *const u16, c_int, *const u16, c_int, *mut u8, c_int, *const YuvConstants, c_int,
    c_int) -> c_int;
#[rustfmt::skip]
type ARGBToABGR = unsafe extern "C" fn(
    *const u8, c_int, *mut u8, c_int, c_int, c_int) -> c_int;
#[rustfmt::skip]
type NVToARGBMatrix = unsafe extern "C" fn(
    *const u8, c_int, *const u8, c_int, *mut u8, c_int, *const YuvConstants, c_int,
    c_int) -> c_int;

#[derive(Debug)]
enum ConversionFunction {
    YUV400ToRGBMatrix(YUV400ToRGBMatrix),
    YUVToRGBMatrixFilter(YUVToRGBMatrixFilter),
    YUVAToRGBMatrixFilter(YUVAToRGBMatrixFilter),
    YUVToRGBMatrix(YUVToRGBMatrix),
    YUVAToRGBMatrix(YUVAToRGBMatrix),
    YUVToRGBMatrixFilterHighBitDepth(YUVToRGBMatrixFilterHighBitDepth),
    YUVAToRGBMatrixFilterHighBitDepth(YUVAToRGBMatrixFilterHighBitDepth),
    YUVToRGBMatrixHighBitDepth(YUVToRGBMatrixHighBitDepth),
    YUVAToRGBMatrixHighBitDepth(YUVAToRGBMatrixHighBitDepth),
    P010ToRGBMatrix(P010ToRGBMatrix, ARGBToABGR),
    NVToARGBMatrix(NVToARGBMatrix),
}

impl ConversionFunction {
    fn is_yuva(&self) -> bool {
        matches!(
            self,
            ConversionFunction::YUVAToRGBMatrixFilter(_)
                | ConversionFunction::YUVAToRGBMatrix(_)
                | ConversionFunction::YUVAToRGBMatrixFilterHighBitDepth(_)
                | ConversionFunction::YUVAToRGBMatrixHighBitDepth(_)
        )
    }
}

fn find_conversion_function(
    yuv_format: PixelFormat,
    yuv_depth: u8,
    rgb: &rgb::Image,
    alpha_preferred: bool,
) -> Option<ConversionFunction> {
    match (alpha_preferred, yuv_depth, rgb.format, yuv_format) {
        (_, 8, Format::Rgba, PixelFormat::AndroidNv12) => {
            // What Android considers to be NV12 is actually NV21 in libyuv.
            Some(ConversionFunction::NVToARGBMatrix(NV21ToARGBMatrix))
        }
        (_, 8, Format::Rgba, PixelFormat::AndroidNv21) => {
            // What Android considers to be NV21 is actually NV12 in libyuv.
            Some(ConversionFunction::NVToARGBMatrix(NV12ToARGBMatrix))
        }
        (_, 10, Format::Rgba1010102, PixelFormat::AndroidP010) => Some(
            ConversionFunction::P010ToRGBMatrix(P010ToAR30Matrix, AR30ToAB30),
        ),
        (_, 10, Format::Rgba, PixelFormat::AndroidP010) => Some(
            ConversionFunction::P010ToRGBMatrix(P010ToARGBMatrix, ARGBToABGR),
        ),
        (true, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixFilterHighBitDepth(
                I210AlphaToARGBMatrixFilter,
            ))
        }
        (true, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixFilterHighBitDepth(
                I010AlphaToARGBMatrixFilter,
            ))
        }
        (_, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilterHighBitDepth(
                I210ToARGBMatrixFilter,
            ))
        }
        (_, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilterHighBitDepth(
                I010ToARGBMatrixFilter,
            ))
        }

        (true, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv444) => Some(
            ConversionFunction::YUVAToRGBMatrixHighBitDepth(I410AlphaToARGBMatrix),
        ),
        (true, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixHighBitDepth(
                I210AlphaToARGBMatrix,
            ))
        }
        (true, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixHighBitDepth(
                I010AlphaToARGBMatrix,
            ))
        }
        (_, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv444) => Some(
            ConversionFunction::YUVToRGBMatrixHighBitDepth(I410ToARGBMatrix),
        ),
        (_, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixHighBitDepth(
                I210ToARGBMatrix,
            ))
        }
        (_, 10, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixHighBitDepth(
                I010ToARGBMatrix,
            ))
        }
        (_, 12, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixHighBitDepth(
                I012ToARGBMatrix,
            ))
        }

        // The fall through here is intentional. If a high bitdepth function was not found, try to
        // see if we can use a low bitdepth function with a downshift.
        //
        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv400) => {
            Some(ConversionFunction::YUV400ToRGBMatrix(I400ToARGBMatrix))
        }

        (true, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixFilter(
                I422AlphaToARGBMatrixFilter,
            ))
        }
        (true, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrixFilter(
                I420AlphaToARGBMatrixFilter,
            ))
        }

        (_, _, Format::Rgb | Format::Bgr, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilter(
                I422ToRGB24MatrixFilter,
            ))
        }
        (_, _, Format::Rgb | Format::Bgr, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilter(
                I420ToRGB24MatrixFilter,
            ))
        }
        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilter(
                I422ToARGBMatrixFilter,
            ))
        }
        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrixFilter(
                I420ToARGBMatrixFilter,
            ))
        }

        (true, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv444) => {
            Some(ConversionFunction::YUVAToRGBMatrix(I444AlphaToARGBMatrix))
        }
        (true, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrix(I422AlphaToARGBMatrix))
        }
        (true, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVAToRGBMatrix(I420AlphaToARGBMatrix))
        }

        (_, _, Format::Rgb | Format::Bgr, PixelFormat::Yuv444) => {
            Some(ConversionFunction::YUVToRGBMatrix(I444ToRGB24Matrix))
        }
        (_, _, Format::Rgb | Format::Bgr, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I420ToRGB24Matrix))
        }

        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv444) => {
            Some(ConversionFunction::YUVToRGBMatrix(I444ToARGBMatrix))
        }
        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I422ToARGBMatrix))
        }
        (_, _, Format::Rgba | Format::Bgra, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I420ToARGBMatrix))
        }

        (_, _, Format::Argb | Format::Abgr, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I422ToRGBAMatrix))
        }
        (_, _, Format::Argb | Format::Abgr, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I420ToRGBAMatrix))
        }

        (_, _, Format::Rgb565, PixelFormat::Yuv422)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I422ToRGB565Matrix))
        }
        (_, _, Format::Rgb565, PixelFormat::Yuv420)
            if rgb.chroma_upsampling.nearest_neighbor_filter_allowed() =>
        {
            Some(ConversionFunction::YUVToRGBMatrix(I420ToRGB565Matrix))
        }

        _ => None,
    }
}

pub fn yuv_to_rgb(image: &image::Image, rgb: &mut rgb::Image) -> AvifResult<bool> {
    if (rgb.depth != 8 && rgb.depth != 10)
        || (image.depth != 8 && image.depth != 10 && image.depth != 12)
    {
        return Err(AvifError::NotImplemented);
    }
    if rgb.depth == 10
        && (image.yuv_format != PixelFormat::AndroidP010 || rgb.format != Format::Rgba1010102)
    {
        return Err(AvifError::NotImplemented);
    }

    let (matrix_yuv, matrix_yvu) = find_constants(image).ok_or(AvifError::NotImplemented)?;
    let alpha_preferred = rgb.has_alpha() && image.has_alpha();
    let conversion_function =
        find_conversion_function(image.yuv_format, image.depth, rgb, alpha_preferred)
            .ok_or(AvifError::NotImplemented)?;
    let is_yvu = matches!(rgb.format, Format::Rgb | Format::Rgba | Format::Argb);
    let matrix = if is_yvu { matrix_yvu } else { matrix_yuv };
    let u_plane_index: usize = if is_yvu { 2 } else { 1 };
    let v_plane_index: usize = if is_yvu { 1 } else { 2 };
    let filter = if rgb.chroma_upsampling.bilinear_or_better_filter_allowed() {
        FilterMode_kFilterBilinear
    } else {
        FilterMode_kFilterNone
    };
    let mut plane_u8: [*const u8; 4] = ALL_PLANES
        .iter()
        .map(|x| {
            if image.has_plane(*x) {
                image.planes[x.to_usize()].unwrap_ref().ptr()
            } else {
                std::ptr::null()
            }
        })
        .collect::<Vec<*const u8>>()
        .try_into()
        .unwrap();
    let plane_u16: [*const u16; 4] = ALL_PLANES
        .iter()
        .map(|x| {
            if image.has_plane(*x) {
                image.planes[x.to_usize()].unwrap_ref().ptr16()
            } else {
                std::ptr::null()
            }
        })
        .collect::<Vec<*const u16>>()
        .try_into()
        .unwrap();
    let mut plane_row_bytes: [i32; 4] = ALL_PLANES
        .iter()
        .map(|x| {
            if image.has_plane(*x) {
                i32_from_u32(image.plane_data(*x).unwrap().row_bytes).unwrap_or_default()
            } else {
                0
            }
        })
        .collect::<Vec<i32>>()
        .try_into()
        .unwrap();
    let rgb_row_bytes = i32_from_u32(rgb.row_bytes)?;
    let width = i32_from_u32(image.width)?;
    let height = i32_from_u32(image.height)?;
    let mut result: c_int;
    unsafe {
        let mut high_bd_matched = true;
        // Apply one of the high bitdepth functions if possible.
        result = match conversion_function {
            ConversionFunction::P010ToRGBMatrix(func1, func2) => {
                let result = func1(
                    plane_u16[0],
                    plane_row_bytes[0] / 2,
                    plane_u16[1],
                    plane_row_bytes[1] / 2,
                    rgb.pixels(),
                    rgb_row_bytes,
                    matrix,
                    width,
                    height,
                );
                if result == 0 {
                    // It is okay to use the same pointer as source and destination for this
                    // conversion.
                    func2(
                        rgb.pixels(),
                        rgb_row_bytes,
                        rgb.pixels(),
                        rgb_row_bytes,
                        width,
                        height,
                    )
                } else {
                    result
                }
            }
            ConversionFunction::YUVToRGBMatrixFilterHighBitDepth(func) => func(
                plane_u16[0],
                plane_row_bytes[0] / 2,
                plane_u16[u_plane_index],
                plane_row_bytes[u_plane_index] / 2,
                plane_u16[v_plane_index],
                plane_row_bytes[v_plane_index] / 2,
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                filter,
            ),
            ConversionFunction::YUVAToRGBMatrixFilterHighBitDepth(func) => func(
                plane_u16[0],
                plane_row_bytes[0] / 2,
                plane_u16[u_plane_index],
                plane_row_bytes[u_plane_index] / 2,
                plane_u16[v_plane_index],
                plane_row_bytes[v_plane_index] / 2,
                plane_u16[3],
                plane_row_bytes[3] / 2,
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                0, // attenuate
                filter,
            ),
            ConversionFunction::YUVToRGBMatrixHighBitDepth(func) => func(
                plane_u16[0],
                plane_row_bytes[0] / 2,
                plane_u16[u_plane_index],
                plane_row_bytes[u_plane_index] / 2,
                plane_u16[v_plane_index],
                plane_row_bytes[v_plane_index] / 2,
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
            ),
            ConversionFunction::YUVAToRGBMatrixHighBitDepth(func) => func(
                plane_u16[0],
                plane_row_bytes[0] / 2,
                plane_u16[u_plane_index],
                plane_row_bytes[u_plane_index] / 2,
                plane_u16[v_plane_index],
                plane_row_bytes[v_plane_index] / 2,
                plane_u16[3],
                plane_row_bytes[3] / 2,
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                0, // attenuate
            ),
            _ => {
                high_bd_matched = false;
                -1
            }
        };
        if high_bd_matched {
            return if result == 0 {
                Ok(!image.has_alpha() || conversion_function.is_yuva())
            } else {
                Err(AvifError::ReformatFailed)
            };
        }
        let mut image8 = image::Image::default();
        if image.depth > 8 {
            downshift_to_8bit(image, &mut image8, conversion_function.is_yuva())?;
            plane_u8 = ALL_PLANES
                .iter()
                .map(|x| {
                    if image8.has_plane(*x) {
                        image8.planes[x.to_usize()].unwrap_ref().ptr()
                    } else {
                        std::ptr::null()
                    }
                })
                .collect::<Vec<*const u8>>()
                .try_into()
                .unwrap();
            plane_row_bytes = ALL_PLANES
                .iter()
                .map(|x| {
                    if image8.has_plane(*x) {
                        i32_from_u32(image8.plane_data(*x).unwrap().row_bytes).unwrap_or_default()
                    } else {
                        0
                    }
                })
                .collect::<Vec<i32>>()
                .try_into()
                .unwrap();
        }
        result = match conversion_function {
            ConversionFunction::NVToARGBMatrix(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                plane_u8[1],
                plane_row_bytes[1],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
            ),
            ConversionFunction::YUV400ToRGBMatrix(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
            ),
            ConversionFunction::YUVToRGBMatrixFilter(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                plane_u8[u_plane_index],
                plane_row_bytes[u_plane_index],
                plane_u8[v_plane_index],
                plane_row_bytes[v_plane_index],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                filter,
            ),
            ConversionFunction::YUVAToRGBMatrixFilter(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                plane_u8[u_plane_index],
                plane_row_bytes[u_plane_index],
                plane_u8[v_plane_index],
                plane_row_bytes[v_plane_index],
                plane_u8[3],
                plane_row_bytes[3],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                0, // attenuate
                filter,
            ),
            ConversionFunction::YUVToRGBMatrix(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                plane_u8[u_plane_index],
                plane_row_bytes[u_plane_index],
                plane_u8[v_plane_index],
                plane_row_bytes[v_plane_index],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
            ),
            ConversionFunction::YUVAToRGBMatrix(func) => func(
                plane_u8[0],
                plane_row_bytes[0],
                plane_u8[u_plane_index],
                plane_row_bytes[u_plane_index],
                plane_u8[v_plane_index],
                plane_row_bytes[v_plane_index],
                plane_u8[3],
                plane_row_bytes[3],
                rgb.pixels(),
                rgb_row_bytes,
                matrix,
                width,
                height,
                0, // attenuate
            ),
            _ => 0,
        };
    }
    if result == 0 {
        Ok(!image.has_alpha() || conversion_function.is_yuva())
    } else {
        Err(AvifError::ReformatFailed)
    }
}

fn downshift_to_8bit(
    image: &image::Image,
    image8: &mut image::Image,
    alpha: bool,
) -> AvifResult<()> {
    image8.width = image.width;
    image8.height = image.height;
    image8.depth = 8;
    image8.yuv_format = image.yuv_format;
    image8.allocate_planes(Category::Color)?;
    if alpha {
        image8.allocate_planes(Category::Alpha)?;
    }
    let scale = 1 << (24 - image.depth);
    for plane in ALL_PLANES {
        if plane == Plane::A && !alpha {
            continue;
        }
        let pd = image.plane_data(plane);
        if pd.is_none() {
            continue;
        }
        let pd = pd.unwrap();
        if pd.width == 0 {
            continue;
        }
        let source_ptr = image.planes[plane.to_usize()].unwrap_ref().ptr16();
        let pd8 = image8.plane_data(plane).unwrap();
        let dst_ptr = image8.planes[plane.to_usize()].unwrap_mut().ptr_mut();
        unsafe {
            Convert16To8Plane(
                source_ptr,
                i32_from_u32(pd.row_bytes / 2)?,
                dst_ptr,
                i32_from_u32(pd8.row_bytes)?,
                scale,
                i32_from_u32(pd.width)?,
                i32_from_u32(pd.height)?,
            );
        }
    }
    Ok(())
}

pub fn process_alpha(rgb: &mut rgb::Image, multiply: bool) -> AvifResult<()> {
    if rgb.depth != 8 {
        return Err(AvifError::NotImplemented);
    }
    match rgb.format {
        Format::Rgba | Format::Bgra => {}
        _ => return Err(AvifError::NotImplemented),
    }
    let result = unsafe {
        if multiply {
            ARGBAttenuate(
                rgb.pixels(),
                i32_from_u32(rgb.row_bytes)?,
                rgb.pixels(),
                i32_from_u32(rgb.row_bytes)?,
                i32_from_u32(rgb.width)?,
                i32_from_u32(rgb.height)?,
            )
        } else {
            ARGBUnattenuate(
                rgb.pixels(),
                i32_from_u32(rgb.row_bytes)?,
                rgb.pixels(),
                i32_from_u32(rgb.row_bytes)?,
                i32_from_u32(rgb.width)?,
                i32_from_u32(rgb.height)?,
            )
        }
    };
    if result == 0 {
        Ok(())
    } else {
        Err(AvifError::ReformatFailed)
    }
}

pub fn convert_to_half_float(rgb: &mut rgb::Image, scale: f32) -> AvifResult<()> {
    let res = unsafe {
        HalfFloatPlane(
            rgb.pixels() as *const u16,
            i32_from_u32(rgb.row_bytes)?,
            rgb.pixels() as *mut u16,
            i32_from_u32(rgb.row_bytes)?,
            scale,
            i32_from_u32(rgb.width * rgb.channel_count())?,
            i32_from_u32(rgb.height)?,
        )
    };
    if res == 0 {
        Ok(())
    } else {
        Err(AvifError::InvalidArgument)
    }
}
