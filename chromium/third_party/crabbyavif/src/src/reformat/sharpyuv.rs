// Copyright 2025 Google LLC
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

use crate::image::*;
use crate::internal_utils::*;
use crate::reformat::rgb;
use crate::reformat::rgb::*;
use crate::*;

use libsharpyuv_sys::bindings::*;

use std::mem::MaybeUninit;
use std::os::raw::c_void;

pub(crate) fn rgb_to_yuv(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let color_space = match (image as &image::Image).into() {
        Mode::YuvCoefficients(kr, _kg, kb) => SharpYuvColorSpace {
            kr,
            kb,
            bit_depth: image.depth as _,
            range: if image.yuv_range == YuvRange::Limited {
                SharpYuvRange_kSharpYuvRangeLimited
            } else {
                SharpYuvRange_kSharpYuvRangeFull
            },
        },
        _ => return Err(AvifError::NotImplemented),
    };
    let mut matrix_uninit: MaybeUninit<SharpYuvConversionMatrix> = MaybeUninit::uninit();
    // SAFETY: Calling into a C function with pointers that are guaranteed to be not null.
    unsafe {
        SharpYuvComputeConversionMatrix(&color_space as *const _, matrix_uninit.as_mut_ptr());
    }
    // bindgen does not expose SHARPYUV_VERSION directly since it's populated with a macro. The
    // code to compute the version is duplicated from libwebp.
    const SHARPYUV_VERSION: i32 = ((SHARPYUV_VERSION_MAJOR << 24)
        | (SHARPYUV_VERSION_MINOR << 16)
        | SHARPYUV_VERSION_PATCH) as i32;
    // SAFETY: matrix_uninit was initialized by the C function above.
    let matrix = unsafe { matrix_uninit.assume_init() };
    let mut options_uninit: MaybeUninit<SharpYuvOptions> = MaybeUninit::uninit();
    // SAFETY: Calling into a C function with pointers that are guaranteed to be not null.
    unsafe {
        SharpYuvOptionsInitInternal(
            &matrix as *const _,
            options_uninit.as_mut_ptr(),
            SHARPYUV_VERSION,
        );
    }
    // SAFETY: options_uninit was initialized by the C function above.
    let mut options = unsafe { options_uninit.assume_init() };
    options.transfer_type =
        if image.transfer_characteristics == TransferCharacteristics::Unspecified {
            SharpYuvTransferFunctionType_kSharpYuvTransferFunctionSrgb
        } else {
            image.transfer_characteristics as _
        };
    let plane_u8 = image.plane_ptrs_mut();
    let plane_row_bytes = image.plane_row_bytes()?;
    let (r_ptr, g_ptr, b_ptr) = if rgb.depth == 8 {
        let rgb_pixels = rgb.pixels();
        if rgb_pixels.is_null() {
            return Err(AvifError::InvalidArgument);
        }
        // SAFETY: Computing pointer offset with non-null pointers and positive offsets.
        unsafe {
            (
                rgb_pixels.add(rgb.format.r_offset()) as *const c_void,
                rgb_pixels.add(rgb.format.g_offset()) as *const c_void,
                rgb_pixels.add(rgb.format.b_offset()) as *const c_void,
            )
        }
    } else {
        let rgb_pixels = rgb.pixels16();
        if rgb_pixels.is_null() {
            return Err(AvifError::InvalidArgument);
        }
        // SAFETY: Computing pointer offset with non-null pointers and positive offsets.
        unsafe {
            (
                rgb_pixels.add(rgb.format.r_offset()) as *const c_void,
                rgb_pixels.add(rgb.format.g_offset()) as *const c_void,
                rgb_pixels.add(rgb.format.b_offset()) as *const c_void,
            )
        }
    };
    // SAFETY: Calling the C library conversion function. Pointers and strides are guaranteed to be
    // valid.
    if unsafe {
        SharpYuvConvertWithOptions(
            r_ptr,
            g_ptr,
            b_ptr,
            rgb.format.pixel_size(rgb.depth as _) as _,
            i32_from_u32(rgb.row_bytes)?,
            rgb.depth as _,
            plane_u8[0] as *mut _,
            plane_row_bytes[0],
            plane_u8[1] as *mut _,
            plane_row_bytes[1],
            plane_u8[2] as *mut _,
            plane_row_bytes[2],
            image.depth as _,
            i32_from_u32(rgb.width)?,
            i32_from_u32(rgb.height)?,
            &options as *const _,
        )
    } == 0
    {
        Err(AvifError::ReformatFailed)
    } else {
        Ok(())
    }
}
