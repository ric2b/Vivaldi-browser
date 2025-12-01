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

use crabby_avif::decoder::CompressionFormat;
use crabby_avif::decoder::ImageContentType;
use crabby_avif::image::*;
use crabby_avif::reformat::rgb;
use crabby_avif::*;

mod utils;
use utils::*;

use std::cell::RefCell;
use std::rc::Rc;
use test_case::test_case;
use test_case::test_matrix;

// From avifalphanoispetest.cc
#[test]
fn alpha_no_ispe() {
    // See https://github.com/AOMediaCodec/libavif/pull/745.
    let mut decoder = get_decoder("alpha_noispe.avif");
    // By default, non-strict files are refused.
    assert!(matches!(
        decoder.settings.strictness,
        decoder::Strictness::All
    ));
    let res = decoder.parse();
    assert!(matches!(res, Err(AvifError::BmffParseFailed(_))));
    // Allow this kind of file specifically.
    decoder.settings.strictness =
        decoder::Strictness::SpecificExclude(vec![decoder::StrictnessFlag::AlphaIspeRequired]);
    let res = decoder.parse();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert!(image.alpha_present);
    assert!(!image.image_sequence_track_present);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    let alpha_plane = image.plane_data(Plane::A);
    assert!(alpha_plane.is_some());
    assert!(alpha_plane.unwrap().row_bytes > 0);
}

