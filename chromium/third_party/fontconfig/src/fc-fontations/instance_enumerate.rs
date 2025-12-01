/*
 * fontconfig/fc-fontations/instance_enumerate.rs
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

use crate::InstanceMode::{self, Named};
use read_fonts::{
    FileRef::{self, Collection, Font},
    FontRef,
};
use skrifa::MetadataProvider;

pub fn fonts_and_indices(
    file_ref: Option<FileRef>,
) -> impl Iterator<Item = (FontRef, Option<i32>)> {
    let (iter_one, iter_two) = match file_ref {
        Some(Font(font)) => (Some((Ok(font.clone()), None)), None),
        Some(Collection(collection)) => (
            None,
            Some(
                collection
                    .iter()
                    .enumerate()
                    .map(|entry| (entry.1, Some(entry.0 as i32))),
            ),
        ),
        None => (None, None),
    };
    iter_two
        .into_iter()
        .flatten()
        .chain(iter_one)
        .filter_map(|(font_result, index)| {
            if let Ok(font) = font_result {
                return Some((font, index));
            }
            None
        })
}

// Produces an iterator over the named instances of the font.
#[allow(unused)]
pub fn enumerate_named_instances<'a>(f: &'a FontRef) -> impl Iterator<Item = InstanceMode> + 'a {
    let default_coords = f.axes().iter().map(|axis| axis.default_value());
    f.named_instances()
        .iter()
        .enumerate()
        .filter_map(move |(i, instance)| {
            let user_coords = instance.user_coords();
            if user_coords.eq(default_coords.clone()) {
                None
            } else {
                Some(Named(i as i32))
            }
        })
}

/// Used for producing the expected set of patterns out of variable fonts.
/// Produces an iterator over
/// * the default instance,
/// * the named instances, and the
/// * variable instance.
#[allow(unused)]
pub fn all_instances<'a>(f: &'a FontRef) -> Box<dyn Iterator<Item = InstanceMode> + 'a> {
    if f.axes().is_empty() {
        Box::new(std::iter::once(InstanceMode::Default))
    } else {
        Box::new(
            std::iter::once(InstanceMode::Default)
                .chain(enumerate_named_instances(f))
                .chain(std::iter::once(InstanceMode::Variable)),
        )
    }
}

#[cfg(test)]
mod test {
    use crate::{
        instance_enumerate::{all_instances, fonts_and_indices},
        InstanceMode,
    };

    use read_fonts::FileRef;
    use std::path::PathBuf;

    fn testfontdir() -> PathBuf {
        let default_build_dir = std::env::current_dir().unwrap().join("build");
        let build_dir = std::env::var("builddir").unwrap_or_else(|_| {
            println!("builddir env var not set, using current directory + \"build/\"");
            default_build_dir.to_string_lossy().to_string()
        });
        PathBuf::from(build_dir)
            .join("testfonts")
            .join("roboto-flex-fonts")
            .join("fonts")
            .join("variable")
    }

    fn variable_collection_testfont() -> PathBuf {
        testfontdir().join(
            "RobotoFlex[GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght].ttf",
        )
    }

    const EXPECTED_INSTANCES: [InstanceMode; 21] = [
        InstanceMode::Default,
        InstanceMode::Named(0),
        InstanceMode::Named(1),
        InstanceMode::Named(2),
        InstanceMode::Named(4),
        InstanceMode::Named(5),
        InstanceMode::Named(6),
        InstanceMode::Named(7),
        InstanceMode::Named(8),
        InstanceMode::Named(9),
        InstanceMode::Named(10),
        InstanceMode::Named(11),
        InstanceMode::Named(12),
        InstanceMode::Named(13),
        InstanceMode::Named(14),
        InstanceMode::Named(15),
        InstanceMode::Named(16),
        InstanceMode::Named(17),
        InstanceMode::Named(18),
        InstanceMode::Named(19),
        InstanceMode::Variable,
    ];

    #[test]
    fn test_instance_iterator() {
        let bytes = std::fs::read(variable_collection_testfont())
            .ok()
            .unwrap_or_default();
        let fileref = FileRef::new(&bytes).ok();
        let fonts = fonts_and_indices(fileref);
        for (font, _ttc_index) in fonts {
            all_instances(&font)
                .zip(EXPECTED_INSTANCES.iter())
                .for_each(|(instance, expected_instance)| {
                    assert_eq!(instance, *expected_instance);
                })
        }
    }
}
