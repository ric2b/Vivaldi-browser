/*
 * fontconfig/fc-fontations/attributes.rs
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

use fontconfig_bindings::{
    FcWeightFromOpenTypeDouble, FC_DUAL, FC_MONO, FC_SLANT_ITALIC, FC_SLANT_OBLIQUE,
    FC_SLANT_ROMAN, FC_WEIGHT_BLACK, FC_WEIGHT_BOLD, FC_WEIGHT_BOOK, FC_WEIGHT_DEMIBOLD,
    FC_WEIGHT_DEMILIGHT, FC_WEIGHT_EXTRABLACK, FC_WEIGHT_EXTRABOLD, FC_WEIGHT_EXTRALIGHT,
    FC_WEIGHT_HEAVY, FC_WEIGHT_LIGHT, FC_WEIGHT_MEDIUM, FC_WEIGHT_NORMAL, FC_WEIGHT_REGULAR,
    FC_WEIGHT_SEMIBOLD, FC_WEIGHT_SEMILIGHT, FC_WEIGHT_THIN, FC_WEIGHT_ULTRABLACK,
    FC_WEIGHT_ULTRABOLD, FC_WEIGHT_ULTRALIGHT, FC_WIDTH_CONDENSED, FC_WIDTH_EXPANDED,
    FC_WIDTH_EXTRACONDENSED, FC_WIDTH_EXTRAEXPANDED, FC_WIDTH_NORMAL, FC_WIDTH_SEMICONDENSED,
    FC_WIDTH_SEMIEXPANDED, FC_WIDTH_ULTRACONDENSED, FC_WIDTH_ULTRAEXPANDED,
};

use fcint_bindings::{
    FC_DECORATIVE_OBJECT, FC_INDEX_OBJECT, FC_NAMED_INSTANCE_OBJECT, FC_SIZE_OBJECT,
    FC_SLANT_OBJECT, FC_SPACING_OBJECT, FC_STYLE_OBJECT, FC_VARIABLE_OBJECT, FC_WEIGHT_OBJECT,
    FC_WIDTH_OBJECT,
};

use crate::{
    pattern_bindings::{
        fc_wrapper::FcRangeWrapper, FcPatternBuilder, PatternElement, PatternValue,
    },
    InstanceMode,
};
use read_fonts::TableProvider;
use skrifa::{
    attribute::{Attributes, Stretch, Style, Weight},
    instance::Location,
    metrics::GlyphMetrics,
    prelude::{LocationRef, Size},
    AxisCollection, FontRef, MetadataProvider, NamedInstance, Tag,
};
use std::ffi::CString;

fn fc_weight(skrifa_weight: Weight) -> f64 {
    (match skrifa_weight {
        Weight::THIN => FC_WEIGHT_THIN,
        Weight::EXTRA_LIGHT => FC_WEIGHT_EXTRALIGHT,
        Weight::LIGHT => FC_WEIGHT_LIGHT,
        Weight::NORMAL => FC_WEIGHT_NORMAL,
        Weight::MEDIUM => FC_WEIGHT_MEDIUM,
        Weight::SEMI_BOLD => FC_WEIGHT_SEMIBOLD,
        Weight::BOLD => FC_WEIGHT_BOLD,
        Weight::EXTRA_BOLD => FC_WEIGHT_EXTRABOLD,
        Weight::BLACK => FC_WEIGHT_BLACK,
        // See fcfreetype.c: When weight is not available, set to medium.
        // This would mean a font did not have a parseable OS/2 table or
        // a weight value could not be retrieved from it.
        _ => FC_WEIGHT_MEDIUM,
    }) as f64
}

fn fc_slant(skrifa_style: Style) -> u32 {
    match skrifa_style {
        Style::Italic => FC_SLANT_ITALIC,
        Style::Oblique(_) => FC_SLANT_OBLIQUE,
        _ => FC_SLANT_ROMAN,
    }
}

fn fc_width(skrifa_stretch: Stretch) -> f64 {
    (match skrifa_stretch {
        Stretch::ULTRA_CONDENSED => FC_WIDTH_ULTRACONDENSED,
        Stretch::EXTRA_CONDENSED => FC_WIDTH_EXTRACONDENSED,
        Stretch::CONDENSED => FC_WIDTH_CONDENSED,
        Stretch::SEMI_CONDENSED => FC_WIDTH_SEMICONDENSED,
        Stretch::NORMAL => FC_WIDTH_NORMAL,
        Stretch::SEMI_EXPANDED => FC_WIDTH_SEMIEXPANDED,
        Stretch::EXPANDED => FC_WIDTH_EXPANDED,
        Stretch::EXTRA_EXPANDED => FC_WIDTH_EXTRAEXPANDED,
        Stretch::ULTRA_EXPANDED => FC_WIDTH_ULTRAEXPANDED,
        _ => FC_WIDTH_NORMAL,
    } as f64)
}

fn fc_weight_from_os2(font_ref: &FontRef) -> Option<f64> {
    let us_weight = font_ref.os2().ok()?.us_weight_class() as f64;
    unsafe {
        let result = FcWeightFromOpenTypeDouble(us_weight);
        if result == -1.0 {
            None
        } else {
            Some(result)
        }
    }
}

fn fc_width_from_os2(font_ref: &FontRef) -> Option<f64> {
    let us_width = font_ref.os2().ok()?.us_width_class();
    let converted = match us_width {
        1 => FC_WIDTH_ULTRACONDENSED,
        2 => FC_WIDTH_EXTRACONDENSED,
        3 => FC_WIDTH_CONDENSED,
        4 => FC_WIDTH_SEMICONDENSED,
        5 => FC_WIDTH_NORMAL,
        6 => FC_WIDTH_SEMIEXPANDED,
        7 => FC_WIDTH_EXPANDED,
        8 => FC_WIDTH_EXTRAEXPANDED,
        9 => FC_WIDTH_ULTRAEXPANDED,
        _ => FC_WIDTH_NORMAL,
    };
    Some(converted as f64)
}

fn fc_size_from_os2(font_ref: &FontRef) -> Option<(f64, f64)> {
    font_ref.os2().ok().and_then(|os2| {
        Some((
            os2.us_lower_optical_point_size()? as f64 / 20.0,
            os2.us_upper_optical_point_size()? as f64 / 20.0,
        ))
    })
}

struct AttributesToPattern<'a> {
    weight_from_os2: Option<f64>,
    width_from_os2: Option<f64>,
    attributes: Attributes,
    axes: AxisCollection<'a>,
    named_instance: Option<NamedInstance<'a>>,
    font_ref: FontRef<'a>,
}

impl<'a> AttributesToPattern<'a> {
    fn new(font: &'a FontRef, instance_mode: &InstanceMode) -> Self {
        let named_instance = match instance_mode {
            InstanceMode::Named(index) => font.named_instances().get(*index as usize),
            _ => None,
        };

        Self {
            weight_from_os2: fc_weight_from_os2(font),
            width_from_os2: fc_width_from_os2(font),
            attributes: Attributes::new(font),
            axes: font.axes(),
            named_instance,
            font_ref: font.clone(),
        }
    }

    fn user_coord_for_tag(&self, tag: Tag) -> Option<f64> {
        let mut axis_coords = self
            .axes
            .iter()
            .map(|axis| axis.tag())
            .zip(self.named_instance.clone()?.user_coords());
        Some(axis_coords.find(|item| item.0 == tag)?.1 as f64)
    }

    fn flags_weight(&self) -> PatternElement {
        PatternElement::new(
            FC_WEIGHT_OBJECT as i32,
            fc_weight(self.attributes.weight).into(),
        )
    }

    fn os2_weight(&self) -> Option<PatternElement> {
        self.weight_from_os2
            .map(|weight| PatternElement::new(FC_WEIGHT_OBJECT as i32, weight.into()))
    }

    fn flags_width(&self) -> PatternElement {
        PatternElement::new(
            FC_WIDTH_OBJECT as i32,
            fc_width(self.attributes.stretch).into(),
        )
    }

    fn os2_width(&self) -> Option<PatternElement> {
        self.width_from_os2
            .map(|width| PatternElement::new(FC_WIDTH_OBJECT as i32, width.into()))
    }

    fn static_slant(&self) -> PatternElement {
        PatternElement::new(
            FC_SLANT_OBJECT as i32,
            (fc_slant(self.attributes.style) as i32).into(),
        )
    }

    fn instance_weight(&self) -> Option<PatternElement> {
        let named_instance_weight = self.user_coord_for_tag(Tag::new(b"wght"))?;
        unsafe {
            Some(PatternElement::new(
                FC_WEIGHT_OBJECT as i32,
                FcWeightFromOpenTypeDouble(named_instance_weight).into(),
            ))
        }
    }

    fn instance_width(&self) -> Option<PatternElement> {
        let named_instance_weight = self.user_coord_for_tag(Tag::new(b"wdth"))?;

        Some(PatternElement::new(
            FC_WIDTH_OBJECT as i32,
            named_instance_weight.into(),
        ))
    }

    fn instance_slant(&self) -> Option<PatternElement> {
        let named_instance_slant = self.user_coord_for_tag(Tag::new(b"slnt"))?;
        if named_instance_slant < 0.0 {
            Some(PatternElement::new(
                FC_SLANT_OBJECT as i32,
                (FC_SLANT_ITALIC as i32).into(),
            ))
        } else {
            Some(PatternElement::new(
                FC_SLANT_OBJECT as i32,
                (FC_SLANT_ROMAN as i32).into(),
            ))
        }
    }

    fn instance_size(&self) -> Option<PatternElement> {
        let named_instance_size = self.user_coord_for_tag(Tag::new(b"opsz"))?;

        Some(PatternElement::new(
            FC_SIZE_OBJECT as i32,
            named_instance_size.into(),
        ))
    }

    fn default_axis_size(&self) -> Option<PatternElement> {
        self.axes.get_by_tag(Tag::new(b"opsz")).map(|opsz_axis| {
            PatternElement::new(
                FC_SIZE_OBJECT as i32,
                (opsz_axis.default_value() as f64).into(),
            )
        })
    }

    fn os2_size(&self) -> Option<PatternElement> {
        fc_size_from_os2(&self.font_ref).and_then(|(lower, higher)| {
            if lower != higher {
                let range = FcRangeWrapper::new(lower, higher)?;
                Some(PatternElement::new(FC_SIZE_OBJECT as i32, range.into()))
            } else {
                Some(PatternElement::new(FC_SIZE_OBJECT as i32, lower.into()))
            }
        })
    }

    fn variable_weight(&self) -> Option<PatternElement> {
        let weight_axis = self.axes.get_by_tag(Tag::new(b"wght"))?;
        unsafe {
            Some(PatternElement::new(
                FC_WEIGHT_OBJECT as i32,
                FcRangeWrapper::new(
                    FcWeightFromOpenTypeDouble(weight_axis.min_value() as f64),
                    FcWeightFromOpenTypeDouble(weight_axis.max_value() as f64),
                )?
                .into(),
            ))
        }
    }

    fn variable_width(&self) -> Option<PatternElement> {
        let width_axis = self.axes.get_by_tag(Tag::new(b"wdth"))?;
        Some(PatternElement::new(
            FC_WIDTH_OBJECT as i32,
            FcRangeWrapper::new(width_axis.min_value() as f64, width_axis.max_value() as f64)?
                .into(),
        ))
    }

    fn variable_opsz(&self) -> Option<PatternElement> {
        let opsz_axis = self.axes.get_by_tag(Tag::new(b"opsz"))?;
        Some(PatternElement::new(
            FC_SIZE_OBJECT as i32,
            FcRangeWrapper::new(opsz_axis.min_value() as f64, opsz_axis.max_value() as f64)?.into(),
        ))
    }

    // Determine FontConfig spacing property.
    // In fcfreetype.c FcFreeTypeSpacing a heuristic checks whether the glyphs
    //  for a bitmap font are "approximately equal".
    // For this purpose, fcfreetype.c selects the strike size closest to 16 px,
    // then iterates over available glyphs and checks whether the advance width
    // is within a relative tolerance of 1/33.
    // We'll do this based on skrifa advances here - if needed, this can be
    // moved to selecting a particular bitmap strike size and compare pixel
    // widths.
    fn spacing_from_advances(advances: impl Iterator<Item = f32>) -> Option<i32> {
        let mut encountered_advances = Vec::new();

        let approximately_equal =
            |a: f32, b: f32| (a - b).abs() / a.abs().max(b.abs()) <= 1.0 / 33.0;

        'outer: for advance in advances {
            let mut may_push: Option<f32> = None;
            if encountered_advances.is_empty() {
                encountered_advances.push(advance);
                continue;
            }

            for encountered in encountered_advances.iter() {
                if encountered_advances.len() >= 3 {
                    break 'outer;
                }
                if approximately_equal(advance, *encountered) {
                    may_push = None;
                    break;
                }
                may_push = Some(advance);
            }
            if let Some(push) = may_push {
                encountered_advances.push(push);
            }
        }

        match encountered_advances.len() {
            1 => Some(FC_MONO as i32),
            2 if approximately_equal(
                encountered_advances[0].min(encountered_advances[1]) * 2.0,
                encountered_advances[0].max(encountered_advances[1]),
            ) =>
            {
                Some(FC_DUAL as i32)
            }

            _ => None,
        }
    }

    fn spacing(&self) -> Option<PatternElement> {
        let mut location = Location::default();
        if let Some(instance) = &self.named_instance {
            location = instance.location().clone();
        };

        let glyph_metrics = GlyphMetrics::new(
            &self.font_ref,
            Size::new(16.0),
            LocationRef::from(&location),
        );

        let advances = self
            .font_ref
            .charmap()
            .mappings()
            .filter_map(|(_codepoint, gid)| {
                glyph_metrics
                    .advance_width(gid)
                    .and_then(|adv| if adv > 0.0 { Some(adv) } else { None })
            });

        Self::spacing_from_advances(advances)
            .map(|spacing| PatternElement::new(FC_SPACING_OBJECT as i32, spacing.into()))
    }
}

#[derive(Default)]
struct AttributesFromStyleString {
    weight: Option<PatternElement>,
    width: Option<PatternElement>,
    slant: Option<PatternElement>,
    decorative: Option<PatternElement>,
}

fn contains_weight(style_name: &CString) -> Option<PatternElement> {
    const WEIGHT_MAP: [(&str, f64); 23] = [
        ("thin", FC_WEIGHT_THIN as f64),
        ("extralight", FC_WEIGHT_EXTRALIGHT as f64),
        ("ultralight", FC_WEIGHT_ULTRALIGHT as f64),
        ("demilight", FC_WEIGHT_DEMILIGHT as f64),
        ("semilight", FC_WEIGHT_SEMILIGHT as f64),
        ("light", FC_WEIGHT_LIGHT as f64),
        ("book", FC_WEIGHT_BOOK as f64),
        ("regular", FC_WEIGHT_REGULAR as f64),
        ("normal", FC_WEIGHT_NORMAL as f64),
        ("medium", FC_WEIGHT_MEDIUM as f64),
        ("demibold", FC_WEIGHT_DEMIBOLD as f64),
        ("demi", FC_WEIGHT_DEMIBOLD as f64),
        ("semibold", FC_WEIGHT_SEMIBOLD as f64),
        ("extrabold", FC_WEIGHT_EXTRABOLD as f64),
        ("superbold", FC_WEIGHT_EXTRABOLD as f64),
        ("ultrabold", FC_WEIGHT_ULTRABOLD as f64),
        ("bold", FC_WEIGHT_BOLD as f64),
        ("ultrablack", FC_WEIGHT_ULTRABLACK as f64),
        ("superblack", FC_WEIGHT_EXTRABLACK as f64),
        ("extrablack", FC_WEIGHT_EXTRABLACK as f64),
        ("ultra", FC_WEIGHT_ULTRABOLD as f64),
        ("black", FC_WEIGHT_BLACK as f64),
        ("heavy", FC_WEIGHT_HEAVY as f64),
    ];

    for weight_mapping in WEIGHT_MAP {
        if style_name
            .to_string_lossy()
            .to_lowercase()
            .contains(weight_mapping.0)
        {
            return Some(PatternElement::new(
                FC_WEIGHT_OBJECT as i32,
                weight_mapping.1.into(),
            ));
        }
    }
    None
}

fn contains_slant(style_name: &CString) -> Option<PatternElement> {
    const SLANT_MAP: [(&str, i32); 3] = [
        ("italic", FC_SLANT_ITALIC as i32),
        ("kursiv", FC_SLANT_ITALIC as i32),
        ("oblique", FC_SLANT_OBLIQUE as i32),
    ];

    for mapping in SLANT_MAP {
        if style_name
            .to_string_lossy()
            .to_lowercase()
            .contains(mapping.0)
        {
            return Some(PatternElement::new(
                FC_SLANT_OBJECT as i32,
                mapping.1.into(),
            ));
        }
    }
    None
}

fn contains_width(style_name: &CString) -> Option<PatternElement> {
    const WIDTH_MAP: [(&str, f64); 10] = [
        ("ultracondensed", FC_WIDTH_ULTRACONDENSED as f64),
        ("extracondensed", FC_WIDTH_EXTRACONDENSED as f64),
        ("semicondensed", FC_WIDTH_SEMICONDENSED as f64),
        ("condensed", FC_WIDTH_CONDENSED as f64),
        ("normal", FC_WIDTH_NORMAL as f64),
        ("semiexpanded", FC_WIDTH_SEMIEXPANDED as f64),
        ("extraexpanded", FC_WIDTH_EXTRAEXPANDED as f64),
        ("ultraexpanded", FC_WIDTH_ULTRAEXPANDED as f64),
        ("expanded", FC_WIDTH_EXPANDED as f64),
        ("extended", FC_WIDTH_EXPANDED as f64),
    ];
    for mapping in WIDTH_MAP {
        if style_name
            .to_string_lossy()
            .to_lowercase()
            .contains(mapping.0)
        {
            return Some(PatternElement::new(
                FC_WIDTH_OBJECT as i32,
                mapping.1.into(),
            ));
        }
    }
    None
}

fn contains_decorative(style_name: &CString) -> Option<PatternElement> {
    let had_decorative = style_name
        .to_string_lossy()
        .to_lowercase()
        .contains("decorative");

    Some(PatternElement::new(
        FC_DECORATIVE_OBJECT as i32,
        had_decorative.into(),
    ))
}

impl AttributesFromStyleString {
    fn new(pattern: &FcPatternBuilder) -> Self {
        let style_string = pattern
            .into_iter()
            .find(|element| element.object_id == FC_STYLE_OBJECT as i32)
            .and_then(|element| match &element.value {
                PatternValue::String(style) => Some(style),
                _ => None,
            });

        if let Some(style) = style_string {
            Self {
                weight: contains_weight(style),
                width: contains_width(style),
                slant: contains_slant(style),
                decorative: contains_decorative(style),
            }
        } else {
            Self {
                weight: None,
                width: None,
                slant: None,
                decorative: Some(PatternElement::new(
                    FC_DECORATIVE_OBJECT as i32,
                    false.into(),
                )),
            }
        }
    }
}

/// Appends style pattern elements such as weight, width, slant, decorative to the pattern.
/// Requires a textual style element to be already added to the pattern, so it's good
/// to run this after names have been added. This is because this method performs certain
/// string matches on the font name to determine style attributes.
pub fn append_style_elements(
    font: &FontRef,
    instance_mode: InstanceMode,
    ttc_index: Option<i32>,
    pattern: &mut FcPatternBuilder,
) {
    // TODO: fcfreetype.c seems to prefer parsing information from the WWS name table entry,
    // but falls back to flags if those are not found. So far, I haven't identified test fonts
    // for which the WWS code path would trigger.

    let attributes_text = AttributesFromStyleString::new(pattern);

    let skrifa_attributes = AttributesToPattern::new(font, &instance_mode);

    if let Some(spacing) = skrifa_attributes.spacing() {
        pattern.append_element(spacing);
    }

    match instance_mode {
        InstanceMode::Default => {
            let pattern_weight = skrifa_attributes
                .os2_weight()
                .or(attributes_text.weight)
                .unwrap_or(skrifa_attributes.flags_weight());
            pattern.append_element(pattern_weight);

            let width = skrifa_attributes
                .os2_width()
                .or(attributes_text.width)
                .unwrap_or(skrifa_attributes.flags_width());
            pattern.append_element(width);

            pattern.append_element(
                attributes_text
                    .slant
                    .unwrap_or(skrifa_attributes.static_slant()),
            );

            if let Some(element) = attributes_text.decorative {
                pattern.append_element(element)
            }

            pattern.append_element(PatternElement::new(FC_VARIABLE_OBJECT as i32, false.into()));
            pattern.append_element(PatternElement::new(
                FC_INDEX_OBJECT as i32,
                ttc_index.unwrap_or_default().into(),
            ));

            if let Some(size) = skrifa_attributes
                .default_axis_size()
                .or(skrifa_attributes.os2_size())
            {
                pattern.append_element(size);
            }

            pattern.append_element(PatternElement::new(
                FC_NAMED_INSTANCE_OBJECT as i32,
                false.into(),
            ));
        }
        InstanceMode::Variable => {
            let weight = skrifa_attributes
                .variable_weight()
                .or(skrifa_attributes.os2_weight())
                .or(attributes_text.weight)
                .unwrap_or(skrifa_attributes.flags_weight());
            pattern.append_element(weight);

            let width = skrifa_attributes
                .variable_width()
                .or(skrifa_attributes.os2_width())
                .or(attributes_text.width)
                .unwrap_or(skrifa_attributes.flags_width());
            pattern.append_element(width);

            if let Some(element) = attributes_text.decorative {
                pattern.append_element(element)
            }

            if let Some(size) = skrifa_attributes.variable_opsz() {
                pattern.append_element(size);
            }

            pattern.append_element(PatternElement::new(FC_VARIABLE_OBJECT as i32, true.into()));

            // TODO: Check if this should have a zero ttc index if not part of a collection.
            pattern.append_element(PatternElement::new(
                FC_INDEX_OBJECT as i32,
                ttc_index.unwrap_or_default().into(),
            ));
            pattern.append_element(PatternElement::new(
                FC_NAMED_INSTANCE_OBJECT as i32,
                false.into(),
            ));
            pattern.append_element(skrifa_attributes.static_slant());
        }
        InstanceMode::Named(index) => {
            let weight = skrifa_attributes
                .instance_weight()
                .or(attributes_text.weight)
                .unwrap_or(skrifa_attributes.flags_weight());
            pattern.append_element(weight);

            let width = skrifa_attributes
                .instance_width()
                .or(attributes_text.width)
                .unwrap_or(skrifa_attributes.flags_width());
            pattern.append_element(width);

            pattern.append_element(
                skrifa_attributes
                    .instance_slant()
                    .or(attributes_text.slant)
                    .unwrap_or(skrifa_attributes.static_slant()),
            );

            if let Some(element) = attributes_text.decorative {
                pattern.append_element(element)
            }

            pattern.append_element(PatternElement::new(FC_VARIABLE_OBJECT as i32, false.into()));
            pattern.append_element(PatternElement::new(
                FC_INDEX_OBJECT as i32,
                (ttc_index.unwrap_or_default() + ((index + 1) << 16)).into(),
            ));
            if let Some(size_element) = skrifa_attributes
                .instance_size()
                .or(skrifa_attributes.default_axis_size())
            {
                pattern.append_element(size_element);
            };

            pattern.append_element(PatternElement::new(
                FC_NAMED_INSTANCE_OBJECT as i32,
                true.into(),
            ));
        }
    }
}

#[cfg(test)]
mod test {
    use crate::attributes::AttributesToPattern;
    use fontconfig_bindings::{FC_DUAL, FC_MONO};

    const THRESHOLD_FACTOR_DOWN: f32 = 1.0 - 1.0 / 33.0;
    const THRESHOLD_FACTOR_UP: f32 = 1.0 + 1.0 / 33.0;

    fn assert_spacing(advances: impl Iterator<Item = f32>, expectation: Option<i32>) {
        assert_eq!(
            AttributesToPattern::spacing_from_advances(advances),
            expectation
        );
    }

    #[test]
    fn spacing_mono() {
        assert_spacing([10.0].iter().cloned(), Some(FC_MONO as i32));

        assert_spacing(
            [
                10.0,
                10.0,
                10.0,
                10.0 * THRESHOLD_FACTOR_UP,
                10.0 * THRESHOLD_FACTOR_DOWN,
            ]
            .iter()
            .cloned(),
            Some(FC_MONO as i32),
        );
    }

    #[test]
    fn spacing_proportional() {
        assert_spacing(
            [10.0, 10.0 * THRESHOLD_FACTOR_UP + 0.01].iter().cloned(),
            None,
        );

        assert_spacing(
            [10.0, 10.0 * THRESHOLD_FACTOR_DOWN - 0.01].iter().cloned(),
            None,
        );

        assert_spacing([10.0, 15.0].iter().cloned(), None);

        assert_spacing([10.0, 15.0, 20.0].iter().cloned(), None);
    }

    #[test]
    fn advances_dual() {
        assert_spacing([10.0, 20.0].iter().cloned(), Some(FC_DUAL as i32));

        assert_spacing(
            [10.0, 20.0, 10.0, 20.0, 10.0, 20.0].iter().cloned(),
            Some(FC_DUAL as i32),
        );

        assert_spacing(
            [
                10.0,
                20.0 * THRESHOLD_FACTOR_UP,
                10.0,
                20.0,
                10.0 * THRESHOLD_FACTOR_DOWN,
                20.0,
            ]
            .iter()
            .cloned(),
            Some(FC_DUAL as i32),
        );
    }
}