#[test]
fn alpha_premultiplied() {
    let mut decoder = get_decoder("alpha_premultiplied.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert!(image.alpha_present);
    assert!(image.alpha_premultiplied);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert!(image.alpha_present);
    assert!(image.alpha_premultiplied);
    let alpha_plane = image.plane_data(Plane::A);
    assert!(alpha_plane.is_some());
    assert!(alpha_plane.unwrap().row_bytes > 0);
}

// From avifanimationtest.cc
#[test_case("colors-animated-8bpc.avif")]
#[test_case("colors-animated-8bpc-audio.avif")]
fn animated_image(filename: &str) {
    let mut decoder = get_decoder(filename);
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(!image.alpha_present);
    assert!(image.image_sequence_track_present);
    assert_eq!(decoder.image_count(), 5);
    assert_eq!(decoder.repetition_count(), RepetitionCount::Finite(0));
    for i in 0..5 {
        assert_eq!(decoder.nearest_keyframe(i), 0);
    }
    if !HAS_DECODER {
        return;
    }
    for _ in 0..5 {
        assert!(decoder.next_image().is_ok());
    }
}

// From avifanimationtest.cc
#[test_case("colors-animated-8bpc.avif")]
#[test_case("colors-animated-8bpc-audio.avif")]
fn animated_image_with_source_set_to_primary_item(filename: &str) {
    let mut decoder = get_decoder(filename);
    decoder.settings.source = decoder::Source::PrimaryItem;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(!image.alpha_present);
    // This will be reported as true irrespective of the preferred source.
    assert!(image.image_sequence_track_present);
    // imageCount is expected to be 1 because we are using primary item as the
    // preferred source.
    assert_eq!(decoder.image_count(), 1);
    assert_eq!(decoder.repetition_count(), RepetitionCount::Finite(0));
    if !HAS_DECODER {
        return;
    }
    // Get the first (and only) image.
    assert!(decoder.next_image().is_ok());
    // Subsequent calls should not return anything since there is only one
    // image in the preferred source.
    assert!(decoder.next_image().is_err());
}

// From avifanimationtest.cc
#[test]
fn animated_image_with_alpha_and_metadata() {
    let mut decoder = get_decoder("colors-animated-8bpc-alpha-exif-xmp.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(image.alpha_present);
    assert!(image.image_sequence_track_present);
    assert_eq!(decoder.image_count(), 5);
    assert_eq!(decoder.repetition_count(), RepetitionCount::Infinite);
    assert_eq!(image.exif.len(), 1126);
    assert_eq!(image.xmp.len(), 3898);
    if !HAS_DECODER {
        return;
    }
    for _ in 0..5 {
        assert!(decoder.next_image().is_ok());
    }
}

#[test]
fn animated_image_with_depth_and_metadata() {
    // Depth map data is not supported and should be ignored.
    let mut decoder = get_decoder("colors-animated-8bpc-depth-exif-xmp.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(!image.alpha_present);
    assert!(image.image_sequence_track_present);
    assert_eq!(decoder.image_count(), 5);
    assert_eq!(decoder.repetition_count(), RepetitionCount::Infinite);
    assert_eq!(image.exif.len(), 1126);
    assert_eq!(image.xmp.len(), 3898);
    if !HAS_DECODER {
        return;
    }
    for _ in 0..5 {
        assert!(decoder.next_image().is_ok());
    }
}

#[test]
fn animated_image_with_depth_and_metadata_source_set_to_primary_item() {
    // Depth map data is not supported and should be ignored.
    let mut decoder = get_decoder("colors-animated-8bpc-depth-exif-xmp.avif");
    decoder.settings.source = decoder::Source::PrimaryItem;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(!image.alpha_present);
    // This will be reported as true irrespective of the preferred source.
    assert!(image.image_sequence_track_present);
    // imageCount is expected to be 1 because we are using primary item as the
    // preferred source.
    assert_eq!(decoder.image_count(), 1);
    assert_eq!(decoder.repetition_count(), RepetitionCount::Finite(0));
    if !HAS_DECODER {
        return;
    }
    // Get the first (and only) image.
    assert!(decoder.next_image().is_ok());
    // Subsequent calls should not return anything since there is only one
    // image in the preferred source.
    assert!(decoder.next_image().is_err());
}

// From avifkeyframetest.cc
#[test]
fn keyframes() {
    let mut decoder = get_decoder("colors-animated-12bpc-keyframes-0-2-3.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(image.image_sequence_track_present);
    assert_eq!(decoder.image_count(), 5);

    // First frame is always a keyframe.
    assert!(decoder.is_keyframe(0));
    assert_eq!(decoder.nearest_keyframe(0), 0);

    assert!(!decoder.is_keyframe(1));
    assert_eq!(decoder.nearest_keyframe(1), 0);

    assert!(decoder.is_keyframe(2));
    assert_eq!(decoder.nearest_keyframe(2), 2);

    assert!(decoder.is_keyframe(3));
    assert_eq!(decoder.nearest_keyframe(3), 3);

    assert!(!decoder.is_keyframe(4));
    assert_eq!(decoder.nearest_keyframe(4), 3);

    // Not an existing frame.
    assert!(!decoder.is_keyframe(15));
    assert_eq!(decoder.nearest_keyframe(15), 3);
}

// From avifdecodetest.cc
#[test]
fn color_grid_alpha_no_grid() {
    // Test case from https://github.com/AOMediaCodec/libavif/issues/1203.
    let mut decoder = get_decoder("color_grid_alpha_nogrid.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(image.alpha_present);
    assert!(!image.image_sequence_track_present);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    let alpha_plane = image.plane_data(Plane::A);
    assert!(alpha_plane.is_some());
    assert!(alpha_plane.unwrap().row_bytes > 0);
}

#[test_case("paris_icc_exif_xmp.avif")]
#[test_case("sofa_grid1x5_420.avif")]
#[test_case("color_grid_alpha_nogrid.avif")]
#[test_case("seine_sdr_gainmap_srgb.avif")]
fn image_content_to_decode_none(filename: &str) {
    let mut decoder = get_decoder(filename);
    decoder.settings.image_content_to_decode = ImageContentType::None;
    assert!(decoder.parse().is_ok());
    assert!(decoder.next_image().is_err());
}

#[test_case("draw_points_idat.avif")]
#[test_case("draw_points_idat_metasize0.avif")]
#[test_case("draw_points_idat_progressive.avif")]
#[test_case("draw_points_idat_progressive_metasize0.avif")]
fn idat(filename: &str) {
    let mut decoder = get_decoder(filename);
    assert!(decoder.parse().is_ok());
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert_eq!(res, Ok(()));
}

// From avifprogressivetest.cc
#[test_case("progressive_dimension_change.avif", 2, 256, 256; "progressive_dimension_change")]
#[test_case("progressive_layered_grid.avif", 2, 512, 256; "progressive_layered_grid")]
#[test_case("progressive_quality_change.avif", 2, 256, 256; "progressive_quality_change")]
#[test_case("progressive_same_layers.avif", 4, 256, 256; "progressive_same_layers")]
#[test_case("tiger_3layer_1res.avif", 3, 1216, 832; "tiger_3layer_1res")]
#[test_case("tiger_3layer_3res.avif", 3, 1216, 832; "tiger_3layer_3res")]
fn progressive(filename: &str, layer_count: u32, width: u32, height: u32) {
    let mut filename_with_prefix = String::from("progressive/");
    filename_with_prefix.push_str(filename);
    let mut decoder = get_decoder(&filename_with_prefix);

    decoder.settings.allow_progressive = false;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(matches!(
        image.progressive_state,
        decoder::ProgressiveState::Available
    ));

    decoder.settings.allow_progressive = true;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert!(matches!(
        image.progressive_state,
        decoder::ProgressiveState::Active
    ));
    assert_eq!(image.width, width);
    assert_eq!(image.height, height);
    assert_eq!(decoder.image_count(), layer_count);
    if !HAS_DECODER {
        return;
    }
    for _i in 0..decoder.image_count() {
        let res = decoder.next_image();
        assert!(res.is_ok());
        let image = decoder.image().expect("image was none");
        assert_eq!(image.width, width);
        assert_eq!(image.height, height);
    }
}

#[test]
fn decoder_parse_exif_non_zero_tiff_offset() {
    let mut decoder = get_decoder("paris_exif_non_zero_tiff_offset.avif");

    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");

    assert_eq!(image.exif.len(), 1129);
    assert_eq!(image.exif[0], 0);
    assert_eq!(image.exif[1], 0);
    assert_eq!(image.exif[2], 0);
    assert_eq!(image.exif[3], 73);
    assert_eq!(image.exif[4], 73);
    assert_eq!(image.exif[5], 42);
    assert_eq!(image.exif[6], 0);
}

// From avifmetadatatest.cc
#[test]
fn decoder_parse_icc_exif_xmp() {
    // Test case from https://github.com/AOMediaCodec/libavif/issues/1086.
    let mut decoder = get_decoder("paris_icc_exif_xmp.avif");

    decoder.settings.ignore_xmp = true;
    decoder.settings.ignore_exif = true;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");

    assert_eq!(image.icc.len(), 596);
    assert_eq!(image.icc[0], 0);
    assert_eq!(image.icc[1], 0);
    assert_eq!(image.icc[2], 2);
    assert_eq!(image.icc[3], 84);

    assert!(image.exif.is_empty());
    assert!(image.xmp.is_empty());

    decoder.settings.ignore_xmp = false;
    decoder.settings.ignore_exif = false;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");

    assert_eq!(image.exif.len(), 1126);
    assert_eq!(image.exif[0], 73);
    assert_eq!(image.exif[1], 73);
    assert_eq!(image.exif[2], 42);
    assert_eq!(image.exif[3], 0);

    assert_eq!(image.xmp.len(), 3898);
    assert_eq!(image.xmp[0], 60);
    assert_eq!(image.xmp[1], 63);
    assert_eq!(image.xmp[2], 120);
    assert_eq!(image.xmp[3], 112);
}

#[test]
fn decode_gainmap() {
    let filename = "tmap_primary_item.avif";
    let mut decoder = get_decoder(filename);
    let res = decoder.parse();
    assert!(res.is_ok());
    // Gain map found but not decoded.
    assert!(decoder.gainmap_present());
    assert!(
        decoder.gainmap().metadata.base_hdr_headroom.0 != 0
            || decoder.gainmap().metadata.alternate_hdr_headroom.0 != 0
    );
    assert_eq!(decoder.gainmap().image.width, 0);

    // Decode again with image_content_to_decode = ImageContentType::All.
    decoder = get_decoder(filename);
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    // Gain map found and decoded.
    assert!(decoder.gainmap_present());
    assert!(
        decoder.gainmap().metadata.base_hdr_headroom.0 != 0
            || decoder.gainmap().metadata.alternate_hdr_headroom.0 != 0
    );
    assert_ne!(decoder.gainmap().image.width, 0);
}

// From avifgainmaptest.cc
#[test]
fn color_grid_gainmap_different_grid() {
    let mut decoder = get_decoder("color_grid_gainmap_different_grid.avif");
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    // Color+alpha: 4x3 grid of 128x200 tiles.
    assert_eq!(image.width, 128 * 4);
    assert_eq!(image.height, 200 * 3);
    assert_eq!(image.depth, 10);
    // Gain map: 2x2 grid of 64x80 tiles.
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 64 * 2);
    assert_eq!(decoder.gainmap().image.height, 80 * 2);
    assert_eq!(decoder.gainmap().image.depth, 8);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 6);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.1, 2);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    assert!(decoder.gainmap().image.row_bytes[0] > 0);
}

// From avifgainmaptest.cc
#[test]
fn color_grid_alpha_grid_gainmap_nogrid() {
    let mut decoder = get_decoder("color_grid_alpha_grid_gainmap_nogrid.avif");
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    // Color+alpha: 4x3 grid of 128x200 tiles.
    assert_eq!(image.width, 128 * 4);
    assert_eq!(image.height, 200 * 3);
    assert_eq!(image.depth, 10);
    // Gain map: single image of size 64x80.
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 64);
    assert_eq!(decoder.gainmap().image.height, 80);
    assert_eq!(decoder.gainmap().image.depth, 8);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 6);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.1, 2);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    assert!(decoder.gainmap().image.row_bytes[0] > 0);
}

