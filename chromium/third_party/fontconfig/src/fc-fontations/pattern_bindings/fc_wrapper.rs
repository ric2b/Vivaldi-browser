/*
 * fontconfig/fc-fontations/pattern_bindings/fc_wrapper.rs
 *
 * Copyright 2025 Google LLC.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

use fontconfig_bindings::{FcLangSet, FcLangSetCopy, FcLangSetCreate, FcLangSetDestroy};

use fcint_bindings::{
    FcCharSet, FcCharSetAddChar, FcCharSetCopy, FcCharSetCreate, FcCharSetDestroy, FcPattern,
    FcPatternCreate, FcPatternDestroy, FcRange, FcRangeCopy, FcRangeCreateDouble, FcRangeDestroy,
};

macro_rules! wrap_fc_object {
    (
        $wrapper_name:ident,
        $wrapped_type:ident,
        $destroy_fn:ident
        $(, $copy_fn:ident)?
    ) => {
        #[allow(unused)]
        #[derive(Debug)]
        pub struct $wrapper_name {
            inner: *mut $wrapped_type,
        }

        impl $wrapper_name {
            #[allow(unused)]
            pub fn from_raw(ptr: *mut $wrapped_type) -> Self {
                Self { inner: ptr }
            }

            #[allow(unused)]
            pub fn into_raw(self) -> *mut $wrapped_type {
                let ptr = self.inner;
                std::mem::forget(self);
                ptr
            }

            #[allow(unused)]
            pub fn as_ptr(&self) -> *mut $wrapped_type {
                assert!(!self.inner.is_null());
                self.inner
            }
        }

        impl Drop for $wrapper_name {
            fn drop(&mut self) {
                unsafe {
                    $destroy_fn(self.inner);
                }
            }
        }

        $(
            impl Clone for $wrapper_name {
                fn clone(&self) -> Self {
                    Self {
                        inner: unsafe { $copy_fn(self.inner) },
                    }
                }
            }
        )?
    };
}

wrap_fc_object! {
    FcRangeWrapper,
    FcRange,
    FcRangeDestroy,
    FcRangeCopy
}

impl FcRangeWrapper {
    #[allow(unused)]
    pub fn new(min: f64, max: f64) -> Option<Self> {
        unsafe {
            let ptr = FcRangeCreateDouble(min, max);
            if ptr.is_null() {
                None
            } else {
                Some(Self { inner: ptr })
            }
        }
    }
}

wrap_fc_object! {
    FcPatternWrapper,
    FcPattern,
    FcPatternDestroy
}

impl FcPatternWrapper {
    #[allow(unused)]
    pub fn new() -> Option<Self> {
        // Corrected new function for FcPattern
        unsafe {
            let ptr = FcPatternCreate();
            if ptr.is_null() {
                None
            } else {
                Some(Self { inner: ptr })
            }
        }
    }
}

wrap_fc_object! {
    FcCharSetWrapper,
    FcCharSet,
    FcCharSetDestroy,
    FcCharSetCopy
}

impl FcCharSetWrapper {
    pub fn new() -> Option<Self> {
        let created_charset: *mut FcCharSet;
        unsafe {
            created_charset = FcCharSetCreate();
        }
        if created_charset.is_null() {
            None
        } else {
            Some(Self {
                inner: created_charset,
            })
        }
    }

    pub fn add_char(&mut self, char: u32) -> Result<(), ()> {
        unsafe {
            if FcCharSetAddChar(self.as_ptr(), char) == 1 {
                Ok(())
            } else {
                Err(())
            }
        }
    }
}

wrap_fc_object! {
    FcLangSetWrapper,
    FcLangSet,
    FcLangSetDestroy,
    FcLangSetCopy
}

impl FcLangSetWrapper {
    #[allow(unused)]
    pub fn new() -> Option<Self> {
        let created_langset: *mut fontconfig_bindings::FcLangSet;
        unsafe {
            created_langset = FcLangSetCreate();
        }
        if created_langset.is_null() {
            None
        } else {
            Some(Self {
                inner: created_langset,
            })
        }
    }

    pub fn is_null(&self) -> bool {
        self.inner.is_null()
    }
}
