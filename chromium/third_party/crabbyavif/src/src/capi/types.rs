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

use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::os::raw::c_uint;
use std::os::raw::c_void;

use crate::utils::clap::*;
use crate::*;

#[repr(C)]
#[derive(PartialEq)]
pub enum avifResult {
    Ok = 0,
    UnknownError = 1,
    InvalidFtyp = 2,
    NoContent = 3,
    NoYuvFormatSelected = 4,
    ReformatFailed = 5,
    UnsupportedDepth = 6,
    EncodeColorFailed = 7,
    EncodeAlphaFailed = 8,
    BmffParseFailed = 9,
    MissingImageItem = 10,
    DecodeColorFailed = 11,
    DecodeAlphaFailed = 12,
    ColorAlphaSizeMismatch = 13,
    IspeSizeMismatch = 14,
    NoCodecAvailable = 15,
    NoImagesRemaining = 16,
    InvalidExifPayload = 17,
    InvalidImageGrid = 18,
    InvalidCodecSpecificOption = 19,
    TruncatedData = 20,
    IoNotSet = 21,
    IoError = 22,
    WaitingOnIo = 23,
    InvalidArgument = 24,
    NotImplemented = 25,
    OutOfMemory = 26,
    CannotChangeSetting = 27,
    IncompatibleImage = 28,
    EncodeGainMapFailed = 29,
    DecodeGainMapFailed = 30,
    InvalidToneMappedImage = 31,
}

impl From<&AvifError> for avifResult {
    fn from(err: &AvifError) -> Self {
        match err {
            AvifError::Ok => avifResult::Ok,
            AvifError::UnknownError(_) => avifResult::UnknownError,
            AvifError::InvalidFtyp => avifResult::InvalidFtyp,
            AvifError::NoContent => avifResult::NoContent,
            AvifError::NoYuvFormatSelected => avifResult::NoYuvFormatSelected,
            AvifError::ReformatFailed => avifResult::ReformatFailed,
            AvifError::UnsupportedDepth => avifResult::UnsupportedDepth,
            AvifError::EncodeColorFailed => avifResult::EncodeColorFailed,
            AvifError::EncodeAlphaFailed => avifResult::EncodeAlphaFailed,
            AvifError::BmffParseFailed(_) => avifResult::BmffParseFailed,
            AvifError::MissingImageItem => avifResult::MissingImageItem,
            AvifError::DecodeColorFailed => avifResult::DecodeColorFailed,
            AvifError::DecodeAlphaFailed => avifResult::DecodeAlphaFailed,
            AvifError::ColorAlphaSizeMismatch => avifResult::ColorAlphaSizeMismatch,
            AvifError::IspeSizeMismatch => avifResult::IspeSizeMismatch,
            AvifError::NoCodecAvailable => avifResult::NoCodecAvailable,
            AvifError::NoImagesRemaining => avifResult::NoImagesRemaining,
            AvifError::InvalidExifPayload => avifResult::InvalidExifPayload,
            AvifError::InvalidImageGrid(_) => avifResult::InvalidImageGrid,
            AvifError::InvalidCodecSpecificOption => avifResult::InvalidCodecSpecificOption,
            AvifError::TruncatedData => avifResult::TruncatedData,
            AvifError::IoNotSet => avifResult::IoNotSet,
            AvifError::IoError => avifResult::IoError,
            AvifError::WaitingOnIo => avifResult::WaitingOnIo,
            AvifError::InvalidArgument => avifResult::InvalidArgument,
            AvifError::NotImplemented => avifResult::NotImplemented,
            AvifError::OutOfMemory => avifResult::OutOfMemory,
            AvifError::CannotChangeSetting => avifResult::CannotChangeSetting,
            AvifError::IncompatibleImage => avifResult::IncompatibleImage,
            AvifError::EncodeGainMapFailed => avifResult::EncodeGainMapFailed,
            AvifError::DecodeGainMapFailed => avifResult::DecodeGainMapFailed,
            AvifError::InvalidToneMappedImage(_) => avifResult::InvalidToneMappedImage,
        }
    }
}

impl<T> From<AvifResult<T>> for avifResult {
    fn from(res: AvifResult<T>) -> Self {
        match res {
            Ok(_) => avifResult::Ok,
            Err(err) => {
                let res: avifResult = (&err).into();
                res
            }
        }
    }
}