// From avifgainmaptest.cc
#[test]
fn color_nogrid_alpha_nogrid_gainmap_grid() {
    let mut decoder = get_decoder("color_nogrid_alpha_nogrid_gainmap_grid.avif");
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    // Color+alpha: single image of size 128x200.
    assert_eq!(image.width, 128);
    assert_eq!(image.height, 200);
    assert_eq!(image.depth, 10);
    // Gain map: 2x2 grid of 64x80 tiles.
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 64 * 2);
    assert_eq!(decoder.gainmap().image.height, 80 * 2);
    assert_eq!(decoder.gainmap().image.depth, 8);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 6);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.1, 2);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    assert!(decoder.gainmap().image.row_bytes[0] > 0);
}

// From aviftransformtest.cc
#[test]
fn lenient_missing_alpha_transformative_properties() {
    let mut decoder_ref = get_decoder("abc_color_irot_alpha_irot.avif");
    assert!(decoder_ref.parse().is_ok());
    let mut decoder = get_decoder("abc_color_irot_alpha_NOirot.avif");
    assert!(decoder.parse().is_ok());
    if HAS_DECODER {
        assert!(decoder_ref.next_image().is_ok());
        assert!(decoder.next_image().is_ok());
    }
    assert!(are_images_equal(decoder_ref.image().unwrap(), decoder.image().unwrap()).unwrap());
}

// From avifgainmaptest.cc
#[test]
fn gainmap_oriented() {
    let mut decoder = get_decoder("gainmap_oriented.avif");
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert_eq!(image.irot_angle, Some(1));
    assert_eq!(image.imir_axis, Some(0));
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.irot_angle, None);
    assert_eq!(decoder.gainmap().image.imir_axis, None);
}

// From avifgainmaptest.cc
// Tests files with gain maps that should be ignored by the decoder for various
// reasons.
// File with unsupported version field.
#[test_case("unsupported_gainmap_version.avif")]
// File with unsupported minimum version field.
#[test_case("unsupported_gainmap_minimum_version.avif")]
// Missing 'tmap' brand in ftyp box.
#[test_case("seine_sdr_gainmap_notmapbrand.avif")]
// Gain map not present before the base image in 'altr' box.
#[test_case("seine_hdr_gainmap_wrongaltr.avif")]
fn decode_unsupported_version(filename: &str) {
    // Parse with various settings.
    let mut decoder = get_decoder(filename);
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    // Gain map marked as not present.
    assert!(!decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 0);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 0);

    // Decode again with image_content_to_decode = ImageContentType::All.
    decoder = get_decoder(filename);
    decoder.settings.image_content_to_decode = ImageContentType::All;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    // Gain map marked as not present.
    assert!(!decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 0);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 0);
}

// From avifgainmaptest.cc
#[test]
fn decode_unsupported_writer_version_with_extra_bytes() {
    let mut decoder = get_decoder("unsupported_gainmap_writer_version_with_extra_bytes.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    // Decodes successfully: there are extra bytes at the end of the gain map
    // metadata but that's expected as the writer_version field is higher
    // that supported.
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 6);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.1, 2);
}

// From avifgainmaptest.cc
#[test]
fn decode_supported_writer_version_with_extra_bytes() {
    let mut decoder = get_decoder("supported_gainmap_writer_version_with_extra_bytes.avif");
    let res = decoder.parse();
    // Fails to decode: there are extra bytes at the end of the gain map metadata
    // that shouldn't be there.
    assert!(matches!(res, Err(AvifError::InvalidToneMappedImage(_))));
}

// From avifgainmaptest.cc
#[test]
fn decode_ignore_gain_map_but_read_metadata() {
    let mut decoder = get_decoder("seine_sdr_gainmap_srgb.avif");

    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    decoder.image().expect("image was none");
    // Gain map not decoded.
    assert!(decoder.gainmap_present());
    // ... but not decoded because enableDecodingGainMap is false by default.
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().image.row_bytes[0], 0);
    // Check that the gain map metadata WAS populated.
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 13);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.1, 10);
}

