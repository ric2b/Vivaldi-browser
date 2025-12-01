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

// Implementation for ISO/IEC 23008-12 3rd edition AMD 2 Low-overhead image file format.
// See drafts at https://www.mpeg.org/standards/MPEG-H/12/.

// Returns true if the image can be encoded with a MinimizedImageBox instead of a full regular MetaBox.
pub fn is_mini_compatible(enc: &Encoder) -> bool {
    // The MinimizedImageBox ("mif3" brand) only supports non-layered, still images.
    if enc.settings.extra_layer_count != 0 || enc.is_sequence() {
        return false;
    }

    // TODO: b/434944440 - Return false if there is any sample transform recipe.

    // Check for maximum field values and maximum chunk sizes.

    // width_minus1 and height_minus1
    if enc.image_metadata.width > (1 << 15) || enc.image_metadata.height > (1 << 15) {
        return false;
    }
    // icc_data_size_minus1, exif_data_size_minus1 and xmp_data_size_minus1
    if enc.image_metadata.icc.len() > (1 << 20)
        || enc.image_metadata.exif.len() > (1 << 20)
        || enc.image_metadata.xmp.len() > (1 << 20)
    {
        return false;
    }
    // gainmap_width_minus1 and gainmap_height_minus1
    if enc.gainmap_image_metadata.width > (1 << 15) || enc.gainmap_image_metadata.height > (1 << 15)
    {
        return false;
    }
    // tmap_icc_data_size_minus1
    if enc.alt_image_metadata.icc.len() > (1 << 20) {
        return false;
    }
    // gainmap_metadata_size
    let tmap = &enc.items.iter().find(|item| item.item_type == "tmap");
    if let Some(tmap) = tmap {
        // Minus one because of the prepended version field which is not part of the MinimizedImageBox syntax.
        let metadata_payload_size = tmap.metadata_payload.len().checked_sub(1).unwrap();
        if metadata_payload_size >= (1 << 20) {
            return false;
        }
    }

    // 4:4:4, 4:2:2, 4:2:0 and 4:0:0 are supported by a MinimizedImageBox.
    // chroma_subsampling
    if enc.image_metadata.yuv_format != PixelFormat::Yuv444
        && enc.image_metadata.yuv_format != PixelFormat::Yuv422
        && enc.image_metadata.yuv_format != PixelFormat::Yuv420
        && enc.image_metadata.yuv_format != PixelFormat::Yuv400
    {
        return false;
    }
    // gainmap_chroma_subsampling
    if tmap.is_some()
        && !matches!(
            enc.gainmap_image_metadata.yuv_format,
            PixelFormat::Yuv444 | PixelFormat::Yuv422 | PixelFormat::Yuv420 | PixelFormat::Yuv400
        )
    {
        return false;
    }

    // colour_primaries, transfer_characteristics and matrix_coefficients
    if enc.image_metadata.color_primaries as u16 > 255
        || enc.image_metadata.transfer_characteristics as u16 > 255
        || enc.image_metadata.matrix_coefficients as u16 > 255
    {
        return false;
    }
    // gainmap_colour_primaries, gainmap_transfer_characteristics and gainmap_matrix_coefficients
    if tmap.is_some()
        && (enc.gainmap_image_metadata.color_primaries as u16 > 255
            || enc.gainmap_image_metadata.transfer_characteristics as u16 > 255
            || enc.gainmap_image_metadata.matrix_coefficients as u16 > 255)
    {
        return false;
    }
    // tmap_colour_primaries, tmap_transfer_characteristics and tmap_matrix_coefficients
    if tmap.is_some()
        && (enc.alt_image_metadata.color_primaries as u16 > 255
            || enc.alt_image_metadata.transfer_characteristics as u16 > 255
            || enc.alt_image_metadata.matrix_coefficients as u16 > 255)
    {
        return false;
    }

    let mut color_item = None;
    for item in &enc.items {
        // Grids are not supported by a MinimizedImageBox.
        if item.grid.is_some() {
            return false;
        }

        if item.id == enc.primary_item_id {
            assert!(color_item.is_none());
            color_item = Some(item);
            // main_item_data_size_minus1
            if item.samples.len() != 1 || item.samples[0].data.len() > (1 << 28) {
                return false;
            }
            if !matches!(item.codec_configuration, CodecConfiguration::Av1(_)) {
                return false;
            }
            continue; // The primary item can be stored in the MinimizedImageBox.
        }
        if item.category == Category::Alpha && item.iref_to_id == Some(enc.primary_item_id) {
            // alpha_item_data_size
            if item.samples.len() != 1 || item.samples[0].data.len() >= (1 << 28) {
                return false;
            }
            if !matches!(item.codec_configuration, CodecConfiguration::Av1(_)) {
                return false;
            }
            continue; // The alpha auxiliary item can be stored in the MinimizedImageBox.
        }
        if item.category == Category::Gainmap {
            // gainmap_item_data_size
            if item.samples.len() != 1 || item.samples[0].data.len() >= (1 << 28) {
                return false;
            }
            if !matches!(item.codec_configuration, CodecConfiguration::Av1(_)) {
                return false;
            }
            continue; // The gainmap input image item can be stored in the MinimizedImageBox.
        }
        if item.item_type == "tmap" {
            assert_eq!(item.category, Category::Color); // Cannot be differentiated from the primary item by its itemCategory.
            continue; // The tone mapping derived image item can be represented in the MinimizedImageBox.
        }
        if item.item_type == "mime" && item.infe_name == "XMP" {
            assert_eq!(item.metadata_payload.len(), enc.image_metadata.xmp.len());
            continue; // XMP metadata can be stored in the MinimizedImageBox.
        }
        if item.item_type == "Exif" && item.infe_name == "Exif" {
            assert_eq!(
                item.metadata_payload.len(),
                enc.image_metadata.exif.len() + 4
            );
            // Unknown endianness. It does not matter when comparing to 0.
            let exif_tiff_header_offset = [
                item.metadata_payload[0],
                item.metadata_payload[1],
                item.metadata_payload[2],
                item.metadata_payload[3],
            ];
            if exif_tiff_header_offset.iter().any(|byte| *byte != 0) {
                return false;
            }
            continue; // Exif metadata can be stored in the MinimizedImageBox if exif_tiff_header_offset is 0.
        }

        // Items besides the color item, the alpha item, the gainmap item and Exif/XMP/ICC/HDR
        // metadata are not directly supported by the MinimizedImageBox.
        return false;
    }
    // A primary item is necessary.
    if color_item.is_none() {
        return false;
    }
    true
}