impl From<avifResult> for AvifError {
    fn from(res: avifResult) -> Self {
        match res {
            avifResult::Ok => AvifError::Ok,
            avifResult::UnknownError => AvifError::UnknownError("".into()),
            avifResult::InvalidFtyp => AvifError::InvalidFtyp,
            avifResult::NoContent => AvifError::NoContent,
            avifResult::NoYuvFormatSelected => AvifError::NoYuvFormatSelected,
            avifResult::ReformatFailed => AvifError::ReformatFailed,
            avifResult::UnsupportedDepth => AvifError::UnsupportedDepth,
            avifResult::EncodeColorFailed => AvifError::EncodeColorFailed,
            avifResult::EncodeAlphaFailed => AvifError::EncodeAlphaFailed,
            avifResult::BmffParseFailed => AvifError::BmffParseFailed("".into()),
            avifResult::MissingImageItem => AvifError::MissingImageItem,
            avifResult::DecodeColorFailed => AvifError::DecodeColorFailed,
            avifResult::DecodeAlphaFailed => AvifError::DecodeAlphaFailed,
            avifResult::ColorAlphaSizeMismatch => AvifError::ColorAlphaSizeMismatch,
            avifResult::IspeSizeMismatch => AvifError::IspeSizeMismatch,
            avifResult::NoCodecAvailable => AvifError::NoCodecAvailable,
            avifResult::NoImagesRemaining => AvifError::NoImagesRemaining,
            avifResult::InvalidExifPayload => AvifError::InvalidExifPayload,
            avifResult::InvalidImageGrid => AvifError::InvalidImageGrid("".into()),
            avifResult::InvalidCodecSpecificOption => AvifError::InvalidCodecSpecificOption,
            avifResult::TruncatedData => AvifError::TruncatedData,
            avifResult::IoNotSet => AvifError::IoNotSet,
            avifResult::IoError => AvifError::IoError,
            avifResult::WaitingOnIo => AvifError::WaitingOnIo,
            avifResult::InvalidArgument => AvifError::InvalidArgument,
            avifResult::NotImplemented => AvifError::NotImplemented,
            avifResult::OutOfMemory => AvifError::OutOfMemory,
            avifResult::CannotChangeSetting => AvifError::CannotChangeSetting,
            avifResult::IncompatibleImage => AvifError::IncompatibleImage,
            avifResult::EncodeGainMapFailed => AvifError::EncodeGainMapFailed,
            avifResult::DecodeGainMapFailed => AvifError::DecodeGainMapFailed,
            avifResult::InvalidToneMappedImage => AvifError::InvalidToneMappedImage("".into()),
        }
    }
}

impl avifResult {
    pub(crate) fn as_usize(&self) -> usize {
        match self {
            Self::Ok => 0,
            Self::UnknownError => 1,
            Self::InvalidFtyp => 2,
            Self::NoContent => 3,
            Self::NoYuvFormatSelected => 4,
            Self::ReformatFailed => 5,
            Self::UnsupportedDepth => 6,
            Self::EncodeColorFailed => 7,
            Self::EncodeAlphaFailed => 8,
            Self::BmffParseFailed => 9,
            Self::MissingImageItem => 10,
            Self::DecodeColorFailed => 11,
            Self::DecodeAlphaFailed => 12,
            Self::ColorAlphaSizeMismatch => 13,
            Self::IspeSizeMismatch => 14,
            Self::NoCodecAvailable => 15,
            Self::NoImagesRemaining => 16,
            Self::InvalidExifPayload => 17,
            Self::InvalidImageGrid => 18,
            Self::InvalidCodecSpecificOption => 19,
            Self::TruncatedData => 20,
            Self::IoNotSet => 21,
            Self::IoError => 22,
            Self::WaitingOnIo => 23,
            Self::InvalidArgument => 24,
            Self::NotImplemented => 25,
            Self::OutOfMemory => 26,
            Self::CannotChangeSetting => 27,
            Self::IncompatibleImage => 28,
            Self::EncodeGainMapFailed => 29,
            Self::DecodeGainMapFailed => 30,
            Self::InvalidToneMappedImage => 31,
        }
    }
}

pub type avifBool = c_int;
pub const AVIF_TRUE: c_int = 1;
pub const AVIF_FALSE: c_int = 0;

pub const AVIF_STRICT_DISABLED: u32 = 0;
pub const AVIF_STRICT_PIXI_REQUIRED: u32 = 1 << 0;
pub const AVIF_STRICT_CLAP_VALID: u32 = 1 << 1;
pub const AVIF_STRICT_ALPHA_ISPE_REQUIRED: u32 = 1 << 2;
pub const AVIF_STRICT_ENABLED: u32 =
    AVIF_STRICT_PIXI_REQUIRED | AVIF_STRICT_CLAP_VALID | AVIF_STRICT_ALPHA_ISPE_REQUIRED;
