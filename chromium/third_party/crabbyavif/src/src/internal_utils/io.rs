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

use crate::internal_utils::*;

use std::fs::File;
use std::io::Read;
use std::io::Seek;
use std::io::SeekFrom;

#[derive(Debug, Default)]
pub struct DecoderFileIO {
    file: Option<File>,
    buffer: Vec<u8>,
}

impl DecoderFileIO {
    pub fn create(filename: &String) -> AvifResult<DecoderFileIO> {
        let file = File::open(filename).or(Err(AvifError::IoError))?;
        Ok(DecoderFileIO {
            file: Some(file),
            buffer: Vec::new(),
        })
    }
}

impl decoder::IO for DecoderFileIO {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]> {
        let file_size = self.size_hint();
        if offset > file_size {
            return Err(AvifError::IoError);
        }
        let available_size: usize = (file_size - offset) as usize;
        let size_to_read: usize =
            if max_read_size > available_size { available_size } else { max_read_size };
        if size_to_read > 0 {
            if self.buffer.capacity() < size_to_read
                && self.buffer.try_reserve_exact(size_to_read).is_err()
            {
                return Err(AvifError::OutOfMemory);
            }
            self.buffer.resize(size_to_read, 0);
            if self
                .file
                .unwrap_ref()
                .seek(SeekFrom::Start(offset))
                .is_err()
                || self.file.unwrap_ref().read_exact(&mut self.buffer).is_err()
            {
                return Err(AvifError::IoError);
            }
        } else {
            self.buffer.clear();
        }
        Ok(self.buffer.as_slice())
    }

    fn size_hint(&self) -> u64 {
        let metadata = self.file.unwrap_ref().metadata();
        if metadata.is_err() {
            return 0;
        }
        metadata.unwrap().len()
    }

    fn persistent(&self) -> bool {
        false
    }
}

pub struct DecoderRawIO<'a> {
    pub data: &'a [u8],
}

impl DecoderRawIO<'_> {
    // SAFETY: This function is only used from the C/C++ API when the input comes from native
    // callers. The assumption is that the caller will always pass in a valid pointer and size.
    pub unsafe fn create(data: *const u8, size: usize) -> Self {
        Self {
            data: unsafe { std::slice::from_raw_parts(data, size) },
        }
    }
}

impl decoder::IO for DecoderRawIO<'_> {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]> {
        let data_len = self.data.len() as u64;
        if offset > data_len {
            return Err(AvifError::IoError);
        }
        let available_size: usize = (data_len - offset) as usize;
        let size_to_read: usize =
            if max_read_size > available_size { available_size } else { max_read_size };
        let slice_start = usize_from_u64(offset)?;
        let slice_end = checked_add!(slice_start, size_to_read)?;
        let range = slice_start..slice_end;
        check_slice_range(self.data.len(), &range)?;
        Ok(&self.data[range])
    }

    fn size_hint(&self) -> u64 {
        self.data.len() as u64
    }

    fn persistent(&self) -> bool {
        true
    }
}

pub struct DecoderMemoryIO {
    pub data: Vec<u8>,
}

impl decoder::IO for DecoderMemoryIO {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]> {
        let data_len = self.data.len() as u64;
        if offset > data_len {
            return Err(AvifError::IoError);
        }
        let available_size: usize = (data_len - offset) as usize;
        let size_to_read: usize =
            if max_read_size > available_size { available_size } else { max_read_size };
        let slice_start = usize_from_u64(offset)?;
        let slice_end = checked_add!(slice_start, size_to_read)?;
        let range = slice_start..slice_end;
        check_slice_range(self.data.len(), &range)?;
        Ok(&self.data[range])
    }

    fn size_hint(&self) -> u64 {
        self.data.len() as u64
    }

    fn persistent(&self) -> bool {
        true
    }
}
