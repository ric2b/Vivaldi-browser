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
use super::io::*;
use super::types::*;

use crate::gainmap::*;
use crate::image::YuvRange;
use crate::utils::*;
use crate::*;

pub type avifContentLightLevelInformationBox = ContentLightLevelInformation;
pub type avifSignedFraction = Fraction;
pub type avifUnsignedFraction = UFraction;

#[repr(C)]
#[derive(Debug)]
pub struct avifGainMap {
    pub image: *mut avifImage,
    pub gainMapMin: [avifSignedFraction; 3],
    pub gainMapMax: [avifSignedFraction; 3],
    pub gainMapGamma: [avifUnsignedFraction; 3],
    pub baseOffset: [avifSignedFraction; 3],
    pub alternateOffset: [avifSignedFraction; 3],
    pub baseHdrHeadroom: avifUnsignedFraction,
    pub alternateHdrHeadroom: avifUnsignedFraction,
    pub useBaseColorSpace: avifBool,
    pub altICC: avifRWData,
    pub altColorPrimaries: ColorPrimaries,
    pub altTransferCharacteristics: TransferCharacteristics,
    pub altMatrixCoefficients: MatrixCoefficients,
    pub altYUVRange: YuvRange,
    pub altDepth: u32,
    pub altPlaneCount: u32,
    pub altCLLI: avifContentLightLevelInformationBox,
}

impl Default for avifGainMap {
    fn default() -> Self {
        avifGainMap {
            image: std::ptr::null_mut(),
            gainMapMin: [Fraction(1, 1), Fraction(1, 1), Fraction(1, 1)],
            gainMapMax: [Fraction(1, 1), Fraction(1, 1), Fraction(1, 1)],
            gainMapGamma: [UFraction(1, 1), UFraction(1, 1), UFraction(1, 1)],
            baseOffset: [Fraction(1, 64), Fraction(1, 64), Fraction(1, 64)],
            alternateOffset: [Fraction(1, 64), Fraction(1, 64), Fraction(1, 64)],
            baseHdrHeadroom: UFraction(0, 1),
            alternateHdrHeadroom: UFraction(1, 1),
            useBaseColorSpace: to_avifBool(false),
            altICC: avifRWData::default(),
            altColorPrimaries: ColorPrimaries::default(),
            altTransferCharacteristics: TransferCharacteristics::default(),
            altMatrixCoefficients: MatrixCoefficients::default(),
            altYUVRange: YuvRange::Full,
            altDepth: 0,
            altPlaneCount: 0,
            altCLLI: Default::default(),
        }
    }
}

impl From<&GainMap> for avifGainMap {
    fn from(gainmap: &GainMap) -> Self {
        avifGainMap {
            gainMapMin: gainmap.metadata.min,
            gainMapMax: gainmap.metadata.max,
            gainMapGamma: gainmap.metadata.gamma,
            baseOffset: gainmap.metadata.base_offset,
            alternateOffset: gainmap.metadata.alternate_offset,
            baseHdrHeadroom: gainmap.metadata.base_hdr_headroom,
            alternateHdrHeadroom: gainmap.metadata.alternate_hdr_headroom,
            useBaseColorSpace: gainmap.metadata.use_base_color_space as avifBool,
            altICC: (&gainmap.alt_icc).into(),
            altColorPrimaries: gainmap.alt_color_primaries,
            altTransferCharacteristics: gainmap.alt_transfer_characteristics,
            altMatrixCoefficients: gainmap.alt_matrix_coefficients,
            altYUVRange: gainmap.alt_yuv_range,
            altDepth: u32::from(gainmap.alt_plane_depth),
            altPlaneCount: u32::from(gainmap.alt_plane_count),
            altCLLI: gainmap.alt_clli,
            ..Self::default()
        }
    }
}

impl From<&avifGainMap> for GainMap {
    fn from(gainmap: &avifGainMap) -> Self {
        Self {
            image: deref_const!(gainmap.image).into(),
            metadata: GainMapMetadata {
                min: gainmap.gainMapMin,
                max: gainmap.gainMapMax,
                gamma: gainmap.gainMapGamma,
                base_offset: gainmap.baseOffset,
                alternate_offset: gainmap.alternateOffset,
                base_hdr_headroom: gainmap.baseHdrHeadroom,
                alternate_hdr_headroom: gainmap.alternateHdrHeadroom,
                use_base_color_space: gainmap.useBaseColorSpace != 0,
            },
            alt_icc: (&gainmap.altICC).into(),
            alt_color_primaries: gainmap.altColorPrimaries,
            alt_transfer_characteristics: gainmap.altTransferCharacteristics,
            alt_matrix_coefficients: gainmap.altMatrixCoefficients,
            alt_yuv_range: gainmap.altYUVRange,
            alt_plane_depth: gainmap.altDepth as u8,
            alt_plane_count: gainmap.altPlaneCount as u8,
            alt_clli: gainmap.altCLLI,
        }
    }
}

/// # Safety
/// C API function that does not perform any unsafe operation.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifGainMapCreate() -> *mut avifGainMap {
    Box::into_raw(Box::<avifGainMap>::default())
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if gainmap is not null, it has to point to a buffer allocated by crabby_avifGainMapCreate.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifGainMapDestroy(gainmap: *mut avifGainMap) {
    if gainmap.is_null() {
        return;
    }
    // SAFETY: Safe because of the pre-condition and the null check.
    let gainmap = unsafe { Box::from_raw(gainmap) };
    if !gainmap.image.is_null() {
        // SAFETY: Pre-conditions are met to call this function.
        unsafe {
            crabby_avifImageDestroy(gainmap.image);
        }
    }
}
