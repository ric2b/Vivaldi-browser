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
mod capabilities;
mod foundries;
mod instance_enumerate;
mod names;
mod pattern_bindings;

use attributes::append_style_elements;
use capabilities::make_capabilities;
use foundries::make_foundry;
use names::add_names;

use fc_fontations_bindgen::{
    fcint::{
        FC_CAPABILITY_OBJECT, FC_COLOR_OBJECT, FC_DECORATIVE_OBJECT, FC_FONTFORMAT_OBJECT,
        FC_FONTVERSION_OBJECT, FC_FONT_HAS_HINT_OBJECT, FC_FOUNDRY_OBJECT, FC_OUTLINE_OBJECT,
        FC_SCALABLE_OBJECT,
    },
    FcFontSet, FcFontSetAdd, FcPattern,
};

use font_types::Tag;
use pattern_bindings::{FcPatternBuilder, PatternElement};
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
    for (font, ttc_index) in fonts {
        for pattern in build_patterns_for_font(&font, font_file, ttc_index) {
            if FcFontSetAdd(font_set, pattern) == 0 {
                return 0;
            }
        }
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

fn build_patterns_for_font(
    font: &FontRef,
    _: *const libc::c_char,
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

    if let Some(capabilities) = make_capabilities(font) {
        pattern.append_element(PatternElement::new(
            FC_CAPABILITY_OBJECT as i32,
            capabilities.into(),
        ));
    };

    let version = font
        .head()
        .ok()
        .map(|head| head.font_revision())
        .unwrap_or_default()
        .to_bits();

    pattern.append_element(PatternElement::new(
        FC_FONTVERSION_OBJECT as i32,
        version.into(),
    ));

    // So far the pattern elements applied to te whole font file, in the below,
    // clone the current pattern state and add instance specific
    // attributes. FontConfig for variable fonts produces a pattern for the
    // default instance, each named instance, and a separate one for the
    // "variable instance", which may contain ranges for pattern elements that
    // describe variable aspects, such as weight of the font.
    all_instances(font)
        .flat_map(move |instance_mode| {
            let mut instance_pattern = pattern.clone();

            // Style names: fcfreetype adds TT_NAME_ID_WWS_SUBFAMILY, TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY,
            // TT_NAME_ID_FONT_SUBFAMILY as FC_STYLE_OBJECT, FC_STYLE_OBJECT_LANG unless a named instance
            // is added,then the instance's name id is used as FC_STYLE_OBJECT.

            append_style_elements(font, instance_mode, ttc_index, &mut instance_pattern);

            // For variable fonts:
            // Names (mainly postscript name and style), weight, width and opsz (font-size?) are affected.
            // * Add the variable font itself, with ranges for weight, width, opsz.
            // * Add an entry for each named instance
            //   * With instance name turning into FC_STYLE_OBJECT.
            //   * Fixed width, wgth, opsz
            // * Add the default instance with fixed values.
            let mut had_decoratve = false;
            // Family and full name.
            add_names(
                font,
                instance_mode,
                &mut instance_pattern,
                &mut had_decoratve,
            );

            instance_pattern.append_element(PatternElement::new(
                FC_DECORATIVE_OBJECT as i32,
                had_decoratve.into(),
            ));

            instance_pattern
                .create_fc_pattern()
                .map(|wrapper| wrapper.into_raw() as *mut FcPattern)
        })
        .collect::<Vec<_>>()
}

#[cfg(test)]
mod test {
    use crate::add_patterns_to_fontset;
    use fc_fontations_bindgen::{FcFontSetCreate, FcFontSetDestroy};
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
