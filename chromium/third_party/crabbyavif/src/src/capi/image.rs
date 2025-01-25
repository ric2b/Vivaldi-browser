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

use super::gainmap::*;
use super::io::*;
use super::types::*;

use crate::image::*;
use crate::internal_utils::*;
use crate::parser::mp4box::*;
use crate::utils::clap::*;
use crate::*;

use std::os::raw::c_int;
use std::os::raw::c_void;

pub type avifPixelAspectRatioBox = PixelAspectRatio;

/// cbindgen:rename-all=CamelCase
#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct avifCleanApertureBox {
    // The u32 members below are actually i32 values, see
    // https://github.com/AOMediaCodec/libavif/pull/1749#discussion_r1391583768.
    pub width_n: u32,
    pub width_d: u32,
    pub height_n: u32,
    pub height_d: u32,
    pub horiz_off_n: u32,
    pub horiz_off_d: u32,
    pub vert_off_n: u32,
    pub vert_off_d: u32,
}

impl From<&Option<CleanAperture>> for avifCleanApertureBox {
    fn from(clap_op: &Option<CleanAperture>) -> Self {
        match clap_op {
            Some(clap) => Self {
                width_n: clap.width.0,
                width_d: clap.width.1,
                height_n: clap.height.0,
                height_d: clap.height.1,
                horiz_off_n: clap.horiz_off.0,
                horiz_off_d: clap.horiz_off.1,
                vert_off_n: clap.vert_off.0,
                vert_off_d: clap.vert_off.1,
            },
            None => Self::default(),
        }
    }
}

