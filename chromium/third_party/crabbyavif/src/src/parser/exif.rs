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

use crate::internal_utils::stream::*;
use crate::*;

fn parse_exif_tiff_header_offset(stream: &mut IStream) -> AvifResult<u32> {
    const TIFF_HEADER_BE: u32 = 0x4D4D002A; // MM0* (read as a big endian u32)
    const TIFF_HEADER_LE: u32 = 0x49492A00; // II*0 (read as a big endian u32)
    let mut expected_offset: u32 = 0;
    let mut size = u32::try_from(stream.bytes_left()?).unwrap_or(u32::MAX);
    while size > 0 {
        let value = stream.read_u32().or(Err(AvifError::InvalidExifPayload))?;
        if value == TIFF_HEADER_BE || value == TIFF_HEADER_LE {
            stream.rewind(4)?;
            return Ok(expected_offset);
        }
        checked_decr!(size, 4);
        checked_incr!(expected_offset, 4);
    }
    // Could not find the TIFF header.
    Err(AvifError::InvalidExifPayload)
}

pub fn parse(stream: &mut IStream) -> AvifResult<()> {
    // unsigned int(32) exif_tiff_header_offset;
    let offset = stream.read_u32().or(Err(AvifError::InvalidExifPayload))?;

    let expected_offset = parse_exif_tiff_header_offset(stream)?;
    if offset != expected_offset {
        return Err(AvifError::InvalidExifPayload);
    }
    Ok(())
}
