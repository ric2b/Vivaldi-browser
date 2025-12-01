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

use crate::image::YuvRange;
use crate::utils::*;
use crate::*;

#[derive(Clone, Debug, Default, PartialEq)]
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
                return AvifError::invalid_argument();
            }
        }
        self.base_hdr_headroom.is_valid()?;
        self.alternate_hdr_headroom.is_valid()?;
        Ok(())
    }

    #[cfg(feature = "encoder")]
    fn identical_channels(&self) -> bool {
        self.min[0] == self.min[1]
            && self.min[0] == self.min[2]
            && self.max[0] == self.max[1]
            && self.max[0] == self.max[2]
            && self.gamma[0] == self.gamma[1]
            && self.gamma[0] == self.gamma[2]
            && self.base_offset[0] == self.base_offset[1]
            && self.base_offset[0] == self.base_offset[2]
            && self.alternate_offset[0] == self.alternate_offset[1]
            && self.alternate_offset[0] == self.alternate_offset[2]
    }

    #[cfg(feature = "encoder")]
    pub(crate) fn channel_count(&self) -> u8 {
        if self.identical_channels() {
            1
        } else {
            3
        }
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

impl PartialEq for GainMap {
    fn eq(&self, other: &Self) -> bool {
        self.metadata == other.metadata
            && self.alt_icc == other.alt_icc
            && self.alt_color_primaries == other.alt_color_primaries
            && self.alt_transfer_characteristics == other.alt_transfer_characteristics
            && self.alt_matrix_coefficients == other.alt_matrix_coefficients
            && self.alt_yuv_range == other.alt_yuv_range
            && self.alt_plane_count == other.alt_plane_count
            && self.alt_plane_depth == other.alt_plane_depth
            && self.alt_clli == other.alt_clli
    }
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "encoder")]
    use super::*;

    #[test]
    #[cfg(feature = "encoder")]
    fn identical_channels() {
        let mut metadata = GainMapMetadata::default();
        assert!(metadata.identical_channels());
        assert_eq!(metadata.channel_count(), 1);
        for i in 0..3 {
            metadata = GainMapMetadata::default();
            metadata.min[i] = Fraction(1, 2);
            assert!(!metadata.identical_channels());
            assert_eq!(metadata.channel_count(), 3);
        }
        for i in 0..3 {
            metadata = GainMapMetadata::default();
            metadata.max[i] = Fraction(1, 2);
            assert!(!metadata.identical_channels());
            assert_eq!(metadata.channel_count(), 3);
        }
        for i in 0..3 {
            metadata = GainMapMetadata::default();
            metadata.gamma[i] = UFraction(1, 2);
            assert!(!metadata.identical_channels());
            assert_eq!(metadata.channel_count(), 3);
        }
        for i in 0..3 {
            metadata = GainMapMetadata::default();
            metadata.base_offset[i] = Fraction(1, 2);
            assert!(!metadata.identical_channels());
            assert_eq!(metadata.channel_count(), 3);
        }
        for i in 0..3 {
            metadata = GainMapMetadata::default();
            metadata.alternate_offset[i] = Fraction(1, 2);
            assert!(!metadata.identical_channels());
            assert_eq!(metadata.channel_count(), 3);
        }
    }
}
