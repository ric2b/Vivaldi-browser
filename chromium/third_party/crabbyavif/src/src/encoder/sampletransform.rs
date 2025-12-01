// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use crate::encoder::*;
use crate::internal_utils::sampletransform::*;
use crate::*;

// Mapping used in the coding of Sample Transform metadata.
#[derive(Debug, Clone, Copy, PartialEq)]
#[repr(u8)]
enum SampleTransformBitDepth {
    Signed8bits = 0,
    Signed16bits = 1,
    Signed32bits = 2,
    Signed64bits = 3,
}

impl SampleTransformBitDepth {
    fn to_bits(self) -> u8 {
        8 << (self as u8)
    }
    fn from_bits(bits: u8) -> SampleTransformBitDepth {
        match bits {
            8 => SampleTransformBitDepth::Signed8bits,
            16 => SampleTransformBitDepth::Signed16bits,
            32 => SampleTransformBitDepth::Signed32bits,
            64 => SampleTransformBitDepth::Signed64bits,
            _ => unreachable!(),
        }
    }
}

impl SampleTransformToken {
    fn to_type(&self) -> u8 {
        match self {
            SampleTransformToken::Constant(_) => 0,
            SampleTransformToken::ImageItem(index) => {
                // SampleTransformToken::ImageItem is 0-based.
                // Image item indices are 1-based.
                (*index).checked_add(1).unwrap().try_into().unwrap()
            }
            SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Negation) => 64,
            SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Absolute) => 65,
            SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Not) => 66,
            SampleTransformToken::UnaryOp(SampleTransformUnaryOp::Bsr) => 67,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Sum) => 128,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Difference) => 129,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product) => 130,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient) => 131,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::And) => 132,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Or) => 133,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Xor) => 134,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Pow) => 135,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Min) => 136,
            SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Max) => 137,
        }
    }
}

fn recipe_to_expression(recipe: SampleTransformRecipe) -> Result<SampleTransform, AvifError> {
    // Postfix (or Reverse Polish) notation.

    match recipe {
        SampleTransformRecipe::None => unreachable!(),
        SampleTransformRecipe::BitDepthExtension8b8b => {
            // reference_count is two: two 8-bit input images.
            //   (base_sample << 8) | hidden_sample
            // Note: base_sample is encoded losslessly. hidden_sample is encoded lossily or losslessly.
            SampleTransform::create_from(
                // SampleTransformBitDepth::Signed32bits is necessary because the two input images
                // once combined use 16-bit unsigned values, but intermediate results are stored in signed integers.
                SampleTransformBitDepth::Signed32bits.to_bits(),
                2,
                vec![
                    // The base image represents the 8 most significant bits of the reconstructed, bit-depth-extended output image.
                    // Left shift the base image (which is also the primary item, or the auxiliary alpha item of the primary item)
                    // by 8 bits. This is equivalent to multiplying by 2^8.
                    SampleTransformToken::Constant(256),
                    SampleTransformToken::ImageItem(0),
                    SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Product),
                    // The second image represents the 8 least significant bits of the reconstructed, bit-depth-extended output image.
                    SampleTransformToken::ImageItem(1),
                    // Combine the two.
                    SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Or),
                ],
            )
        }
    }
}

fn write_sato(recipe: SampleTransformRecipe) -> AvifResult<Vec<u8>> {
    let expression = recipe_to_expression(recipe)?;
    let bit_depth = SampleTransformBitDepth::from_bits(expression.bit_depth);

    let mut stream = OStream::default();
    stream.write_bits(0, 2)?; // unsigned int(2) version = 0;
    stream.write_bits(0, 4)?; // unsigned int(4) reserved;
    stream.write_bits(bit_depth as u32, 2)?; // unsigned int(2) bit_depth;

    stream.write_u8(expression.tokens.len().try_into().unwrap())?; // unsigned int(8) token_count;
    for token in &expression.tokens {
        stream.write_u8(token.to_type())?; // unsigned int(8) token;
        if let SampleTransformToken::Constant(constant) = token {
            let constant = i32::try_from(*constant).unwrap();
            let bytes = &constant.to_be_bytes();
            assert_eq!(bytes.len() * 8, bit_depth.to_bits().into());
            stream.write_slice(bytes)?; // signed int(1<<(bit_depth+3)) constant;
        }
    }
    Ok(stream.data)
}

