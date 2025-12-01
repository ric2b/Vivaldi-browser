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

#[cfg(feature = "dav1d")]
pub mod dav1d;

#[cfg(feature = "libgav1")]
pub mod libgav1;

#[cfg(feature = "android_mediacodec")]
pub mod android_mediacodec;

#[cfg(feature = "aom")]
pub mod aom;

use crate::decoder::CodecChoice;
use crate::decoder::GridImageHelper;
use crate::image::Image;
use crate::parser::mp4box::CodecConfiguration;
use crate::AndroidMediaCodecOutputColorFormat;
use crate::AvifResult;
use crate::Category;

#[cfg(feature = "encoder")]
use crate::encoder::*;

use std::num::NonZero;

// Not all fields of this struct are used in all the configurations.
#[allow(unused)]
#[derive(Clone, Default)]
pub(crate) struct DecoderConfig {
    pub operating_point: u8,
    pub all_layers: bool,
    pub width: u32,
    pub height: u32,
    pub depth: u8,
    pub max_threads: u32,
    pub image_size_limit: Option<NonZero<u32>>,
    pub max_input_size: usize,
    pub codec_config: CodecConfiguration,
    pub category: Category,
    pub android_mediacodec_output_color_format: AndroidMediaCodecOutputColorFormat,
}

pub(crate) trait Decoder {
    fn codec(&self) -> CodecChoice;
    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()>;
    // Decode a single image and write the output into |image|.
    fn get_next_image(
        &mut self,
        av1_payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()>;
    // Decode a list of input images and outputs them into the |grid_image_helper|.
    fn get_next_image_grid(
        &mut self,
        payloads: &[Vec<u8>],
        spatial_id: u8,
        grid_image_helper: &mut GridImageHelper,
    ) -> AvifResult<()>;
    // Destruction must be implemented using Drop.
}

// Not all fields of this struct are used in all the configurations.
#[allow(unused)]
#[cfg(feature = "encoder")]
#[derive(Clone, Copy, Default, PartialEq)]
pub(crate) struct EncoderConfig {
    pub tile_rows_log2: i32,
    pub tile_columns_log2: i32,
    pub quantizer: i32,
    pub disable_lagged_output: bool,
    pub is_single_image: bool,
    pub speed: Option<u32>,
    pub extra_layer_count: u32,
    pub threads: u32,
    pub scaling_mode: ScalingMode,
}

#[cfg(feature = "encoder")]
pub(crate) trait Encoder {
    fn encode_image(
        &mut self,
        image: &Image,
        category: Category,
        config: &EncoderConfig,
        output_samples: &mut Vec<crate::encoder::Sample>,
    ) -> AvifResult<()>;
    fn finish(&mut self, output_samples: &mut Vec<crate::encoder::Sample>) -> AvifResult<()>;
    // Destruction must be implemented using Drop.
}
