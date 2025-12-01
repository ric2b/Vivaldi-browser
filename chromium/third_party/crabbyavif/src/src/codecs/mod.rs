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
#[allow(dead_code)]
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
#[allow(dead_code)]
#[cfg(feature = "encoder")]
#[derive(Clone, Default, PartialEq)]
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
    pub codec_specific_options: CodecSpecificOptions,
}

#[cfg(feature = "encoder")]
#[allow(dead_code)] // These functions are used only in tests and when aom is enabled.
impl EncoderConfig {
    pub(crate) fn codec_specific_option(&self, category: Category, key: String) -> Option<String> {
        match self
            .codec_specific_options
            .get(&(Some(category), key.clone()))
        {
            Some(value) => Some(value.clone()),
            None => self
                .codec_specific_options
                .get(&(None, key.clone()))
                .cloned(),
        }
    }

    pub(crate) fn codec_specific_options(&self, category: Category) -> Vec<(String, String)> {
        let options: Vec<(String, String)> = self
            .codec_specific_options
            .iter()
            .filter(|(key, _value)| {
                // If there is a key in a requested category, return it. Otherwise, return the
                // value from the "None" category only if there is no value in the requested
                // category.
                key.0 == Some(category)
                    || (key.0.is_none()
                        && !self
                            .codec_specific_options
                            .contains_key(&(Some(category), key.1.clone())))
            })
            .map(|(key, value)| (key.1.clone(), value.clone()))
            .collect();
        options
    }

    pub(crate) fn min_max_quantizers(&self) -> (u32, u32) {
        if self.quantizer == 0 {
            (0, 0)
        } else {
            (
                std::cmp::max(self.quantizer - 4, 0) as u32,
                std::cmp::min(self.quantizer + 4, 63) as u32,
            )
        }
    }
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

#[cfg(feature = "encoder")]
#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    #[test]
    fn codec_specific_options() {
        let codec_specific_options = HashMap::from([
            (
                (Some(Category::Color), String::from("abcd")),
                String::from("color_value1"),
            ),
            ((None, String::from("abcd")), String::from("generic_value1")),
            (
                (Some(Category::Alpha), String::from("efgh")),
                String::from("alpha_value1"),
            ),
            ((None, String::from("hjkl")), String::from("generic_value2")),
        ]);
        let config = EncoderConfig {
            codec_specific_options,
            ..Default::default()
        };

        assert_eq!(
            config.codec_specific_option(Category::Color, String::from("abcd")),
            Some(String::from("color_value1")),
        );
        assert_eq!(
            config.codec_specific_option(Category::Alpha, String::from("abcd")),
            Some(String::from("generic_value1")),
        );
        assert_eq!(
            config.codec_specific_option(Category::Gainmap, String::from("abcd")),
            Some(String::from("generic_value1")),
        );
        assert_eq!(
            config.codec_specific_option(Category::Color, String::from("hjkl")),
            Some(String::from("generic_value2")),
        );

        let mut actual = config.codec_specific_options(Category::Color);
        actual.sort();
        let mut expected = vec![
            (String::from("hjkl"), String::from("generic_value2")),
            (String::from("abcd"), String::from("color_value1")),
        ];
        expected.sort();
        assert_eq!(expected, actual);

        actual = config.codec_specific_options(Category::Alpha);
        actual.sort();
        expected = vec![
            (String::from("hjkl"), String::from("generic_value2")),
            (String::from("efgh"), String::from("alpha_value1")),
            (String::from("abcd"), String::from("generic_value1")),
        ];
        expected.sort();
        assert_eq!(expected, actual);
    }
}
