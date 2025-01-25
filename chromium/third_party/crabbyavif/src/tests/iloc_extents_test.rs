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

#[path = "./mod.rs"]
mod tests;

use crabby_avif::reformat::rgb::*;
use image::ImageReader;
use tests::*;

#[test]
fn iloc_extents() {
    let mut decoder = get_decoder("sacre_coeur_2extents.avif");
    assert!(decoder.parse().is_ok());
    if !HAS_DECODER {
        return;
    }
    assert!(decoder.next_image().is_ok());
    let decoded = decoder.image().expect("image was none");
    let mut rgb = Image::create_from_yuv(decoded);
    rgb.format = Format::Rgb;
    assert!(rgb.allocate().is_ok());
    assert!(rgb.convert_from_yuv(decoded).is_ok());

    let source = ImageReader::open(get_test_file("sacre_coeur.png"));
    let source = source.unwrap().decode().unwrap();

    // sacre_coeur_2extents.avif was generated with
    //   avifenc --lossless --ignore-exif --ignore-xmp --ignore-icc sacre_coeur.png
    // so pixels can be compared byte by byte.
    assert_eq!(
        source.as_bytes(),
        rgb.pixels
            .as_ref()
            .unwrap()
            .slice(0, source.as_bytes().len() as u32)
            .unwrap()
    );
}

#[test]
fn nth_image_max_extent() {
    let mut decoder = get_decoder("sacre_coeur_2extents.avif");
    assert!(decoder.parse().is_ok());

    let max_extent = decoder.nth_image_max_extent(0).unwrap();
    assert_eq!(max_extent.offset, 290);
    assert_eq!(max_extent.size, 1000 + 1 + 5778); // '\0' in the middle.
}
