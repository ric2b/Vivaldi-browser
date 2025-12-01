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

use crate::decoder::Image;
use crate::image::YuvRange;
use crate::utils::*;
use crate::*;

#[derive(Debug, Default)]
pub struct GainMapMetadata {
    pub min: [Fraction; 3],
    pub max: [Fraction; 3],
    pub gamma: [UFraction; 3],
    pub base_offset: [Fraction; 3],
    pub alternate_offset: [Fraction; 3],
    pub base_hdr_headroom: UFraction,
    pub alternate_hdr_headroom: UFraction,
    pub use_base_color_space: bool,
}

impl GainMapMetadata {
    pub(crate) fn is_valid(&self) -> AvifResult<()> {
        for i in 0..3 {
            self.min[i].is_valid()?;
            self.max[i].is_valid()?;
            self.gamma[i].is_valid()?;
            self.base_offset[i].is_valid()?;
            self.alternate_offset[i].is_valid()?;
            if self.max[i].as_f64()? < self.min[i].as_f64()? || self.gamma[i].0 == 0 {
                return Err(AvifError::InvalidArgument);
            }
        }
        self.base_hdr_headroom.is_valid()?;
        self.alternate_hdr_headroom.is_valid()?;
        Ok(())
    }
}

#[derive(Default)]
pub struct GainMap {
    pub image: Image,
    pub metadata: GainMapMetadata,

    pub alt_icc: Vec<u8>,
    pub alt_color_primaries: ColorPrimaries,
    pub alt_transfer_characteristics: TransferCharacteristics,
    pub alt_matrix_coefficients: MatrixCoefficients,
    pub alt_yuv_range: YuvRange,

    pub alt_plane_count: u8,
    pub alt_plane_depth: u8,

    pub alt_clli: ContentLightLevelInformation,
}