impl Encoder {
    pub(crate) fn write_ftyp_and_mini(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_box("ftyp")?;
        stream.write_string(&String::from("mif3"))?; // unsigned int(32) major_brand;
        stream.write_string(&String::from("avif"))?; // unsigned int(32) minor_version;
        stream.finish_box()?;
        self.write_mini(stream)
    }

    fn write_mini(&self, stream: &mut OStream) -> AvifResult<()> {
        let color_item = self
            .items
            .iter()
            .find(|item| item.id == self.primary_item_id)
            .unwrap();
        let alpha_item = self.items.iter().find(|item| {
            item.category == Category::Alpha && item.iref_to_id == Some(self.primary_item_id)
        });
        let gainmap_item = self
            .items
            .iter()
            .find(|item| item.category == Category::Gainmap);

        let color_data = &color_item.samples.first().unwrap().data;
        let alpha_data = alpha_item.map(|item| &item.samples.first().unwrap().data);
        let gainmap_data = gainmap_item.map(|item| &item.samples.first().unwrap().data);

        let image = &self.image_metadata;
        let gainmap_image = &self.gainmap_image_metadata;

        let has_alpha = alpha_item.is_some();
        let alpha_is_premultiplied = image.alpha_premultiplied;
        let has_gainmap = gainmap_item.is_some();
        let has_hdr = has_gainmap; // CrabbyAvif only supports gainmap-based HDR encoding for now.
        let has_icc = !image.icc.is_empty();
        let chroma_subsampling = pixel_format_to_chroma_subsampling(image.yuv_format);

        let default_color_primaries =
            if has_icc { ColorPrimaries::Unspecified } else { ColorPrimaries::Bt709 };
        let default_transfer_characteristics = if has_icc {
            TransferCharacteristics::Unspecified
        } else {
            TransferCharacteristics::Srgb
        };
        let default_matrix_coefficients = if chroma_subsampling == 0 {
            MatrixCoefficients::Unspecified
        } else {
            MatrixCoefficients::Bt601
        };

        let has_explicit_cicp = image.color_primaries != default_color_primaries
            || image.transfer_characteristics != default_transfer_characteristics
            || image.matrix_coefficients != default_matrix_coefficients;

        let float_flag = false;
        let full_range = image.yuv_range == YuvRange::Full;

        // In AV1, the chroma_sample_position syntax element is not present for the YUV 4:2:2 format.
        // Assume that AV1 uses the same 4:2:2 chroma sample location as HEVC and VVC (colocated).
        if image.yuv_format != PixelFormat::Yuv420
            && image.chroma_sample_position != ChromaSamplePosition::Unknown
        {
            // YUV chroma sample position [image.chroma_sample_position] is only supported with 4:2:0 YUV format in AV1.
            return AvifError::invalid_argument();
        }
        // For the YUV 4:2:0 format, assume centered sample position unless specified otherwise.
        let chroma_is_horizontally_centered = image.yuv_format == PixelFormat::Yuv420
            && image.chroma_sample_position != ChromaSamplePosition::Vertical
            && image.chroma_sample_position != ChromaSamplePosition::Colocated;
        let chroma_is_vertically_centered = image.yuv_format == PixelFormat::Yuv420
            && image.chroma_sample_position != ChromaSamplePosition::Colocated;

        let orientation_minus1 = image_irot_imir_to_exif_orientation(image)? - 1;

        let infe_type;
        let codec_config_type;
        let has_explicit_codec_types;
        if matches!(color_item.codec_configuration, CodecConfiguration::Av1(_)) {
            infe_type = "av01";
            codec_config_type = "av1C";
            has_explicit_codec_types = false;
        } else {
            // TODO: b/437292541 - Support AVM (av02/av2C)
            return AvifError::not_implemented();
        }

        // _minus1 is encoded for these fields.
        assert_ne!(image.width, 0);
        assert_ne!(image.height, 0);
        assert_ne!(color_data.len(), 0);

        let mut large_dimensions_flag = image.width > (1 << 7) || image.height > (1 << 7);
        let codec_config_size = 4; // 'av1_c' always uses 4 bytes.
        let mut alpha_codec_config_size = 0; // 0 if same codec config as main. Equal to codec_config_size otherwise.
        let mut gainmap_codec_config_size = 0; // 0 if same codec config as main. Equal to codec_config_size otherwise.
        let mut gainmap_metadata_size = 0;
        let large_codec_config_flag = codec_config_size >= (1 << 3);
        let mut large_item_data_flag = color_data.len() > (1 << 15)
            || (alpha_data.is_some() && alpha_data.unwrap().len() >= (1 << 15));
        let mut large_metadata_flag = (has_icc && image.icc.len() > (1 << 10))
            || (!image.exif.is_empty() && image.exif.len() > (1 << 10))
            || (!image.xmp.is_empty() && image.xmp.len() > (1 << 10));

        if has_gainmap {
            // Minus one because of the prepended version field which is not part of the MinimizedImageBox syntax.
            gainmap_metadata_size = self
                .items
                .iter()
                .find(|item| item.item_type == "tmap")
                .unwrap()
                .metadata_payload
                .len()
                .checked_sub(1)
                .unwrap();

            // _minus1 is encoded for these fields.
            assert_ne!(gainmap_image.width, 0);
            assert_ne!(gainmap_image.height, 0);

            large_dimensions_flag = large_dimensions_flag
                || gainmap_image.width > (1 << 7)
                || gainmap_image.height > (1 << 7);
            large_item_data_flag = large_item_data_flag
                || (gainmap_data.is_some() && gainmap_data.unwrap().len() >= (1 << 15));
            large_metadata_flag = large_metadata_flag
                || (!self.alt_image_metadata.icc.is_empty()
                    && self.alt_image_metadata.icc.len() > (1 << 10))
                || gainmap_metadata_size >= (1 << 10);
            // gainmap_image.icc is ignored.
        }

        stream.start_box("mini")?;
        stream.write_bits(0, 2)?; // bit(2) version = 0;

        // flags
        stream.write_bool(has_explicit_codec_types)?; // bit(1) explicit_codec_types_flag;
        stream.write_bool(float_flag)?; // bit(1) float_flag;
        stream.write_bool(full_range)?; // bit(1) full_range_flag;
        stream.write_bool(alpha_item.is_some())?; // bit(1) alpha_flag;
        stream.write_bool(has_explicit_cicp)?; // bit(1) explicit_cicp_flag;
        stream.write_bool(has_hdr)?; // bit(1) hdr_flag;
        stream.write_bool(has_icc)?; // bit(1) icc_flag;
        stream.write_bool(!image.exif.is_empty())?; // bit(1) exif_flag;
        stream.write_bool(!image.xmp.is_empty())?; // bit(1) xmp_flag;

        stream.write_bits(chroma_subsampling, 2)?; // bit(2) chroma_subsampling;
        stream.write_bits(orientation_minus1.into(), 3)?; // bit(3) orientation_minus1;

        // Spatial extents
        stream.write_bool(large_dimensions_flag)?; // bit(1) large_dimensions_flag;
        stream.write_bits(image.width - 1, if large_dimensions_flag { 15 } else { 7 })?; // unsigned int(large_dimensions_flag ? 15 : 7) width_minus1;
        stream.write_bits(image.height - 1, if large_dimensions_flag { 15 } else { 7 })?; // unsigned int(large_dimensions_flag ? 15 : 7) height_minus1;

        // Pixel information
        if chroma_subsampling == 1 || chroma_subsampling == 2 {
            stream.write_bool(chroma_is_horizontally_centered)?; // bit(1) chroma_is_horizontally_centered;
        }
        if chroma_subsampling == 1 {
            stream.write_bool(chroma_is_vertically_centered)?; // bit(1) chroma_is_vertically_centered;
        }

        if float_flag {
            // bit(2) bit_depth_log2_minus4;
            return AvifError::not_implemented();
        } else {
            stream.write_bool(image.depth > 8)?; // bit(1) high_bit_depth_flag;
            if image.depth > 8 {
                stream.write_bits((image.depth - 9).into(), 3)?; // bit(3) bit_depth_minus9;
            }
        }

        if alpha_item.is_some() {
            stream.write_bool(alpha_is_premultiplied)?; // bit(1) alpha_is_premultiplied;
        }

        // Colour properties
        if has_explicit_cicp {
            stream.write_bits(image.color_primaries as u32, 8)?; // bit(8) colour_primaries;
            stream.write_bits(image.transfer_characteristics as u32, 8)?; // bit(8) transfer_characteristics;
            if chroma_subsampling != 0 {
                stream.write_bits(image.matrix_coefficients as u32, 8)?; // bit(8) matrix_coefficients;
            } else if image.matrix_coefficients != MatrixCoefficients::Unspecified {
                return AvifError::invalid_argument();
            }
        }

        if has_explicit_codec_types {
            stream.write_str(infe_type)?; // bit(32) infe_type;
            stream.write_str(codec_config_type)?; // bit(32) codec_config_type;
        }

        // High Dynamic Range properties
        let mut tmap_icc_size = 0;
        if has_hdr {
            stream.write_bool(has_gainmap)?; // bit(1) gainmap_flag;
            if has_gainmap {
                let tmap = &self.alt_image_metadata;
                let gainmap = &self.gainmap_image_metadata;
                stream.write_bits(
                    gainmap.width - 1,
                    if large_dimensions_flag { 15 } else { 7 },
                )?; // unsigned int(large_dimensions_flag ? 15 : 7) gainmap_width_minus1;
                stream.write_bits(
                    gainmap.height - 1,
                    if large_dimensions_flag { 15 } else { 7 },
                )?; // unsigned int(large_dimensions_flag ? 15 : 7) gainmap_height_minus1;
                stream.write_bits(gainmap.matrix_coefficients as u32, 8)?; // bit(8) gainmap_matrix_coefficients;
                stream.write_bool(gainmap.yuv_range == YuvRange::Full)?; // bit(1) gainmap_full_range_flag;
                let gainmap_chroma_subsampling =
                    pixel_format_to_chroma_subsampling(gainmap.yuv_format);
                stream.write_bits(gainmap_chroma_subsampling, 2)?; // bit(1) gainmap_chroma_subsampling;
                if gainmap_chroma_subsampling == 1 || gainmap_chroma_subsampling == 2 {
                    stream.write_bool(
                        gainmap.yuv_format == PixelFormat::Yuv420
                            && gainmap.chroma_sample_position != ChromaSamplePosition::Vertical
                            && gainmap.chroma_sample_position != ChromaSamplePosition::Colocated,
                    )?; // bit(1) gainmap_chroma_is_horizontally_centered;
                }
                if gainmap_chroma_subsampling == 1 {
                    stream.write_bool(
                        gainmap.yuv_format == PixelFormat::Yuv420
                            && gainmap.chroma_sample_position != ChromaSamplePosition::Colocated,
                    )?; // bit(1) gainmap_chroma_is_vertically_centered;
                }

                let gainmap_float_flag = false;
                stream.write_bool(gainmap_float_flag)?; // bit(1) gainmap_float_flag;
                if gainmap_float_flag {
                    // bit(2) gainmap_bit_depth_log2_minus4;
                    return AvifError::not_implemented();
                } else {
                    stream.write_bool(gainmap.depth > 8)?; // bit(1) gainmap_high_bit_depth_flag;
                    if gainmap.depth > 8 {
                        stream.write_bits((gainmap.depth - 9).into(), 3)?; // bit(3) gainmap_bit_depth_minus9;
                    }
                }

                tmap_icc_size = self.alt_image_metadata.icc.len();
                stream.write_bool(tmap_icc_size != 0)?; // bit(1) tmap_icc_flag;
                let tmap_has_explicit_cicp = tmap.color_primaries != ColorPrimaries::Bt709
                    || tmap.transfer_characteristics != TransferCharacteristics::Srgb
                    || tmap.matrix_coefficients != MatrixCoefficients::Bt601
                    || tmap.yuv_range != YuvRange::Full;
                stream.write_bool(tmap_has_explicit_cicp)?; // bit(1) tmap_explicit_cicp_flag;
                if tmap_has_explicit_cicp {
                    stream.write_bits(tmap.color_primaries as u32, 8)?; // bit(8) tmap_colour_primaries;
                    stream.write_bits(tmap.transfer_characteristics as u32, 8)?; // bit(8) tmap_transfer_characteristics;
                    stream.write_bits(tmap.matrix_coefficients as u32, 8)?; // bit(8) tmap_matrix_coefficients;
                    stream.write_bool(tmap.yuv_range == YuvRange::Full)?; // bit(8) tmap_full_range_flag;
                }
                // gainmap.icc is ignored.
            }

            write_mini_hdr_properties(image, stream)?;
            if has_gainmap {
                write_mini_hdr_properties(&self.alt_image_metadata, stream)?;
            }
        }

        // Chunk sizes
        if has_icc || !image.exif.is_empty() || !image.xmp.is_empty() || (has_hdr && has_gainmap) {
            stream.write_bool(large_metadata_flag)?; // bit(1) large_metadata_flag;
        }
        stream.write_bool(large_codec_config_flag)?; // bit(1) large_codec_config_flag;
        stream.write_bool(large_item_data_flag)?; // bit(1) large_item_data_flag;

        if has_icc {
            stream.write_bits(
                (image.icc.len() - 1).try_into().unwrap(),
                if large_metadata_flag { 20 } else { 10 },
            )?; // unsigned int(large_metadata_flag ? 20 : 10) icc_data_size_minus1;
        }
        if has_hdr && has_gainmap && tmap_icc_size != 0 {
            stream.write_bits(
                (tmap_icc_size - 1).try_into().unwrap(),
                if large_metadata_flag { 20 } else { 10 },
            )?; // unsigned int(large_metadata_flag ? 20 : 10) tmap_icc_data_size_minus1;
        }

        if has_hdr && has_gainmap {
            stream.write_bits(
                gainmap_metadata_size.try_into().unwrap(),
                if large_metadata_flag { 20 } else { 10 },
            )?; // unsigned int(large_metadata_flag ? 20 : 10) gainmap_metadata_size;
        }
        if has_hdr && has_gainmap {
            stream.write_bits(
                gainmap_data.unwrap().len().try_into().unwrap(),
                if large_item_data_flag { 28 } else { 15 },
            )?; // unsigned int(large_item_data_flag ? 28 : 15) gainmap_item_data_size;
        }
        if has_hdr && has_gainmap && !gainmap_data.unwrap().is_empty() {
            if gainmap_item.unwrap().codec_configuration == color_item.codec_configuration {
                // The gainmap codec config is copied from the main codec config.
                // This is signaled by a size of 0.
                gainmap_codec_config_size = 0;
            } else {
                gainmap_codec_config_size = codec_config_size;
            }
            stream.write_bits(
                gainmap_codec_config_size,
                if large_codec_config_flag { 12 } else { 3 },
            )?; // unsigned int(large_codec_config_flag ? 12 : 3) gainmap_item_codec_config_size;
        }

        stream.write_bits(
            codec_config_size,
            if large_codec_config_flag { 12 } else { 3 },
        )?; // unsigned int(large_codec_config_flag ? 12 : 3) main_item_codec_config_size;
        stream.write_bits(
            (color_data.len() - 1).try_into().unwrap(),
            if large_item_data_flag { 28 } else { 15 },
        )?; // unsigned int(large_item_data_flag ? 28 : 15) main_item_data_size_minus1;

        if has_alpha {
            stream.write_bits(
                alpha_data.unwrap().len().try_into().unwrap(),
                if large_item_data_flag { 28 } else { 15 },
            )?; // unsigned int(large_item_data_flag ? 28 : 15) alpha_item_data_size;
        }
        if has_alpha && !alpha_data.unwrap().is_empty() {
            if alpha_item.unwrap().codec_configuration == color_item.codec_configuration {
                // The alpha codec config is copied from the main codec config.
                // This is signaled by a size of 0.
                alpha_codec_config_size = 0;
            } else {
                alpha_codec_config_size = codec_config_size;
            }
            stream.write_bits(
                alpha_codec_config_size,
                if large_codec_config_flag { 12 } else { 3 },
            )?; // unsigned int(large_codec_config_flag ? 12 : 3) alpha_item_codec_config_size;
        }

        if !image.exif.is_empty() || !image.xmp.is_empty() {
            stream.write_bool(false)?; // unsigned int(1) exif_xmp_compressed_flag
        }
        if !image.exif.is_empty() {
            stream.write_bits(
                (image.exif.len() - 1).try_into().unwrap(),
                if large_metadata_flag { 20 } else { 10 },
            )?; // unsigned int(large_metadata_flag ? 20 : 10) exif_data_size_minus_one;
        }
        if !image.xmp.is_empty() {
            stream.write_bits(
                (image.xmp.len() - 1).try_into().unwrap(),
                if large_metadata_flag { 20 } else { 10 },
            )?; // unsigned int(large_metadata_flag ? 20 : 10) xmp_data_size_minus_one;
        }

        // trailing_bits(); // bit padding till byte alignment
        stream.pad()?;
        let header_bytes = stream.offset();

        // Chunks
        if codec_config_size > 0 {
            if let CodecConfiguration::Av1(config) = &color_item.codec_configuration {
                Item::write_codec_config(config, stream)?; // unsigned int(8) main_item_codec_config[main_item_codec_config_size];
            } else {
                return AvifError::unknown_error("Unexpected codec configuration");
            }
        }
        if has_alpha && !alpha_data.unwrap().is_empty() && alpha_codec_config_size != 0 {
            if let CodecConfiguration::Av1(config) = &alpha_item.unwrap().codec_configuration {
                Item::write_codec_config(config, stream)?; // unsigned int(8) alpha_item_codec_config[alpha_item_codec_config_size];
            } else {
                return AvifError::unknown_error("Unexpected codec configuration");
            }
        }
        if has_hdr && has_gainmap && gainmap_codec_config_size != 0 {
            if let CodecConfiguration::Av1(config) = &gainmap_item.unwrap().codec_configuration {
                Item::write_codec_config(config, stream)?; // unsigned int(8) gainmap_item_codec_config[gainmap_item_codec_config_size];
            } else {
                return AvifError::unknown_error("Unexpected codec configuration");
            }
        }

        if has_icc {
            stream.write_slice(image.icc.as_slice())?; // unsigned int(8) icc_data[icc_data_size_minus1 + 1];
        }
        if has_hdr && has_gainmap && tmap_icc_size != 0 {
            assert_eq!(self.alt_image_metadata.icc.len(), tmap_icc_size);
            stream.write_slice(self.alt_image_metadata.icc.as_slice())?; // unsigned int(8) tmap_icc_data[tmap_icc_data_size_minus1 + 1];
        }
        if has_hdr && has_gainmap && gainmap_metadata_size != 0 {
            // Minus one because of the prepended version field which is not part of the MinimizedImageBox syntax.
            let gainmap_metadata = &self
                .items
                .iter()
                .find(|item| item.item_type == "tmap")
                .unwrap()
                .metadata_payload
                .as_slice()[1..];
            assert_eq!(gainmap_metadata.len(), gainmap_metadata_size);
            stream.write_slice(gainmap_metadata)?; // unsigned int(8) gainmap_metadata[gainmap_metadata_size];
        }

        if has_alpha && !alpha_data.unwrap().is_empty() {
            stream.write_slice(alpha_data.unwrap().as_slice())?; // unsigned int(8) alpha_item_data[alpha_item_data_size];
        }
        if has_hdr && has_gainmap && !gainmap_data.unwrap().is_empty() {
            stream.write_slice(gainmap_data.unwrap().as_slice())?; // unsigned int(8) gainmap_item_data[gainmap_item_data_size];
        }

        stream.write_slice(color_data.as_slice())?; // unsigned int(8) main_item_data[main_item_data_size_minus1 + 1];

        if !image.exif.is_empty() {
            stream.write_slice(image.exif.as_slice())?; // unsigned int(8) exif_data[exif_data_size_minus1 + 1];
        }
        if !image.xmp.is_empty() {
            stream.write_slice(image.xmp.as_slice())?; // unsigned int(8) xmp_data[xmp_data_size_minus1 + 1];
        }

        let expected_chunk_bytes = codec_config_size as usize
            + alpha_codec_config_size as usize
            + gainmap_codec_config_size as usize
            + image.icc.len()
            + tmap_icc_size
            + gainmap_metadata_size
            + if has_alpha { alpha_data.unwrap().len() } else { 0 }
            + if has_gainmap { gainmap_data.unwrap().len() } else { 0 }
            + color_data.len()
            + image.exif.len()
            + image.xmp.len();
        assert_eq!(stream.offset(), header_bytes + expected_chunk_bytes);
        stream.finish_box()?;

        Ok(())
    }
}

