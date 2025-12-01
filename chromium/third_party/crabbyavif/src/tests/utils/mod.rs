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

// Not all functions are used from all test targets. So allow unused functions in this module.
#![allow(unused)]

use crabby_avif::image::*;
use crabby_avif::*;
use std::fs::File;

pub fn get_test_file(filename: &str) -> String {
    let base_path = if cfg!(google3) {
        format!(
            "{}/google3/third_party/crabbyavif/",
            std::env::var("TEST_SRCDIR").expect("TEST_SRCDIR is not defined")
        )
    } else {
        "".to_string()
    };
    format!("{base_path}tests/data/{filename}")
}

pub fn get_decoder(filename: &str) -> decoder::Decoder {
    let abs_filename = get_test_file(filename);
    let mut decoder = decoder::Decoder::default();
    decoder
        .set_io_file(&abs_filename)
        .expect("Failed to set IO");
    decoder
}

#[cfg(feature = "png")]
pub fn decode_png(filename: &str) -> Vec<u8> {
    let decoder = png::Decoder::new(File::open(get_test_file(filename)).unwrap());
    let mut reader = decoder.read_info().unwrap();
    // Indexed colors are not supported.
    assert_ne!(reader.output_color_type().0, png::ColorType::Indexed);
    let mut pixels = vec![0; reader.output_buffer_size()];
    let info = reader.next_frame(&mut pixels).unwrap();
    pixels
}

fn full_to_limited_pixel(min: i32, max: i32, full: i32, v: u16) -> u16 {
    let v = v as i32;
    let v = (((v * (max - min)) + (full / 2)) / full) + min;
    if v < min {
        min as u16
    } else if v > max {
        max as u16
    } else {
        v as u16
    }
}

fn full_to_limited(v: u16, plane: Plane, depth: u8) -> u16 {
    match (plane, depth) {
        (Plane::Y, 8) => full_to_limited_pixel(16, 235, 255, v),
        (Plane::Y, 10) => full_to_limited_pixel(64, 940, 1023, v),
        (Plane::Y, 12) => full_to_limited_pixel(256, 3760, 4095, v),
        (Plane::U | Plane::V, 8) => full_to_limited_pixel(16, 240, 255, v),
        (Plane::U | Plane::V, 10) => full_to_limited_pixel(64, 960, 1023, v),
        (Plane::U | Plane::V, 12) => full_to_limited_pixel(256, 3840, 4095, v),
        _ => unreachable!(""),
    }
}

pub fn generate_gradient_image(
    width: u32,
    height: u32,
    depth: u8,
    yuv_format: PixelFormat,
    yuv_range: YuvRange,
    alpha: bool,
) -> AvifResult<Image> {
    let mut image = image::Image {
        width,
        height,
        depth,
        yuv_format,
        yuv_range,
        ..Default::default()
    };
    image.allocate_planes(Category::Color)?;
    if alpha {
        image.allocate_planes(Category::Alpha)?;
        image.alpha_present = true;
    }
    for plane in ALL_PLANES {
        if !image.has_plane(plane) {
            continue;
        }
        let plane_data = image.plane_data(plane).unwrap();
        let max_xy_sum = plane_data.width + plane_data.height - 2;
        for y in 0..plane_data.height {
            if image.depth == 8 {
                let row = image.row_mut(plane, y)?;
                for x in 0..plane_data.width {
                    let value = (x + y) % (max_xy_sum + 1);
                    row[x as usize] = (value * 255 / std::cmp::max(1, max_xy_sum)) as u8;
                    if yuv_range == YuvRange::Limited && plane != Plane::A {
                        row[x as usize] =
                            full_to_limited(row[x as usize] as u16, plane, depth) as u8;
                    }
                }
            } else {
                let max_channel = image.max_channel() as u32;
                let row = image.row16_mut(plane, y)?;
                for x in 0..plane_data.width {
                    let value = (x + y) % (max_xy_sum + 1);
                    row[x as usize] = (value * max_channel / std::cmp::max(1, max_xy_sum)) as u16;
                    if yuv_range == YuvRange::Limited && plane != Plane::A {
                        row[x as usize] = full_to_limited(row[x as usize], plane, depth);
                    }
                }
            }
        }
    }
    Ok(image)
}

pub fn are_images_equal(image1: &Image, image2: &Image) -> AvifResult<()> {
    assert!(image1.has_same_properties_and_cicp(image2));
    for plane in image::ALL_PLANES {
        assert_eq!(image1.has_plane(plane), image2.has_plane(plane));
        if !image1.has_plane(plane) {
            continue;
        }
        let width = image1.width(plane);
        let height = image1.height(plane);
        for y in 0..height as u32 {
            if image1.depth > 8 {
                assert_eq!(
                    image1.row16(plane, y)?[..width],
                    image2.row16(plane, y)?[..width]
                );
            } else {
                assert_eq!(
                    image1.row(plane, y)?[..width],
                    image2.row(plane, y)?[..width]
                );
            }
        }
    }
    Ok(())
}

fn squared_diff_sum(pixel1: u16, pixel2: u16) -> u64 {
    let diff = pixel1 as i32 - pixel2 as i32;
    (diff * diff) as u64
}

pub fn psnr(image1: &Image, image2: &Image) -> AvifResult<f64> {
    assert!(image1.has_same_properties_and_cicp(image2));
    let mut diff_sum = 0u64;
    let mut num_samples = 0;
    for plane in image::ALL_PLANES {
        assert_eq!(image1.has_plane(plane), image2.has_plane(plane));
        if !image1.has_plane(plane) {
            continue;
        }
        let width = image1.width(plane);
        let height = image1.height(plane);
        if width == 0 || height == 0 {
            continue;
        }
        for y in 0..height as u32 {
            if image1.depth > 8 {
                let row1 = image1.row16(plane, y)?;
                let row2 = image2.row16(plane, y)?;
                for x in 0..width {
                    diff_sum += squared_diff_sum(row1[x], row2[x]);
                }
            } else {
                let row1 = image1.row(plane, y)?;
                let row2 = image2.row(plane, y)?;
                for x in 0..width {
                    diff_sum += squared_diff_sum(row1[x] as u16, row2[x] as u16);
                }
            }
            num_samples += width;
        }
    }
    if diff_sum == 0 {
        return Ok(99.0);
    }
    let max_channel_f = image1.max_channel() as f64;
    let normalized_error = diff_sum as f64 / (num_samples as f64 * max_channel_f * max_channel_f);
    if normalized_error <= f64::EPSILON {
        Ok(98.99)
    } else {
        Ok((-10.0 * normalized_error.log10()).min(98.99))
    }
}

pub fn fill_plane(image: &mut Image, plane: Plane, value: u16) -> AvifResult<()> {
    let plane_data = image.plane_data(plane).ok_or(AvifError::NoContent)?;
    for y in 0..plane_data.height {
        if image.depth == 8 {
            for pixel in &mut image.row_mut(Plane::A, y)?[..plane_data.width as usize] {
                *pixel = value as u8;
            }
        } else {
            for pixel in &mut image.row16_mut(Plane::A, y)?[..plane_data.width as usize] {
                *pixel = value;
            }
        }
    }
    Ok(())
}

pub const HAS_DECODER: bool = cfg!(any(
    feature = "dav1d",
    feature = "libgav1",
    feature = "android_mediacodec"
));

pub const HAS_ENCODER: bool = cfg!(feature = "aom");
