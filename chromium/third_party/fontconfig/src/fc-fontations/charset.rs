/*
 * fontconfig/fc-fontations/charset.rs
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

use font_types::GlyphId;

use skrifa::outline::DrawSettings;
use skrifa::prelude::{LocationRef, Size};
use skrifa::{outline::pen::OutlinePen, FontRef, MetadataProvider};

use crate::pattern_bindings::fc_wrapper::FcCharSetWrapper;

struct HasAnyContourPen {
    has_ink: bool,
}

impl HasAnyContourPen {
    pub fn new() -> Self {
        Self { has_ink: false }
    }
}

impl OutlinePen for HasAnyContourPen {
    fn move_to(&mut self, _x: f32, _y: f32) {}
    fn line_to(&mut self, _x: f32, _y: f32) {}
    fn quad_to(&mut self, _cx0: f32, _cy0: f32, _x: f32, _y: f32) {}
    fn curve_to(&mut self, _cx0: f32, _cy0: f32, _cx1: f32, _cy1: f32, _x: f32, _y: f32) {}
    fn close(&mut self) {
        self.has_ink = true;
    }
}

const CONTROL_CHAR_MAX: u32 = 0x1F;

// For control characters, check for whether they have ink/any-contour since
// these might be mapped to space. Simulates fcfreetype.c behavior.
// Specifically, FreeType checks for face->glyph->outline.n_contours - so,
// number of contours, not for actually pixel coverage / contour area.
fn control_char_without_ink(font: &FontRef, char: u32, glyph: GlyphId) -> bool {
    let mut pen = HasAnyContourPen::new();
    if char <= CONTROL_CHAR_MAX {
        font.outline_glyphs().get(glyph).and_then(|outline| {
            outline
                .draw(
                    DrawSettings::unhinted(Size::new(12.0), LocationRef::default()),
                    &mut pen,
                )
                .ok()
        });
        if !pen.has_ink {
            return true;
        }
    }
    false
}

pub fn make_charset(font: &FontRef) -> Option<FcCharSetWrapper> {
    let mut charset = FcCharSetWrapper::new()?;
    let mappings = font.charmap().mappings();
    for mapping in mappings {
        if control_char_without_ink(font, mapping.0, mapping.1) {
            continue;
        }

        if charset.add_char(mapping.0).is_err() {
            return None;
        }
    }
    Some(charset)
}
