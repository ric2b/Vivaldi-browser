/*
 * fontconfig/fc-fontations/mod.rs
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

mod attributes;
mod bitmap;
mod capabilities;
mod charset;
mod foundries;
mod instance_enumerate;
mod lang;
mod name_records;
mod names;
mod pattern_bindings;

use attributes::append_style_elements;
use bitmap::add_pixel_size;
use capabilities::make_capabilities;
use foundries::make_foundry;
use lang::exclusive_lang;
use names::add_names;

use fontconfig_bindings::{FcFontSet, FcFontSetAdd, FcPattern};

use fcint_bindings::{
    FcLangSetFromCharSet, FC_CAPABILITY_OBJECT, FC_CHARSET_OBJECT, FC_COLOR_OBJECT, FC_FILE_OBJECT,
    FC_FONTFORMAT_OBJECT, FC_FONTVERSION_OBJECT, FC_FONT_HAS_HINT_OBJECT, FC_FONT_WRAPPER_OBJECT,
    FC_FOUNDRY_OBJECT, FC_LANG_OBJECT, FC_ORDER_OBJECT, FC_OUTLINE_OBJECT, FC_SCALABLE_OBJECT,
    FC_SYMBOL_OBJECT,
};

use font_types::Tag;
use pattern_bindings::{fc_wrapper::FcLangSetWrapper, FcPatternBuilder, PatternElement};
use skrifa::MetadataProvider;
use std::str::FromStr;

use read_fonts::{FileRef, FontRef, TableProvider};

use std::{
    ffi::{CStr, CString, OsStr},
    iter::IntoIterator,
    os::unix::ffi::OsStrExt,
};

use instance_enumerate::{all_instances, fonts_and_indices};

#[no_mangle]
/// Externally called in fcfontations.c as the file scanner function
/// similar to the job that FreeType performs.
///
/// # Safety
/// * At this point, the font file path is not dereferenced.
/// * In this initial sanity check mock call, only one empty pattern
///   is added to the FontSet, which is null checked, which is sound.
pub unsafe extern "C" fn add_patterns_to_fontset(
    font_file: *const libc::c_char,
    font_set: *mut FcFontSet,
) -> libc::c_int {
    let font_path = unsafe { OsStr::from_bytes(CStr::from_ptr(font_file).to_bytes()) };
    let bytes = std::fs::read(font_path).ok().unwrap_or_default();
    let fileref = FileRef::new(&bytes).ok();

    let fonts = fonts_and_indices(fileref);

    let mut patterns_added: u32 = 0;
    for (font, ttc_index) in fonts {
        for pattern in build_patterns_for_font(&font, font_file, ttc_index) {
            unsafe {
                if FcFontSetAdd(font_set, pattern) == 0 {
                    return 0;
                }
                patterns_added += 1;
            }
        }
    }

    // Fontations does not natively understand WOFF/WOFF2 compressed file,
    // if we are asked to scan one of those, only add wrapper information
    // and filename.
    if patterns_added == 0 {
        try_append_woff_pattern(font_set, bytes.as_slice(), font_file);
    }

    1
}

/// Used for controlling FontConfig's behavior per font instance.
///
/// We add one pattern for the default instance, one for each named instance,
/// and one for using the font as a variable font, with ranges of values where applicable.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(dead_code)]
enum InstanceMode {
    Default,
    Named(i32),
    Variable,
}

fn try_append_woff_pattern(font_set: *mut FcFontSet, bytes: &[u8], font_file: *const libc::c_char) {
    let wrapper: Option<CString> = match bytes.get(0..4) {
        Some(b"wOFF") => CString::new("WOFF").ok(),
        Some(b"wOF2") => CString::new("WOFF2").ok(),
        _ => None,
    };

    if let Some(w) = wrapper {
        let mut pattern = FcPatternBuilder::new();
        pattern.append_element(PatternElement::new(FC_FONT_WRAPPER_OBJECT as i32, w.into()));
        add_font_file_name(&mut pattern, font_file);

        pattern
            .create_fc_pattern()
            .map(|p| unsafe { FcFontSetAdd(font_set, p.into_raw() as *mut FcPattern) });
    }
}

fn has_one_of_tables<I>(font_ref: &FontRef, tags: I) -> bool
where
    I: IntoIterator,
    I::Item: ToString,
{
    tags.into_iter().fold(false, |has_tag, tag| {
        let tag = Tag::from_str(tag.to_string().as_str());
        if let Ok(tag_converted) = tag {
            return has_tag | font_ref.data_for_tag(tag_converted).is_some();
        }
        has_tag
    })
}

fn has_hint(font_ref: &FontRef) -> bool {
    if has_one_of_tables(font_ref, ["fpgm", "cvt "]) {
        return true;
    }

    if let Some(prep_table) = font_ref.data_for_tag(Tag::new(b"prep")) {
        return prep_table.len() > 7;
    }
    false
}

fn add_font_file_name(pattern: &mut FcPatternBuilder, font_file: *const libc::c_char) {
    if !font_file.is_null() {
        let filename = unsafe { std::ffi::CStr::from_ptr(font_file) };

        pattern.append_element(PatternElement::new(
            FC_FILE_OBJECT as i32,
            filename.to_owned().into(),
        ));
    }
}

fn build_patterns_for_font(
    font: &FontRef,
    font_file: *const libc::c_char,
    ttc_index: Option<i32>,
) -> Vec<*mut FcPattern> {
    let mut pattern = FcPatternBuilder::new();

    let has_glyf = has_one_of_tables(font, ["glyf"]);
    let has_cff = has_one_of_tables(font, ["CFF ", "CFF2"]);
    let has_color = has_one_of_tables(font, ["COLR", "SVG ", "CBLC", "SBIX"]);

    // Color and Outlines
    let has_outlines = has_glyf | has_cff;

    pattern.append_element(PatternElement::new(
        FC_OUTLINE_OBJECT as i32,
        has_outlines.into(),
    ));
    pattern.append_element(PatternElement::new(
        FC_COLOR_OBJECT as i32,
        has_color.into(),
    ));
    pattern.append_element(PatternElement::new(
        FC_SCALABLE_OBJECT as i32,
        (has_outlines || has_color).into(),
    ));
    pattern.append_element(PatternElement::new(
        FC_FONT_HAS_HINT_OBJECT as i32,
        has_hint(font).into(),
    ));

    match (has_glyf, has_cff) {
        (_, true) => {
            pattern.append_element(PatternElement::new(
                FC_FONTFORMAT_OBJECT as i32,
                CString::new("CFF").unwrap().into(),
            ));
        }
        _ => {
            pattern.append_element(PatternElement::new(
                FC_FONTFORMAT_OBJECT as i32,
                CString::new("TrueType").unwrap().into(),
            ));
        }
    }

    let foundry_string = make_foundry(font).unwrap_or(CString::new("unknown").unwrap());

    pattern.append_element(PatternElement::new(
        FC_FOUNDRY_OBJECT as i32,
        foundry_string.into(),
    ));

    pattern.append_element(PatternElement::new(
        FC_SYMBOL_OBJECT as i32,
        font.charmap().is_symbol().into(),
    ));

    if let Some(capabilities) = make_capabilities(font) {
        pattern.append_element(PatternElement::new(
            FC_CAPABILITY_OBJECT as i32,
            capabilities.into(),
        ));
    };

    // CharSet and Langset.
    if let Some(charset) = charset::make_charset(font) {
        let exclusive_lang = exclusive_lang(font);

        unsafe {
            let langset = FcLangSetWrapper::from_raw(FcLangSetFromCharSet(
                charset.as_ptr(),
                exclusive_lang
                    .as_ref()
                    .map_or(std::ptr::null(), |lang| lang.as_bytes_with_nul().as_ptr()),
            ));

            pattern.append_element(PatternElement::new(
                FC_CHARSET_OBJECT as i32,
                charset.into(),
            ));

            // TODO: Move FcFreeTypeLangSet to a different name, as the function does not actually depend on FreeType.
            if !langset.is_null() {
                pattern.append_element(PatternElement::new(FC_LANG_OBJECT as i32, langset.into()));
            }
        }
    };

    let version = font
        .head()
        .ok()
        .map(|head| head.font_revision())
        .unwrap_or_default()
        .to_bits();

    add_font_file_name(&mut pattern, font_file);

    pattern.append_element(PatternElement::new(
        FC_FONT_WRAPPER_OBJECT as i32,
        CString::new("SFNT").unwrap().into(),
    ));

    pattern.append_element(PatternElement::new(
        FC_FONTVERSION_OBJECT as i32,
        version.into(),
    ));

    add_pixel_size(&mut pattern, font);

    pattern.append_element(PatternElement::new(FC_ORDER_OBJECT as i32, 0.into()));

    // So far the pattern elements applied to te whole font file, in the below,
    // clone the current pattern state and add instance specific
    // attributes. FontConfig for variable fonts produces a pattern for the
    // default instance, each named instance, and a separate one for the
    // "variable instance", which may contain ranges for pattern elements that
    // describe variable aspects, such as weight of the font.
    all_instances(font)
        .flat_map(move |instance_mode| {
            let mut instance_pattern = pattern.clone();

            // Family, full name, postscript name, etc.
            // Includes adding style name to the pattern, which is then used by append_style_elements.
            add_names(font, instance_mode, &mut instance_pattern);

            // Style names: fcfreetype adds TT_NAME_ID_WWS_SUBFAMILY, TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY,
            // TT_NAME_ID_FONT_SUBFAMILY as FC_STYLE_OBJECT, FC_STYLE_OBJECT_LANG unless a named instance
            // is added,then the instance's name id is used as FC_STYLE_OBJECT.
            append_style_elements(font, instance_mode, ttc_index, &mut instance_pattern);

            instance_pattern
                .create_fc_pattern()
                .map(|wrapper| wrapper.into_raw() as *mut FcPattern)
        })
        .collect::<Vec<_>>()
}

#[cfg(test)]
mod test {
    use crate::add_patterns_to_fontset;
    use fontconfig_bindings::{FcFontSetCreate, FcFontSetDestroy};
    use std::ffi::CString;

    #[test]
    fn basic_pattern_construction() {
        unsafe {
            let font_set = FcFontSetCreate();
            assert!(add_patterns_to_fontset(CString::new("").unwrap().into_raw(), font_set) == 1);
            FcFontSetDestroy(font_set);
        }
    }
}
