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

use fc_fontations_bindgen::{
    fcint::{
        FC_INDEX_OBJECT, FC_NAMED_INSTANCE_OBJECT, FC_SIZE_OBJECT, FC_SLANT_OBJECT,
        FC_VARIABLE_OBJECT, FC_WEIGHT_OBJECT, FC_WIDTH_OBJECT,
    },
    FcWeightFromOpenTypeDouble, FC_SLANT_ITALIC, FC_SLANT_OBLIQUE, FC_SLANT_ROMAN, FC_WEIGHT_BLACK,
    FC_WEIGHT_BOLD, FC_WEIGHT_EXTRABOLD, FC_WEIGHT_EXTRALIGHT, FC_WEIGHT_LIGHT, FC_WEIGHT_MEDIUM,
    FC_WEIGHT_NORMAL, FC_WEIGHT_SEMIBOLD, FC_WEIGHT_THIN, FC_WIDTH_CONDENSED, FC_WIDTH_EXPANDED,
    FC_WIDTH_EXTRACONDENSED, FC_WIDTH_EXTRAEXPANDED, FC_WIDTH_NORMAL, FC_WIDTH_SEMICONDENSED,
    FC_WIDTH_SEMIEXPANDED, FC_WIDTH_ULTRACONDENSED, FC_WIDTH_ULTRAEXPANDED,
};

use crate::{
    pattern_bindings::{fc_wrapper::FcRangeWrapper, FcPatternBuilder, PatternElement},
    InstanceMode,
};
use read_fonts::TableProvider;
use skrifa::{
    attribute::{Attributes, Stretch, Style, Weight},
    AxisCollection, FontRef, MetadataProvider, NamedInstance, Tag,
};

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

struct AttributesToPattern<'a> {
    weight_from_os2: Option<f64>,
    width_from_os2: Option<f64>,
    attributes: Attributes,
    axes: AxisCollection<'a>,
    named_instance: Option<NamedInstance<'a>>,
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

    fn static_weight(&self) -> PatternElement {
        self.weight_from_os2.map_or_else(
            || {
                PatternElement::new(
                    FC_WEIGHT_OBJECT as i32,
                    fc_weight(self.attributes.weight).into(),
                )
            },
            |os2_weight| PatternElement::new(FC_WEIGHT_OBJECT as i32, os2_weight.into()),
        )
    }

    fn static_width(&self) -> PatternElement {
        self.width_from_os2.map_or_else(
            || {
                PatternElement::new(
                    FC_WIDTH_OBJECT as i32,
                    fc_width(self.attributes.stretch).into(),
                )
            },
            |os2_width| PatternElement::new(FC_WIDTH_OBJECT as i32, os2_width.into()),
        )
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

    fn default_size(&self) -> Option<PatternElement> {
        self.axes.get_by_tag(Tag::new(b"opsz")).map(|opsz_axis| {
            PatternElement::new(
                FC_SIZE_OBJECT as i32,
                (opsz_axis.default_value() as f64).into(),
            )
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
}

pub fn append_style_elements(
    font: &FontRef,
    instance_mode: InstanceMode,
    ttc_index: Option<i32>,
    pattern: &mut FcPatternBuilder,
) {
    // TODO: fcfreetype.c seems to prefer parsing information from the WWS name table entry,
    // but falls back to flags if those are not found. So far, I haven't identified test fonts
    // for which the WWS code path would trigger.

    let attributes_converter = AttributesToPattern::new(font, &instance_mode);

    match instance_mode {
        InstanceMode::Default => {
            pattern.append_element(attributes_converter.static_weight());
            pattern.append_element(attributes_converter.static_width());
            pattern.append_element(attributes_converter.static_slant());

            pattern.append_element(PatternElement::new(FC_VARIABLE_OBJECT as i32, false.into()));
            pattern.append_element(PatternElement::new(
                FC_INDEX_OBJECT as i32,
                ttc_index.unwrap_or_default().into(),
            ));

            pattern.append_element(PatternElement::new(
                FC_NAMED_INSTANCE_OBJECT as i32,
                false.into(),
            ));
        }
        InstanceMode::Variable => {
            if let Some(weight_to_add) = attributes_converter
                .variable_weight()
                .or(Some(attributes_converter.static_weight()))
            {
                pattern.append_element(weight_to_add);
            }
            if let Some(width_to_add) = attributes_converter
                .variable_width()
                .or(Some(attributes_converter.static_width()))
            {
                pattern.append_element(width_to_add);
            }
            if let Some(size) = attributes_converter.variable_opsz() {
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
            pattern.append_element(attributes_converter.static_slant());
        }
        InstanceMode::Named(index) => {
            if let Some(weight) = attributes_converter.instance_weight() {
                pattern.append_element(weight);
            }
            if let Some(width) = attributes_converter.instance_width() {
                pattern.append_element(width);
            }
            pattern.append_element(PatternElement::new(FC_VARIABLE_OBJECT as i32, false.into()));
            pattern.append_element(PatternElement::new(
                FC_INDEX_OBJECT as i32,
                (ttc_index.unwrap_or_default() + ((index + 1) << 16)).into(),
            ));
            if let Some(size_element) = attributes_converter
                .instance_size()
                .or(attributes_converter.default_size())
            {
                pattern.append_element(size_element);
            };
            if let Some(slant_element) = attributes_converter.instance_slant() {
                pattern.append_element(slant_element);
            }
            pattern.append_element(PatternElement::new(
                FC_NAMED_INSTANCE_OBJECT as i32,
                true.into(),
            ));
        }
    }
}
