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
use super::image::*;
use super::io::*;
use super::types::*;

use std::ffi::CStr;
use std::os::raw::c_char;

use crate::decoder::track::*;
use crate::decoder::*;
use crate::*;

#[repr(C)]
pub struct avifDecoder {
    pub codecChoice: avifCodecChoice,
    pub maxThreads: i32,
    pub requestedSource: Source,
    pub allowProgressive: avifBool,
    pub allowIncremental: avifBool,
    pub ignoreExif: avifBool,
    pub ignoreXMP: avifBool,
    pub imageSizeLimit: u32,
    pub imageDimensionLimit: u32,
    pub imageCountLimit: u32,
    pub strictFlags: avifStrictFlags,

    // Output params.
    pub image: *mut avifImage,
    pub imageIndex: i32,
    pub imageCount: i32,
    pub progressiveState: ProgressiveState,
    pub imageTiming: ImageTiming,
    pub timescale: u64,
    pub duration: f64,
    pub durationInTimescales: u64,
    pub repetitionCount: i32,

    pub alphaPresent: avifBool,

    pub ioStats: IOStats,
    pub diag: avifDiagnostics,
    //avifIO * io;
    pub data: *mut avifDecoderData,
    pub imageContentToDecode: avifImageContentTypeFlags,
    pub imageSequenceTrackPresent: avifBool,

    // TODO: maybe wrap these fields in a private data kind of field?
    rust_decoder: Box<Decoder>,
    image_object: avifImage,
    gainmap_object: avifGainMap,
    gainmap_image_object: avifImage,
}