// From avifgainmaptest.cc
#[test]
fn decode_ignore_color_and_alpha() {
    let mut decoder = get_decoder("seine_sdr_gainmap_srgb.avif");
    decoder.settings.image_content_to_decode = ImageContentType::GainMap;

    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);

    let image = decoder.image().expect("image was none");
    // Main image metadata is available.
    assert_eq!(image.width, 400);
    // The gain map metadata is available.
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 400);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 13);

    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    let image = decoder.image().expect("image was none");
    assert!(res.is_ok());
    // Main image pixels are not available.
    assert_eq!(image.row_bytes[0], 0);
    // Gain map pixels are available.
    assert!(decoder.gainmap().image.row_bytes[0] > 0);
}

// From avifgainmaptest.cc
#[test_case("paris_icc_exif_xmp.avif")]
#[test_case("sofa_grid1x5_420.avif")]
#[test_case("color_grid_alpha_nogrid.avif")]
#[test_case("seine_sdr_gainmap_srgb.avif")]
fn decode_ignore_all(filename: &str) {
    let mut decoder = get_decoder(filename);
    // Ignore both the main image and the gain map.
    decoder.settings.image_content_to_decode = ImageContentType::None;
    // But do read the gain map metadata

    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    // Main image metadata is available.
    assert!(image.width > 0);
    // But trying to access the next image should give an error because both
    // ignoreColorAndAlpha and enableDecodingGainMap are set.
    let res = decoder.next_image();
    assert!(res.is_err());
}

// From avifcllitest.cc
#[test_case("clli_0_0.avif", 0, 0; "clli_0_0")]
#[test_case("clli_0_1.avif", 0, 1; "clli_0_1")]
#[test_case("clli_0_65535.avif", 0, 65535; "clli_0_65535")]
#[test_case("clli_1_0.avif", 1, 0; "clli_1_0")]
#[test_case("clli_1_1.avif", 1, 1; "clli_1_1")]
#[test_case("clli_1_65535.avif", 1, 65535; "clli_1_65535")]
#[test_case("clli_65535_0.avif", 65535, 0; "clli_65535_0")]
#[test_case("clli_65535_1.avif", 65535, 1; "clli_65535_1")]
#[test_case("clli_65535_65535.avif", 65535, 65535; "clli_65535_65535")]
fn clli(filename: &str, max_cll: u16, max_pall: u16) {
    let mut filename_with_prefix = String::from("clli/");
    filename_with_prefix.push_str(filename);
    let mut decoder = get_decoder(&filename_with_prefix);
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    if max_cll == 0 && max_pall == 0 {
        assert!(image.clli.is_none());
    } else {
        assert!(image.clli.is_some());
        let clli = image.clli.as_ref().unwrap();
        assert_eq!(clli.max_cll, max_cll);
        assert_eq!(clli.max_pall, max_pall);
    }
}

#[test]
fn raw_io() {
    let data =
        std::fs::read(get_test_file("colors-animated-8bpc.avif")).expect("Unable to read file");
    let mut decoder = decoder::Decoder::default();
    unsafe {
        decoder
            .set_io_raw(data.as_ptr(), data.len())
            .expect("Failed to set IO");
    }
    assert!(decoder.parse().is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    assert_eq!(decoder.image_count(), 5);
    if !HAS_DECODER {
        return;
    }
    for _ in 0..5 {
        assert!(decoder.next_image().is_ok());
    }
}

struct CustomIO {
    data: Vec<u8>,
    available_size_rc: Rc<RefCell<usize>>,
}

impl decoder::IO for CustomIO {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]> {
        let available_size = self.available_size_rc.borrow();
        let start = usize::try_from(offset).unwrap();
        let end = start + max_read_size;
        if start > self.data.len() || end > self.data.len() {
            return Err(AvifError::IoError);
        }
        let mut ssize = max_read_size;
        if ssize > self.data.len() - start {
            ssize = self.data.len() - start;
        }
        let end = start + ssize;
        if *available_size < end {
            return Err(AvifError::WaitingOnIo);
        }
        Ok(&self.data[start..end])
    }

    fn size_hint(&self) -> u64 {
        self.data.len() as u64
    }

    fn persistent(&self) -> bool {
        false
    }
}

