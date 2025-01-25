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

use super::image::*;
use super::types::*;

use crate::decoder::Category;
use crate::image::*;
use crate::internal_utils::pixels::*;
use crate::internal_utils::*;
use crate::reformat::rgb;
use crate::*;

/// cbindgen:rename-all=CamelCase
#[repr(C)]
pub struct avifRGBImage {
    pub width: u32,
    pub height: u32,
    pub depth: u32,
    pub format: rgb::Format,
    pub chroma_upsampling: rgb::ChromaUpsampling,
    pub chroma_downsampling: rgb::ChromaDownsampling,
    pub ignore_alpha: bool,
    pub alpha_premultiplied: bool,
    pub is_float: bool,
    pub max_threads: i32,
    pub pixels: *mut u8,
    pub row_bytes: u32,
}

impl From<rgb::Image> for avifRGBImage {
    fn from(mut rgb: rgb::Image) -> avifRGBImage {
        avifRGBImage {
            width: rgb.width,
            height: rgb.height,
            depth: rgb.depth as u32,
            format: rgb.format,
            chroma_upsampling: rgb.chroma_upsampling,
            chroma_downsampling: rgb.chroma_downsampling,
            ignore_alpha: false,
            alpha_premultiplied: rgb.premultiply_alpha,
            is_float: rgb.is_float,
            max_threads: rgb.max_threads,
            pixels: rgb.pixels(),
            row_bytes: rgb.row_bytes,
        }
    }
}

impl From<&avifRGBImage> for rgb::Image {
    fn from(rgb: &avifRGBImage) -> rgb::Image {
        let dst = rgb::Image {
            width: rgb.width,
            height: rgb.height,
            depth: rgb.depth as u8,
            format: rgb.format,
            chroma_upsampling: rgb.chroma_upsampling,
            chroma_downsampling: rgb.chroma_downsampling,
            premultiply_alpha: rgb.alpha_premultiplied,
            is_float: rgb.is_float,
            max_threads: rgb.max_threads,
            pixels: Pixels::from_raw_pointer(rgb.pixels, rgb.depth, rgb.height, rgb.row_bytes).ok(),
            row_bytes: rgb.row_bytes,
        };
        let format = match (rgb.format, rgb.ignore_alpha) {
            (rgb::Format::Rgb, _) => rgb::Format::Rgb,
            (rgb::Format::Rgba, true) => rgb::Format::Rgb,
            (rgb::Format::Rgba, false) => rgb::Format::Rgba,
            (rgb::Format::Argb, true) => rgb::Format::Rgb,
            (rgb::Format::Argb, false) => rgb::Format::Argb,
            (rgb::Format::Bgr, _) => rgb::Format::Bgr,
            (rgb::Format::Bgra, true) => rgb::Format::Bgr,
            (rgb::Format::Bgra, false) => rgb::Format::Bgra,
            (rgb::Format::Abgr, true) => rgb::Format::Bgr,
            (rgb::Format::Abgr, false) => rgb::Format::Abgr,
            (rgb::Format::Rgb565, _) => rgb::Format::Rgb565,
            (rgb::Format::Rgba1010102, _) => rgb::Format::Rgba1010102,
        };
        dst.shuffle_channels_to(format).unwrap()
    }
}

