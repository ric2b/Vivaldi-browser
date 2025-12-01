/*
 * fontconfig/fc-fontations/name_records.rs
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

use font_types::NameId;
use read_fonts::tables::cmap::PlatformId;
use read_fonts::tables::name::{Name, NameRecord};

// Compare platform_order[] in fcfreetype.c.
// https://learn.microsoft.com/en-us/typography/opentype/spec/name#platform-encoding-and-language-ids
const PLATFORM_ORDER: [PlatformId; 4] = [
    PlatformId::Windows,
    PlatformId::Unicode,
    PlatformId::Macintosh,
    PlatformId::ISO, // deprecated.
];

// Compare name_order[] in fcfreetype.c.
const NAME_ID_ORDER: [NameId; 10] = [
    NameId::WWS_FAMILY_NAME,
    NameId::TYPOGRAPHIC_FAMILY_NAME,
    NameId::FAMILY_NAME,
    NameId::COMPATIBLE_FULL_NAME,
    NameId::FULL_NAME,
    NameId::WWS_SUBFAMILY_NAME,
    NameId::TYPOGRAPHIC_SUBFAMILY_NAME,
    NameId::SUBFAMILY_NAME,
    NameId::TRADEMARK,
    NameId::MANUFACTURER,
];

pub struct FcSortedNameRecords<'a> {
    name: &'a Name<'a>,
    sorted_record_indices: std::vec::IntoIter<u16>,
}

impl<'a> FcSortedNameRecords<'a> {
    pub fn new(name: &'a Name) -> Self {
        let mut sorted_name_id_indices: Vec<u16> = (0..name.name_record().len() as u16).collect();
        Self::sort_name_records(name, &mut sorted_name_id_indices);
        Self {
            name,
            sorted_record_indices: sorted_name_id_indices.into_iter(),
        }
    }

    // In instances of this comparison function, return Greater for an id a that
    // is higher priorty than id b, Less for one that is lower priority.
    fn cmp_id_ordered<T>(priorities: &[T], a: &T, b: &T) -> std::cmp::Ordering
    where
        T: std::cmp::Ord,
    {
        match (
            priorities.iter().position(|x| *x == *a),
            priorities.iter().position(|x| *x == *b),
        ) {
            (Some(pos_a), Some(pos_b)) => pos_a.cmp(&pos_b).reverse(),
            (Some(_), None) => std::cmp::Ordering::Greater,
            (None, Some(_)) => std::cmp::Ordering::Less,
            (None, None) => a.cmp(b).reverse(),
        }
    }

    fn language_cmp_english_first(a: (u16, u16), b: (u16, u16)) -> std::cmp::Ordering {
        const MS_ENGLISH_US: (u16, u16) = (0x409, 3);
        const MAC_ENGLISH: (u16, u16) = (0, 1);

        match (a, b) {
            (MS_ENGLISH_US, MS_ENGLISH_US) | (MAC_ENGLISH, MAC_ENGLISH) => {
                std::cmp::Ordering::Equal
            }
            (MS_ENGLISH_US, _) | (MAC_ENGLISH, _) => std::cmp::Ordering::Greater,
            (_, MS_ENGLISH_US) | (_, MAC_ENGLISH) => std::cmp::Ordering::Less,
            _ => a.cmp(&b).reverse(),
        }
    }

    fn sort_name_records(name: &'a Name, indices: &mut [u16]) {
        indices.sort_by(|a, b| {
            let name_record_a = name.name_record()[*a as usize];
            let name_record_b = name.name_record()[*b as usize];

            Self::cmp_id_ordered(
                &PLATFORM_ORDER,
                &PlatformId::new(name_record_a.platform_id()),
                &PlatformId::new(name_record_b.platform_id()),
            )
            .then(Self::cmp_id_ordered(
                &NAME_ID_ORDER,
                &name_record_a.name_id(),
                &name_record_b.name_id(),
            ))
            .then(
                name_record_a
                    .encoding_id()
                    .cmp(&name_record_b.encoding_id()),
            )
            .then(Self::language_cmp_english_first(
                (name_record_a.language_id(), name_record_a.platform_id()),
                (name_record_b.language_id(), name_record_b.platform_id()),
            ))
        });
        // Return in descending order, from most important to least important.
        indices.reverse();
    }
}

impl Iterator for FcSortedNameRecords<'_> {
    type Item = NameRecord;

    fn next(&mut self) -> Option<Self::Item> {
        let record_index = self.sorted_record_indices.next()?;
        Some(self.name.name_record()[record_index as usize])
    }
}

#[cfg(test)]
mod test {
    use crate::name_records::FcSortedNameRecords;
    use read_fonts::TableProvider;
    use std::{env, fs, path::PathBuf};

    fn get_test_font_bytes() -> Option<Vec<u8>> {
        let base_path_str = env::var("builddir").unwrap_or(String::from("build/"));
        let file_path = PathBuf::from(base_path_str)
            .join("testfonts/roboto-flex-fonts/fonts/variable/RobotoFlex[GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght].ttf");

        let file_contents: Vec<u8> = fs::read(&file_path).ok()?;
        Some(file_contents)
    }

    #[test]
    fn test_sort_name_records() {
        let font_bytes = get_test_font_bytes().expect("Could not read test font file.");
        let font = read_fonts::FontRef::new(font_bytes.as_slice()).unwrap();
        let name_table = font.name().unwrap();
        let sorted_records = FcSortedNameRecords::new(&name_table);
        let sorted_platform_ids = sorted_records
            .map(|entry| {
                (
                    entry.platform_id(),
                    entry.name_id().to_u16(),
                    entry.encoding_id(),
                    entry.language_id(),
                )
            })
            .collect::<Vec<_>>();
        assert_eq!(
            sorted_platform_ids,
            [
                (3, 1, 1, 1033),
                (3, 4, 1, 1033),
                (3, 2, 1, 1033),
                (3, 7, 1, 1033),
                (3, 8, 1, 1033),
                (3, 0, 1, 1033),
                (3, 3, 1, 1033),
                (3, 5, 1, 1033),
                (3, 6, 1, 1033),
                (3, 9, 1, 1033),
                (3, 11, 1, 1033),
                (3, 12, 1, 1033),
                (3, 13, 1, 1033),
                (3, 14, 1, 1033),
                (3, 25, 1, 1033),
                (3, 256, 1, 1033),
                (3, 257, 1, 1033),
                (3, 258, 1, 1033),
                (3, 259, 1, 1033),
                (3, 260, 1, 1033),
                (3, 261, 1, 1033),
                (3, 262, 1, 1033),
                (3, 263, 1, 1033),
                (3, 264, 1, 1033),
                (3, 265, 1, 1033),
                (3, 266, 1, 1033),
                (3, 267, 1, 1033),
                (3, 268, 1, 1033),
                (3, 269, 1, 1033),
                (3, 270, 1, 1033),
                (3, 271, 1, 1033),
                (3, 272, 1, 1033),
                (3, 273, 1, 1033),
                (3, 274, 1, 1033),
                (3, 275, 1, 1033),
                (3, 276, 1, 1033),
                (3, 277, 1, 1033),
                (3, 278, 1, 1033),
                (3, 279, 1, 1033),
                (3, 280, 1, 1033),
                (3, 281, 1, 1033),
                (3, 282, 1, 1033),
                (3, 283, 1, 1033),
                (3, 284, 1, 1033),
                (3, 285, 1, 1033),
                (3, 286, 1, 1033),
                (3, 287, 1, 1033),
                (3, 288, 1, 1033),
                (3, 289, 1, 1033),
                (3, 290, 1, 1033),
                (3, 291, 1, 1033),
                (3, 292, 1, 1033),
                (3, 293, 1, 1033),
                (3, 294, 1, 1033),
                (3, 295, 1, 1033),
                (3, 296, 1, 1033),
                (3, 297, 1, 1033),
                (3, 298, 1, 1033),
                (3, 299, 1, 1033),
                (3, 300, 1, 1033),
                (3, 301, 1, 1033),
                (3, 302, 1, 1033),
                (3, 303, 1, 1033),
                (3, 304, 1, 1033),
                (3, 305, 1, 1033),
                (3, 306, 1, 1033),
                (3, 307, 1, 1033),
                (3, 308, 1, 1033),
                (3, 309, 1, 1033),
                (3, 310, 1, 1033),
                (3, 311, 1, 1033),
                (3, 312, 1, 1033),
                (3, 313, 1, 1033),
                (3, 314, 1, 1033),
                (3, 315, 1, 1033),
                (3, 316, 1, 1033),
                (3, 317, 1, 1033),
                (3, 318, 1, 1033),
                (3, 319, 1, 1033),
                (3, 320, 1, 1033),
                (3, 321, 1, 1033),
                (3, 322, 1, 1033),
                (3, 323, 1, 1033),
                (3, 324, 1, 1033),
                (3, 325, 1, 1033),
                (3, 326, 1, 1033),
                (3, 327, 1, 1033),
                (3, 328, 1, 1033),
                (3, 329, 1, 1033),
                (3, 330, 1, 1033),
                (3, 331, 1, 1033),
                (3, 332, 1, 1033),
                (3, 333, 1, 1033),
                (3, 334, 1, 1033),
                (3, 335, 1, 1033),
                (3, 336, 1, 1033),
                (3, 337, 1, 1033),
                (3, 338, 1, 1033),
                (3, 339, 1, 1033),
                (3, 340, 1, 1033),
                (3, 341, 1, 1033),
                (3, 342, 1, 1033),
                (3, 343, 1, 1033),
                (3, 344, 1, 1033),
                (3, 345, 1, 1033),
                (3, 346, 1, 1033),
                (3, 347, 1, 1033),
                (3, 348, 1, 1033),
                (3, 349, 1, 1033),
                (1, 25, 0, 0),
                (1, 256, 0, 0),
                (1, 257, 0, 0),
                (1, 258, 0, 0),
                (1, 259, 0, 0),
                (1, 260, 0, 0),
                (1, 261, 0, 0),
                (1, 262, 0, 0),
                (1, 263, 0, 0),
                (1, 264, 0, 0),
                (1, 265, 0, 0),
                (1, 266, 0, 0),
                (1, 267, 0, 0),
                (1, 268, 0, 0),
                (1, 269, 0, 0),
                (1, 270, 0, 0),
                (1, 271, 0, 0),
                (1, 272, 0, 0),
                (1, 273, 0, 0),
                (1, 274, 0, 0),
                (1, 275, 0, 0),
                (1, 276, 0, 0),
                (1, 277, 0, 0),
                (1, 278, 0, 0),
                (1, 279, 0, 0),
                (1, 280, 0, 0),
                (1, 281, 0, 0),
                (1, 282, 0, 0),
                (1, 283, 0, 0),
                (1, 284, 0, 0),
                (1, 285, 0, 0),
                (1, 286, 0, 0),
                (1, 287, 0, 0),
                (1, 288, 0, 0),
                (1, 289, 0, 0),
                (1, 290, 0, 0),
                (1, 291, 0, 0),
                (1, 292, 0, 0),
                (1, 293, 0, 0),
                (1, 294, 0, 0),
                (1, 295, 0, 0),
                (1, 296, 0, 0),
                (1, 297, 0, 0),
                (1, 298, 0, 0),
                (1, 299, 0, 0),
                (1, 300, 0, 0),
                (1, 301, 0, 0),
                (1, 302, 0, 0),
                (1, 303, 0, 0),
                (1, 304, 0, 0),
                (1, 305, 0, 0),
                (1, 306, 0, 0),
                (1, 307, 0, 0),
                (1, 308, 0, 0),
                (1, 309, 0, 0),
                (1, 310, 0, 0),
                (1, 311, 0, 0),
                (1, 312, 0, 0),
                (1, 313, 0, 0),
                (1, 314, 0, 0),
                (1, 315, 0, 0),
                (1, 316, 0, 0),
                (1, 317, 0, 0),
                (1, 318, 0, 0),
                (1, 319, 0, 0),
                (1, 320, 0, 0),
                (1, 321, 0, 0),
                (1, 322, 0, 0),
                (1, 323, 0, 0),
                (1, 324, 0, 0),
                (1, 325, 0, 0),
                (1, 326, 0, 0),
                (1, 327, 0, 0),
                (1, 328, 0, 0),
                (1, 329, 0, 0),
                (1, 330, 0, 0),
                (1, 331, 0, 0),
                (1, 332, 0, 0),
                (1, 333, 0, 0),
                (1, 334, 0, 0),
                (1, 335, 0, 0),
                (1, 336, 0, 0),
                (1, 337, 0, 0),
                (1, 338, 0, 0),
                (1, 339, 0, 0),
                (1, 340, 0, 0),
                (1, 341, 0, 0),
                (1, 342, 0, 0),
                (1, 343, 0, 0),
                (1, 344, 0, 0),
                (1, 345, 0, 0),
                (1, 346, 0, 0),
                (1, 347, 0, 0),
                (1, 348, 0, 0),
                (1, 349, 0, 0)
            ]
        );
    }
}
