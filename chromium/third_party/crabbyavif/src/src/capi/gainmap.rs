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

use crate::decoder::gainmap::*;
use crate::image::YuvRange;
use crate::parser::mp4box::*;
use crate::*;

pub type avifContentLightLevelInformationBox = ContentLightLevelInformation;

#[repr(C)]
#[derive(Debug, Default)]
pub struct avifGainMapMetadata {
    pub gainMapMinN: [i32; 3],
    pub gainMapMinD: [u32; 3],
    pub gainMapMaxN: [i32; 3],
    pub gainMapMaxD: [u32; 3],
    pub gainMapGammaN: [u32; 3],
    pub gainMapGammaD: [u32; 3],
    pub baseOffsetN: [i32; 3],
    pub baseOffsetD: [u32; 3],
    pub alternateOffsetN: [i32; 3],
    pub alternateOffsetD: [u32; 3],
    pub baseHdrHeadroomN: u32,
    pub baseHdrHeadroomD: u32,
    pub alternateHdrHeadroomN: u32,
    pub alternateHdrHeadroomD: u32,
    pub useBaseColorSpace: avifBool,
}

impl From<&GainMapMetadata> for avifGainMapMetadata {
    fn from(m: &GainMapMetadata) -> Self {
        avifGainMapMetadata {
            gainMapMinN: [m.min[0].0, m.min[1].0, m.min[2].0],
            gainMapMinD: [m.min[0].1, m.min[1].1, m.min[2].1],
            gainMapMaxN: [m.max[0].0, m.max[1].0, m.max[2].0],
            gainMapMaxD: [m.max[0].1, m.max[1].1, m.max[2].1],
            gainMapGammaN: [m.gamma[0].0, m.gamma[1].0, m.gamma[2].0],
            gainMapGammaD: [m.gamma[0].1, m.gamma[1].1, m.gamma[2].1],
            baseOffsetN: [m.base_offset[0].0, m.base_offset[1].0, m.base_offset[2].0],
            baseOffsetD: [m.base_offset[0].1, m.base_offset[1].1, m.base_offset[2].1],
            alternateOffsetN: [
                m.alternate_offset[0].0,
                m.alternate_offset[1].0,
                m.alternate_offset[2].0,
            ],
            alternateOffsetD: [
                m.alternate_offset[0].1,
                m.alternate_offset[1].1,
                m.alternate_offset[2].1,
            ],
            baseHdrHeadroomN: m.base_hdr_headroom.0,
            baseHdrHeadroomD: m.base_hdr_headroom.1,
            alternateHdrHeadroomN: m.alternate_hdr_headroom.0,
            alternateHdrHeadroomD: m.alternate_hdr_headroom.1,
            useBaseColorSpace: m.use_base_color_space as avifBool,
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct avifGainMap {
    pub image: *mut avifImage,
    pub metadata: avifGainMapMetadata,
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
            metadata: avifGainMapMetadata::default(),
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
            metadata: (&gainmap.metadata).into(),
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