pub type avifStrictFlags = u32;

pub const AVIF_IMAGE_CONTENT_NONE: u32 = 0;
pub const AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA: u32 = (1 << 0) | (1 << 1);
pub const AVIF_IMAGE_CONTENT_GAIN_MAP: u32 = 1 << 2;
pub const AVIF_IMAGE_CONTENT_ALL: u32 =
    AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA | AVIF_IMAGE_CONTENT_GAIN_MAP;
pub type avifImageContentTypeFlags = u32;

#[repr(C)]
pub struct avifDecoderData {}

pub const AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE: usize = 256;
#[repr(C)]
pub struct avifDiagnostics {
    error: [c_char; AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE],
}

impl Default for avifDiagnostics {
    fn default() -> Self {
        Self {
            error: [0; AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE],
        }
    }
}

impl avifDiagnostics {
    pub(crate) fn set_from_result<T>(&mut self, res: &AvifResult<T>) {
        match res {
            Ok(_) => self.set_error_empty(),
            Err(AvifError::BmffParseFailed(s))
            | Err(AvifError::UnknownError(s))
            | Err(AvifError::InvalidImageGrid(s))
            | Err(AvifError::InvalidToneMappedImage(s)) => self.set_error_string(s),
            _ => self.set_error_empty(),
        }
    }

    fn set_error_string(&mut self, error: &str) {
        if let Ok(s) = std::ffi::CString::new(error.to_owned()) {
            let len = std::cmp::min(
                s.as_bytes_with_nul().len(),
                AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE,
            );
            self.error
                .get_mut(..len)
                .unwrap()
                .iter_mut()
                .zip(&s.as_bytes_with_nul()[..len])
                .for_each(|(dst, src)| *dst = *src as c_char);
            self.error[AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE - 1] = 0;
        } else {
            self.set_error_empty();
        }
    }

    pub(crate) fn set_error_empty(&mut self) {
        self.error[0] = 0;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub enum avifCodecChoice {
    #[default]
    Auto = 0,
    Aom = 1,
    Dav1d = 2,
    Libgav1 = 3,
    Rav1e = 4,
    Svt = 5,
    Avm = 6,
}

impl avifCodecChoice {
    fn from_name(name: &str) -> Self {
        let available_codecs: &[(avifCodecChoice, &str)] = &[
            #[cfg(feature = "aom")]
            (Self::Aom, "aom"),
            #[cfg(feature = "dav1d")]
            (Self::Dav1d, "dav1d"),
            #[cfg(feature = "libgav1")]
            (Self::Libgav1, "libgav1"),
        ];
        for available_codec in available_codecs {
            if name == available_codec.1 {
                return available_codec.0;
            }
        }
        avifCodecChoice::Auto
    }
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if name is not null, it has to be a valid C-style string.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifCodecChoiceFromName(name: *const c_char) -> avifCodecChoice {
    if name.is_null() {
        return avifCodecChoice::Auto;
    }
    // SAFETY: name is guaranteed to be a valid C-style string based on the pre-condition.
    let name = unsafe { CStr::from_ptr(name) }.to_str();
    if name.is_err() {
        return avifCodecChoice::Auto;
    }
    avifCodecChoice::from_name(name.unwrap())
}

/// # Safety
/// C API function that does not do any unsafe operations.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifCodecName(
    _choice: avifCodecChoice,
    requiredFlags: avifCodecFlags,
) -> *const c_char {
    // This function will just return "dav1d" or "aom" based on whether encoder or decoder is being
    // queried. It simply exists for compatibility with libavif.
    CStr::from_bytes_with_nul(if (requiredFlags & avifCodecFlag::CanEncode as u32) != 0 {
        b"aom\0"
    } else {
        b"dav1d\0"
    })
    .unwrap()
    .as_ptr()
}

/// # Safety
/// C API function that does not do any unsafe operations.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifPixelFormatToString(format: PixelFormat) -> *const c_char {
    CStr::from_bytes_with_nul(match format {
        PixelFormat::Yuv444 => b"YUV444\0",
        PixelFormat::Yuv422 => b"YUV422\0",
        PixelFormat::Yuv420 => b"YUV420\0",
        PixelFormat::Yuv400 => b"YUV400\0",
        PixelFormat::AndroidP010 => b"P010\0",
        PixelFormat::AndroidNv12 => b"NV12\0",
        PixelFormat::AndroidNv21 => b"NV21\0",
        _ => b"Unknown\0",
    })
    .unwrap()
    .as_ptr()
}

pub(crate) fn to_avifBool(val: bool) -> avifBool {
    if val {
        AVIF_TRUE
    } else {
        AVIF_FALSE
    }
}

const RESULT_TO_STRING: &[&str] = &[
    "Ok\0",
    "Unknown Error\0",
    "Invalid ftyp\0",
    "No content\0",
    "No YUV format selected\0",
    "Reformat failed\0",
    "Unsupported depth\0",
    "Encoding of color planes failed\0",
    "Encoding of alpha plane failed\0",
    "BMFF parsing failed\0",
    "Missing or empty image item\0",
    "Decoding of color planes failed\0",
    "Decoding of alpha plane failed\0",
    "Color and alpha planes size mismatch\0",
    "Plane sizes don't match ispe values\0",
    "No codec available\0",
    "No images remaining\0",
    "Invalid Exif payload\0",
    "Invalid image grid\0",
    "Invalid codec-specific option\0",
    "Truncated data\0",
    "IO not set\0",
    "IO Error\0",
    "Waiting on IO\0",
    "Invalid argument\0",
    "Not implemented\0",
    "Out of memory\0",
    "Cannot change some setting during encoding\0",
    "The image is incompatible with already encoded images\0",
    "Encoding of gain map planes failed\0",
    "Decoding of gain map planes failed\0",
    "Invalid tone mapped image item\0",
];

/// # Safety
/// C API function.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifResultToString(res: avifResult) -> *const c_char {
    // SAFETY: Constructs a CStr from a static list of strings above.
    unsafe {
        std::ffi::CStr::from_bytes_with_nul_unchecked(RESULT_TO_STRING[res.as_usize()].as_bytes())
            .as_ptr() as *const _
    }
}