fn pixel_format_to_chroma_subsampling(yuv_format: PixelFormat) -> u32 {
    match yuv_format {
        PixelFormat::Yuv400 => 0,
        PixelFormat::Yuv420 => 1,
        PixelFormat::Yuv422 => 2,
        PixelFormat::Yuv444 => 3,
        _ => panic!(), // is_mini_compatible() should have returned false.
    }
}

fn image_irot_imir_to_exif_orientation(image: &Image) -> AvifResult<u8> {
    Ok(match (image.irot_angle, image.imir_axis) {
        (None | Some(0), None) => 1, // The 0th row is at the visual top of the image, and the 0th column is the visual left-hand side.
        (None | Some(0), Some(0)) => 4, // The 0th row is at the visual bottom of the image, and the 0th column is the visual left-hand side.
        (None | Some(0), Some(1)) => 2, // The 0th row is at the visual top of the image, and the 0th column is the visual right-hand side.
        (Some(1), None) => 8, // The 0th row is the visual left-hand side of the image, and the 0th column is the visual bottom.
        (Some(1), Some(0)) => 5, // The 0th row is the visual left-hand side of the image, and the 0th column is the visual top.
        (Some(1), Some(1)) => 7, // The 0th row is the visual right-hand side of the image, and the 0th column is the visual bottom.
        (Some(2), None) => 3, // The 0th row is at the visual bottom of the image, and the 0th column is the visual right-hand side.
        (Some(2), Some(0)) => 2, // The 0th row is at the visual top of the image, and the 0th column is the visual right-hand side.
        (Some(2), Some(1)) => 4, // The 0th row is at the visual bottom of the image, and the 0th column is the visual left-hand side.
        (Some(3), None) => 6, // The 0th row is the visual right-hand side of the image, and the 0th column is the visual top.
        (Some(3), Some(0)) => 7, // The 0th row is the visual right-hand side of the image, and the 0th column is the visual bottom.
        (Some(3), Some(1)) => 5, // The 0th row is the visual left-hand side of the image, and the 0th column is the visual top.
        _ => return AvifError::invalid_argument(),
    })
}

