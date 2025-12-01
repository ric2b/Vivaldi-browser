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

#![cfg(feature = "encoder")]

use crabby_avif::decoder::{CompressionFormat, ImageContentType};
use crabby_avif::gainmap::{GainMap, GainMapMetadata};
use crabby_avif::image::*;
use crabby_avif::utils::{Fraction, UFraction};
use crabby_avif::*;

mod utils;
use utils::*;

use test_case::test_matrix;

#[test_matrix(
    [8, 10, 12],
    [PixelFormat::Yuv420, PixelFormat::Yuv422, PixelFormat::Yuv444, PixelFormat::Yuv400],
    [YuvRange::Limited, YuvRange::Full],
    [false, true],
    [false, true]
)]
fn encode_decode(
    depth: u8,
    yuv_format: PixelFormat,
    yuv_range: YuvRange,
    alpha: bool,
    gainmap: bool,
) -> AvifResult<()> {
    if !HAS_ENCODER {
        return Ok(());
    }
    let mut input_image = generate_gradient_image(100, 200, depth, yuv_format, yuv_range, alpha)?;
    let input_gainmap = if gainmap {
        input_image.transfer_characteristics = TransferCharacteristics::Srgb;
        input_image.clli = Some(ContentLightLevelInformation {
            max_cll: 2,
            max_pall: 1,
        });
        Some(GainMap {
            image: generate_gradient_image(6, 17, 8, PixelFormat::Yuv420, YuvRange::Full, false)?,
            alt_plane_count: 3,
            alt_matrix_coefficients: MatrixCoefficients::Smpte2085,
            alt_clli: ContentLightLevelInformation {
                max_cll: 10,
                max_pall: 5,
            },
            alt_plane_depth: 10,
            alt_color_primaries: ColorPrimaries::Bt2020,
            alt_transfer_characteristics: TransferCharacteristics::Pq,
            metadata: GainMapMetadata {
                use_base_color_space: true,
                base_hdr_headroom: UFraction(0, 1),
                alternate_hdr_headroom: UFraction(6, 2),
                base_offset: [Fraction(0, 1000), Fraction(10, 1000), Fraction(20, 1000)],
                alternate_offset: [Fraction(0, 1000), Fraction(20, 1000), Fraction(40, 1000)],
                gamma: [UFraction(1, 1), UFraction(1, 2), UFraction(1, 3)],
                min: [Fraction(-1, 1), Fraction(-1, 2), Fraction(-1, 3)],
                max: [Fraction(11, 1), Fraction(12, 2), Fraction(13, 3)],
            },
            ..Default::default()
        })
    } else {
        None
    };

    let settings = encoder::Settings {
        speed: Some(10),
        header_format: HeaderFormat::Mini,
        mutable: encoder::MutableSettings {
            quality: 90,
            quality_gainmap: 90,
            quality_alpha: 90,
            ..Default::default()
        },
        ..Default::default()
    };
    let mut encoder = encoder::Encoder::create_with_settings(&settings)?;
    if let Some(input_gainmap) = &input_gainmap {
        encoder.add_image_gainmap(&input_image, input_gainmap)?;
    } else {
        encoder.add_image(&input_image)?;
    };
    let edata = encoder.finish()?;
    assert!(!edata.is_empty());
    // Make sure a MinimizedImageBox was written and not just a regular MetaBox.
    assert_eq!(&edata.as_slice()[4..16], "ftypmif3avif".as_bytes());

    let mut decoder = decoder::Decoder::default();
    decoder.settings.image_content_to_decode = ImageContentType::All;
    decoder.set_io_vec(edata);
    assert_eq!(decoder.parse(), Ok(()));
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    assert_eq!(decoder.image_count(), 1);

    let image = decoder.image().expect("image was none");
    assert_eq!(image.alpha_present, alpha);
    assert!(!image.image_sequence_track_present);
    assert_eq!(image.width, input_image.width);
    assert_eq!(image.height, input_image.height);
    assert_eq!(image.depth, depth);
    assert_eq!(image.yuv_format, yuv_format);
    assert_eq!(image.yuv_range, yuv_range);
    assert_eq!(image.pasp, input_image.pasp);
    assert_eq!(image.clli, input_image.clli);

    if let Some(input_gainmap) = &input_gainmap {
        assert!(decoder.gainmap_present());
        let gainmap = decoder.gainmap();
        assert_eq!(gainmap.image.width, input_gainmap.image.width);
        assert_eq!(gainmap.image.height, input_gainmap.image.height);
        assert_eq!(gainmap.image.depth, input_gainmap.image.depth);
        assert_eq!(gainmap.image.yuv_format, input_gainmap.image.yuv_format);
        assert_eq!(gainmap.image.yuv_range, input_gainmap.image.yuv_range);
    };

    if !HAS_DECODER {
        return Ok(());
    }
    assert!(decoder.next_image().is_ok());
    let image = decoder.image().expect("image was none");
    assert!(psnr(image, &input_image)? >= 50.0);
    if let Some(input_gainmap) = &input_gainmap {
        assert!(psnr(&decoder.gainmap().image, &input_gainmap.image)? >= 50.0);
    };
    Ok(())
}
