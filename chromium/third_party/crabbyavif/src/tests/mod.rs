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

use crabby_avif::*;

#[cfg(test)]
pub fn get_test_file(filename: &str) -> String {
    let base_path = if cfg!(google3) {
        format!(
            "{}/google3/third_party/crabbyavif/",
            std::env::var("TEST_SRCDIR").expect("TEST_SRCDIR is not defined")
        )
    } else {
        "".to_string()
    };
    String::from(format!("{base_path}tests/data/{filename}"))
}

#[cfg(test)]
pub fn get_decoder(filename: &str) -> decoder::Decoder {
    let abs_filename = get_test_file(filename);
    let mut decoder = decoder::Decoder::default();
    let _ = decoder
        .set_io_file(&abs_filename)
        .expect("Failed to set IO");
    decoder
}

#[cfg(test)]
#[allow(dead_code)]
pub const HAS_DECODER: bool = if cfg!(any(
    feature = "dav1d",
    feature = "libgav1",
    feature = "android_mediacodec"
)) {
    true
} else {
    false
};
