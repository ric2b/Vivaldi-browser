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

use crate::image::*;
use crate::internal_utils::*;
use crate::reformat::rgb;
use crate::utils::pixels::*;
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
            pixels: rgb.pixels_mut(),
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

#[no_mangle]
pub unsafe extern "C" fn crabby_avifRGBImageSetDefaults(
    rgb: *mut avifRGBImage,
    image: *const avifImage,
) {
    let image: image::Image = deref_const!(image).into();
    *deref_mut!(rgb) = rgb::Image::create_from_yuv(&image).into();
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageYUVToRGB(
    image: *const avifImage,
    rgb: *mut avifRGBImage,
) -> avifResult {
    if deref_const!(image).yuvPlanes[0].is_null() {
        return avifResult::Ok;
    }
    let mut rgb: rgb::Image = deref_const!(rgb).into();
    let image: image::Image = deref_const!(image).into();
    rgb.convert_from_yuv(&image).into()
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
                        dst_planes[plane.as_usize()]
                            .offset(isize_from_u32(y * dst_row_bytes[plane.as_usize()])?),
                        usize_from_u32(plane_data.width)?,
                    )
                };
                dst_slice.copy_from_slice(src_slice);
            }
        } else {
            // When scaling a P010 image, the scaling code converts the image into Yuv420 with
            // an explicit V plane. So if the V plane is missing in |dst|, we will have to allocate
            // it here. It is safe to do so since it will be free'd with the other plane buffers
            // when the image object is destroyed.
            if plane == Plane::V && dst.yuvPlanes[2].is_null() {
                let plane_size = usize_from_u32(plane_data.width * plane_data.height * 2)?;
                dst.yuvPlanes[2] = unsafe { crabby_avifAlloc(plane_size) } as *mut _;
                if dst.yuvPlanes[2].is_null() {
                    return Err(AvifError::OutOfMemory);
                }
                dst.yuvRowBytes[2] = plane_data.width * 2;
            }
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
                        dst_planes[plane.as_usize()]
                            .offset(isize_from_u32(y * dst_row_bytes[plane.as_usize()])?),
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
    let dst_image = deref_mut!(image);
    if dstWidth > dst_image.width || dstHeight > dst_image.height {
        // To avoid buffer reallocations, we only support scaling to a smaller size.
        return avifResult::NotImplemented;
    }
    if dstWidth == dst_image.width && dstHeight == dst_image.height {
        return avifResult::Ok;
    }

    let mut rust_image: image::Image = deref_const!(image).into();
    let res = rust_image.scale(dstWidth, dstHeight, Category::Color);
    if res.is_err() {
        return res.into();
    }
    // The scale function is designed to work only for one category at a time.
    // Restore the width and height to the original values before scaling the
    // alpha plane.
    rust_image.width = deref_const!(image).width;
    rust_image.height = deref_const!(image).height;
    let res = rust_image.scale(dstWidth, dstHeight, Category::Alpha);
    if res.is_err() {
        return res.into();
    }

    dst_image.width = rust_image.width;
    dst_image.height = rust_image.height;
    dst_image.depth = rust_image.depth as _;
    dst_image.yuvFormat = rust_image.yuv_format;
    CopyPlanes(dst_image, &rust_image).into()
}