impl Default for avifDecoder {
    fn default() -> Self {
        Self {
            codecChoice: avifCodecChoice::Auto,
            maxThreads: 1,
            requestedSource: Source::Auto,
            allowIncremental: AVIF_FALSE,
            allowProgressive: AVIF_FALSE,
            ignoreExif: AVIF_FALSE,
            ignoreXMP: AVIF_FALSE,
            imageSizeLimit: DEFAULT_IMAGE_SIZE_LIMIT,
            imageDimensionLimit: DEFAULT_IMAGE_DIMENSION_LIMIT,
            imageCountLimit: DEFAULT_IMAGE_COUNT_LIMIT,
            strictFlags: AVIF_STRICT_ENABLED,
            image: std::ptr::null_mut(),
            imageIndex: -1,
            imageCount: 0,
            progressiveState: ProgressiveState::Unavailable,
            imageTiming: ImageTiming::default(),
            timescale: 0,
            duration: 0.0,
            durationInTimescales: 0,
            repetitionCount: 0,
            alphaPresent: AVIF_FALSE,
            ioStats: Default::default(),
            diag: avifDiagnostics::default(),
            data: std::ptr::null_mut(),
            imageContentToDecode: AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA,
            imageSequenceTrackPresent: AVIF_FALSE,
            rust_decoder: Box::<Decoder>::default(),
            image_object: avifImage::default(),
            gainmap_image_object: avifImage::default(),
            gainmap_object: avifGainMap::default(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderCreate() -> *mut avifDecoder {
    Box::into_raw(Box::<avifDecoder>::default())
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderSetIO(decoder: *mut avifDecoder, io: *mut avifIO) {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        rust_decoder.set_io(Box::new(avifIOWrapper::create(*io)));
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderSetIOFile(
    decoder: *mut avifDecoder,
    filename: *const c_char,
) -> avifResult {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        let filename = String::from(CStr::from_ptr(filename).to_str().unwrap_or(""));
        to_avifResult(&rust_decoder.set_io_file(&filename))
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderSetIOMemory(
    decoder: *mut avifDecoder,
    data: *const u8,
    size: usize,
) -> avifResult {
    let rust_decoder = unsafe { &mut (*decoder).rust_decoder };
    to_avifResult(unsafe { &rust_decoder.set_io_raw(data, size) })
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderSetSource(
    decoder: *mut avifDecoder,
    source: Source,
) -> avifResult {
    unsafe {
        (*decoder).requestedSource = source;
    }
    // TODO: should decoder be reset here in case this is called after parse?
    avifResult::Ok
}

impl From<&avifDecoder> for Settings {
    fn from(decoder: &avifDecoder) -> Self {
        let strictness = if decoder.strictFlags == AVIF_STRICT_DISABLED {
            Strictness::None
        } else if decoder.strictFlags == AVIF_STRICT_ENABLED {
            Strictness::All
        } else {
            let mut flags: Vec<StrictnessFlag> = Vec::new();
            if (decoder.strictFlags & AVIF_STRICT_PIXI_REQUIRED) != 0 {
                flags.push(StrictnessFlag::PixiRequired);
            }
            if (decoder.strictFlags & AVIF_STRICT_CLAP_VALID) != 0 {
                flags.push(StrictnessFlag::ClapValid);
            }
            if (decoder.strictFlags & AVIF_STRICT_ALPHA_ISPE_REQUIRED) != 0 {
                flags.push(StrictnessFlag::AlphaIspeRequired);
            }
            Strictness::SpecificInclude(flags)
        };
        let image_content_to_decode_flags: ImageContentType = match decoder.imageContentToDecode {
            AVIF_IMAGE_CONTENT_ALL => ImageContentType::All,
            AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA => ImageContentType::ColorAndAlpha,
            AVIF_IMAGE_CONTENT_GAIN_MAP => ImageContentType::GainMap,
            _ => ImageContentType::None,
        };
        Self {
            source: decoder.requestedSource,
            strictness,
            allow_progressive: decoder.allowProgressive == AVIF_TRUE,
            allow_incremental: decoder.allowIncremental == AVIF_TRUE,
            ignore_exif: decoder.ignoreExif == AVIF_TRUE,
            ignore_xmp: decoder.ignoreXMP == AVIF_TRUE,
            image_content_to_decode: image_content_to_decode_flags,
            codec_choice: match decoder.codecChoice {
                avifCodecChoice::Auto => CodecChoice::Auto,
                avifCodecChoice::Dav1d => CodecChoice::Dav1d,
                avifCodecChoice::Libgav1 => CodecChoice::Libgav1,
                // Silently treat all other choices the same as Auto.
                _ => CodecChoice::Auto,
            },
            image_size_limit: decoder.imageSizeLimit,
            image_dimension_limit: decoder.imageDimensionLimit,
            image_count_limit: decoder.imageCountLimit,
            max_threads: u32::try_from(decoder.maxThreads).unwrap_or(0),
        }
    }
}

fn rust_decoder_to_avifDecoder(src: &Decoder, dst: &mut avifDecoder) {
    // Copy image.
    let image = src.image().unwrap();
    dst.image_object = image.into();

    // Copy decoder properties.
    dst.alphaPresent = to_avifBool(image.alpha_present);
    dst.imageSequenceTrackPresent = to_avifBool(image.image_sequence_track_present);
    dst.progressiveState = image.progressive_state;

    dst.imageTiming = src.image_timing();
    dst.imageCount = src.image_count() as i32;
    dst.imageIndex = src.image_index();
    dst.repetitionCount = match src.repetition_count() {
        RepetitionCount::Unknown => AVIF_REPETITION_COUNT_UNKNOWN,
        RepetitionCount::Infinite => AVIF_REPETITION_COUNT_INFINITE,
        RepetitionCount::Finite(x) => x,
    };
    dst.timescale = src.timescale();
    dst.durationInTimescales = src.duration_in_timescales();
    dst.duration = src.duration();
    dst.ioStats = src.io_stats();

    if src.gainmap_present() {
        dst.gainmap_image_object = (&src.gainmap().image).into();
        dst.gainmap_object = src.gainmap().into();
        if src.settings.image_content_to_decode.gainmap() {
            dst.gainmap_object.image = (&mut dst.gainmap_image_object) as *mut avifImage;
        }
        dst.image_object.gainMap = (&mut dst.gainmap_object) as *mut avifGainMap;
    }
    dst.image = (&mut dst.image_object) as *mut avifImage;
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderParse(decoder: *mut avifDecoder) -> avifResult {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        rust_decoder.settings = (&(*decoder)).into();

        let res = rust_decoder.parse();
        (*decoder).diag.set_from_result(&res);
        if res.is_err() {
            return to_avifResult(&res);
        }
        rust_decoder_to_avifDecoder(rust_decoder, &mut (*decoder));
        avifResult::Ok
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderNextImage(decoder: *mut avifDecoder) -> avifResult {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        rust_decoder.settings = (&(*decoder)).into();

        let previous_decoded_row_count = rust_decoder.decoded_row_count();

        let res = rust_decoder.next_image();
        (*decoder).diag.set_from_result(&res);
        let mut early_return = false;
        if res.is_err() {
            early_return = true;
            if rust_decoder.settings.allow_incremental
                && matches!(res.as_ref().err().unwrap(), AvifError::WaitingOnIo)
            {
                early_return = previous_decoded_row_count == rust_decoder.decoded_row_count();
            }
        }
        if early_return {
            return to_avifResult(&res);
        }
        rust_decoder_to_avifDecoder(rust_decoder, &mut (*decoder));
        to_avifResult(&res)
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderNthImage(
    decoder: *mut avifDecoder,
    frameIndex: u32,
) -> avifResult {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        rust_decoder.settings = (&(*decoder)).into();

        let previous_decoded_row_count = rust_decoder.decoded_row_count();
        let image_index = (rust_decoder.image_index() + 1) as u32;

        let res = rust_decoder.nth_image(frameIndex);
        (*decoder).diag.set_from_result(&res);
        let mut early_return = false;
        if res.is_err() {
            early_return = true;
            if rust_decoder.settings.allow_incremental
                && matches!(res.as_ref().err().unwrap(), AvifError::WaitingOnIo)
            {
                if image_index != frameIndex {
                    early_return = false;
                } else {
                    early_return = previous_decoded_row_count == rust_decoder.decoded_row_count();
                }
            }
        }
        if early_return {
            return to_avifResult(&res);
        }
        rust_decoder_to_avifDecoder(rust_decoder, &mut (*decoder));
        to_avifResult(&res)
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderNthImageTiming(
    decoder: *const avifDecoder,
    frameIndex: u32,
    outTiming: *mut ImageTiming,
) -> avifResult {
    let rust_decoder = unsafe { &(*decoder).rust_decoder };
    let image_timing = rust_decoder.nth_image_timing(frameIndex);
    if let Ok(timing) = image_timing {
        unsafe {
            *outTiming = timing;
        }
    }
    to_avifResult(&image_timing)
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderDestroy(decoder: *mut avifDecoder) {
    unsafe {
        let _ = Box::from_raw(decoder);
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderRead(
    decoder: *mut avifDecoder,
    image: *mut avifImage,
) -> avifResult {
    unsafe {
        let rust_decoder = &mut (*decoder).rust_decoder;
        rust_decoder.settings = (&(*decoder)).into();

        let res = rust_decoder.parse();
        if res.is_err() {
            return to_avifResult(&res);
        }
        let res = rust_decoder.next_image();
        if res.is_err() {
            return to_avifResult(&res);
        }
        rust_decoder_to_avifDecoder(rust_decoder, &mut (*decoder));
        *image = (*decoder).image_object.clone();
        avifResult::Ok
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderReadMemory(
    decoder: *mut avifDecoder,
    image: *mut avifImage,
    data: *const u8,
    size: usize,
) -> avifResult {
    unsafe {
        let res = crabby_avifDecoderSetIOMemory(decoder, data, size);
        if res != avifResult::Ok {
            return res;
        }
        crabby_avifDecoderRead(decoder, image)
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderReadFile(
    decoder: *mut avifDecoder,
    image: *mut avifImage,
    filename: *const c_char,
) -> avifResult {
    unsafe {
        let res = crabby_avifDecoderSetIOFile(decoder, filename);
        if res != avifResult::Ok {
            return res;
        }
        crabby_avifDecoderRead(decoder, image)
    }
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderIsKeyframe(
    decoder: *const avifDecoder,
    frameIndex: u32,
) -> avifBool {
    let rust_decoder = unsafe { &(*decoder).rust_decoder };
    to_avifBool(rust_decoder.is_keyframe(frameIndex))
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderNearestKeyframe(
    decoder: *const avifDecoder,
    frameIndex: u32,
) -> u32 {
    let rust_decoder = unsafe { &(*decoder).rust_decoder };
    rust_decoder.nearest_keyframe(frameIndex)
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderDecodedRowCount(decoder: *const avifDecoder) -> u32 {
    let rust_decoder = unsafe { &(*decoder).rust_decoder };
    rust_decoder.decoded_row_count()
}

#[allow(non_camel_case_types)]
pub type avifExtent = Extent;

#[no_mangle]
pub unsafe extern "C" fn crabby_avifDecoderNthImageMaxExtent(
    decoder: *const avifDecoder,
    frameIndex: u32,
    outExtent: *mut avifExtent,
) -> avifResult {
    let rust_decoder = unsafe { &(*decoder).rust_decoder };
    let res = rust_decoder.nth_image_max_extent(frameIndex);
    if res.is_err() {
        return to_avifResult(&res);
    }
    unsafe {
        *outExtent = res.unwrap();
    }
    avifResult::Ok
}

#[no_mangle]
pub unsafe extern "C" fn crabby_avifPeekCompatibleFileType(input: *const avifROData) -> avifBool {
    let data = unsafe { std::slice::from_raw_parts((*input).data, (*input).size) };
    to_avifBool(Decoder::peek_compatible_file_type(data))
}
