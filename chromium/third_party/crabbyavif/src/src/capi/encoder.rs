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

#![allow(unused)]

use super::gainmap::*;
use super::image::*;
use super::io::*;
use super::types::*;

use std::ffi::CStr;
use std::num::NonZero;
use std::os::raw::c_char;

use crate::encoder::*;
use crate::*;

#[repr(C)]
#[derive(Default)]
pub struct avifEncoder {
    pub codecChoice: avifCodecChoice,
    pub maxThreads: i32,
    pub speed: i32,
    pub keyframeInterval: i32,
    pub timescale: u64,
    pub repetitionCount: i32,
    pub extraLayerCount: u32,
    pub quality: i32,
    pub qualityAlpha: i32,
    pub tileRowsLog2: i32,
    pub tileColsLog2: i32,
    pub autoTiling: avifBool,
    scalingMode: ScalingMode,
    pub ioStats: crate::decoder::IOStats,
    pub qualityGainMap: i32,
    rust_encoder: Box<Encoder>,
    rust_encoder_initialized: bool,
}

impl From<&avifEncoder> for Settings {
    fn from(encoder: &avifEncoder) -> Self {
        Self {
            threads: encoder.maxThreads as u32,
            speed: Some(encoder.speed as u32),
            keyframe_interval: encoder.keyframeInterval,
            timescale: encoder.timescale,
            repetition_count: encoder.repetitionCount,
            extra_layer_count: encoder.extraLayerCount,
            mutable: MutableSettings {
                quality: encoder.quality,
                // TODO - b/416560730: Convert to proper tiling mode.
                tiling_mode: TilingMode::Auto,
                scaling_mode: encoder.scalingMode,
            },
        }
    }
}

fn rust_encoder<'a>(encoder: *mut avifEncoder) -> &'a mut Encoder {
    &mut deref_mut!(encoder).rust_encoder
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderCreate() -> *mut avifEncoder {
    Box::into_raw(Box::<avifEncoder>::default())
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderDestroy(encoder: *mut avifEncoder) {
    let _ = unsafe { Box::from_raw(encoder) };
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderWrite(
    encoder: *mut avifEncoder,
    image: *const avifImage,
    output: *mut avifRWData,
) -> avifResult {
    let res = unsafe { crabby_avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE) };
    if res != avifResult::Ok {
        return res;
    }
    unsafe { crabby_avifEncoderFinish(encoder, output) }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderAddImage(
    encoder: *mut avifEncoder,
    image: *const avifImage,
    durationInTimescales: u64,
    addImageFlags: avifAddImageFlags,
) -> avifResult {
    let encoder_ref = deref_mut!(encoder);
    if !encoder_ref.rust_encoder_initialized {
        let settings: Settings = (&*encoder_ref).into();
        match Encoder::create_with_settings(&settings) {
            Ok(encoder) => encoder_ref.rust_encoder = Box::new(encoder),
            Err(err) => return (&err).into(),
        }
        encoder_ref.rust_encoder_initialized = true;
    } else {
        // TODO - b/416560730: Validate the immutable settings and update the mutable settings for
        // subsequent frames.
    }
    let image: image::Image = deref_const!(image).into();
    rust_encoder(encoder).add_image(&image).into()
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderAddImageGrid(
    encoder: *mut avifEncoder,
    gridCols: u32,
    gridRows: u32,
    cellImages: *const *const avifImage,
    addImageFlags: avifAddImageFlags,
) -> avifResult {
    todo!();
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderFinish(
    encoder: *mut avifEncoder,
    output: *mut avifRWData,
) -> avifResult {
    match rust_encoder(encoder).finish() {
        Ok(encoded_data) => unsafe {
            crabby_avifRWDataSet(output, encoded_data.as_ptr(), encoded_data.len())
        },
        Err(err) => (&err).into(),
    }
}
