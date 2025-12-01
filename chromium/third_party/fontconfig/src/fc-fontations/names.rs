/*
 * fontconfig/fc-fontations/names.rs
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

use skrifa::string::LocalizedString;
use skrifa::{string::StringId, MetadataProvider};

use fcint_bindings::{
    FC_FAMILYLANG_OBJECT, FC_FAMILY_OBJECT, FC_FULLNAMELANG_OBJECT, FC_FULLNAME_OBJECT,
    FC_GENERIC_FAMILY_OBJECT, FC_INVALID_OBJECT, FC_POSTSCRIPT_NAME_OBJECT, FC_STYLELANG_OBJECT,
    FC_STYLE_OBJECT,
};
use fontconfig_bindings::{
    FC_FAMILY_EMOJI, FC_FAMILY_MATH, FC_FAMILY_MONO, FC_FAMILY_SANS, FC_FAMILY_SERIF,
    FC_FAMILY_UNKNOWN,
};

use crate::{name_records::FcSortedNameRecords, FcPatternBuilder, InstanceMode, PatternElement};
use read_fonts::{FontRef, TableProvider};
use std::ffi::CStr;
use std::{
    error::Error,
    ffi::{CString, OsStr},
    fmt,
    path::Path,
};

use std::collections::HashSet;

#[derive(Debug, Clone)]
pub enum NameAddingError {
    NoFamilyName,
    NoNameTable,
}

impl fmt::Display for NameAddingError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            NameAddingError::NoFamilyName => write!(f, "Could not determine font family name."),
            NameAddingError::NoNameTable => write!(f, "Font has no 'name' table."),
        }
    }
}

impl Error for NameAddingError {}

fn object_ids_for_name_id(string_id: StringId) -> Option<(i32, i32)> {
    match string_id {
        StringId::FAMILY_NAME | StringId::WWS_FAMILY_NAME | StringId::TYPOGRAPHIC_FAMILY_NAME => {
            Some((FC_FAMILY_OBJECT as i32, FC_FAMILYLANG_OBJECT as i32))
        }
        StringId::FULL_NAME | StringId::COMPATIBLE_FULL_NAME => {
            Some((FC_FULLNAME_OBJECT as i32, FC_FULLNAMELANG_OBJECT as i32))
        }
        StringId::POSTSCRIPT_NAME => {
            Some((FC_POSTSCRIPT_NAME_OBJECT as i32, FC_INVALID_OBJECT as i32))
        }
        StringId::SUBFAMILY_NAME
        | StringId::WWS_SUBFAMILY_NAME
        | StringId::TYPOGRAPHIC_SUBFAMILY_NAME => {
            Some((FC_STYLE_OBJECT as i32, FC_STYLELANG_OBJECT as i32))
        }
        _ => None,
    }
}

fn normalize_name(name: &CString) -> String {
    name.clone()
        .into_string()
        .unwrap_or_default()
        .to_lowercase()
        .replace(' ', "")
}

fn mangle_postscript_name_for_named_instance(
    font: &FontRef,
    named_instance_id: i32,
) -> Option<CString> {
    let instance_ps_name_id = font
        .named_instances()
        .get(named_instance_id as usize)?
        .postscript_name_id();

    if let Some(ps_name_id) = instance_ps_name_id {
        let ps_name = font
            .localized_strings(ps_name_id)
            .english_or_first()?
            .clone()
            .to_string();
        CString::new(ps_name).ok()
    } else {
        let instance_subfamily_name_id = font
            .named_instances()
            .get(named_instance_id as usize)?
            .subfamily_name_id();
        let prefix = font
            .localized_strings(StringId::VARIATIONS_POSTSCRIPT_NAME_PREFIX)
            .english_or_first()
            .or(font
                .localized_strings(StringId::FAMILY_NAME)
                .english_or_first())?
            .to_string()
            + "-";
        let subfam = font
            .localized_strings(instance_subfamily_name_id)
            .english_or_first()?
            .to_string();

        let assembled = prefix + &subfam;
        let assembled = assembled.replace(" ", "");
        CString::new(assembled).ok()
    }
}

fn mangle_subfamily_name_for_named_instance(
    font: &FontRef,
    named_instance_id: i32,
) -> Option<CString> {
    let instance_subfamily_name_id = font
        .named_instances()
        .get(named_instance_id as usize)?
        .subfamily_name_id();
    let subfamily = font
        .localized_strings(instance_subfamily_name_id)
        .english_or_first()?
        .clone()
        .to_string();
    CString::new(subfamily).ok()
}

fn mangle_full_name_for_named_instance(font: &FontRef, named_instance_id: i32) -> Option<CString> {
    let instance_subfamily_name_id = font
        .named_instances()
        .get(named_instance_id as usize)?
        .subfamily_name_id();
    let full_name = font
        .localized_strings(StringId::FAMILY_NAME)
        .english_or_first()?
        .to_string()
        + " ";
    let subfam = font
        .localized_strings(instance_subfamily_name_id)
        .english_or_first()?
        .to_string();

    CString::new(full_name + &subfam).ok()
}

fn get_generic_family(family_name: &CStr) -> i32 {
    [
        ("mono", FC_FAMILY_MONO),
        ("sans", FC_FAMILY_SANS),
        ("serif", FC_FAMILY_SERIF),
        ("emoji", FC_FAMILY_EMOJI),
        ("math", FC_FAMILY_MATH),
    ]
    .into_iter()
    .find_map(|(font_sub_name, generic_family_id)| {
        family_name
            .to_string_lossy()
            .into_owned()
            .to_lowercase()
            .contains(font_sub_name)
            .then_some(generic_family_id)
    })
    .unwrap_or(FC_FAMILY_UNKNOWN) as i32
}

pub fn add_names(
    font: &FontRef,
    font_file: &Path,
    instance_mode: InstanceMode,
    pattern: &mut FcPatternBuilder,
) -> Result<(), NameAddingError> {
    let mut already_encountered_names: HashSet<(i32, String)> = HashSet::new();
    let name_table = font.name().map_err(|_| NameAddingError::NoNameTable)?;

    for name_record in FcSortedNameRecords::new(&name_table) {
        let string_id = name_record.name_id();
        if let Some(object_ids) = object_ids_for_name_id(string_id) {
            let localized = LocalizedString::new(&name_table, &name_record);

            let name = if localized.to_string().is_empty() {
                None
            } else {
                let mut name_trimmed = localized.to_string().trim().to_owned();
                // PostScript name sanitization.
                if object_ids.0 == FC_POSTSCRIPT_NAME_OBJECT as i32 {
                    name_trimmed = name_trimmed.replace(" ", "");
                }
                CString::new(name_trimmed).ok()
            };
            let language = localized.language().or(Some("und")).and_then(|lang| {
                let lang = lang.to_lowercase();
                let lang = if lang.starts_with("zh") {
                    lang
                } else {
                    lang.split('-').next().unwrap_or(&lang).to_string()
                };
                CString::new(lang).ok()
            });

            // Instance postscript name.
            let name = match (instance_mode, string_id) {
                (InstanceMode::Named(instance), StringId::POSTSCRIPT_NAME) => {
                    mangle_postscript_name_for_named_instance(font, instance).or(name)
                }
                (InstanceMode::Named(instance), StringId::SUBFAMILY_NAME) => {
                    mangle_subfamily_name_for_named_instance(font, instance).or(name)
                }
                (InstanceMode::Named(instance), StringId::FULL_NAME) => {
                    mangle_full_name_for_named_instance(font, instance).or(name)
                }
                (
                    InstanceMode::Variable,
                    StringId::SUBFAMILY_NAME
                    | StringId::WWS_SUBFAMILY_NAME
                    | StringId::TYPOGRAPHIC_SUBFAMILY_NAME
                    | StringId::FULL_NAME
                    | StringId::POSTSCRIPT_NAME,
                ) => None,
                _ => name,
            };

            if let (Some(name), Some(language)) = (name, language) {
                if !name.is_empty() {
                    let normalized_name = normalize_name(&name);
                    if already_encountered_names.contains(&(object_ids.0, normalized_name.clone()))
                    {
                        continue;
                    }
                    already_encountered_names.insert((object_ids.0, normalized_name));
                    pattern.append_element(PatternElement::new(object_ids.0, name.into()));
                    // Postscriptname for example does not attach a language.
                    if object_ids.1 != FC_INVALID_OBJECT as i32 {
                        pattern.append_element(PatternElement::new(object_ids.1, language.into()));
                    }
                }
            }
        }
    }

    // Fallback to a family name based on filename.
    if pattern.family_names().next().is_none() {
        let basename = font_file
            .file_stem()
            .and_then(OsStr::to_str)
            .and_then(|s| CString::new(s).ok())
            .ok_or(NameAddingError::NoFamilyName)?;
        pattern.append_element(PatternElement::new(
            FC_FAMILY_OBJECT as i32,
            basename.into(),
        ));
        if let Ok(lang) = CString::new("en") {
            pattern.append_element(PatternElement::new(
                FC_FAMILYLANG_OBJECT as i32,
                lang.into(),
            ));
        }
    }

    // Determine generic family and append.
    let generic_family = pattern
        .family_names()
        .find_map(|family_name| {
            let id = get_generic_family(family_name);
            if id != FC_FAMILY_UNKNOWN as i32 {
                Some(id)
            } else {
                None
            }
        })
        .unwrap_or(FC_FAMILY_UNKNOWN as i32);
    pattern.append_element(PatternElement::new(
        FC_GENERIC_FAMILY_OBJECT as i32,
        generic_family.into(),
    ));

    Ok(())
}