fn write_mini_hdr_properties(image_metadata: &Image, stream: &mut OStream) -> AvifResult<()> {
    let has_clli = image_metadata.clli.is_some();
    let has_mdcv = false;
    let has_cclv = false;
    let has_amve = false;
    let has_reve = false;
    let has_ndwt = false;
    stream.write_bool(has_clli)?; // bit(1) clli_flag;
    stream.write_bool(has_mdcv)?; // bit(1) mdcv_flag;
    stream.write_bool(has_cclv)?; // bit(1) cclv_flag;
    stream.write_bool(has_amve)?; // bit(1) amve_flag;
    stream.write_bool(has_reve)?; // bit(1) reve_flag;
    stream.write_bool(has_ndwt)?; // bit(1) ndwt_flag;

    if let Some(clli) = &image_metadata.clli {
        // ContentLightLevel clli;
        write_content_light_level_information(clli, stream)?;
    }
    if has_mdcv {
        // MasteringDisplayColourVolume mdcv;
    }
    if has_cclv {
        // ContentColourVolume cclv;
    }
    if has_amve {
        // AmbientViewingEnvironment amve;
    }
    if has_reve {
        // ReferenceViewingEnvironment reve;
    }
    if has_ndwt {
        // NominalDiffuseWhite ndwt;
    }
    Ok(())
}

fn write_content_light_level_information(
    clli: &ContentLightLevelInformation,
    stream: &mut OStream,
) -> AvifResult<()> {
    stream.write_bits(clli.max_cll.into(), 16)?; // unsigned int(16) max_content_light_level;
    stream.write_bits(clli.max_pall.into(), 16)?; // unsigned int(16) max_pic_average_light_level;

    Ok(())
}
