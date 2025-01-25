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

use crabby_avif::decoder::track::RepetitionCount;
use crabby_avif::image::*;
use crabby_avif::reformat::rgb;
use crabby_avif::*;

#[path = "./mod.rs"]
mod tests;

use std::cell::RefCell;
use std::rc::Rc;
use tests::*;

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

// From avifanimationtest.cc
#[test]
fn animated_image() {
    let mut decoder = get_decoder("colors-animated-8bpc.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
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
#[test]
fn animated_image_with_source_set_to_primary_item() {
    let mut decoder = get_decoder("colors-animated-8bpc.avif");
    decoder.settings.source = decoder::Source::PrimaryItem;
    let res = decoder.parse();
    assert!(res.is_ok());
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

// From avifkeyframetest.cc
#[test]
fn keyframes() {
    let mut decoder = get_decoder("colors-animated-12bpc-keyframes-0-2-3.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
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

// From avifprogressivetest.cc
#[test_case::test_case("progressive_dimension_change.avif", 2, 256, 256; "progressive_dimension_change")]
#[test_case::test_case("progressive_layered_grid.avif", 2, 512, 256; "progressive_layered_grid")]
#[test_case::test_case("progressive_quality_change.avif", 2, 256, 256; "progressive_quality_change")]
#[test_case::test_case("progressive_same_layers.avif", 4, 256, 256; "progressive_same_layers")]
#[test_case::test_case("tiger_3layer_1res.avif", 3, 1216, 832; "tiger_3layer_1res")]
#[test_case::test_case("tiger_3layer_3res.avif", 3, 1216, 832; "tiger_3layer_3res")]
fn progressive(filename: &str, layer_count: u32, width: u32, height: u32) {
    let mut filename_with_prefix = String::from("progressive/");
    filename_with_prefix.push_str(filename);
    let mut decoder = get_decoder(&filename_with_prefix);

    decoder.settings.allow_progressive = false;
    let res = decoder.parse();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert!(matches!(
        image.progressive_state,
        decoder::ProgressiveState::Available
    ));

    decoder.settings.allow_progressive = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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

// From avifmetadatatest.cc
#[test]
fn decoder_parse_icc_exif_xmp() {
    // Test case from https://github.com/AOMediaCodec/libavif/issues/1086.
    let mut decoder = get_decoder("paris_icc_exif_xmp.avif");

    decoder.settings.ignore_xmp = true;
    decoder.settings.ignore_exif = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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

// From avifgainmaptest.cc
#[test]
fn color_grid_gainmap_different_grid() {
    let mut decoder = get_decoder("color_grid_gainmap_different_grid.avif");
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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
}

// From avifgainmaptest.cc
#[test]
fn color_grid_alpha_grid_gainmap_nogrid() {
    let mut decoder = get_decoder("color_grid_alpha_grid_gainmap_nogrid.avif");
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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
}

// From avifgainmaptest.cc
#[test]
fn color_nogrid_alpha_nogrid_gainmap_grid() {
    let mut decoder = get_decoder("color_nogrid_alpha_nogrid_gainmap_grid.avif");
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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
}

// From avifgainmaptest.cc
#[test]
fn gainmap_oriented() {
    let mut decoder = get_decoder("gainmap_oriented.avif");
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
    let image = decoder.image().expect("image was none");
    assert_eq!(image.irot_angle, Some(1));
    assert_eq!(image.imir_axis, Some(0));
    assert!(decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.irot_angle, None);
    assert_eq!(decoder.gainmap().image.imir_axis, None);
}

// The two test files should produce the same results:
// One has an unsupported 'version' field, the other an unsupported
// 'minimum_version' field, but the behavior of these two files is the same.
// From avifgainmaptest.cc
#[test_case::test_case("unsupported_gainmap_version.avif")]
#[test_case::test_case("unsupported_gainmap_minimum_version.avif")]
fn decode_unsupported_version(filename: &str) {
    // Parse with various enable_decoding_gainmap and
    // enable_parsing_gainmap_metadata settings.
    let mut decoder = get_decoder(filename);
    decoder.settings.enable_decoding_gainmap = false;
    decoder.settings.enable_parsing_gainmap_metadata = false;
    let res = decoder.parse();
    assert!(res.is_ok());
    // Gain map not found since enable_parsing_gainmap_metadata is false.
    assert!(!decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 0);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 0);

    decoder = get_decoder(filename);
    decoder.settings.enable_decoding_gainmap = false;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
    // Gain map marked as not present because the metadata is not supported.
    assert!(!decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 0);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 0);

    decoder = get_decoder(filename);
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = false;
    let res = decoder.parse();
    // Invalid enableDecodingGainMap=true and enable_parsing_gainmap_metadata
    // combination.
    assert_eq!(res.err(), Some(AvifError::InvalidArgument));

    decoder = get_decoder(filename);
    decoder.settings.enable_decoding_gainmap = true;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
    // Gainmap not found: its metadata is not supported.
    assert!(!decoder.gainmap_present());
    assert_eq!(decoder.gainmap().image.width, 0);
    assert_eq!(decoder.gainmap().metadata.base_hdr_headroom.0, 0);
    assert_eq!(decoder.gainmap().metadata.alternate_hdr_headroom.0, 0);
}

// From avifgainmaptest.cc
#[test]
fn decode_unsupported_writer_version_with_extra_bytes() {
    let mut decoder = get_decoder("unsupported_gainmap_writer_version_with_extra_bytes.avif");
    decoder.settings.enable_decoding_gainmap = false;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    assert!(res.is_ok());
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
    decoder.settings.enable_decoding_gainmap = false;
    decoder.settings.enable_parsing_gainmap_metadata = true;
    let res = decoder.parse();
    // Fails to decode: there are extra bytes at the end of the gain map metadata
    // that shouldn't be there.
    assert!(matches!(res, Err(AvifError::InvalidToneMappedImage(_))));
}

// From avifcllitest.cc
#[test_case::test_case("clli_0_0.avif", 0, 0; "clli_0_0")]
#[test_case::test_case("clli_0_1.avif", 0, 1; "clli_0_1")]
#[test_case::test_case("clli_0_65535.avif", 0, 65535; "clli_0_65535")]
#[test_case::test_case("clli_1_0.avif", 1, 0; "clli_1_0")]
#[test_case::test_case("clli_1_1.avif", 1, 1; "clli_1_1")]
#[test_case::test_case("clli_1_65535.avif", 1, 65535; "clli_1_65535")]
#[test_case::test_case("clli_65535_0.avif", 65535, 0; "clli_65535_0")]
#[test_case::test_case("clli_65535_1.avif", 65535, 1; "clli_65535_1")]
#[test_case::test_case("clli_65535_65535.avif", 65535, 65535; "clli_65535_65535")]
fn clli(filename: &str, max_cll: u16, max_pall: u16) {
    let mut filename_with_prefix = String::from("clli/");
    filename_with_prefix.push_str(filename);
    let mut decoder = get_decoder(&filename_with_prefix);
    let res = decoder.parse();
    assert!(res.is_ok());
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
    let _ = unsafe {
        decoder
            .set_io_raw(data.as_ptr(), data.len())
            .expect("Failed to set IO")
    };
    assert!(decoder.parse().is_ok());
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
    grid_cell_offsets: &Vec<usize>,
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
        1 * cell_height,
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
        1 * cell_height,
        expected_min_decoded_row_count(462, cell_height, 2, 17846, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        2 * cell_height,
        expected_min_decoded_row_count(462, cell_height, 2, 23000, 30000, &grid_cell_offsets)
    );
    assert_eq!(
        1 * cell_height,
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
                println!("parse returned waiting on io after full file.");
                assert!(false);
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
                println!("next_image returned waiting on io after full file.");
                assert!(false);
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
fn nth_image() {
    let mut decoder = get_decoder("colors-animated-8bpc.avif");
    let res = decoder.parse();
    assert!(res.is_ok());
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