impl Encoder {
    pub(crate) fn create_bit_depth_extension_items(&mut self, grid: &Grid) -> AvifResult<()> {
        // There are multiple possible ISOBMFF box hierarchies for translucent images,
        // using 'sato' (Sample Transform) derived image items:
        //  - a primary 'sato' item uses a main color coded item and a hidden color coded item; each color coded
        //    item has an auxiliary alpha coded item; the main color coded item and the 'sato' item are in
        //    an 'altr' group (backward-compatible, implemented)
        //  - a primary 'sato' item uses a main color coded item and a hidden color coded item; the primary
        //    'sato' item has an auxiliary alpha 'sato' item using two alpha coded items (backward-incompatible)
        // Likewise, there are multiple possible ISOBMFF box hierarchies for bit-depth-extended grids,
        // using 'sato' (Sample Transform) derived image items:
        //  - a primary color 'grid', an auxiliary alpha 'grid', a hidden color 'grid', a hidden auxiliary alpha 'grid'
        //    and a 'sato' using the two color 'grid's as input items in this order; the primary color item
        //    and the 'sato' item being in an 'altr' group (backward-compatible, implemented)
        //  - a primary 'grid' of 'sato' cells and an auxiliary alpha 'grid' of 'sato' cells (backward-incompatible)
        let item = Item {
            id: u16_from_usize(self.items.len() + 1)?,
            item_type: "sato".into(),
            category: Category::Color,
            metadata_payload: write_sato(self.settings.sample_transform_recipe)?,
            hidden_image: false,
            ..Default::default()
        };
        let sato_item_id = item.id;
        self.items.push(item);

        // 'altr' group
        if !self.alternative_item_ids.is_empty() {
            return AvifError::not_implemented();
        }
        self.alternative_item_ids.push(sato_item_id);
        self.alternative_item_ids.push(self.primary_item_id);

        let bit_depth_extension_item_id =
            self.add_items(grid, Category::Color, /*hidden=*/ true)?;
        // Set the color and bit depth extension items' dimgFromID value to point to the sample transform item.
        // The color item shall be first, and the bit depth extension item second. avifEncoderFinish() writes the
        // dimg item references in item id order, so as long as colorItemID < bitDepthExtensionColorItemId, the order
        // will be correct.
        assert!(self.primary_item_id < bit_depth_extension_item_id);
        let primary_index = self.primary_item_id as usize - 1;
        if self.items[primary_index].dimg_from_id.is_some() {
            // The internal API only allows one dimg value per item.
            return AvifError::not_implemented();
        }
        self.items[primary_index].dimg_from_id = Some(sato_item_id);
        let bit_depth_extension_item_index = bit_depth_extension_item_id as usize - 1;
        self.items[bit_depth_extension_item_index].dimg_from_id = Some(sato_item_id);
        self.items[bit_depth_extension_item_index].is_sato_least_significant_input = true;

        if self.alpha_present {
            let bit_depth_extension_alpha_item_id =
                self.add_items(grid, Category::Alpha, /*hidden=*/ true)?;
            let bit_depth_extension_alpha_item_index =
                bit_depth_extension_alpha_item_id as usize - 1;
            self.items[bit_depth_extension_alpha_item_index].iref_type = Some("auxl".into());
            self.items[bit_depth_extension_alpha_item_index].iref_to_id =
                Some(bit_depth_extension_item_id);
            self.items[bit_depth_extension_alpha_item_index].is_sato_least_significant_input = true;
            if self.image_metadata.alpha_premultiplied {
                self.items[bit_depth_extension_item_index].iref_type = Some("prem".into());
                self.items[bit_depth_extension_item_index].iref_to_id =
                    Some(bit_depth_extension_alpha_item_id);
            }
        }
        Ok(())
    }

    pub(crate) fn create_bit_depth_extension_image(
        full_depth_image: &Image,
        item: &Item,
    ) -> AvifResult<Image> {
        assert_eq!(full_depth_image.depth, 16);
        if item.is_sato_least_significant_input {
            // 8-bit image containing the least significant bits of the 16-bit image.
            let mut lsb = full_depth_image.shallow_clone();
            lsb.depth = 8;
            lsb.allocate_planes(item.category)?;
            let op = SampleTransform::create_from(
                /*bit_depth=*/ 32, // Unsigned so 16 is not enough.
                /*num_inputs=*/ 1,
                vec![
                    // Postfix notation.
                    SampleTransformToken::ImageItem(0),
                    SampleTransformToken::Constant(255),
                    SampleTransformToken::BinaryOp(SampleTransformBinaryOp::And),
                ],
            )?;
            op.apply_to_planes(
                item.category,
                std::slice::from_ref(full_depth_image),
                &mut lsb,
            )?;
            Ok(lsb)
        } else {
            // 8-bit image containing the most significant bits of the 16-bit image.
            let mut msb = full_depth_image.shallow_clone();
            msb.depth = 8;
            msb.allocate_planes(item.category)?;
            let op = SampleTransform::create_from(
                /*bit_depth=*/ 32, // Unsigned so 16 is not enough.
                /*num_inputs=*/ 1,
                vec![
                    // Postfix notation.
                    SampleTransformToken::ImageItem(0),
                    SampleTransformToken::Constant(256),
                    SampleTransformToken::BinaryOp(SampleTransformBinaryOp::Quotient),
                ],
            )?;
            op.apply_to_planes(
                item.category,
                std::slice::from_ref(full_depth_image),
                &mut msb,
            )?;
            Ok(msb)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_case::test_case;
    #[test_case(SampleTransformBitDepth::Signed8bits)]
    fn test_sample_transform_bit_depth(depth: SampleTransformBitDepth) {
        assert_eq!(depth, SampleTransformBitDepth::from_bits(depth.to_bits()));
    }
}
