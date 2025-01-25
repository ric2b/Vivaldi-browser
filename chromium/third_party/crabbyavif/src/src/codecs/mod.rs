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

use crate::decoder::Category;
use crate::image::Image;
use crate::AvifResult;

pub trait Decoder {
    fn initialize(&mut self, operating_point: u8, all_layers: bool) -> AvifResult<()>;
    fn get_next_image(
        &mut self,
        av1_payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()>;
    // Destruction must be implemented using Drop.
}
