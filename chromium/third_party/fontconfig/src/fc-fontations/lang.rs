/*
 * fontconfig/fc-fontations/lang.rs
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
use skrifa::FontRef;
use std::ffi::CString;

// See logic and comment in FcFreeTypeLangSet, check for Han charsets to make fonts
// which advertise support for a single language not support other Han languages.
fn exclusive_lang_in_codepage_range(codepage_ranges: u64) -> Option<String> {
    // u32 is the position of the bit in the OS/2 codepage range.
    const LANG_MAP: [(u32, &str); 4] = [(17, "ja"), (18, "zh-cn"), (19, "ko"), (20, "zh-tw")];

    let mut found_languages = LANG_MAP
        .iter()
        .filter(move |bit_lang_pair| codepage_ranges & (1 << bit_lang_pair.0) != 0);

    match (found_languages.next(), found_languages.next()) {
        (Some(bit_lang_pair), None) => Some(bit_lang_pair.1.to_string()),
        _ => None,
    }
}

// Returns the language identifier string if according to OS/2 codepage ranges
// the font advertises support for only one Han font, otherwise none.
pub fn exclusive_lang(font: &FontRef) -> Option<CString> {
    let os2 = font.os2().ok()?;
    let codepage_ranges: u64 =
        ((os2.ul_code_page_range_2()? as u64) << 32) + os2.ul_code_page_range_1()? as u64;

    exclusive_lang_in_codepage_range(codepage_ranges).and_then(|string| CString::new(string).ok())
}

#[cfg(test)]
mod test {
    use super::exclusive_lang_in_codepage_range;
    #[test]
    fn exclusive_lang() {
        let non_exclusive = 1 << 17 + 1 << 20;
        assert!(exclusive_lang_in_codepage_range(non_exclusive).is_none());
        let exclusive = 1 << 17;
        assert_eq!(exclusive_lang_in_codepage_range(exclusive).unwrap(), "ja");
        let unmatched_non_han = 1 << 4 + 1 << 21;
        assert!(exclusive_lang_in_codepage_range(unmatched_non_han).is_none());
    }
}
