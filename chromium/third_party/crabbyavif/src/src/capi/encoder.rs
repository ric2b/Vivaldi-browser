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

use super::image::*;
use super::io::*;
use super::types::*;

use crate::encoder::*;
use crate::gainmap::GainMap;
use crate::internal_utils::*;
use crate::*;

use std::ffi::CStr;
use std::os::raw::c_char;

#[repr(C)]
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
    pub minQuantizer: i32,
    pub maxQuantizer: i32,
    pub minQuantizerAlpha: i32,
    pub maxQuantizerAlpha: i32,
    pub tileRowsLog2: i32,
    pub tileColsLog2: i32,
    pub autoTiling: avifBool,
    scalingMode: ScalingMode,
    pub ioStats: crate::decoder::IOStats,
    pub diag: avifDiagnostics,
    pub qualityGainMap: i32,
    rust_encoder: Box<Encoder>,
    rust_encoder_initialized: bool,
    codec_specific_options: Box<CodecSpecificOptions>,
}

impl Default for avifEncoder {
    fn default() -> Self {
        let settings = Settings::default();
        Self {
            codecChoice: avifCodecChoice::Aom,
            maxThreads: settings.threads as _,
            speed: -1,
            keyframeInterval: settings.keyframe_interval,
            timescale: settings.timescale,
            repetitionCount: AVIF_REPETITION_COUNT_INFINITE,
            extraLayerCount: settings.extra_layer_count,
            quality: AVIF_QUALITY_DEFAULT,
            qualityAlpha: AVIF_QUALITY_DEFAULT,
            minQuantizer: AVIF_QUANTIZER_BEST_QUALITY as i32,
            maxQuantizer: AVIF_QUANTIZER_WORST_QUALITY as i32,
            minQuantizerAlpha: AVIF_QUANTIZER_BEST_QUALITY as i32,
            maxQuantizerAlpha: AVIF_QUANTIZER_WORST_QUALITY as i32,
            tileRowsLog2: 0,
            tileColsLog2: 0,
            autoTiling: AVIF_FALSE,
            scalingMode: settings.mutable.scaling_mode,
            ioStats: Default::default(),
            diag: Default::default(),
            qualityGainMap: settings.mutable.quality,
            rust_encoder: Default::default(),
            rust_encoder_initialized: false,
            codec_specific_options: Default::default(),
        }
    }
}

fn quality_from_quantizers(minQuantizer: i32, maxQuantizer: i32) -> i32 {
    100 - ((50 * (minQuantizer.clamp(0, 63) + maxQuantizer.clamp(0, 63)) - 50) / 63)
}

impl From<&avifEncoder> for MutableSettings {
    fn from(encoder: &avifEncoder) -> Self {
        Self {
            quality: if encoder.quality == -1 {
                quality_from_quantizers(encoder.minQuantizer, encoder.maxQuantizer)
            } else {
                encoder.quality
            },
            // TODO - b/416560730: Convert to proper tiling mode.
            tiling_mode: TilingMode::Auto,
            scaling_mode: encoder.scalingMode,
        }
    }
}

impl From<&avifEncoder> for Settings {
    fn from(encoder: &avifEncoder) -> Self {
        Self {
            threads: encoder.maxThreads as u32,
            speed: if encoder.speed >= 0 && encoder.speed <= 10 {
                Some(encoder.speed as u32)
            } else {
                None
            },
            keyframe_interval: encoder.keyframeInterval,
            timescale: if encoder.timescale == 0 { 1 } else { encoder.timescale },
            repetition_count: RepetitionCount::create_from(encoder.repetitionCount),
            extra_layer_count: encoder.extraLayerCount,
            mutable: encoder.into(),
        }
    }
}

impl avifEncoder {
    fn initialize_or_update_rust_encoder(&mut self) -> avifResult {
        if self.rust_encoder_initialized {
            // TODO - b/416560730: Validate the immutable settings.
            let mutable_settings: MutableSettings = (&*self).into();
            let res = self.rust_encoder.update_settings(&mutable_settings);
            self.diag.set_from_result(&res);
            res.into()
        } else {
            let settings: Settings = (&*self).into();
            let res = Encoder::create_with_settings(&settings);
            self.diag.set_from_result(&res);
            match res {
                Ok(encoder) => self.rust_encoder = Box::new(encoder),
                Err(err) => return (&err).into(),
            }
            self.rust_encoder_initialized = true;
            // Push any existing codec specific options.
            for (key, value) in self.codec_specific_options.iter() {
                self.rust_encoder.set_codec_specific_option(
                    key.0,
                    key.1.to_string(),
                    value.to_string(),
                );
            }
            self.codec_specific_options.clear();
            avifResult::Ok
        }
    }
}

