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

use super::types::*;

use std::ffi::CStr;
use std::os::raw::c_char;
use std::os::raw::c_void;

use crate::decoder::GenericIO;
use crate::internal_utils::io::DecoderFileIO;
use crate::internal_utils::io::DecoderRawIO;
use crate::*;

#[repr(C)]
#[derive(Clone)]
pub struct avifROData {
    pub data: *const u8,
    pub size: usize,
}

impl Default for avifROData {
    fn default() -> Self {
        avifROData {
            data: std::ptr::null(),
            size: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Debug)]
pub struct avifRWData {
    pub data: *mut u8,
    pub size: usize,
}

impl Default for avifRWData {
    fn default() -> Self {
        avifRWData {
            data: std::ptr::null_mut(),
            size: 0,
        }
    }
}

impl From<&Vec<u8>> for avifRWData {
    fn from(v: &Vec<u8>) -> Self {
        avifRWData {
            data: v.as_ptr() as *mut u8,
            size: v.len(),
        }
    }
}

impl From<&avifRWData> for Vec<u8> {
    fn from(data: &avifRWData) -> Vec<u8> {
        if data.size == 0 {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(data.data, data.size).to_vec() }
        }
    }
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifRWDataRealloc(
    raw: *mut avifRWData,
    newSize: usize,
) -> avifResult {
    check_pointer!(raw);
    let raw = deref_mut!(raw);
    if raw.size == newSize {
        return avifResult::Ok;
    }
    // Ok to use size as capacity here since we use reserve_exact.
    let mut newData: Vec<u8> = Vec::new();
    if newData.try_reserve_exact(newSize).is_err() {
        return avifResult::OutOfMemory;
    }
    if !raw.data.is_null() {
        // SAFETY: raw.data and raw.size are guaranteed to be valid. This code is basically
        // free()'ing the manually managed memory.
        let oldData = unsafe { Box::from_raw(std::slice::from_raw_parts_mut(raw.data, raw.size)) };
        let sizeToCopy = std::cmp::min(newSize, oldData.len());
        newData.extend_from_slice(&oldData[..sizeToCopy]);
    }
    newData.resize(newSize, 0);
    let mut b = newData.into_boxed_slice();
    raw.data = b.as_mut_ptr();
    std::mem::forget(b);
    raw.size = newSize;
    avifResult::Ok
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
/// - if data is not null, it has to point to a valid buffer of size bytes.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifRWDataSet(
    raw: *mut avifRWData,
    data: *const u8,
    size: usize,
) -> avifResult {
    if size != 0 {
        check_pointer!(raw);
        check_pointer!(data);
        // SAFETY: Pre-conditions are met to call this function.
        let res = unsafe { crabby_avifRWDataRealloc(raw, size) };
        if res != avifResult::Ok {
            return res;
        }
        // SAFETY: The pointers are guaranteed to be valid because of the pre-conditions.
        unsafe {
            std::ptr::copy_nonoverlapping(data, (*raw).data, size);
        }
    } else {
        // SAFETY: Pre-conditions are met to call this function.
        unsafe {
            crabby_avifRWDataFree(raw);
        }
    }
    avifResult::Ok
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifRWDataFree(raw: *mut avifRWData) {
    check_pointer_or_return!(raw);
    let raw = deref_mut!(raw);
    if raw.data.is_null() {
        return;
    }
    // SAFETY: The pointers are guaranteed to be valid because of the pre-conditions.
    let _ = unsafe { Box::from_raw(std::slice::from_raw_parts_mut(raw.data, raw.size)) };
}

pub type avifIODestroyFunc = Option<unsafe extern "C" fn(io: *mut avifIO)>;
pub type avifIOReadFunc = unsafe extern "C" fn(
    io: *mut avifIO,
    readFlags: u32,
    offset: u64,
    size: usize,
    out: *mut avifROData,
) -> avifResult;
pub type avifIOWriteFunc = unsafe extern "C" fn(
    io: *mut avifIO,
    writeFlags: u32,
    offset: u64,
    data: *const u8,
    size: usize,
) -> avifResult;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct avifIO {
    destroy: avifIODestroyFunc,
    read: avifIOReadFunc,
    write: avifIOWriteFunc,
    sizeHint: u64,
    persistent: avifBool,
    data: *mut c_void,
}

pub struct avifIOWrapper {
    data: avifROData,
    io: *mut avifIO,
}

impl avifIOWrapper {
    pub fn create(io: *mut avifIO) -> Self {
        Self {
            io,
            data: Default::default(),
        }
    }
}

impl Drop for avifIOWrapper {
    fn drop(&mut self) {
        if !self.io.is_null() {
            if let Some(destroy) = deref_const!(self.io).destroy {
                // SAFETY: Calling into a C function.
                unsafe {
                    destroy(self.io);
                }
            }
        }
    }
}

impl crate::decoder::IO for avifIOWrapper {
    #[cfg_attr(feature = "disable_cfi", sanitize(cfi = "off"))]
    fn read(&mut self, offset: u64, size: usize) -> AvifResult<&[u8]> {
        // SAFETY: Calling into a C function.
        let res = unsafe {
            ((*self.io).read)(self.io, 0, offset, size, &mut self.data as *mut avifROData)
        };
        if res != avifResult::Ok {
            let err: AvifError = res.into();
            return Err(err);
        }
        if self.data.size == 0 {
            Ok(&[])
        } else if self.data.data.is_null() {
            AvifError::unknown_error("data pointer was null but size was not zero")
        } else {
            // SAFETY: The pointers are guaranteed to be valid based on the checks above.
            Ok(unsafe { std::slice::from_raw_parts(self.data.data, self.data.size) })
        }
    }
    fn size_hint(&self) -> u64 {
        deref_const!(self.io).sizeHint
    }
    fn persistent(&self) -> bool {
        deref_const!(self.io).persistent != 0
    }
}

pub struct avifCIOWrapper {
    io: GenericIO,
    buf: Vec<u8>,
}

/// # Safety
/// Unused C API function.
#[no_mangle]
unsafe extern "C" fn cioDestroy(_io: *mut avifIO) {}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if io is not null, it has to point to a valid avifIO object.
/// - if out is not null, it has to point to a valid avifROData object.
#[no_mangle]
unsafe extern "C" fn cioRead(
    io: *mut avifIO,
    _readFlags: u32,
    offset: u64,
    size: usize,
    out: *mut avifROData,
) -> avifResult {
    if io.is_null() || out.is_null() {
        return avifResult::IoError;
    }
    let io = deref_mut!(io);
    if io.data.is_null() {
        return avifResult::IoError;
    }
    let cio = deref_mut!(io.data as *mut avifCIOWrapper);
    match cio.io.read(offset, size) {
        Ok(data) => {
            cio.buf.clear();
            if cio.buf.try_reserve_exact(data.len()).is_err() {
                return avifResult::OutOfMemory;
            }
            cio.buf.extend_from_slice(data);
        }
        Err(_) => return avifResult::IoError,
    }
    deref_mut!(out).data = cio.buf.as_ptr();
    deref_mut!(out).size = cio.buf.len();
    avifResult::Ok
}

/// # Safety
/// Unused C API function.
#[no_mangle]
unsafe extern "C" fn cioWrite(
    _io: *mut avifIO,
    _writeFlags: u32,
    _offset: u64,
    _data: *const u8,
    _size: usize,
) -> avifResult {
    avifResult::Ok
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if data is not null, it has to be a valid buffer of size bytes.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifIOCreateMemoryReader(
    data: *const u8,
    size: usize,
) -> *mut avifIO {
    if data.is_null() {
        return std::ptr::null_mut();
    }
    let cio = Box::new(avifCIOWrapper {
        // SAFETY: The pointers are guaranteed to be valid because of the pre-conditions.
        io: Box::new(unsafe { DecoderRawIO::create(data, size) }),
        buf: Vec::new(),
    });
    let io = Box::new(avifIO {
        destroy: Some(cioDestroy),
        read: cioRead,
        write: cioWrite,
        sizeHint: size as u64,
        persistent: 0,
        data: Box::into_raw(cio) as *mut c_void,
    });
    Box::into_raw(io)
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if filename is not null, it has to be a valid C-style string.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifIOCreateFileReader(filename: *const c_char) -> *mut avifIO {
    if filename.is_null() {
        return std::ptr::null_mut();
    }
    // SAFETY: filename is guaranteed to be a valid C-style string based on the pre-condition.
    let filename = String::from(unsafe { CStr::from_ptr(filename) }.to_str().unwrap_or(""));
    let file_io = match DecoderFileIO::create(&filename) {
        Ok(x) => x,
        Err(_) => return std::ptr::null_mut(),
    };
    let cio = Box::new(avifCIOWrapper {
        io: Box::new(file_io),
        buf: Vec::new(),
    });
    let io = Box::new(avifIO {
        destroy: Some(cioDestroy),
        read: cioRead,
        write: cioWrite,
        sizeHint: cio.io.size_hint(),
        persistent: 0,
        data: Box::into_raw(cio) as *mut c_void,
    });
    Box::into_raw(io)
}

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if io is not null, it has to point to a valid avifIO object.
#[no_mangle]
pub unsafe extern "C" fn crabby_avifIODestroy(io: *mut avifIO) {
    check_pointer_or_return!(io);
    // SAFETY: the pointers are guaranteed to be valid based on the pre-condition.
    unsafe {
        let data = (*io).data as *mut avifCIOWrapper;
        if !data.is_null() {
            let _ = Box::from_raw(data);
        }
        let _ = Box::from_raw(io);
    }
}