pub type avifCropRect = CropRect;

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if cropRect is not null, it has to point to a valid avifCropRect object.
/// - if clap is not null, it has to point to a valid avifCleanApertureBox object.
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifCropRectConvertCleanApertureBox(
    cropRect: *mut avifCropRect,
    clap: *const avifCleanApertureBox,
    imageW: u32,
    imageH: u32,
    yuvFormat: PixelFormat,
    _diag: *mut avifDiagnostics,
) -> avifBool {
    if cropRect.is_null() || clap.is_null() {
        return AVIF_FALSE;
    }
    let rust_clap: CleanAperture = deref_const!(clap).into();
    let rect = deref_mut!(cropRect);
    *rect = match CropRect::create_from(&rust_clap, imageW, imageH, yuvFormat) {
        Ok(x) => x,
        Err(_) => return AVIF_FALSE,
    };
    AVIF_TRUE
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if cropRect is not null, it has to point to a valid avifCropRect object.
/// - if clap is not null, it has to point to a valid avifCleanApertureBox object.
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifCleanApertureBoxConvertCropRect(
    clap: *mut avifCleanApertureBox,
    cropRect: *const avifCropRect,
    imageW: u32,
    imageH: u32,
    yuvFormat: PixelFormat,
    _diag: *mut avifDiagnostics,
) -> avifBool {
    if cropRect.is_null() || clap.is_null() {
        return AVIF_FALSE;
    }
    *deref_mut!(clap) =
        match CleanAperture::create_from(deref_const!(cropRect), imageW, imageH, yuvFormat) {
            Ok(x) => (&Some(x)).into(),
            Err(_) => return AVIF_FALSE,
        };
    AVIF_TRUE
}

// Constants and definitions from libavif that are not used in rust.

pub const AVIF_PLANE_COUNT_YUV: usize = 3;
pub const AVIF_REPETITION_COUNT_INFINITE: i32 = -1;
pub const AVIF_REPETITION_COUNT_UNKNOWN: i32 = -2;

/// cbindgen:rename-all=ScreamingSnakeCase
#[repr(C)]
pub enum avifPlanesFlag {
    AvifPlanesYuv = 1 << 0,
    AvifPlanesA = 1 << 1,
    AvifPlanesAll = 0xFF,
}
pub type avifPlanesFlags = u32;

/// cbindgen:rename-all=ScreamingSnakeCase
#[repr(C)]
pub enum avifChannelIndex {
    AvifChanY = 0,
    AvifChanU = 1,
    AvifChanV = 2,
    AvifChanA = 3,
}

#[repr(C)]
pub struct avifPixelFormatInfo {
    monochrome: avifBool,
    chromaShiftX: c_int,
    chromaShiftY: c_int,
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if info is not null, it has to point to a valid avifPixelFormatInfo object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifGetPixelFormatInfo(
    format: PixelFormat,
    info: *mut avifPixelFormatInfo,
) {
    check_pointer_or_return!(info);
    let info = deref_mut!(info);
    match format {
        PixelFormat::Yuv444 => {
            info.chromaShiftX = 0;
            info.chromaShiftY = 0;
            info.monochrome = AVIF_FALSE;
        }
        PixelFormat::Yuv422 => {
            info.chromaShiftX = 1;
            info.chromaShiftY = 0;
            info.monochrome = AVIF_FALSE;
        }
        PixelFormat::Yuv420 => {
            info.chromaShiftX = 1;
            info.chromaShiftY = 1;
            info.monochrome = AVIF_FALSE;
        }
        PixelFormat::Yuv400 => {
            info.chromaShiftX = 1;
            info.chromaShiftY = 1;
            info.monochrome = AVIF_TRUE;
        }
        _ => {}
    }
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifDiagnosticsClearError(diag: *mut avifDiagnostics) {
    check_pointer_or_return!(diag);
    deref_mut!(diag).error[0] = 0;
}

#[repr(C)]
pub enum avifCodecFlag {
    CanDecode = (1 << 0),
    CanEncode = (1 << 1),
}
pub type avifCodecFlags = u32;

pub const AVIF_TRANSFORM_NONE: u32 = 0;
pub const AVIF_TRANSFORM_PASP: u32 = 1 << 0;
pub const AVIF_TRANSFORM_CLAP: u32 = 1 << 1;
pub const AVIF_TRANSFORM_IROT: u32 = 1 << 2;
pub const AVIF_TRANSFORM_IMIR: u32 = 1 << 3;
pub type avifTransformFlags = u32;

pub const AVIF_COLOR_PRIMARIES_BT709: u16 = 1;
pub const AVIF_COLOR_PRIMARIES_IEC61966_2_4: u16 = 1;
pub const AVIF_COLOR_PRIMARIES_BT2100: u16 = 9;
pub const AVIF_COLOR_PRIMARIES_DCI_P3: u16 = 12;
pub const AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084: u16 = 16;

/// # Safety
/// C API function that does not perform any unsafe operation.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifAlloc(size: usize) -> *mut c_void {
    let mut data: Vec<u8> = Vec::new();
    if data.try_reserve_exact(size).is_err() {
        return std::ptr::null_mut();
    }
    data.resize(size, 0);
    let mut boxed_slice = data.into_boxed_slice();
    let ptr = boxed_slice.as_mut_ptr();
    std::mem::forget(boxed_slice);
    ptr as *mut c_void
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if p is not null, it has to point to a buffer allocated by crabby_avifAlloc.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifFree(p: *mut c_void) {
    if !p.is_null() {
        // SAFETY: Safe because of the pre-condition and the null check.
        let _ = unsafe { Box::from_raw(p as *mut u8) };
    }
}

pub const AVIF_ADD_IMAGE_FLAG_NONE: u32 = 0;
pub const AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME: u32 = 1 << 0;
pub const AVIF_ADD_IMAGE_FLAG_SINGLE: u32 = 1 << 1;
pub type avifAddImageFlags = u32;

pub const AVIF_QUALITY_WORST: u32 = 0;
pub const AVIF_QUALITY_BEST: u32 = 100;
pub const AVIF_QUALITY_LOSSLESS: u32 = 100;
pub const AVIF_QUALITY_DEFAULT: i32 = -1;

pub const AVIF_QUANTIZER_WORST_QUALITY: u32 = 63;
pub const AVIF_QUANTIZER_BEST_QUALITY: u32 = 0;
pub const AVIF_QUANTIZER_LOSSLESS: u32 = 0;

pub const AVIF_SPEED_SLOWEST: u32 = 0;
pub const AVIF_SPEED_FASTEST: u32 = 10;
pub const AVIF_SPEED_DEFAULT: u32 = 6;

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if outBuffer is not null, it has to be point a valid char buffer of size at least 256.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifCodecVersions(outBuffer: *mut c_char) {
    if outBuffer.is_null() {
        return;
    }
    let versions = match CString::new(codec_versions()) {
        Ok(s) => s,
        Err(_) => return,
    };
    let bytes = versions.as_bytes_with_nul();
    if bytes.len() > 256 {
        return;
    }
    // SAFETY: Safe because of the pre-condition, the null check and the size check above.
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr() as _, outBuffer, bytes.len());
    }
}

/// # Safety
/// C API function that does not perform any unsafe operations.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifLibYUVVersion() -> c_uint {
    #[cfg(feature = "libyuv")]
    return libyuv_sys::bindings::LIBYUV_VERSION as _;
    #[cfg(not(feature = "libyuv"))]
    return 0;
}