impl From<&avifCleanApertureBox> for CleanAperture {
    fn from(clap: &avifCleanApertureBox) -> Self {
        Self {
            width: UFraction(clap.width_n, clap.width_d),
            height: UFraction(clap.height_n, clap.height_d),
            horiz_off: UFraction(clap.horiz_off_n, clap.horiz_off_d),
            vert_off: UFraction(clap.vert_off_n, clap.vert_off_d),
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct avifImageRotation {
    pub angle: u8,
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct avifImageMirror {
    pub axis: u8,
}

#[derive(Clone, Debug)]
#[repr(C)]
pub struct avifImage {
    pub width: u32,
    pub height: u32,
    pub depth: u32,

    pub yuvFormat: PixelFormat,
    pub yuvRange: YuvRange,
    pub yuvChromaSamplePosition: ChromaSamplePosition,
    pub yuvPlanes: [*mut u8; AVIF_PLANE_COUNT_YUV],
    pub yuvRowBytes: [u32; AVIF_PLANE_COUNT_YUV],
    pub imageOwnsYUVPlanes: avifBool,

    pub alphaPlane: *mut u8,
    pub alphaRowBytes: u32,
    pub imageOwnsAlphaPlane: avifBool,
    pub alphaPremultiplied: avifBool,

    pub icc: avifRWData,
    pub colorPrimaries: ColorPrimaries,
    pub transferCharacteristics: TransferCharacteristics,
    pub matrixCoefficients: MatrixCoefficients,

    pub clli: avifContentLightLevelInformationBox,
    pub transformFlags: avifTransformFlags,
    pub pasp: avifPixelAspectRatioBox,
    pub clap: avifCleanApertureBox,
    pub irot: avifImageRotation,
    pub imir: avifImageMirror,

    pub exif: avifRWData,
    pub xmp: avifRWData,
    pub gainMap: *mut avifGainMap,
}

impl Default for avifImage {
    fn default() -> Self {
        avifImage {
            width: 0,
            height: 0,
            depth: 0,
            yuvFormat: Default::default(),
            yuvRange: YuvRange::Full,
            yuvChromaSamplePosition: Default::default(),
            yuvPlanes: [std::ptr::null_mut(); 3],
            yuvRowBytes: [0; 3],
            imageOwnsYUVPlanes: AVIF_FALSE,
            alphaPlane: std::ptr::null_mut(),
            alphaRowBytes: 0,
            imageOwnsAlphaPlane: AVIF_FALSE,
            alphaPremultiplied: AVIF_FALSE,
            icc: Default::default(),
            colorPrimaries: Default::default(),
            transferCharacteristics: Default::default(),
            matrixCoefficients: Default::default(),
            clli: Default::default(),
            transformFlags: AVIF_TRANSFORM_NONE,
            pasp: Default::default(),
            clap: Default::default(),
            irot: Default::default(),
            imir: Default::default(),
            exif: Default::default(),
            xmp: Default::default(),
            gainMap: std::ptr::null_mut(),
        }
    }
}

impl From<&Image> for avifImage {
    fn from(image: &Image) -> Self {
        let mut dst_image: avifImage = avifImage {
            width: image.width,
            height: image.height,
            depth: image.depth as u32,
            yuvFormat: image.yuv_format,
            yuvRange: image.yuv_range,
            yuvChromaSamplePosition: image.chroma_sample_position,
            alphaPremultiplied: image.alpha_premultiplied as avifBool,
            icc: (&image.icc).into(),
            colorPrimaries: image.color_primaries,
            transferCharacteristics: image.transfer_characteristics,
            matrixCoefficients: image.matrix_coefficients,
            clli: image.clli.unwrap_or_default(),
            transformFlags: {
                let mut flags = 0;
                if image.pasp.is_some() {
                    flags |= AVIF_TRANSFORM_PASP;
                }
                if image.clap.is_some() {
                    flags |= AVIF_TRANSFORM_CLAP;
                }
                if image.irot_angle.is_some() {
                    flags |= AVIF_TRANSFORM_IROT;
                }
                if image.imir_axis.is_some() {
                    flags |= AVIF_TRANSFORM_IMIR;
                }
                flags
            },
            pasp: image.pasp.unwrap_or_default(),
            clap: (&image.clap).into(),
            irot: avifImageRotation {
                angle: image.irot_angle.unwrap_or_default(),
            },
            imir: avifImageMirror {
                axis: image.imir_axis.unwrap_or_default(),
            },
            exif: (&image.exif).into(),
            xmp: (&image.xmp).into(),
            ..Self::default()
        };
        for i in 0usize..3 {
            if !image.has_plane(i.into()) {
                continue;
            }
            dst_image.yuvPlanes[i] = if image.depth > 8 {
                image.planes[i].unwrap_ref().ptr16() as *mut u8
            } else {
                image.planes[i].unwrap_ref().ptr() as *mut u8
            };
            dst_image.yuvRowBytes[i] = image.row_bytes[i];
        }
        if image.has_plane(Plane::A) {
            dst_image.alphaPlane = if image.depth > 8 {
                image.planes[3].unwrap_ref().ptr16() as *mut u8
            } else {
                image.planes[3].unwrap_ref().ptr() as *mut u8
            };
            dst_image.alphaRowBytes = image.row_bytes[3];
        }
        dst_image
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageCreateEmpty() -> *mut avifImage {
    Box::into_raw(Box::<avifImage>::default())
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageCreate(
    width: u32,
    height: u32,
    depth: u32,
    yuvFormat: PixelFormat,
) -> *mut avifImage {
    Box::into_raw(Box::new(avifImage {
        width,
        height,
        depth,
        yuvFormat,
        ..avifImage::default()
    }))
}

fn avif_image_allocate_planes_helper(
    image: &mut avifImage,
    planes: avifPlanesFlags,
) -> AvifResult<()> {
    if image.width == 0 || image.height == 0 {
        return Err(AvifError::InvalidArgument);
    }
    let channel_size = if image.depth == 8 { 1 } else { 2 };
    let y_row_bytes = usize_from_u32(image.width * channel_size)?;
    let y_size = y_row_bytes
        .checked_mul(usize_from_u32(image.height)?)
        .ok_or(avifResult::InvalidArgument)?;
    if (planes & 1) != 0 && image.yuvFormat != PixelFormat::None {
        image.imageOwnsYUVPlanes = AVIF_TRUE;
        if image.yuvPlanes[0].is_null() {
            image.yuvRowBytes[0] = u32_from_usize(y_row_bytes)?;
            image.yuvPlanes[0] = unsafe { crabby_avifAlloc(y_size) as *mut u8 };
        }
        if !image.yuvFormat.is_monochrome() {
            let csx = image.yuvFormat.chroma_shift_x() as u64;
            let width = ((image.width as u64) + csx) >> csx;
            let csy = image.yuvFormat.chroma_shift_y() as u64;
            let height = ((image.height as u64) + csy) >> csy;
            let uv_row_bytes = usize_from_u64(width * channel_size as u64)?;
            let uv_size = usize_from_u64(uv_row_bytes as u64 * height)?;
            for plane in 1usize..=2 {
                if !image.yuvPlanes[plane].is_null() {
                    continue;
                }
                image.yuvRowBytes[plane] = u32_from_usize(uv_row_bytes)?;
                image.yuvPlanes[plane] = unsafe { crabby_avifAlloc(uv_size) as *mut u8 };
            }
        }
    }
    if (planes & 2) != 0 {
        image.imageOwnsAlphaPlane = AVIF_TRUE;
        image.alphaRowBytes = u32_from_usize(y_row_bytes)?;
        image.alphaPlane = unsafe { crabby_avifAlloc(y_size) as *mut u8 };
    }
    Ok(())
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageAllocatePlanes(
    image: *mut avifImage,
    planes: avifPlanesFlags,
) -> avifResult {
    let image = unsafe { &mut (*image) };
    to_avifResult(&avif_image_allocate_planes_helper(image, planes))
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageFreePlanes(
    image: *mut avifImage,
    planes: avifPlanesFlags,
) {
    let image = unsafe { &mut (*image) };
    if (planes & 1) != 0 {
        for plane in 0usize..3 {
            if image.imageOwnsYUVPlanes == AVIF_TRUE {
                unsafe {
                    crabby_avifFree(image.yuvPlanes[plane] as *mut c_void);
                }
            }
            image.yuvPlanes[plane] = std::ptr::null_mut();
            image.yuvRowBytes[plane] = 0;
        }
        image.imageOwnsYUVPlanes = AVIF_FALSE;
    }
    if (planes & 2) != 0 {
        if image.imageOwnsAlphaPlane == AVIF_TRUE {
            unsafe {
                crabby_avifFree(image.alphaPlane as *mut c_void);
            }
        }
        image.alphaPlane = std::ptr::null_mut();
        image.alphaRowBytes = 0;
        image.imageOwnsAlphaPlane = AVIF_FALSE;
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageDestroy(image: *mut avifImage) {
    unsafe {
        let _ = Box::from_raw(image);
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageUsesU16(image: *const avifImage) -> avifBool {
    unsafe { to_avifBool(!image.is_null() && (*image).depth > 8) }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageIsOpaque(image: *const avifImage) -> avifBool {
    unsafe {
        // TODO: Check for pixel level opacity as well.
        to_avifBool(!image.is_null() && !(*image).alphaPlane.is_null())
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImagePlane(image: *const avifImage, channel: c_int) -> *mut u8 {
    if image.is_null() {
        return std::ptr::null_mut();
    }
    unsafe {
        match channel {
            0..=2 => (*image).yuvPlanes[channel as usize],
            3 => (*image).alphaPlane,
            _ => std::ptr::null_mut(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImagePlaneRowBytes(
    image: *const avifImage,
    channel: c_int,
) -> u32 {
    if image.is_null() {
        return 0;
    }
    unsafe {
        match channel {
            0..=2 => (*image).yuvRowBytes[channel as usize],
            3 => (*image).alphaRowBytes,
            _ => 0,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImagePlaneWidth(
    image: *const avifImage,
    channel: c_int,
) -> u32 {
    if image.is_null() {
        return 0;
    }
    unsafe {
        match channel {
            0 => (*image).width,
            1 | 2 => {
                if (*image).yuvFormat.is_monochrome() {
                    0
                } else {
                    let shift_x = (*image).yuvFormat.chroma_shift_x();
                    ((*image).width + shift_x) >> shift_x
                }
            }
            3 => {
                if !(*image).alphaPlane.is_null() {
                    (*image).width
                } else {
                    0
                }
            }
            _ => 0,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImagePlaneHeight(
    image: *const avifImage,
    channel: c_int,
) -> u32 {
    if image.is_null() {
        return 0;
    }
    unsafe {
        match channel {
            0 => (*image).height,
            1 | 2 => {
                if (*image).yuvFormat.is_monochrome() {
                    0
                } else {
                    let shift_y = (*image).yuvFormat.chroma_shift_y();
                    ((*image).height + shift_y) >> shift_y
                }
            }
            3 => {
                if !(*image).alphaPlane.is_null() {
                    (*image).height
                } else {
                    0
                }
            }
            _ => 0,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifImageSetViewRect(
    dstImage: *mut avifImage,
    srcImage: *const avifImage,
    rect: *const avifCropRect,
) -> avifResult {
    let dst = unsafe { &mut (*dstImage) };
    let src = unsafe { &(*srcImage) };
    let rect = unsafe { &(*rect) };
    if rect.width > src.width
        || rect.height > src.height
        || rect.x > (src.width - rect.width)
        || rect.y > (src.height - rect.height)
    {
        return avifResult::InvalidArgument;
    }
    if !src.yuvFormat.is_monochrome()
        && ((rect.x & src.yuvFormat.chroma_shift_x()) != 0
            || (rect.y & src.yuvFormat.chroma_shift_y()) != 0)
    {
        return avifResult::InvalidArgument;
    }
    // TODO: This is avifimagecopynoalloc.
    *dst = avifImage {
        width: src.width,
        height: src.height,
        depth: src.depth,
        yuvFormat: src.yuvFormat,
        yuvRange: src.yuvRange,
        yuvChromaSamplePosition: src.yuvChromaSamplePosition,
        alphaPremultiplied: src.alphaPremultiplied,
        colorPrimaries: src.colorPrimaries,
        transferCharacteristics: src.transferCharacteristics,
        matrixCoefficients: src.matrixCoefficients,
        clli: src.clli,
        transformFlags: src.transformFlags,
        pasp: src.pasp,
        clap: src.clap,
        irot: src.irot,
        imir: src.imir,
        ..avifImage::default()
    };
    dst.width = rect.width;
    dst.height = rect.height;
    let pixel_size: u32 = if src.depth == 8 { 1 } else { 2 };
    for plane in 0usize..3 {
        if src.yuvPlanes[plane].is_null() {
            continue;
        }
        let x = if plane == 0 { rect.x } else { rect.x >> src.yuvFormat.chroma_shift_x() };
        let y = if plane == 0 { rect.y } else { rect.y >> src.yuvFormat.chroma_shift_y() };
        let offset = match isize_from_u32(y * src.yuvRowBytes[plane] + x * pixel_size) {
            Ok(x) => x,
            _ => return avifResult::InvalidArgument,
        };
        dst.yuvPlanes[plane] = unsafe { src.yuvPlanes[plane].offset(offset) };
        dst.yuvRowBytes[plane] = src.yuvRowBytes[plane];
    }
    if !src.alphaPlane.is_null() {
        let offset = match isize_from_u32(rect.y * src.alphaRowBytes + rect.x * pixel_size) {
            Ok(x) => x,
            _ => return avifResult::InvalidArgument,
        };
        dst.alphaPlane = unsafe { src.alphaPlane.offset(offset) };
        dst.alphaRowBytes = src.alphaRowBytes;
    }
    avifResult::Ok
}
