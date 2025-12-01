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

use crabby_avif::image::*;
use crabby_avif::utils::reader::y4m::Y4MReader;
use crabby_avif::utils::reader::Config;
use crabby_avif::utils::reader::Reader;
use crabby_avif::utils::writer::y4m::Y4MWriter;
use crabby_avif::utils::writer::Writer;
use crabby_avif::*;

mod utils;
use utils::*;

use std::fs::File;
use tempfile::NamedTempFile;
use test_case::test_matrix;

fn get_tempfile() -> String {
    let file = NamedTempFile::new().expect("unable to open tempfile");
    let path = file.into_temp_path();
    let filename = String::from(path.to_str().unwrap());
    let _ = path.close();
    filename
}

#[test_matrix(
    [100, 121],
    [200, 107],
    [8, 10, 12],
    [PixelFormat::Yuv420, PixelFormat::Yuv422, PixelFormat::Yuv444, PixelFormat::Yuv400],
    [YuvRange::Limited, YuvRange::Full],
    [false, true]
)]
fn roundtrip(
    width: u32,
    height: u32,
    depth: u8,
    yuv_format: PixelFormat,
    yuv_range: YuvRange,
    alpha: bool,
) -> AvifResult<()> {
    if alpha && (depth != 8 || yuv_format != PixelFormat::Yuv444) {
        // alpha in y4m is supported only for 8-bit 444 images.
        return Ok(());
    }
    let image1 = generate_gradient_image(width, height, depth, yuv_format, yuv_range, alpha)?;
    let output_filename = get_tempfile();
    // Write the image.
    {
        let mut writer = Y4MWriter::create(false);
        let mut output_file =
            File::create(output_filename.clone()).expect("output file creation failed");
        writer.write_frame(&mut output_file, &image1)?;
    }
    // Read the image.
    let mut reader = Y4MReader::create(&output_filename)?;
    let (image2, _) = reader.read_frame(&Config::default())?;
    are_images_equal(&image1, &image2)?;
    Ok(())
}