#[test]
fn custom_io() {
    let data =
        std::fs::read(get_test_file("colors-animated-8bpc.avif")).expect("Unable to read file");
    let mut decoder = decoder::Decoder::default();
    let available_size_rc = Rc::new(RefCell::new(data.len()));
    let io = Box::new(CustomIO {
        available_size_rc: available_size_rc.clone(),
        data,
    });
    decoder.set_io(io);
    assert!(decoder.parse().is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    assert_eq!(decoder.image_count(), 5);
    if !HAS_DECODER {
        return;
    }
    for _ in 0..5 {
        assert!(decoder.next_image().is_ok());
    }
}

fn expected_min_decoded_row_count(
    height: u32,
    cell_height: u32,
    cell_columns: u32,
    available_size: usize,
    size: usize,
    grid_cell_offsets: &[usize],
) -> u32 {
    if available_size >= size {
        return height;
    }
    let mut cell_index: Option<usize> = None;
    for (index, offset) in grid_cell_offsets.iter().enumerate().rev() {
        if available_size >= *offset {
            cell_index = Some(index);
            break;
        }
    }
    if cell_index.is_none() {
        return 0;
    }
    let cell_index = cell_index.unwrap() as u32;
    let cell_row = cell_index / cell_columns;
    let cell_column = cell_index % cell_columns;
    let cell_rows_decoded = if cell_column == cell_columns - 1 { cell_row + 1 } else { cell_row };
    cell_rows_decoded * cell_height
}

#[test]
fn expected_min_decoded_row_count_computation() {
    let grid_cell_offsets: Vec<usize> = vec![3258, 10643, 17846, 22151, 25409, 30000];
    let cell_height = 154;
    assert_eq!(
        0,
        expected_min_decoded_row_count(770, cell_height, 1, 1000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        cell_height,
        expected_min_decoded_row_count(770, cell_height, 1, 4000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        2 * cell_height,
        expected_min_decoded_row_count(770, cell_height, 1, 12000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        3 * cell_height,
        expected_min_decoded_row_count(770, cell_height, 1, 17846, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        cell_height,
        expected_min_decoded_row_count(462, cell_height, 2, 17846, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        2 * cell_height,
        expected_min_decoded_row_count(462, cell_height, 2, 23000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        cell_height,
        expected_min_decoded_row_count(308, cell_height, 3, 23000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        2 * cell_height,
        expected_min_decoded_row_count(308, cell_height, 3, 30000, 30000, &grid_cell_offsets)
    );
}

#[test]
fn incremental_decode() {
    // Grid item offsets for sofa_grid1x5_420.avif:
    // Each line is "$extent_offset + $extent_length".
    let grid_cell_offsets: Vec<usize> = vec![
        578 + 2680,
        3258 + 7385,
        10643 + 7203,
        17846 + 4305,
        22151 + 3258,
    ];

    let data = std::fs::read(get_test_file("sofa_grid1x5_420.avif")).expect("Unable to read file");
    let len = data.len();
    let available_size_rc = Rc::new(RefCell::new(0usize));
    let mut decoder = decoder::Decoder::default();
    decoder.settings.allow_incremental = true;
    let io = Box::new(CustomIO {
        available_size_rc: available_size_rc.clone(),
        data,
    });
    decoder.set_io(io);
    let step: usize = std::cmp::max(1, len / 10000) as usize;

    // Parsing is not incremental.
    let mut parse_result = decoder.parse();
    while parse_result.is_err()
        && matches!(parse_result.as_ref().err().unwrap(), AvifError::WaitingOnIo)
    {
        {
            let mut available_size = available_size_rc.borrow_mut();
            if *available_size >= len {
                panic!("parse returned waiting on io after full file.");
            }
            *available_size = std::cmp::min(*available_size + step, len);
        }
        parse_result = decoder.parse();
    }
    assert!(parse_result.is_ok());
    if !HAS_DECODER {
        return;
    }

    // Decoding is incremental.
    let mut previous_decoded_row_count = 0;
    let mut decode_result = decoder.next_image();
    while decode_result.is_err()
        && matches!(
            decode_result.as_ref().err().unwrap(),
            AvifError::WaitingOnIo
        )
    {
        {
            let mut available_size = available_size_rc.borrow_mut();
            if *available_size >= len {
                panic!("next_image returned waiting on io after full file.");
            }
            let decoded_row_count = decoder.decoded_row_count();
            assert!(decoded_row_count >= previous_decoded_row_count);
            let expected_min_decoded_row_count = expected_min_decoded_row_count(
                decoder.image().unwrap().height,
                154,
                1,
                *available_size,
                len,
                &grid_cell_offsets,
            );
            assert!(decoded_row_count >= expected_min_decoded_row_count);
            previous_decoded_row_count = decoded_row_count;
            *available_size = std::cmp::min(*available_size + step, len);
        }
        decode_result = decoder.next_image();
    }
    assert!(decode_result.is_ok());
    assert_eq!(decoder.decoded_row_count(), decoder.image().unwrap().height);

    // TODO: check if incremental and non incremental produces same output.
}

#[test]
fn progressive_partial_data() -> AvifResult<()> {
    let data = std::fs::read(get_test_file(
        "progressive/progressive_dimension_change.avif",
    ))
    .expect("Unable to read file");
    let len = data.len();
    let available_size_rc = Rc::new(RefCell::new(0usize));
    let mut decoder = decoder::Decoder::default();
    decoder.settings.allow_progressive = true;
    let io = Box::new(CustomIO {
        available_size_rc: available_size_rc.clone(),
        data,
    });
    decoder.set_io(io);

    // Parse.
    let mut parse_result = decoder.parse();
    while parse_result.is_err()
        && matches!(parse_result.as_ref().err().unwrap(), AvifError::WaitingOnIo)
    {
        {
            let mut available_size = available_size_rc.borrow_mut();
            if *available_size >= len {
                panic!("parse returned waiting on io after full file.");
            }
            *available_size = std::cmp::min(*available_size + 1, len);
        }
        parse_result = decoder.parse();
    }
    assert!(parse_result.is_ok());
    if !HAS_DECODER {
        return Ok(());
    }

    assert_eq!(decoder.image_count(), 2);
    let extent0 = decoder.nth_image_max_extent(0)?;
    assert_eq!(extent0.offset, 306);
    assert_eq!(extent0.size, 2250);
    let extent1 = decoder.nth_image_max_extent(1)?;
    assert_eq!(extent1.offset, 306);
    assert_eq!(extent1.size, 3813);

    // Getting the first frame now should fail.
    assert_eq!(decoder.nth_image(0), Err(AvifError::WaitingOnIo));
    // Set the available size to 1 byte less than the first frame's extent.
    *available_size_rc.borrow_mut() = extent0.offset as usize + extent0.size - 1;
    assert_eq!(decoder.nth_image(0), Err(AvifError::WaitingOnIo));
    // Set the available size to exactly the first frame's extent.
    *available_size_rc.borrow_mut() = extent0.offset as usize + extent0.size;
    assert!(decoder.nth_image(0).is_ok());
    let image = decoder.image().expect("unable to get image");
    assert_eq!(image.width, 256);
    assert_eq!(image.height, 256);
    assert!(image.has_plane(Plane::Y));
    assert!(image.has_plane(Plane::U));
    assert!(image.has_plane(Plane::V));
    // Set the available size to an offset between the first and second frame's extents.
    *available_size_rc.borrow_mut() = extent0.offset as usize + extent0.size + 100;
    assert!(decoder.nth_image(0).is_ok());
    assert_eq!(decoder.nth_image(1), Err(AvifError::WaitingOnIo));
    // Set the available size to 1 byte less than the second frame's extent.
    *available_size_rc.borrow_mut() = extent1.offset as usize + extent1.size - 1;
    assert!(decoder.nth_image(0).is_ok());
    assert_eq!(decoder.nth_image(1), Err(AvifError::WaitingOnIo));
    // Set the available size to 1 byte less than the second frame's extent.
    *available_size_rc.borrow_mut() = extent1.offset as usize + extent1.size;
    assert!(decoder.nth_image(1).is_ok());
    let image = decoder.image().expect("unable to get image");
    assert_eq!(image.width, 256);
    assert_eq!(image.height, 256);
    assert!(image.has_plane(Plane::Y));
    assert!(image.has_plane(Plane::U));
    assert!(image.has_plane(Plane::V));
    // At this point, we should be able to fetch both the frames in any order.
    assert!(decoder.nth_image(0).is_ok());
    assert!(decoder.nth_image(1).is_ok());
    assert!(decoder.nth_image(1).is_ok());
    assert!(decoder.nth_image(0).is_ok());

    Ok(())
}

#[test]
fn nth_image() {
    let mut decoder = get_decoder("colors-animated-8bpc.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    assert_eq!(decoder.image_count(), 5);
    if !HAS_DECODER {
        return;
    }
    assert!(decoder.nth_image(3).is_ok());
    assert!(decoder.next_image().is_ok());
    assert!(decoder.next_image().is_err());
    assert!(decoder.nth_image(1).is_ok());
    assert!(decoder.nth_image(4).is_ok());
    assert!(decoder.nth_image(50).is_err());
}

#[test]
fn color_and_alpha_dimensions_do_not_match() {
    let mut decoder = get_decoder("invalid_color10x10_alpha5x5.avif");
    // Parsing should succeed.
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert_eq!(image.width, 10);
    assert_eq!(image.height, 10);
    if !HAS_DECODER {
        return;
    }
    // Decoding should fail.
    let res = decoder.next_image();
    assert!(res.is_err());
}

#[test]
fn rgb_conversion_alpha_premultiply() -> AvifResult<()> {
    let mut decoder = get_decoder("alpha.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    if !HAS_DECODER {
        return Ok(());
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    let mut rgb = rgb::Image::create_from_yuv(image);
    rgb.premultiply_alpha = true;
    rgb.allocate()?;
    assert!(rgb.convert_from_yuv(image).is_ok());
    Ok(())
}

#[test]
fn white_1x1() -> AvifResult<()> {
    let mut decoder = get_decoder("white_1x1.avif");
    assert_eq!(decoder.parse(), Ok(()));
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    if !HAS_DECODER {
        return Ok(());
    }
    assert_eq!(decoder.next_image(), Ok(()));

    let image = decoder.image().expect("image was none");
    let mut rgb = rgb::Image::create_from_yuv(image);
    rgb.allocate()?;
    assert!(rgb.convert_from_yuv(image).is_ok());
    assert_eq!(rgb.width * rgb.height, 1);
    let format = rgb.format;
    for i in [format.r_offset(), format.g_offset(), format.b_offset()] {
        assert_eq!(rgb.row(0)?[i], 253); // Compressed with loss, not pure white.
    }
    if rgb.has_alpha() {
        assert_eq!(rgb.row(0)?[rgb.format.alpha_offset()], 255);
    }
    Ok(())
}

#[test]
fn white_1x1_mdat_size0() -> AvifResult<()> {
    // Edit the file to simulate an 'mdat' box with size 0 (meaning it ends at EOF).
    let mut file_bytes = std::fs::read(get_test_file("white_1x1.avif")).unwrap();
    let mdat = [b'm', b'd', b'a', b't'];
    let mdat_size_pos = file_bytes.windows(4).position(|w| w == mdat).unwrap() - 4;
    file_bytes[mdat_size_pos + 3] = b'\0';

    let mut decoder = decoder::Decoder::default();
    decoder.set_io_vec(file_bytes);
    assert_eq!(decoder.parse(), Ok(()));
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    Ok(())
}

#[test]
fn white_1x1_meta_size0() -> AvifResult<()> {
    // Edit the file to simulate a 'meta' box with size 0 (invalid).
    let mut file_bytes = std::fs::read(get_test_file("white_1x1.avif")).unwrap();
    let meta = [b'm', b'e', b't', b'a'];
    let meta_size_pos = file_bytes.windows(4).position(|w| w == meta).unwrap() - 4;
    file_bytes[meta_size_pos + 3] = b'\0';

    let mut decoder = decoder::Decoder::default();
    decoder.set_io_vec(file_bytes);

    // This should fail because the meta box contains the mdat box.
    // However, the section 8.11.3.1 of ISO/IEC 14496-12 does not explicitly require the coded image
    // item extents to be read from the MediaDataBox if the construction_method is 0.
    // Maybe another section or specification enforces that.
    assert_eq!(decoder.parse(), Ok(()));
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    if !HAS_DECODER {
        return Ok(());
    }
    assert_eq!(decoder.next_image(), Ok(()));
    Ok(())
}

#[test]
fn white_1x1_ftyp_size0() -> AvifResult<()> {
    // Edit the file to simulate a 'ftyp' box with size 0 (invalid).
    let mut file_bytes = std::fs::read(get_test_file("white_1x1.avif")).unwrap();
    file_bytes[3] = b'\0';

    let mut decoder = decoder::Decoder::default();
    decoder.set_io_vec(file_bytes);
    assert!(matches!(
        decoder.parse(),
        Err(AvifError::BmffParseFailed(_))
    ));
    Ok(())
}

#[test]
fn white_1x1_unknown_top_level_box_size0() -> AvifResult<()> {
    // Edit the file to insert an unknown top level box with size 0 after ftyp (invalid).
    let mut file_bytes = std::fs::read(get_test_file("white_1x1.avif")).unwrap();
    // Insert a top level box after ftyp (box type and size all 0s).
    for _ in 0..8 {
        file_bytes.insert(32, 0);
    }

    let mut decoder = decoder::Decoder::default();
    decoder.set_io_vec(file_bytes);
    assert!(decoder.parse().is_err());
    Ok(())
}

#[test]
fn dimg_repetition() {
    let mut decoder = get_decoder("sofa_grid1x5_420_dimg_repeat.avif");
    assert_eq!(
        decoder.parse(),
        Err(AvifError::BmffParseFailed(
            "multiple dimg references for item ID 1".into()
        ))
    );
}

#[test]
fn dimg_shared() {
    let mut decoder = get_decoder("color_grid_alpha_grid_tile_shared_in_dimg.avif");
    assert_eq!(decoder.parse(), Err(AvifError::NotImplemented));
}

#[test]
fn dimg_ordering() {
    if !HAS_DECODER {
        return;
    }
    let mut decoder1 = get_decoder("sofa_grid1x5_420.avif");
    let res = decoder1.parse();
    assert!(res.is_ok());
    let res = decoder1.next_image();
    assert!(res.is_ok());
    let mut decoder2 = get_decoder("sofa_grid1x5_420_random_dimg_order.avif");
    let res = decoder2.parse();
    assert!(res.is_ok());
    let res = decoder2.next_image();
    assert!(res.is_ok());
    let image1 = decoder1.image().expect("image1 was none");
    let image2 = decoder2.image().expect("image2 was none");
    // Ensure that the pixels in image1 and image2 are not the same.
    let row1 = image1.row(Plane::Y, 0).expect("row1 was none");
    let row2 = image2.row(Plane::Y, 0).expect("row2 was none");
    assert_ne!(row1, row2);
}

#[test]
fn grid_image_icc_associated_with_individual_cells() {
    let mut decoder = get_decoder("grid_icc_individual_cells.avif");
    assert!(decoder.parse().is_ok());
    let image = decoder.image().expect("image was none");
    assert!(!image.icc.is_empty());
}

#[test]
fn grid_image_nclx_associated_with_individual_cells() {
    let mut decoder = get_decoder("grid_nclx_individual_cells.avif");
    assert!(decoder.parse().is_ok());
    let image = decoder.image().expect("image was none");
    assert_eq!(image.color_primaries, ColorPrimaries::Bt470bg);
    assert_eq!(
        image.transfer_characteristics,
        TransferCharacteristics::Bt470bg
    );
    assert_eq!(image.matrix_coefficients, MatrixCoefficients::Bt470bg);
}

#[test]
fn heic_peek() {
    let file_data = std::fs::read(get_test_file("blue.heic")).expect("could not read file");
    assert_eq!(
        decoder::Decoder::peek_compatible_file_type(&file_data),
        cfg!(feature = "heic")
    );
}

#[test]
fn heic_parsing() {
    let mut decoder = get_decoder("blue.heic");
    let res = decoder.parse();
    if cfg!(feature = "heic") {
        assert!(res.is_ok());
        let image = decoder.image().expect("image was none");
        assert_eq!(image.width, 320);
        assert_eq!(image.height, 240);
        assert_eq!(decoder.compression_format(), CompressionFormat::Heic);
        if cfg!(feature = "android_mediacodec") {
            // Decoding is available only via android_mediacodec.
            assert!(!matches!(
                decoder.next_image(),
                Err(AvifError::NoCodecAvailable)
            ));
        }
    } else {
        assert!(res.is_err());
    }
}

#[test]
fn clap_irot_imir_non_essential() {
    let mut decoder = get_decoder("clap_irot_imir_non_essential.avif");
    let res = decoder.parse();
    assert!(res.is_err());
}

#[derive(Clone)]
struct ExpectedOverlayImageInfo<'a> {
    filename: &'a str,
    width: u32,
    height: u32,
    expected_pixels: &'a [(usize, u32, [u8; 4])], // (x, y, [rgba]).
}

const RED: [u8; 4] = [255, 0, 0, 255];
const GREEN: [u8; 4] = [0, 255, 0, 255];
const BLUE: [u8; 4] = [0, 0, 255, 255];
const BLACK: [u8; 4] = [0, 0, 0, 255];
const YELLOW: [u8; 4] = [255, 255, 0, 255];

const EXPECTED_OVERLAY_IMAGE_INFOS: [ExpectedOverlayImageInfo; 4] = [
    ExpectedOverlayImageInfo {
        // Three 80x60 sub-images with the following offsets:
        // horizontal_offsets: [0, 40, 80]
        // vertical_offsets: [0, 40, 80]
        filename: "overlay_exact_bounds.avif",
        width: 160,
        height: 140,
        expected_pixels: &[
            // Top left should be red.
            (0, 0, RED),
            (10, 10, RED),
            (20, 20, RED),
            // Green should be overlaid on top of the red block starting at (40, 40).
            (40, 40, GREEN),
            (50, 50, GREEN),
            (60, 60, GREEN),
            // Blue should be overlaid on top of the green block starting at (80, 80).
            (80, 80, BLUE),
            (90, 90, BLUE),
            (100, 100, BLUE),
            // Top right should be background color.
            (159, 0, BLACK),
            // Bottom left should be background color.
            (0, 139, BLACK),
        ],
    },
    ExpectedOverlayImageInfo {
        // Three 80x60 sub-images with the following offsets:
        // horizontal_offsets: [20, 60, 100]
        // vertical_offsets: [20, 60, 100]
        filename: "overlay_with_border.avif",
        width: 200,
        height: 180,
        expected_pixels: &[
            // Top left should be background color.
            (0, 0, BLACK),
            // Red should be overlaid starting at (20, 20).
            (20, 20, RED),
            (30, 30, RED),
            (40, 40, RED),
            // Green should be overlaid on top of the red block starting at (60, 60).
            (60, 60, GREEN),
            (70, 70, GREEN),
            (80, 80, GREEN),
            // Blue should be overlaid on top of the green block starting at (100, 100).
            (100, 100, BLUE),
            (110, 110, BLUE),
            (120, 120, BLUE),
            // Top right should be background color.
            (199, 0, BLACK),
            // Bottom left should be background color.
            (0, 179, BLACK),
            // Bottom right should be background color.
            (199, 179, BLACK),
        ],
    },
    ExpectedOverlayImageInfo {
        // Two 80x60 sub-images with the following offsets:
        // horizontal_offsets: [-40, 120]
        // vertical_offsets: [-40, 100]
        filename: "overlay_outside_bounds.avif",
        width: 160,
        height: 140,
        expected_pixels: &[
            // Red overlay is 40x20 in the top left.
            (0, 0, RED),
            (15, 15, RED),
            (39, 19, RED),
            (40, 20, BLACK),
            // Blue overlay is 40x40 in the bottom right.
            (119, 99, BLACK),
            (120, 100, BLUE),
            (140, 120, BLUE),
            (159, 139, BLUE),
            // Center of the image should be background color.
            (80, 70, BLACK),
            // Top right should be background color.
            (159, 0, BLACK),
            // Bottom left should be background color.
            (0, 139, BLACK),
        ],
    },
    ExpectedOverlayImageInfo {
        // Three 80x60 sub-images with the following offsets:
        // horizontal_offsets: [0, 40, 80]
        // vertical_offsets: [0, 40, 80]
        // canvas background color: yellow.
        filename: "overlay_yellow_bg.avif",
        width: 160,
        height: 140,
        expected_pixels: &[
            // Top left should be red.
            (0, 0, RED),
            (10, 10, RED),
            (20, 20, RED),
            // Green should be overlaid on top of the red block starting at (40, 40).
            (40, 40, GREEN),
            (50, 50, GREEN),
            (60, 60, GREEN),
            // Blue should be overlaid on top of the green block starting at (80, 80).
            (80, 80, BLUE),
            (90, 90, BLUE),
            (100, 100, BLUE),
            // Top right should be background color.
            (159, 0, YELLOW),
            // Bottom left should be background color.
            (0, 139, YELLOW),
        ],
    },
];

macro_rules! pixel_eq {
    ($a:expr, $b:expr) => {
        assert!((i32::from($a) - i32::from($b)).abs() <= 3);
    };
}

#[allow(clippy::zero_prefixed_literal)]
#[test_matrix(0usize..4)]
fn overlay(index: usize) {
    let info = &EXPECTED_OVERLAY_IMAGE_INFOS[index];
    let mut decoder = get_decoder(info.filename);
    decoder.settings.strictness = decoder::Strictness::None;
    let res = decoder.parse();
    assert!(res.is_ok());
    assert_eq!(decoder.compression_format(), CompressionFormat::Avif);
    let image = decoder.image().expect("image was none");
    assert_eq!(image.width, info.width);
    assert_eq!(image.height, info.height);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert_eq!(image.width, info.width);
    assert_eq!(image.height, info.height);
    let mut rgb = rgb::Image::create_from_yuv(image);
    rgb.format = rgb::Format::Rgba;
    assert!(rgb.allocate().is_ok());
    assert!(rgb.convert_from_yuv(image).is_ok());
    for expected_pixel in info.expected_pixels {
        let column = expected_pixel.0;
        let row = expected_pixel.1;
        let pixels = rgb.row(row).expect("row was none");
        let r = pixels[column * 4];
        let g = pixels[(column * 4) + 1];
        let b = pixels[(column * 4) + 2];
        let a = pixels[(column * 4) + 3];
        pixel_eq!(r, expected_pixel.2[0]);
        pixel_eq!(g, expected_pixel.2[1]);
        pixel_eq!(b, expected_pixel.2[2]);
        pixel_eq!(a, expected_pixel.2[3]);
    }
}

#[test_case("mismatch_colr_0_0.avif", YuvRange::Limited ; "mismatch case 0")]
#[test_case("mismatch_colr_0_1.avif", YuvRange::Limited ; "mismatch case 1")]
#[test_case("mismatch_colr_0_2.avif", YuvRange::Limited ; "mismatch case 2")]
#[test_case("mismatch_colr_1_0.avif", YuvRange::Full ; "mismatch case 3")]
#[test_case("mismatch_colr_1_1.avif", YuvRange::Full ; "mismatch case 4")]
#[test_case("mismatch_colr_1_2.avif", YuvRange::Full ; "mismatch case 5")]
#[test_case("missing_colr_0_0.avif", YuvRange::Limited ; "missing colr case 0")]
#[test_case("missing_colr_0_1.avif", YuvRange::Limited ; "missing colr case 1")]
#[test_case("missing_colr_0_2.avif", YuvRange::Limited ; "missing colr case 2")]
#[test_case("missing_colr_1_0.avif", YuvRange::Full ; "missing colr case 3")]
#[test_case("missing_colr_1_1.avif", YuvRange::Full ; "missing colr case 4")]
#[test_case("missing_colr_1_2.avif", YuvRange::Full ; "missing colr case 5")]
fn yuv_range(filename: &str, expected_yuv_range: YuvRange) {
    let mut decoder = get_decoder(filename);
    let res = decoder.parse();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert_eq!(image.yuv_range, expected_yuv_range);
}

#[test_case("weld_sato_8plus8bit.avif", false)]
#[test_case("weld_sato_8plus8bit_alpha.avif", true)]
#[test_case("weld_sato_12plus4bit.avif", false)]
fn sato_16bit(filename: &str, has_alpha: bool) {
    let mut decoder = get_decoder(filename);
    decoder.settings.allow_sample_transform = true;
    assert!(decoder.parse().is_ok());
    assert_eq!(has_alpha, decoder.image().unwrap().alpha_present);
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert_eq!(res, Ok(()));
    assert_eq!(16, decoder.image().unwrap().depth);
    // TODO: compare with reference weld_16bit.png
}

#[test]
fn sato_disabled() {
    let mut decoder = get_decoder("weld_sato_8plus8bit.avif");
    decoder.settings.allow_sample_transform = false;
    assert!(decoder.parse().is_ok());
    if !HAS_DECODER {
        return;
    }
    let res = decoder.next_image();
    assert_eq!(res, Ok(()));
    assert_eq!(decoder.image().unwrap().depth, 8);
}