fn rust_encoder<'a>(encoder: *mut avifEncoder) -> &'a mut Encoder {
    &mut deref_mut!(encoder).rust_encoder
}

/// # Safety
/// Used by the C API to create an avifEncoder object with default values.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderCreate() -> *mut avifEncoder {
    Box::into_raw(Box::<avifEncoder>::default())
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderDestroy(encoder: *mut avifEncoder) {
    check_pointer_or_return!(encoder);
    // SAFETY: encoder is guaranteed to be not null, so this is ok.
    let _ = unsafe { Box::from_raw(encoder) };
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if image is not null, it has to point to a valid avifImage object.
/// - if output is not null, it has to point to a valid avifRWData object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderWrite(
    encoder: *mut avifEncoder,
    image: *const avifImage,
    output: *mut avifRWData,
) -> avifResult {
    check_pointer!(encoder);
    check_pointer!(image);
    check_pointer!(output);

    // SAFETY: Pre-conditions are met to call this function.
    let res = unsafe { crabby_avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE) };
    if res != avifResult::Ok {
        return res;
    }
    // SAFETY: Pre-conditions are met to call this function.
    unsafe { crabby_avifEncoderFinish(encoder, output) }
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if image is not null, it has to point to a valid avifImage object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderAddImage(
    encoder: *mut avifEncoder,
    image: *const avifImage,
    durationInTimescales: u64,
    addImageFlags: avifAddImageFlags,
) -> avifResult {
    check_pointer!(encoder);
    check_pointer!(image);

    let encoder_ref = deref_mut!(encoder);
    let res = encoder_ref.initialize_or_update_rust_encoder();
    if res != avifResult::Ok {
        return res;
    }
    let gainmap = deref_const!(image).gainmap();
    let image: image::Image = deref_const!(image).into();
    let res =
        if (addImageFlags & AVIF_ADD_IMAGE_FLAG_SINGLE) != 0 || encoder_ref.extraLayerCount != 0 {
            match &gainmap {
                Some(gainmap) => rust_encoder(encoder).add_image_gainmap(&image, gainmap),
                None => rust_encoder(encoder).add_image(&image),
            }
        } else {
            rust_encoder(encoder).add_image_for_sequence(&image, durationInTimescales)
        };
    encoder_ref.diag.set_from_result(&res);
    res.into()
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if cellImages is not null, it has to point to valid array of avifImage objects of size
///   |gridCols * gridRows|.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderAddImageGrid(
    encoder: *mut avifEncoder,
    gridCols: u32,
    gridRows: u32,
    cellImages: *const *const avifImage,
    addImageFlags: avifAddImageFlags,
) -> avifResult {
    check_pointer!(encoder);
    check_pointer!(cellImages);

    let encoder_ref = deref_mut!(encoder);
    if cellImages.is_null()
        || gridCols == 0
        || gridRows == 0
        // If we are not encoding a grid image with multiple layers, AVIF_ADD_IMAGE_FLAG_SINGLE has
        // to be set.
        || ((addImageFlags & AVIF_ADD_IMAGE_FLAG_SINGLE) == 0 && encoder_ref.extraLayerCount == 0)
    {
        return avifResult::InvalidArgument;
    }
    let res = encoder_ref.initialize_or_update_rust_encoder();
    if res != avifResult::Ok {
        return res;
    }
    let cell_count = match gridCols.checked_mul(gridRows) {
        Some(value) => value as usize,
        None => return avifResult::InvalidArgument,
    };
    let mut images: Vec<image::Image> = match create_vec_exact(cell_count) {
        Ok(x) => x,
        Err(_) => return avifResult::OutOfMemory,
    };
    // SAFETY: Pre-condition of this function ensures that |cellImages| contains |cell_count|
    // avifImage objects. So this operation is safe.
    let image_ptrs: &[*const avifImage] =
        unsafe { std::slice::from_raw_parts(cellImages, cell_count) };
    for image_ptr in image_ptrs {
        check_pointer!(image_ptr);
    }
    let mut gainmaps: Vec<Option<GainMap>> = Vec::new();
    for image_ptr in image_ptrs {
        gainmaps.push(deref_const!(*image_ptr).gainmap());
        images.push(deref_const!(*image_ptr).into());
    }
    let image_refs: Vec<&Image> = images.iter().collect();
    let res = if gainmaps.iter().all(|x| x.is_some()) {
        let gainmap_refs: Vec<&GainMap> = gainmaps.iter().map(|x| x.unwrap_ref()).collect();
        rust_encoder(encoder).add_image_gainmap_grid(gridCols, gridRows, &image_refs, &gainmap_refs)
    } else if gainmaps.iter().all(|x| x.is_none()) {
        rust_encoder(encoder).add_image_grid(gridCols, gridRows, &image_refs)
    } else {
        // Some cells had GainMap and some did not. This is invalid.
        Err(AvifError::InvalidArgument)
    };
    encoder_ref.diag.set_from_result(&res);
    res.into()
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if output is not null, it has to point to a valid avifRWData object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderFinish(
    encoder: *mut avifEncoder,
    output: *mut avifRWData,
) -> avifResult {
    check_pointer!(encoder);
    check_pointer!(output);

    let res = rust_encoder(encoder).finish();
    deref_mut!(encoder).diag.set_from_result(&res);
    match res {
        // SAFETY: Pre-conditions are met to call this function.
        Ok(encoded_data) => unsafe {
            crabby_avifRWDataSet(output, encoded_data.as_ptr(), encoded_data.len())
        },
        Err(err) => (&err).into(),
    }
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if key is not null, it has to point to a valid C-style string.
/// - if value is not null, it has to point to a valid C-style string.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifEncoderSetCodecSpecificOption(
    encoder: *mut avifEncoder,
    key: *const c_char,
    value: *const c_char,
) -> avifResult {
    check_pointer!(encoder);
    check_pointer!(key);
    check_pointer!(value);

    // SAFETY: Pointers are guaranteed to be not-null and contain a valid c-string as per the
    // pre-conditions of this function.
    let (key, value) = unsafe { (CStr::from_ptr(key), CStr::from_ptr(value)) };
    let (key, value) = (key.to_str(), value.to_str());
    if key.is_err() || value.is_err() {
        return avifResult::InvalidArgument;
    }
    let (key, value) = (key.unwrap().to_owned(), value.unwrap().to_owned());
    let (key, category) = if key.starts_with("c:") {
        (
            key.strip_prefix("c:").unwrap().to_string(),
            Some(Category::Color),
        )
    } else if key.starts_with("color:") {
        (
            key.strip_prefix("color:").unwrap().to_string(),
            Some(Category::Color),
        )
    } else if key.starts_with("a:") {
        (
            key.strip_prefix("a:").unwrap().to_string(),
            Some(Category::Alpha),
        )
    } else if key.starts_with("alpha:") {
        (
            key.strip_prefix("alpha:").unwrap().to_string(),
            Some(Category::Alpha),
        )
    } else if key.starts_with("g:") {
        (
            key.strip_prefix("g:").unwrap().to_string(),
            Some(Category::Gainmap),
        )
    } else if key.starts_with("gainmap:") {
        (
            key.strip_prefix("gainmap:").unwrap().to_string(),
            Some(Category::Gainmap),
        )
    } else {
        (key, None)
    };
    if deref_const!(encoder).rust_encoder_initialized {
        rust_encoder(encoder).set_codec_specific_option(category, key, value);
    } else {
        deref_mut!(encoder)
            .codec_specific_options
            .insert((category, key), value);
    }
    avifResult::Ok
}

#[cfg(test)]
mod tests {
    #[test]
    fn quality_from_quantizers() {
        // Test the extreme values and middle values.
        assert_eq!(super::quality_from_quantizers(0, 0), 100);
        assert_eq!(super::quality_from_quantizers(23, 32), 58);
        assert_eq!(super::quality_from_quantizers(63, 63), 1);
        // Invalid values should be clamped.
        assert_eq!(super::quality_from_quantizers(-1, -20), 100);
        assert_eq!(super::quality_from_quantizers(100, 200), 1);

        // Test all valid combinations to make sure they return a valid quality value.
        for min_quantizer in 0..63 {
            for max_quantizer in 0..63 {
                let quality = super::quality_from_quantizers(min_quantizer, max_quantizer);
                assert!(quality >= 0 && quality <= 100);
            }
        }
    }
}
