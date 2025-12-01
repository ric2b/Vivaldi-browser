/*
 * fontconfig/fc-fontations/foundries.rs
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

use read_fonts::TableProvider;
use skrifa::{
    string::{LocalizedStrings, StringId},
    FontRef, MetadataProvider,
};
use std::ffi::{CStr, CString};

fn foundry_name_to_taglike(foundry: &str) -> Option<&'static str> {
    match foundry {
        "Adobe" => Some("adobe"),
        "Bigelow" => Some("b&h"),
        "Bitstream" => Some("bitstream"),
        "Gnat" | "Iorsh" => Some("culmus"),
        "HanYang System" => Some("hanyang"),
        "Font21" => Some("hwan"),
        "IBM" => Some("ibm"),
        "International Typeface Corporation" => Some("itc"),
        "Linotype" | "LINOTYPE-HELL" => Some("linotype"),
        "Microsoft" => Some("microsoft"),
        "Monotype" => Some("monotype"),
        "Omega" => Some("omega"),
        "Tiro Typeworks" => Some("tiro"),
        "URW" => Some("urw"),
        "XFree86" => Some("xfree86"),
        "Xorg" => Some("xorg"),
        _ => None,
    }
}

fn map_foundry_from_name_entry(localized_strings: &mut LocalizedStrings) -> Option<CString> {
    localized_strings.into_iter().find_map(|foundry_name| {
        foundry_name_to_taglike(foundry_name.to_string().as_str())
            .map(|foundry| CString::new(foundry).unwrap())
    })
}

pub fn make_foundry(font: &FontRef) -> Option<CString> {
    if let Ok(os2) = font.os2() {
        let vend_bytes = os2.ach_vend_id().to_be_bytes();
        let foundry = if vend_bytes.contains(&0) {
            CStr::from_bytes_until_nul(&vend_bytes)
                .ok()
                .map(|cstr| cstr.to_owned())
        } else {
            CString::new(vend_bytes).ok()
        };

        if let Some(foundry) = foundry {
            if !foundry.is_empty() {
                return Some(foundry);
            }
        }
    }

    map_foundry_from_name_entry(&mut font.localized_strings(StringId::TRADEMARK)).or_else(|| {
        map_foundry_from_name_entry(&mut font.localized_strings(StringId::MANUFACTURER))
    })
}
