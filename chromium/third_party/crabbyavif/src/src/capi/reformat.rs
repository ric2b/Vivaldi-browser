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

use crate::image;
use crate::internal_utils::pixels::*;
use crate::reformat::rgb;

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

impl From<*mut avifRGBImage> for rgb::Image {
    fn from(rgb: *mut avifRGBImage) -> rgb::Image {
        let rgb = unsafe { &(*rgb) };
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
            pixels: Some(Pixels::from_raw_pointer(rgb.pixels, rgb.depth)),
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
        };
        dst.shuffle_channels_to(format).unwrap()
    }
}

impl From<*const avifImage> for image::Image {
    // Only copies fields necessary for reformatting.
    fn from(image: *const avifImage) -> image::Image {
        let image = unsafe { &(*image) };
        image::Image {
            width: image.width,
            height: image.height,
            depth: image.depth as u8,
            yuv_format: image.yuvFormat.into(),
            yuv_range: image.yuvRange,
            alpha_present: !image.alphaPlane.is_null(),
            alpha_premultiplied: image.alphaPremultiplied == AVIF_TRUE,
            planes: [
                Some(Pixels::from_raw_pointer(image.yuvPlanes[0], image.depth)),
                Some(Pixels::from_raw_pointer(image.yuvPlanes[1], image.depth)),
                Some(Pixels::from_raw_pointer(image.yuvPlanes[2], image.depth)),
                Some(Pixels::from_raw_pointer(image.alphaPlane, image.depth)),
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
    let image: image::Image = image.into();
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
    let mut rgb: rgb::Image = rgb.into();
    let image: image::Image = image.into();
    to_avifResult(&rgb.convert_from_yuv(&image))
}
