/*
 * fontconfig/fc-fontations/capabilities.rs
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

use font_types::Tag;
use read_fonts::tables::layout::ScriptRecord;
use read_fonts::TableProvider;
use skrifa::FontRef;
use std::ffi::CString;

// Mimicking issue in FreeType indexer inserting two delimiting spaces.
const SILF_CAPABILITIES_PREFIX: &str = "ttable:Silf";
const SILF_TAG: Tag = Tag::new(b"Silf");

fn capabilities_string<T: IntoIterator<Item = Tag>>(tags: T, has_silf: bool) -> Option<CString> {
    let mut deduplicated_tags: Vec<Tag> = tags.into_iter().collect::<Vec<_>>();
    deduplicated_tags.sort();
    deduplicated_tags.dedup();
    let mut capabilities_set = deduplicated_tags
        .into_iter()
        .map(|tag| format!("otlayout:{}", tag))
        .collect::<Vec<_>>();
    has_silf.then(|| capabilities_set.insert(0, SILF_CAPABILITIES_PREFIX.to_string()));

    let capabilities = capabilities_set.join(" ");
    if capabilities.is_empty() {
        return None;
    }
    CString::new(capabilities).ok()
}

fn gsub_tags<'a>(font: &'a FontRef) -> Option<&'a [ScriptRecord]> {
    Some(font.gsub().ok()?.script_list().ok()?.script_records())
}

fn gpos_tags<'a>(font: &'a FontRef) -> Option<&'a [ScriptRecord]> {
    Some(font.gpos().ok()?.script_list().ok()?.script_records())
}

pub fn make_capabilities(font: &FontRef) -> Option<CString> {
    let has_silf = font.table_data(SILF_TAG).is_some();
    capabilities_string(
        gsub_tags(font)
            .into_iter()
            .flatten()
            .chain(gpos_tags(font).into_iter().flatten())
            .map(|script_record| script_record.script_tag()),
        has_silf,
    )
}

#[cfg(test)]
mod test {
    use super::capabilities_string;
    use font_types::Tag;
    #[test]
    fn capabilities() {
        let tags = [Tag::new(b"DFLT"), Tag::new(b"cyrl"), Tag::new(b"grek")];
        assert_eq!(
            "otlayout:DFLT otlayout:cyrl otlayout:grek",
            capabilities_string(tags, false).unwrap().to_str().unwrap()
        );
        let tags = [
            Tag::new(b"DFLT"),
            Tag::new(b"cyrl"),
            Tag::new(b"cyrl"),
            Tag::new(b"grek"),
        ];

        assert_eq!(
            "otlayout:DFLT otlayout:cyrl otlayout:grek",
            capabilities_string(tags, false).unwrap().to_str().unwrap()
        );
    }

    #[test]
    fn empty_capabilities() {
        assert_eq!(None, capabilities_string([], false));
    }

    #[test]
    fn graphite_capabilities() {
        let tags = [Tag::new(b"grek")];
        assert_eq!(
            "ttable:Silf otlayout:grek",
            capabilities_string(tags, true).unwrap().to_str().unwrap()
        );
    }
}
