/*
 * fontconfig/fc-fontations/pattern_bindings/mod.rs
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

pub mod fc_wrapper;

use std::ffi::{CStr, CString};
use std::fmt::Debug;

use fcint_bindings::{
    FcLangSet, FcPattern, FcPatternObjectAddBool, FcPatternObjectAddCharSet,
    FcPatternObjectAddDouble, FcPatternObjectAddInteger, FcPatternObjectAddLangSet,
    FcPatternObjectAddRange, FcPatternObjectAddString, FC_FAMILY_OBJECT, FC_FILE_OBJECT,
};

use fc_wrapper::{FcCharSetWrapper, FcLangSetWrapper, FcPatternWrapper, FcRangeWrapper};

#[allow(unused)]
#[derive(Debug, Clone)]
pub enum PatternValue {
    String(CString),
    Boolean(bool),
    Integer(i32),
    Double(f64),
    Range(FcRangeWrapper),
    LangSet(FcLangSetWrapper),
    CharSet(FcCharSetWrapper),
}

impl From<CString> for PatternValue {
    fn from(item: CString) -> Self {
        PatternValue::String(item)
    }
}

impl From<i32> for PatternValue {
    fn from(item: i32) -> Self {
        PatternValue::Integer(item)
    }
}

impl From<bool> for PatternValue {
    fn from(item: bool) -> Self {
        PatternValue::Boolean(item)
    }
}

impl From<f64> for PatternValue {
    fn from(item: f64) -> Self {
        PatternValue::Double(item)
    }
}

impl From<FcRangeWrapper> for PatternValue {
    fn from(item: FcRangeWrapper) -> Self {
        PatternValue::Range(item)
    }
}

impl From<FcCharSetWrapper> for PatternValue {
    fn from(value: FcCharSetWrapper) -> Self {
        PatternValue::CharSet(value)
    }
}

impl From<FcLangSetWrapper> for PatternValue {
    fn from(value: FcLangSetWrapper) -> Self {
        PatternValue::LangSet(value)
    }
}

#[derive(Debug, Clone)]
pub struct PatternElement {
    pub object_id: i32,
    pub value: PatternValue,
}

impl PatternElement {
    #[allow(unused)]
    pub fn new(object_id: i32, value: PatternValue) -> Self {
        Self { object_id, value }
    }
}

#[derive(Debug, Clone)]
struct PatternAddError;

impl std::fmt::Display for PatternAddError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Failed to add object to Fontconfig pattern.")
    }
}

impl PatternElement {
    fn append_to_fc_pattern(self, pattern: *mut FcPattern) -> Result<(), PatternAddError> {
        let pattern_add_success = match self.value {
            PatternValue::String(string) => unsafe {
                FcPatternObjectAddString(pattern, self.object_id, string.as_ptr() as *const u8)
            },
            PatternValue::Boolean(value) => unsafe {
                FcPatternObjectAddBool(pattern, self.object_id, value as i32)
            },
            PatternValue::Integer(value) => unsafe {
                FcPatternObjectAddInteger(pattern, self.object_id, value)
            },
            PatternValue::Double(value) => unsafe {
                FcPatternObjectAddDouble(pattern, self.object_id, value)
            },
            PatternValue::Range(value) => unsafe {
                FcPatternObjectAddRange(pattern, self.object_id, value.as_ptr())
            },
            PatternValue::CharSet(value) => unsafe {
                FcPatternObjectAddCharSet(pattern, self.object_id, value.as_ptr())
            },
            PatternValue::LangSet(value) => unsafe {
                FcPatternObjectAddLangSet(
                    pattern,
                    self.object_id,
                    value.as_ptr() as *const FcLangSet,
                )
            },
        } == 1;
        if pattern_add_success {
            return Ok(());
        }
        Err(PatternAddError)
    }
}

#[derive(Default, Debug, Clone)]
pub struct FcPatternBuilder {
    elements: Vec<PatternElement>,
}

impl FcPatternBuilder {
    #[allow(unused)]
    pub fn new() -> Self {
        Self::default()
    }

    #[allow(unused)]
    pub fn append_element(&mut self, element: PatternElement) {
        self.elements.push(element);
    }

    pub fn family_names(&self) -> impl Iterator<Item = &CStr> {
        self.elements.iter().filter_map(|element| {
            if element.object_id == FC_FAMILY_OBJECT as i32 {
                if let PatternValue::String(s) = &element.value {
                    if !s.is_empty() {
                        return Some(s.as_c_str());
                    }
                }
            }
            None
        })
    }

    #[allow(unused)]
    pub fn create_fc_pattern(&mut self) -> Option<FcPatternWrapper> {
        let pattern = FcPatternWrapper::new()?;

        let mut family_name_or_file_name_encountered = false;

        const FAMILY_ID: i32 = FC_FAMILY_OBJECT as i32;
        const FILE_ID: i32 = FC_FILE_OBJECT as i32;

        for element in self.elements.drain(0..) {
            if let PatternElement {
                object_id: FAMILY_ID | FILE_ID,
                value: PatternValue::String(ref file_fam_name),
            } = element
            {
                if !file_fam_name.is_empty() {
                    family_name_or_file_name_encountered = true;
                }
            }
            element.append_to_fc_pattern(pattern.as_ptr()).ok()?;
        }

        if !family_name_or_file_name_encountered {
            return None;
        }

        Some(pattern)
    }
}

/// Mainly needed for finding the style PatternElement in attributes.rs.
impl<'a> IntoIterator for &'a FcPatternBuilder {
    type Item = &'a PatternElement;
    type IntoIter = std::slice::Iter<'a, PatternElement>;

    fn into_iter(self) -> Self::IntoIter {
        self.elements.iter()
    }
}

#[cfg(test)]
mod test {
    use std::ffi::CString;

    use crate::pattern_bindings::fc_wrapper::FcLangSetWrapper;

    use super::{
        fc_wrapper::FcCharSetWrapper, FcPatternBuilder, FcRangeWrapper, PatternElement,
        PatternValue,
    };
    use fontconfig_bindings::{
        FcCharSet, FcCharSetAddChar, FcCharSetHasChar, FcLangSet, FcLangSetAdd, FcLangSetHasLang,
        _FcLangResult_FcLangEqual,
    };

    use fcint_bindings::{
        FcPatternObjectGetBool, FcPatternObjectGetCharSet, FcPatternObjectGetDouble,
        FcPatternObjectGetInteger, FcPatternObjectGetLangSet, FcPatternObjectGetRange,
        FcPatternObjectGetString, FcRange, FC_CHARSET_OBJECT, FC_COLOR_OBJECT, FC_FAMILY_OBJECT,
        FC_LANG_OBJECT, FC_SLANT_OBJECT, FC_WEIGHT_OBJECT, FC_WIDTH_OBJECT,
    };

    #[test]
    fn verify_pattern_bindings() {
        let mut pattern_builder = FcPatternBuilder::new();

        // Add a bunch of test properties.
        pattern_builder.append_element(PatternElement::new(
            FC_COLOR_OBJECT as i32,
            PatternValue::Boolean(true),
        ));
        pattern_builder.append_element(PatternElement::new(
            FC_WEIGHT_OBJECT as i32,
            PatternValue::Double(800.),
        ));
        pattern_builder.append_element(PatternElement::new(
            FC_SLANT_OBJECT as i32,
            PatternValue::Integer(15),
        ));

        pattern_builder.append_element(PatternElement::new(
            FC_WIDTH_OBJECT as i32,
            PatternValue::Range(FcRangeWrapper::new(100., 400.).unwrap()),
        ));

        pattern_builder.append_element(PatternElement::new(
            FC_FAMILY_OBJECT as i32,
            PatternValue::String(CString::new("TestFont").unwrap()),
        ));

        let test_charset = FcCharSetWrapper::new().unwrap();
        const BIANG: u32 = 0x30EDE;
        unsafe {
            assert!(FcCharSetAddChar(test_charset.as_ptr(), BIANG) != 0);
        }

        pattern_builder.append_element(PatternElement::new(
            FC_CHARSET_OBJECT as i32,
            PatternValue::CharSet(test_charset),
        ));

        let test_langset = FcLangSetWrapper::new().unwrap();
        const LANG_EN: &[u8] = b"en\0";
        unsafe {
            assert!(FcLangSetAdd(test_langset.as_ptr(), LANG_EN.as_ptr()) != 0);
        }

        pattern_builder.append_element(PatternElement::new(
            FC_LANG_OBJECT as i32,
            PatternValue::LangSet(test_langset),
        ));

        let pattern = pattern_builder.create_fc_pattern().unwrap();

        let fontconfig_pattern = pattern.as_ptr();
        unsafe {
            // Verify color properties.
            let mut result: i32 = 0;
            let get_result =
                FcPatternObjectGetBool(fontconfig_pattern, FC_COLOR_OBJECT as i32, 0, &mut result);
            assert_eq!(get_result, 0);
            assert_eq!(result, 1);

            // Verify weight value.
            let mut weight_result: f64 = 0.;
            let get_result = FcPatternObjectGetDouble(
                fontconfig_pattern,
                FC_WEIGHT_OBJECT as i32,
                0,
                &mut weight_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(weight_result, 800.0);

            // Verify that weight is not a range.
            let range_result: *mut *mut FcRange = std::mem::zeroed();
            assert_eq!(
                FcPatternObjectGetRange(
                    fontconfig_pattern,
                    FC_WEIGHT_OBJECT as i32,
                    0,
                    range_result
                ),
                2
            );

            // Verify slant.
            let mut slant_result: i32 = 0;
            let get_result = FcPatternObjectGetInteger(
                fontconfig_pattern,
                FC_SLANT_OBJECT as i32,
                0,
                &mut slant_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(slant_result, 15);

            // Verify width.
            let mut width_result: *mut FcRange = std::mem::zeroed();
            let get_result = FcPatternObjectGetRange(
                fontconfig_pattern,
                FC_WIDTH_OBJECT as i32,
                0,
                &mut width_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!((*width_result).begin, 100.);
            assert_eq!((*width_result).end, 400.);

            // Verify family name.
            let mut family_result: *mut u8 = std::mem::zeroed();
            let get_result = FcPatternObjectGetString(
                fontconfig_pattern,
                FC_FAMILY_OBJECT as i32,
                0,
                &mut family_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(
                std::ffi::CStr::from_ptr(family_result as *const i8)
                    .to_str()
                    .unwrap(),
                "TestFont"
            );

            // Verify CharSet.
            let mut retrieved_charset: *mut FcCharSet = std::mem::zeroed();
            let get_result = FcPatternObjectGetCharSet(
                fontconfig_pattern,
                FC_CHARSET_OBJECT as i32,
                0,
                &mut retrieved_charset,
            );
            assert_eq!(get_result, 0);
            assert_eq!(FcCharSetHasChar(retrieved_charset, BIANG), 1);

            // Verify LangSet.
            let mut retrieved_langset: *mut FcLangSet = std::mem::zeroed();
            let get_result = FcPatternObjectGetLangSet(
                fontconfig_pattern,
                FC_LANG_OBJECT as i32,
                0,
                &mut retrieved_langset,
            );
            assert_eq!(get_result, 0);
            assert_eq!(
                FcLangSetHasLang(retrieved_langset, LANG_EN.as_ptr()),
                _FcLangResult_FcLangEqual
            );
        }
    }
}
