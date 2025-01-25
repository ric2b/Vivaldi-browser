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
use crate::*;

#[derive(Clone, Debug)]
pub enum Pixels {
    // Intended for use from the C API. Used for 8-bit images.
    Pointer(*mut u8),
    // Intended for use from the C API. Used for 10-bit, 12-bit and 16-bit images.
    Pointer16(*mut u16),
    // Used for 8-bit images.
    Buffer(Vec<u8>),
    // Used for 10-bit, 12-bit and 16-bit images.
    Buffer16(Vec<u16>),
}

impl Pixels {
    pub fn from_raw_pointer(ptr: *mut u8, depth: u32) -> Self {
        if depth > 8 {
            Pixels::Pointer16(ptr as *mut u16)
        } else {
            Pixels::Pointer(ptr)
        }
    }

    pub fn size(&self) -> usize {
        match self {
            Pixels::Pointer(_) => 0,
            Pixels::Pointer16(_) => 0,
            Pixels::Buffer(buffer) => buffer.len(),
            Pixels::Buffer16(buffer) => buffer.len(),
        }
    }

    pub fn pixel_bit_size(&self) -> usize {
        match self {
            Pixels::Pointer(_) => 0,
            Pixels::Pointer16(_) => 0,
            Pixels::Buffer(_) => 8,
            Pixels::Buffer16(_) => 16,
        }
    }

    pub fn has_data(&self) -> bool {
        match self {
            Pixels::Pointer(ptr) => !ptr.is_null(),
            Pixels::Pointer16(ptr) => !ptr.is_null(),
            Pixels::Buffer(buffer) => !buffer.is_empty(),
            Pixels::Buffer16(buffer) => !buffer.is_empty(),
        }
    }

    pub fn resize(&mut self, size: usize, default: u16) -> AvifResult<()> {
        match self {
            Pixels::Pointer(_) => return Err(AvifError::InvalidArgument),
            Pixels::Pointer16(_) => return Err(AvifError::InvalidArgument),
            Pixels::Buffer(buffer) => {
                if buffer.capacity() < size && buffer.try_reserve_exact(size).is_err() {
                    return Err(AvifError::OutOfMemory);
                }
                buffer.resize(size, default as u8);
            }
            Pixels::Buffer16(buffer) => {
                if buffer.capacity() < size && buffer.try_reserve_exact(size).is_err() {
                    return Err(AvifError::OutOfMemory);
                }
                buffer.resize(size, default);
            }
        }
        Ok(())
    }

    pub fn is_pointer(&self) -> bool {
        matches!(self, Pixels::Pointer(_) | Pixels::Pointer16(_))
    }

    pub fn ptr(&self) -> *const u8 {
        match self {
            Pixels::Pointer(ptr) => *ptr as *const u8,
            Pixels::Buffer(buffer) => buffer.as_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    pub fn ptr16(&self) -> *const u16 {
        match self {
            Pixels::Pointer16(ptr) => *ptr as *const u16,
            Pixels::Buffer16(buffer) => buffer.as_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    pub fn ptr_mut(&mut self) -> *mut u8 {
        match self {
            Pixels::Pointer(ptr) => *ptr,
            Pixels::Buffer(buffer) => buffer.as_mut_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    pub fn ptr16_mut(&mut self) -> *mut u16 {
        match self {
            Pixels::Pointer16(ptr) => *ptr,
            Pixels::Buffer16(buffer) => buffer.as_mut_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    pub fn clone_pointer(&self) -> Option<Pixels> {
        match self {
            Pixels::Pointer(ptr) => Some(Pixels::Pointer(*ptr)),
            Pixels::Pointer16(ptr) => Some(Pixels::Pointer16(*ptr)),
            _ => None,
        }
    }

    pub fn slice(&self, offset: u32, size: u32) -> AvifResult<&[u8]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(ptr) => {
                let offset = isize_from_usize(offset)?;
                Ok(unsafe { std::slice::from_raw_parts(ptr.offset(offset), size) })
            }
            Pixels::Pointer16(_) => Err(AvifError::NoContent),
            Pixels::Buffer(buffer) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                let range = offset..end;
                check_slice_range(buffer.len(), &range)?;
                Ok(&buffer[range])
            }
            Pixels::Buffer16(_) => Err(AvifError::NoContent),
        }
    }

    pub fn slice_mut(&mut self, offset: u32, size: u32) -> AvifResult<&mut [u8]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(ptr) => {
                let offset = isize_from_usize(offset)?;
                Ok(unsafe { std::slice::from_raw_parts_mut(ptr.offset(offset), size) })
            }
            Pixels::Pointer16(_) => Err(AvifError::NoContent),
            Pixels::Buffer(buffer) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                let range = offset..end;
                check_slice_range(buffer.len(), &range)?;
                Ok(&mut buffer[range])
            }
            Pixels::Buffer16(_) => Err(AvifError::NoContent),
        }
    }

    pub fn slice16(&self, offset: u32, size: u32) -> AvifResult<&[u16]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(_) => Err(AvifError::NoContent),
            Pixels::Pointer16(ptr) => {
                let offset = isize_from_usize(offset)?;
                let ptr = (*ptr) as *const u16;
                Ok(unsafe { std::slice::from_raw_parts(ptr.offset(offset), size) })
            }
            Pixels::Buffer(_) => Err(AvifError::NoContent),
            Pixels::Buffer16(buffer) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                let range = offset..end;
                check_slice_range(buffer.len(), &range)?;
                Ok(&buffer[range])
            }
        }
    }

    pub fn slice16_mut(&mut self, offset: u32, size: u32) -> AvifResult<&mut [u16]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(_) => Err(AvifError::NoContent),
            Pixels::Pointer16(ptr) => {
                let offset = isize_from_usize(offset)?;
                let ptr = *ptr;
                Ok(unsafe { std::slice::from_raw_parts_mut(ptr.offset(offset), size) })
            }
            Pixels::Buffer(_) => Err(AvifError::NoContent),
            Pixels::Buffer16(buffer) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                let range = offset..end;
                check_slice_range(buffer.len(), &range)?;
                Ok(&mut buffer[range])
            }
        }
    }
}