impl From<&avifImage> for image::Image {
    // Only copies fields necessary for reformatting.
    fn from(image: &avifImage) -> image::Image {
        image::Image {
            width: image.width,
            height: image.height,
            depth: image.depth as u8,
            yuv_format: image.yuvFormat,
            yuv_range: image.yuvRange,
            alpha_present: !image.alphaPlane.is_null(),
            alpha_premultiplied: image.alphaPremultiplied == AVIF_TRUE,
            planes: [
                Pixels::from_raw_pointer(
                    image.yuvPlanes[0],
                    image.depth,
                    image.height,
                    image.yuvRowBytes[0],
                )
                .ok(),
                Pixels::from_raw_pointer(
                    image.yuvPlanes[1],
                    image.depth,
                    image.height,
                    image.yuvRowBytes[1],
                )
                .ok(),
                Pixels::from_raw_pointer(
                    image.yuvPlanes[2],
                    image.depth,
                    image.height,
                    image.yuvRowBytes[2],
                )
                .ok(),
                Pixels::from_raw_pointer(
                    image.alphaPlane,
                    image.depth,
                    image.height,
                    image.alphaRowBytes,
                )
                .ok(),
            ],
            row_bytes: [
                image.yuvRowBytes[0],
                image.yuvRowBytes[1],
                image.yuvRowBytes[2],
                image.alphaRowBytes,
            ],
            color_primaries: image.colorPrimaries,
            transfer_characteristics: image.transferCharacteristics,
            matrix_coefficients: image.matrixCoefficients,
            ..Default::default()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifRGBImageSetDefaults(
    rgb: *mut avifRGBImage,
    image: *const avifImage,
) {
    let rgb = unsafe { &mut (*rgb) };
    let image: image::Image = unsafe { &(*image) }.into();
    *rgb = rgb::Image::create_from_yuv(&image).into();
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageYUVToRGB(
    image: *const avifImage,
    rgb: *mut avifRGBImage,
) -> avifResult {
    unsafe {
        if (*image).yuvPlanes[0].is_null() {
            return avifResult::Ok;
        }
    }
    let mut rgb: rgb::Image = unsafe { &(*rgb) }.into();
    let image: image::Image = unsafe { &(*image) }.into();
    to_avifResult(&rgb.convert_from_yuv(&image))
}

fn CopyPlanes(dst: &mut avifImage, src: &Image) -> AvifResult<()> {
    for plane in ALL_PLANES {
        if !src.has_plane(plane) {
            continue;
        }
        let plane_data = src.plane_data(plane).unwrap();
        if src.depth == 8 {
            let dst_planes = [
                dst.yuvPlanes[0],
                dst.yuvPlanes[1],
                dst.yuvPlanes[2],
                dst.alphaPlane,
            ];
            let dst_row_bytes = [
                dst.yuvRowBytes[0],
                dst.yuvRowBytes[1],
                dst.yuvRowBytes[2],
                dst.alphaRowBytes,
            ];
            for y in 0..plane_data.height {
                let src_slice = &src.row(plane, y).unwrap()[..plane_data.width as usize];
                let dst_slice = unsafe {
                    std::slice::from_raw_parts_mut(
                        dst_planes[plane.to_usize()]
                            .offset(isize_from_u32(y * dst_row_bytes[plane.to_usize()])?),
                        usize_from_u32(plane_data.width)?,
                    )
                };
                dst_slice.copy_from_slice(src_slice);
            }
        } else {
            let dst_planes = [
                dst.yuvPlanes[0] as *mut u16,
                dst.yuvPlanes[1] as *mut u16,
                dst.yuvPlanes[2] as *mut u16,
                dst.alphaPlane as *mut u16,
            ];
            let dst_row_bytes = [
                dst.yuvRowBytes[0] / 2,
                dst.yuvRowBytes[1] / 2,
                dst.yuvRowBytes[2] / 2,
                dst.alphaRowBytes / 2,
            ];
            for y in 0..plane_data.height {
                let src_slice = &src.row16(plane, y).unwrap()[..plane_data.width as usize];
                let dst_slice = unsafe {
                    std::slice::from_raw_parts_mut(
                        dst_planes[plane.to_usize()]
                            .offset(isize_from_u32(y * dst_row_bytes[plane.to_usize()])?),
                        usize_from_u32(plane_data.width)?,
                    )
                };
                dst_slice.copy_from_slice(src_slice);
            }
        }
    }
    Ok(())
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageScale(
    image: *mut avifImage,
    dstWidth: u32,
    dstHeight: u32,
    _diag: *mut avifDiagnostics,
) -> avifResult {
    // To avoid buffer reallocations, we only support scaling to a smaller size.
    let dst_image = unsafe { &mut (*image) };
    if dstWidth > dst_image.width || dstHeight > dst_image.height {
        return avifResult::NotImplemented;
    }

    let mut rust_image: image::Image = unsafe { &(*image) }.into();
    let res = rust_image.scale(dstWidth, dstHeight, Category::Color);
    if res.is_err() {
        return to_avifResult(&res);
    }
    let res = rust_image.scale(dstWidth, dstHeight, Category::Alpha);
    if res.is_err() {
        return to_avifResult(&res);
    }

    dst_image.width = rust_image.width;
    dst_image.height = rust_image.height;
    to_avifResult(&CopyPlanes(dst_image, &rust_image))
}
