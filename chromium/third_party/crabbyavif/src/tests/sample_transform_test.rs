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

#![cfg(feature = "png")]
#![cfg(feature = "encoder")]

mod utils;
use crabby_avif::utils::reader::png::PngReader;
use crabby_avif::utils::reader::Config;
use crabby_avif::utils::reader::Reader;
use crabby_avif::*;
use utils::*;

#[test]
fn lossless_sample_transform_roundtrip() -> AvifResult<()> {
    if !HAS_ENCODER {
        return Ok(());
    }
    let input_file = get_test_file("weld_16bit.png");
    let (image, _) = PngReader::create(&input_file)?.read_frame(&Config {
        yuv_format: Some(PixelFormat::Yuv444),
        matrix_coefficients: Some(MatrixCoefficients::Identity),
        ..Default::default()
    })?;
    assert_eq!(image.depth, 16);

    let settings = encoder::Settings {
        speed: Some(10),
        mutable: encoder::MutableSettings {
            quality: 100,
            ..Default::default()
        },
        sample_transform_recipe: SampleTransformRecipe::BitDepthExtension8b8b,
        ..Default::default()
    };
    let mut encoder = encoder::Encoder::create_with_settings(&settings)?;
    encoder.add_image(&image)?;
    let edata = encoder.finish()?;
    assert!(!edata.is_empty());

    if !HAS_DECODER {
        return Ok(());
    }

    let mut decoder = decoder::Decoder::default();
    decoder.set_io_vec(edata.clone());
    decoder.settings.allow_sample_transform = true;
    assert!(decoder.parse().is_ok());
    assert!(decoder.next_image().is_ok());
    let decoded_image = decoder.image().unwrap();
    assert!(are_images_equal(&image, decoded_image)?);
    Ok(())
}
