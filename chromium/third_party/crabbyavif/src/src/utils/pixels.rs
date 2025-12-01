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

// This struct must not be derived from the default `Clone` trait as it has to be cloned with error
// checking using the `try_clone` function.
#[derive(Debug)]
pub enum Pixels {
    // Intended for holding data from underlying native libraries. Used for 8-bit images.
    Pointer(PointerSlice<u8>),
    // Intended for holding data from underlying native libraries. Used for 10-bit, 12-bit and
    // 16-bit images.
    Pointer16(PointerSlice<u16>),
    // Used for 8-bit images.
    Buffer(Vec<u8>),
    // Used for 10-bit, 12-bit and 16-bit images.
    Buffer16(Vec<u16>),
}

impl Pixels {
    // This function may not be used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn from_raw_pointer(
        ptr: *mut u8,
        depth: u32,
        height: u32,
        mut row_bytes: u32,
    ) -> AvifResult<Self> {
        if depth > 8 {
            row_bytes /= 2;
        }
        let size = usize_from_u32(checked_mul!(height, row_bytes)?)?;
        if depth > 8 {
            Ok(Pixels::Pointer16(unsafe {
                PointerSlice::create(ptr as *mut u16, size)?
            }))
        } else {
            Ok(Pixels::Pointer(unsafe { PointerSlice::create(ptr, size)? }))
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

    pub(crate) fn pixel_bit_size(&self) -> usize {
        match self {
            Pixels::Pointer(_) => 0,
            Pixels::Pointer16(_) => 0,
            Pixels::Buffer(_) => 8,
            Pixels::Buffer16(_) => 16,
        }
    }

    pub(crate) fn has_data(&self) -> bool {
        match self {
            Pixels::Pointer(ptr) => !ptr.is_empty(),
            Pixels::Pointer16(ptr) => !ptr.is_empty(),
            Pixels::Buffer(buffer) => !buffer.is_empty(),
            Pixels::Buffer16(buffer) => !buffer.is_empty(),
        }
    }

    pub(crate) fn resize(&mut self, size: usize, default: u16) -> AvifResult<()> {
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

    pub(crate) fn is_pointer(&self) -> bool {
        matches!(self, Pixels::Pointer(_) | Pixels::Pointer16(_))
    }

    // This function may not be used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn ptr(&self) -> *const u8 {
        match self {
            Pixels::Pointer(ptr) => ptr.ptr(),
            Pixels::Buffer(buffer) => buffer.as_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    // This function may not be used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn ptr16(&self) -> *const u16 {
        match self {
            Pixels::Pointer16(ptr) => ptr.ptr(),
            Pixels::Buffer16(buffer) => buffer.as_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    // This function may not be used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn ptr_mut(&mut self) -> *mut u8 {
        match self {
            Pixels::Pointer(ptr) => ptr.ptr_mut(),
            Pixels::Buffer(buffer) => buffer.as_mut_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    // This function may not be used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn ptr16_mut(&mut self) -> *mut u16 {
        match self {
            Pixels::Pointer16(ptr) => ptr.ptr_mut(),
            Pixels::Buffer16(buffer) => buffer.as_mut_ptr(),
            _ => std::ptr::null_mut(),
        }
    }

    pub(crate) fn ptr_generic(&self) -> *const u8 {
        match self {
            Pixels::Pointer(ptr) => ptr.ptr(),
            Pixels::Pointer16(ptr) => ptr.ptr() as *const u8,
            Pixels::Buffer(buffer) => buffer.as_ptr(),
            Pixels::Buffer16(buffer) => buffer.as_ptr() as *const u8,
        }
    }

    pub fn ptr_mut_generic(&mut self) -> *mut u8 {
        match self {
            Pixels::Pointer(ptr) => ptr.ptr_mut(),
            Pixels::Pointer16(ptr) => ptr.ptr_mut() as *mut u8,
            Pixels::Buffer(buffer) => buffer.as_mut_ptr(),
            Pixels::Buffer16(buffer) => buffer.as_mut_ptr() as *mut u8,
        }
    }

    pub(crate) fn try_clone(&self) -> AvifResult<Pixels> {
        match self {
            Pixels::Pointer(ptr) => Ok(Pixels::Pointer(*ptr)),
            Pixels::Pointer16(ptr) => Ok(Pixels::Pointer16(*ptr)),
            Pixels::Buffer(buffer) => {
                let mut cloned_buffer: Vec<u8> = vec![];
                cloned_buffer
                    .try_reserve_exact(buffer.len())
                    .or(Err(AvifError::OutOfMemory))?;
                cloned_buffer.extend_from_slice(buffer);
                Ok(Pixels::Buffer(cloned_buffer))
            }
            Pixels::Buffer16(buffer16) => {
                let mut cloned_buffer16: Vec<u16> = vec![];
                cloned_buffer16
                    .try_reserve_exact(buffer16.len())
                    .or(Err(AvifError::OutOfMemory))?;
                cloned_buffer16.extend_from_slice(buffer16);
                Ok(Pixels::Buffer16(cloned_buffer16))
            }
        }
    }

    pub fn slice(&self, offset: u32, size: u32) -> AvifResult<&[u8]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(ptr) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                ptr.slice(offset..end)
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

    pub(crate) fn slice_mut(&mut self, offset: u32, size: u32) -> AvifResult<&mut [u8]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(ptr) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                ptr.slice_mut(offset..end)
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
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                ptr.slice(offset..end)
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

    pub(crate) fn slice16_mut(&mut self, offset: u32, size: u32) -> AvifResult<&mut [u16]> {
        let offset: usize = usize_from_u32(offset)?;
        let size: usize = usize_from_u32(size)?;
        match self {
            Pixels::Pointer(_) => Err(AvifError::NoContent),
            Pixels::Pointer16(ptr) => {
                let end = offset.checked_add(size).ok_or(AvifError::NoContent)?;
                ptr.slice_mut(offset..end)
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
