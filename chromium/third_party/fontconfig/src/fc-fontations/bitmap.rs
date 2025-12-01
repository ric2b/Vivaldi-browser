/*
 * fontconfig/fc-fontations/bitmap.rs
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

use skrifa::{bitmap::BitmapFormat, FontRef};

use skrifa::bitmap::BitmapStrikes;

use crate::pattern_bindings::{FcPatternBuilder, PatternElement};
use fcint_bindings::{FC_ANTIALIAS_OBJECT, FC_PIXEL_SIZE_OBJECT};

pub fn add_pixel_size(pattern: &mut FcPatternBuilder, font: &FontRef) {
    let strikes = BitmapStrikes::new(font);

    if let Some(BitmapFormat::Ebdt) = strikes.format() {
        return;
    }

    let has_strikes = !strikes.is_empty();

    for strike in strikes.iter() {
        pattern.append_element(PatternElement::new(
            FC_PIXEL_SIZE_OBJECT as i32,
            (strike.ppem() as f64).into(),
        ));
    }

    if has_strikes {
        pattern.append_element(PatternElement::new(
            FC_ANTIALIAS_OBJECT as i32,
            false.into(),
        ));
    }
}
