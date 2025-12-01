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

use crate::internal_utils::stream::*;
use crate::parser::mp4box::*;
use crate::*;

// Implementation for ISO/IEC 23008-12 3rd edition AMD 2 Low-overhead image file format.
// See drafts at https://www.mpeg.org/standards/MPEG-H/12/.

// Parses a MinimizedImageBox and returns a virtually reconstructed MetaBox.
pub(crate) fn parse_mini(stream: &mut IStream, offset: usize) -> AvifResult<MetaBox> {
    let mut meta = MetaBox::default();

    let version = stream.read_bits(2)?; // bit(2) version = 0;
    if version != 0 {
        return AvifError::bmff_parse_failed(format!(
            "version {version} should be 0 in 'mini' box"
        ));
    }

    // flags
    let has_explicit_codec_types = stream.read_bool()?; // bit(1) explicit_codec_types_flag;
    let float_flag = stream.read_bool()?; // bit(1) float_flag;
    let full_range = stream.read_bool()?; // bit(1) full_range_flag;
    let has_alpha = stream.read_bool()?; // bit(1) alpha_flag;
    let has_explicit_cicp = stream.read_bool()?; // bit(1) explicit_cicp_flag;
    let has_hdr = stream.read_bool()?; // bit(1) hdr_flag;
    let has_icc = stream.read_bool()?; // bit(1) icc_flag;
    let has_exif = stream.read_bool()?; // bit(1) exif_flag;
    let has_xmp = stream.read_bool()?; // bit(1) xmp_flag;

    let chroma_subsampling = stream.read_bits(2)?; // bit(2) chroma_subsampling;
    let orientation = stream.read_bits(3)? + 1; // bit(3) orientation_minus1;

    // Spatial extents
    let large_dimensions_flag = stream.read_bool()?; // bit(1) large_dimensions_flag;
    let width = stream.read_bits(if large_dimensions_flag { 15 } else { 7 })? + 1; // unsigned int(large_dimensions_flag ? 15 : 7) width_minus1;
    let height = stream.read_bits(if large_dimensions_flag { 15 } else { 7 })? + 1; // unsigned int(large_dimensions_flag ? 15 : 7) height_minus1;

    // Pixel information
    let chroma_is_horizontally_centered = if chroma_subsampling == 1 || chroma_subsampling == 2 {
        stream.read_bool()? // bit(1) chroma_is_horizontally_centered;
    } else {
        false
    };
    let chroma_is_vertically_centered = if chroma_subsampling == 1 {
        stream.read_bool()? // bit(1) chroma_is_vertically_centered;
    } else {
        false
    };

    let bit_depth = if float_flag {
        // bit(2) bit_depth_log2_minus4;
        return AvifError::not_implemented(); // Either invalid AVIF or unsupported non-AVIF.
    } else {
        let high_bit_depth_flag = stream.read_bool()?; // bit(1) high_bit_depth_flag;
        if high_bit_depth_flag {
            stream.read_bits(3)? + 9 // bit(3) bit_depth_minus9;
        } else {
            8
        }
    };

    let alpha_is_premultiplied = if has_alpha {
        stream.read_bool()? // bit(1) alpha_is_premultiplied;
    } else {
        false
    };

    // Colour properties
    let color_primaries;
    let transfer_characteristics;
    let matrix_coefficients;
    if has_explicit_cicp {
        color_primaries = ColorPrimaries::from(stream.read_bits(8)? as u16); // bit(8) colour_primaries;
        transfer_characteristics = TransferCharacteristics::from(stream.read_bits(8)? as u16); // bit(8) transfer_characteristics;
        matrix_coefficients = if chroma_subsampling != 0 {
            MatrixCoefficients::from(stream.read_bits(8)? as u16) // bit(8) matrix_coefficients;
        } else {
            MatrixCoefficients::Unspecified // 2
        };
    } else {
        color_primaries = if has_icc {
            ColorPrimaries::Unspecified // 2
        } else {
            ColorPrimaries::Bt709
        }; // 1
        transfer_characteristics = if has_icc {
            TransferCharacteristics::Unspecified // 2
        } else {
            TransferCharacteristics::Srgb
        }; // 13
        matrix_coefficients = if chroma_subsampling == 0 {
            MatrixCoefficients::Unspecified // 2
        } else {
            MatrixCoefficients::Bt601
        }; // 6
    }

    let infe_type;
    let _codec_config_type;
    if has_explicit_codec_types {
        // bit(32) infe_type;
        let infe = [
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
        ];
        let infe = String::from_utf8(infe.into());
        // bit(32) codec_config_type;
        let codec_config = [
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
            stream.read_bits(8)? as u8,
        ];
        let codec_config = String::from_utf8(codec_config.into());
        // TODO: b/437292541 - Support AVM
        if infe != Ok("av01".into()) || codec_config != Ok("av1C".into()) {
            return AvifError::bmff_parse_failed(format!(
                "Unsupported infe_type {infe:?} or codec_config_type {codec_config:?}"
            ));
        }
        infe_type = infe.unwrap();
        _codec_config_type = codec_config.unwrap();
    } else {
        infe_type = "av01".into();
        _codec_config_type = "av1C".into();
    }

    // High Dynamic Range properties
    let mut has_gainmap = false;
    let mut tmap_has_icc = false;
    let mut gainmap_width = 0;
    let mut gainmap_height = 0;
    let mut gainmap_matrix_coefficients = MatrixCoefficients::Identity;
    let mut gainmap_full_range = false;
    let mut gainmap_chroma_subsampling = 0;
    let mut gainmap_chroma_is_horizontally_centered = false;
    let mut gainmap_chroma_is_vertically_centered = false;
    let mut gainmap_bit_depth = 0;
    let mut tmap_has_explicit_cicp = false;
    let mut tmap_color_primaries = ColorPrimaries::Unknown;
    let mut tmap_transfer_characteristics = TransferCharacteristics::Unknown;
    let mut tmap_matrix_coefficients = MatrixCoefficients::Identity;
    let mut tmap_full_range = false;
    let mut clli = None;
    let mut tmap_clli = None;
    if has_hdr {
        has_gainmap = stream.read_bool()?; // bit(1) gainmap_flag;
        if has_gainmap {
            gainmap_width = stream.read_bits(if large_dimensions_flag { 15 } else { 7 })? + 1; // unsigned int(large_dimensions_flag ? 15 : 7) gainmap_width_minus1;
            gainmap_height = stream.read_bits(if large_dimensions_flag { 15 } else { 7 })? + 1; // unsigned int(large_dimensions_flag ? 15 : 7) gainmap_height_minus1;
            gainmap_matrix_coefficients = MatrixCoefficients::from(stream.read_bits(8)? as u16); // bit(8) gainmap_matrix_coefficients;
            gainmap_full_range = stream.read_bool()?; // bit(1) gainmap_full_range_flag;

            gainmap_chroma_subsampling = stream.read_bits(2)?; // bit(2) gainmap_chroma_subsampling;
            if gainmap_chroma_subsampling == 1 || gainmap_chroma_subsampling == 2 {
                gainmap_chroma_is_horizontally_centered = stream.read_bool()?; // bit(1) gainmap_chroma_is_horizontally_centered;
            }
            if gainmap_chroma_subsampling == 1 {
                gainmap_chroma_is_vertically_centered = stream.read_bool()?; // bit(1) gainmap_chroma_is_vertically_centered;
            }

            let gainmap_float_flag = stream.read_bool()?; // bit(1) gainmap_float_flag;
            gainmap_bit_depth = if gainmap_float_flag {
                // bit(2) gainmap_bit_depth_log2_minus4;
                // Either invalid AVIF or unsupported non-AVIF.
                return AvifError::bmff_parse_failed("gainmap_float_flag cannot be 1 for AV1");
            } else {
                let gainmap_high_bit_depth_flag = stream.read_bool()?; // bit(1) gainmap_high_bit_depth_flag;
                if gainmap_high_bit_depth_flag {
                    stream.read_bits(3)? + 9 // bit(3) gainmap_bit_depth_minus9;
                } else {
                    8
                }
            };

            tmap_has_icc = stream.read_bool()?; // bit(1) tmap_icc_flag;
            tmap_has_explicit_cicp = stream.read_bool()?; // bit(1) tmap_explicit_cicp_flag;
            if tmap_has_explicit_cicp {
                tmap_color_primaries = ColorPrimaries::from(stream.read_bits(8)? as u16); // bit(8) tmap_colour_primaries;
                tmap_transfer_characteristics =
                    TransferCharacteristics::from(stream.read_bits(8)? as u16); // bit(8) tmap_transfer_characteristics;
                tmap_matrix_coefficients = MatrixCoefficients::from(stream.read_bits(8)? as u16); // bit(8) tmap_matrix_coefficients;
                tmap_full_range = stream.read_bool()?; // bit(1) tmap_full_range_flag;
            } else {
                tmap_color_primaries = ColorPrimaries::Bt709; // 1
                tmap_transfer_characteristics = TransferCharacteristics::Srgb; // 13
                tmap_matrix_coefficients = MatrixCoefficients::Bt601; // 6
                tmap_full_range = true;
            }
        }
        clli = parse_mini_hdrproperties(stream)?;
        if has_gainmap {
            tmap_clli = parse_mini_hdrproperties(stream)?;
        }
    }

    // Chunk sizes
    let large_metadata_flag = if has_icc || has_exif || has_xmp || (has_hdr && has_gainmap) {
        stream.read_bool()? // bit(1) large_metadata_flag;
    } else {
        false
    };
    let large_codec_config_flag = stream.read_bool()?; // bit(1) large_codec_config_flag;
    let large_item_data_flag = stream.read_bool()?; // bit(1) large_item_data_flag;

    let icc_data_size = if has_icc {
        stream.read_bits(if large_metadata_flag { 20 } else { 10 })? + 1 // unsigned int(large_metadata_flag ? 20 : 10) icc_data_size_minus1;
    } else {
        0
    };
    let tmap_icc_data_size = if has_hdr && has_gainmap && tmap_has_icc {
        stream.read_bits(if large_metadata_flag { 20 } else { 10 })? + 1 // unsigned int(large_metadata_flag ? 20 : 10) tmap_icc_data_size_minus1;
    } else {
        0
    };

    let gainmap_metadata_size = if has_hdr && has_gainmap {
        stream.read_bits(if large_metadata_flag { 20 } else { 10 })? // unsigned int(large_metadata_flag ? 20 : 10) gainmap_metadata_size;
    } else {
        0
    };
    let gainmap_item_data_size = if has_hdr && has_gainmap {
        stream.read_bits(if large_item_data_flag { 28 } else { 15 })? // unsigned int(large_item_data_flag ? 28 : 15) gainmap_item_data_size;
    } else {
        0
    };
    let mut gainmap_item_codec_config_size = if has_hdr && has_gainmap && gainmap_item_data_size > 0
    {
        stream.read_bits(if large_codec_config_flag { 12 } else { 3 })? // unsigned int(large_codec_config_flag ? 12 : 3) gainmap_item_codec_config_size;
    } else {
        0
    };

    let main_item_codec_config_size =
        stream.read_bits(if large_codec_config_flag { 12 } else { 3 })?; // unsigned int(large_codec_config_flag ? 12 : 3) main_item_codec_config_size;
    let main_item_data_size = stream.read_bits(if large_item_data_flag { 28 } else { 15 })? + 1; // unsigned int(large_item_data_flag ? 28 : 15) main_item_data_size_minus1;

    let alpha_item_data_size = if has_alpha {
        stream.read_bits(if large_item_data_flag { 28 } else { 15 })? // unsigned int(large_item_data_flag ? 28 : 15) alpha_item_data_size;
    } else {
        0
    };
    let mut alpha_item_codec_config_size = if has_alpha && alpha_item_data_size != 0 {
        stream.read_bits(if large_codec_config_flag { 12 } else { 3 })? // unsigned int(large_codec_config_flag ? 12 : 3) alpha_item_codec_config_size;
    } else {
        0
    };

    if has_exif || has_xmp {
        let exif_xmp_compressed_flag = stream.read_bool()?; // unsigned int(1) exif_xmp_compressed_flag;
        if exif_xmp_compressed_flag {
            return AvifError::not_implemented();
        }
    }
    let exif_data_size = if has_exif {
        stream.read_bits(if large_metadata_flag { 20 } else { 10 })? + 1 // unsigned int(large_metadata_flag ? 20 : 10) exif_data_size_minus_one;
    } else {
        0
    };
    let xmp_data_size = if has_xmp {
        stream.read_bits(if large_metadata_flag { 20 } else { 10 })? + 1 // unsigned int(large_metadata_flag ? 20 : 10) xmp_data_size_minus_one;
    } else {
        0
    };

    // trailing_bits(); // bit padding till byte alignment
    stream.pad()?;

    let main_item_codec_config = Av1CodecConfiguration::parse(
        &mut stream.sub_stream(&BoxSize::FixedSize(main_item_codec_config_size as usize))?,
    )?; // unsigned int(8) main_item_codec_config[main_item_codec_config_size];
    let alpha_item_codec_config = if has_alpha && alpha_item_data_size != 0 {
        Some(if alpha_item_codec_config_size == 0 {
            alpha_item_codec_config_size = main_item_codec_config_size;
            main_item_codec_config.clone()
        } else {
            Av1CodecConfiguration::parse(
                &mut stream
                    .sub_stream(&BoxSize::FixedSize(alpha_item_codec_config_size as usize))?,
            )? // unsigned int(8) alpha_item_codec_config[alpha_item_codec_config_size];
        })
    } else {
        None
    };
    let gainmap_item_codec_config = if has_hdr && has_gainmap {
        Some(if gainmap_item_codec_config_size == 0 {
            gainmap_item_codec_config_size = main_item_codec_config_size;
            main_item_codec_config.clone()
        } else {
            Av1CodecConfiguration::parse(
                &mut stream
                    .sub_stream(&BoxSize::FixedSize(gainmap_item_codec_config_size as usize))?,
            )? // unsigned int(8) gainmap_item_codec_config[gainmap_item_codec_config_size];
        })
    } else {
        None
    };

    // Verify subsampling information consistency.
    check_subsampling(
        chroma_subsampling,
        chroma_is_horizontally_centered,
        chroma_is_vertically_centered,
        &main_item_codec_config,
    )?;
    if has_hdr && has_gainmap {
        check_subsampling(
            gainmap_chroma_subsampling,
            gainmap_chroma_is_horizontally_centered,
            gainmap_chroma_is_vertically_centered,
            gainmap_item_codec_config.unwrap_ref(),
        )?;
    }

    // Make sure all metadata and coded chunks fit.
    // There should be no missing nor unused byte.

    let offset_till_remaining_bytes = stream.offset;
    let remaining_bytes = &stream.data[offset_till_remaining_bytes..];
    if remaining_bytes.len() as u32
        != icc_data_size
            + tmap_icc_data_size
            + gainmap_metadata_size
            + alpha_item_data_size
            + gainmap_item_data_size
            + main_item_data_size
            + exif_data_size
            + xmp_data_size
    {
        return AvifError::bmff_parse_failed("Unexpected mini size");
    }

    let offset_till_remaining_bytes = offset + offset_till_remaining_bytes;
    let mut remaining_bytes_offset = 0usize;

    // Create the items and properties generated by the MinimizedImageBox.
    // The MinimizedImageBox always creates a fixed number of properties for
    // specification easiness. Use FreeSpaceBoxes as no-op placeholder
    // properties when necessary.
    // There is no need to use placeholder items because item IDs do not have to
    // be contiguous, whereas property indices shall be 1, 2, 3, 4, 5 etc.

    // Start with the properties.
    meta.iprp.properties = vec![
        // entry 1
        if main_item_codec_config_size != 0 {
            ItemProperty::CodecConfiguration(CodecConfiguration::Av1(main_item_codec_config))
        } else {
            ItemProperty::Unused
        },
        // entry 2
        ItemProperty::ImageSpatialExtents(ImageSpatialExtents { width, height }),
        // entry 3
        // TODO: b/437307282 - Support extended pixi when available
        ItemProperty::PixelInformation(PixelInformation {
            plane_depths: vec![
                bit_depth as u8;
                chroma_subsampling_to_pixel_format(chroma_subsampling).plane_count()
            ],
        }),
        // entry 4
        ItemProperty::ColorInformation(ColorInformation::Nclx(Nclx {
            color_primaries,
            transfer_characteristics,
            matrix_coefficients,
            yuv_range: if full_range { YuvRange::Full } else { YuvRange::Limited },
        })),
        // entry 5
        if has_icc {
            let icc = &remaining_bytes
                [remaining_bytes_offset..(remaining_bytes_offset + icc_data_size as usize)];
            remaining_bytes_offset += icc_data_size as usize;
            ItemProperty::ColorInformation(ColorInformation::Icc(icc.into()))
        } else {
            ItemProperty::Unused
        },
        // entry 6
        if alpha_item_codec_config_size != 0 {
            ItemProperty::CodecConfiguration(CodecConfiguration::Av1(
                alpha_item_codec_config.unwrap(),
            ))
        } else {
            ItemProperty::Unused
        },
        // entry 7
        if alpha_item_data_size != 0 {
            ItemProperty::AuxiliaryType("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha".into())
        } else {
            ItemProperty::Unused
        },
        // entry 8
        if alpha_item_data_size != 0 {
            // TODO: b/437307282 - Support extended pixi when available
            ItemProperty::PixelInformation(PixelInformation {
                // Note that alpha's av1C is_monochrome may be false.
                // Some encoders do not support 4:0:0 and encode alpha with
                // placeholder chroma planes to be ignored at decoding.
                plane_depths: vec![bit_depth as u8],
            })
        } else {
            ItemProperty::Unused
        },
        // entry 9
        match orientation {
            3 => ItemProperty::ImageRotation(2),
            5 => ItemProperty::ImageRotation(1),
            6 => ItemProperty::ImageRotation(3),
            7 => ItemProperty::ImageRotation(1),
            8 => ItemProperty::ImageRotation(1),
            _ => ItemProperty::Unused,
        },
        // entry 10
        match orientation {
            2 => ItemProperty::ImageMirror(1),
            4 => ItemProperty::ImageMirror(0),
            5 => ItemProperty::ImageMirror(0),
            7 => ItemProperty::ImageMirror(1),
            _ => ItemProperty::Unused,
        },
        // entry 11
        if let Some(clli) = clli {
            ItemProperty::ContentLightLevelInformation(clli)
        } else {
            ItemProperty::Unused
        },
        // entry 12
        ItemProperty::Unused, // mdcv
        // entry 13
        ItemProperty::Unused, // cclv
        // entry 14
        ItemProperty::Unused, // amve
        // entry 15
        ItemProperty::Unused, // reve
        // entry 16
        ItemProperty::Unused, // ndwt
        // entry 17
        if gainmap_item_codec_config_size != 0 {
            ItemProperty::CodecConfiguration(CodecConfiguration::Av1(
                gainmap_item_codec_config.unwrap(),
            ))
        } else {
            ItemProperty::Unused
        },
        // entry 18
        if gainmap_item_data_size != 0 {
            ItemProperty::ImageSpatialExtents(ImageSpatialExtents {
                width: gainmap_width,
                height: gainmap_height,
            })
        } else {
            ItemProperty::Unused
        },
        // entry 19
        if gainmap_item_data_size != 0 {
            // TODO: b/437307282 - Support extended pixi when available
            ItemProperty::PixelInformation(PixelInformation {
                plane_depths: vec![
                    gainmap_bit_depth as u8;
                    chroma_subsampling_to_pixel_format(gainmap_chroma_subsampling)
                        .plane_count()
                ],
            })
        } else {
            ItemProperty::Unused
        },
        // entry 20
        if gainmap_item_data_size != 0 {
            ItemProperty::ColorInformation(ColorInformation::Nclx(Nclx {
                color_primaries: ColorPrimaries::Unspecified,
                transfer_characteristics: TransferCharacteristics::Unspecified,
                matrix_coefficients: gainmap_matrix_coefficients,
                yuv_range: if gainmap_full_range { YuvRange::Full } else { YuvRange::Limited },
            }))
        } else {
            ItemProperty::Unused
        },
        // entry 21
        if has_gainmap {
            ItemProperty::ImageSpatialExtents(ImageSpatialExtents {
                width: match orientation {
                    0..3 => width,
                    _ => height,
                },
                height: match orientation {
                    0..3 => height,
                    _ => width,
                },
            })
        } else {
            ItemProperty::Unused
        },
        // entry 22
        if has_gainmap && (tmap_has_explicit_cicp || !tmap_has_icc) {
            ItemProperty::ColorInformation(ColorInformation::Nclx(Nclx {
                color_primaries: tmap_color_primaries,
                transfer_characteristics: tmap_transfer_characteristics,
                matrix_coefficients: tmap_matrix_coefficients,
                yuv_range: if tmap_full_range { YuvRange::Full } else { YuvRange::Limited },
            }))
        } else {
            ItemProperty::Unused
        },
        // entry 23
        if has_gainmap && tmap_has_icc {
            let tmap_icc = &remaining_bytes
                [remaining_bytes_offset..(remaining_bytes_offset + tmap_icc_data_size as usize)];
            remaining_bytes_offset += tmap_icc_data_size as usize;
            ItemProperty::ColorInformation(ColorInformation::Icc(tmap_icc.into()))
        } else {
            ItemProperty::Unused
        },
        // entry 24
        if has_gainmap && tmap_clli.is_some() {
            ItemProperty::ContentLightLevelInformation(tmap_clli.unwrap())
        } else {
            ItemProperty::Unused
        },
        // entry 25
        ItemProperty::Unused, // tmap_mdcv
        // entry 26
        ItemProperty::Unused, // tmap_cclv
        // entry 27
        ItemProperty::Unused, // tmap_amve
        // entry 28
        ItemProperty::Unused, // tmap_reve
        // entry 29
        ItemProperty::Unused, // tmap_ndwt
        // entry 30
        if has_alpha && alpha_item_data_size != 0 {
            // TODO: Use an AlphaInformationProperty when supported
            ItemProperty::Unused
        } else {
            ItemProperty::Unused
        },
        // entry 31
        ItemProperty::Unused, // reserved
        // entry 32
        ItemProperty::Unused, // reserved
    ];

    // Color item
    let color_item_id = 1;
    meta.primary_item_id = color_item_id;
    meta.iinf.push(ItemInfo {
        item_id: color_item_id,
        item_type: infe_type.clone(),
        ..Default::default()
    });
    meta.iprp.associations.push(ItemPropertyAssociation {
        item_id: color_item_id,
        associations: vec![(1, true), (2, false), (3, false), (4, true), (5, true)],
    });
    if has_alpha && alpha_item_data_size == 0 {
        meta.iprp
            .associations
            .last_mut()
            .unwrap()
            .associations
            .push((30, true));
    }
    if has_hdr {
        meta.iprp
            .associations
            .last_mut()
            .unwrap()
            .associations
            .extend_from_slice(&[
                (11, false),
                (12, false),
                (13, false),
                (14, false),
                (15, false),
                (16, false),
            ]);
    }
    // ISO/IEC 23008-12 Section 6.5.1:
    //   Writers should arrange the descriptive properties specified in 6.5 prior to any other properties in the
    //   sequence associating properties with an item.
    //
    // irot and imir are transformative properties, so associate them last.
    meta.iprp
        .associations
        .last_mut()
        .unwrap()
        .associations
        .extend_from_slice(&[(9, true), (10, true)]);

    // Alpha item
    let alpha_item_id = 2;
    if has_alpha {
        meta.iinf.push(ItemInfo {
            item_id: alpha_item_id,
            item_type: infe_type.clone(),
            ..Default::default()
        });
        meta.iref.push(ItemReference {
            from_item_id: alpha_item_id,
            to_item_id: color_item_id,
            reference_type: "auxl".into(),
            index: meta.iref.len() as u32,
        });
        if alpha_is_premultiplied {
            meta.iref.push(ItemReference {
                from_item_id: color_item_id,
                to_item_id: alpha_item_id,
                reference_type: "prem".into(),
                index: meta.iref.len() as u32,
            });
        }

        // Subsampling is not checked. Alpha is only interesting for its luma
        // plane. The other planes are ignored if any.

        assert_ne!(alpha_item_data_size, 0);
        meta.iprp.associations.push(ItemPropertyAssociation {
            item_id: alpha_item_id,
            associations: vec![
                (6, true),
                (2, false),
                (7, true),
                (8, false),
                // ISO/IEC 23008-12 Section 6.5.1:
                //   Writers should arrange the descriptive properties specified in 6.5 prior to any other properties in the
                //   sequence associating properties with an item.
                //
                // irot and imir are transformative properties, so associate them last.
                (9, true),
                (10, true),
            ],
        });
    }

    // HDR items
    let tmap_item_id = 3;
    let gainmap_item_id = 4;
    let _alternative_group_id = 5;
    if has_gainmap {
        meta.iinf.push(ItemInfo {
            item_id: tmap_item_id,
            item_type: "tmap".into(),
            ..Default::default()
        });
        meta.iref.push(ItemReference {
            from_item_id: tmap_item_id,
            to_item_id: color_item_id,
            reference_type: "dimg".into(),
            index: meta.iref.len() as u32,
        });
        meta.grpl.push(EntityGroup {
            // id: _alternative_group_id
            grouping_type: "altr".into(),
            entity_ids: vec![tmap_item_id, color_item_id],
        });

        meta.iprp.associations.push(ItemPropertyAssociation {
            item_id: tmap_item_id,
            associations: vec![
                (21, false),
                (22, true),
                (23, true),
                (24, false),
                (25, false),
                (26, false),
                (27, false),
                (28, false),
                (29, false),
            ],
        });
    }
    if gainmap_item_data_size != 0 {
        meta.iinf.push(ItemInfo {
            item_id: gainmap_item_id,
            item_type: infe_type.clone(),
            ..Default::default()
        });
        meta.iref.push(ItemReference {
            from_item_id: tmap_item_id,
            to_item_id: gainmap_item_id,
            reference_type: "dimg".into(),
            index: meta.iref.len() as u32,
        });

        meta.iprp.associations.push(ItemPropertyAssociation {
            item_id: gainmap_item_id,
            associations: vec![
                (17, true),
                (18, false),
                (19, false),
                (20, true),
                // ISO/IEC 23008-12 Section 6.5.1:
                //   Writers should arrange the descriptive properties specified in 6.5 prior to any other properties in the
                //   sequence associating properties with an item.
                //
                // irot and imir are transformative properties, so associate them last.
                (9, true),
                (10, true),
            ],
        });
    }

    // Extents.

    if gainmap_metadata_size != 0 {
        // The following must be prepended to form the tone-mapping derived image item data:
        //   unsigned int(8) version = 0;
        // Copy the GainMapMetadata bytes to a virtual 'idat' box to that end.
        assert!(meta.idat.is_empty());
        meta.idat = vec![0]; // unsigned int(8) version = 0;
        meta.idat.extend_from_slice(
            &remaining_bytes
                [remaining_bytes_offset..(remaining_bytes_offset + gainmap_metadata_size as usize)],
        ); // GainMapMetadata
        remaining_bytes_offset += gainmap_metadata_size as usize;
        meta.iloc.items.push(ItemLocationEntry {
            item_id: tmap_item_id,
            construction_method: 1, // idat
            base_offset: 0,
            extent_count: 0,
            extents: vec![decoder::Extent {
                offset: 0,
                size: meta.idat.len(),
            }],
        });
    }

    if has_alpha {
        meta.iloc.items.push(ItemLocationEntry {
            item_id: alpha_item_id,
            construction_method: 0,
            base_offset: 0,
            extent_count: 0,
            extents: vec![decoder::Extent {
                offset: (offset_till_remaining_bytes + remaining_bytes_offset) as u64,
                size: alpha_item_data_size as usize,
            }],
        });
        remaining_bytes_offset += alpha_item_data_size as usize;
    }

    if gainmap_item_data_size != 0 {
        meta.iloc.items.push(ItemLocationEntry {
            item_id: gainmap_item_id,
            construction_method: 0,
            base_offset: 0,
            extent_count: 0,
            extents: vec![decoder::Extent {
                offset: (offset_till_remaining_bytes + remaining_bytes_offset) as u64,
                size: gainmap_item_data_size as usize,
            }],
        });
        remaining_bytes_offset += gainmap_item_data_size as usize;
    }

    meta.iloc.items.push(ItemLocationEntry {
        item_id: color_item_id,
        construction_method: 0,
        base_offset: 0,
        extent_count: 0,
        extents: vec![decoder::Extent {
            offset: (offset_till_remaining_bytes + remaining_bytes_offset) as u64,
            size: main_item_data_size as usize,
        }],
    });
    remaining_bytes_offset += main_item_data_size as usize;

    let exif_item_id = 6;
    if has_exif {
        meta.iinf.push(ItemInfo {
            item_id: exif_item_id,
            item_type: "Exif".into(),
            ..Default::default()
        });
        meta.iref.push(ItemReference {
            from_item_id: exif_item_id,
            to_item_id: color_item_id,
            reference_type: "cdsc".into(),
            index: meta.iref.len() as u32,
        });

        meta.iloc.items.push(ItemLocationEntry {
            item_id: exif_item_id,
            construction_method: 0,
            base_offset: 0,
            extent_count: 0,
            extents: vec![decoder::Extent {
                // Does not include unsigned int(32) exif_tiff_header_offset;
                offset: (offset_till_remaining_bytes + remaining_bytes_offset) as u64,
                size: exif_data_size as usize,
            }],
        });
        remaining_bytes_offset += exif_data_size as usize;
    }

    let xmp_item_id = 7;
    if has_xmp {
        meta.iinf.push(ItemInfo {
            item_id: xmp_item_id,
            item_type: "mime".into(),
            content_type: "application/rdf+xml".into(),
            ..Default::default()
        });
        meta.iref.push(ItemReference {
            from_item_id: xmp_item_id,
            to_item_id: color_item_id,
            reference_type: "cdsc".into(),
            index: meta.iref.len() as u32,
        });

        meta.iloc.items.push(ItemLocationEntry {
            item_id: xmp_item_id,
            construction_method: 0,
            base_offset: 0,
            extent_count: 0,
            extents: vec![decoder::Extent {
                offset: (offset_till_remaining_bytes + remaining_bytes_offset) as u64,
                size: xmp_data_size as usize,
            }],
        });
    }

    Ok(meta)
}

fn skip_mastering_display_colour_volume(stream: &mut IStream) -> Result<(), AvifError> {
    for _ in [0, 1, 2] {
        stream.skip_bits(16)?; // unsigned int(16) display_primaries_x;
        stream.skip_bits(16)?; // unsigned int(16) display_primaries_y;
    }
    stream.skip_bits(16)?; // unsigned int(16) white_point_x;
    stream.skip_bits(16)?; // unsigned int(16) white_point_y;
    stream.skip_bits(32)?; // unsigned int(32) max_display_mastering_luminance;
    stream.skip_bits(32)?; // unsigned int(32) min_display_mastering_luminance;
    Ok(())
}

fn skip_content_colour_volume(stream: &mut IStream) -> Result<(), AvifError> {
    stream.skip_bits(1)?; // unsigned int(1) reserved = 0; // ccv_cancel_flag
    stream.skip_bits(1)?; // unsigned int(1) reserved = 0; // ccv_persistence_flag
    let ccv_primaries_present = stream.read_bool()?; // unsigned int(1) ccv_primaries_present_flag;
    let ccv_min_luminance_value_present = stream.read_bool()?; // unsigned int(1) ccv_min_luminance_value_present_flag;
    let ccv_max_luminance_value_present = stream.read_bool()?; // unsigned int(1) ccv_max_luminance_value_present_flag;
    let ccv_avg_luminance_value_present = stream.read_bool()?; // unsigned int(1) ccv_avg_luminance_value_present_flag;
    stream.skip_bits(2)?; // unsigned int(2) reserved = 0;

    if ccv_primaries_present {
        for _ in [0, 1, 2] {
            stream.skip_bits(32)?; // signed int(32) ccv_primaries_x[[c]];
            stream.skip_bits(32)?; // signed int(32) ccv_primaries_y[[c]];
        }
    }
    if ccv_min_luminance_value_present {
        stream.skip_bits(32)?; // unsigned int(32) ccv_min_luminance_value;
    }
    if ccv_max_luminance_value_present {
        stream.skip_bits(32)?; // unsigned int(32) ccv_max_luminance_value;
    }
    if ccv_avg_luminance_value_present {
        stream.skip_bits(32)?; // unsigned int(32) ccv_avg_luminance_value;
    }
    Ok(())
}

fn skip_ambient_viewing_environment(stream: &mut IStream) -> Result<(), AvifError> {
    stream.skip_bits(32)?; // unsigned int(32) ambient_illuminance;
    stream.skip_bits(16)?; // unsigned int(16) ambient_light_x;
    stream.skip_bits(16)?; // unsigned int(16) ambient_light_y;
    Ok(())
}

fn skip_reference_viewing_environment(stream: &mut IStream) -> Result<(), AvifError> {
    stream.skip_bits(32)?; // unsigned int(32) surround_luminance;
    stream.skip_bits(16)?; // unsigned int(16) surround_light_x;
    stream.skip_bits(16)?; // unsigned int(16) surround_light_y;
    stream.skip_bits(32)?; // unsigned int(32) periphery_luminance;
    stream.skip_bits(16)?; // unsigned int(16) periphery_light_x;
    stream.skip_bits(16)?; // unsigned int(16) periphery_light_y;
    Ok(())
}

fn skip_nominal_diffuse_white(stream: &mut IStream) -> Result<(), AvifError> {
    stream.skip_bits(32)?; // unsigned int(32) diffuse_white_luminance;
    Ok(())
}

fn parse_mini_hdrproperties(
    stream: &mut IStream,
) -> Result<Option<ContentLightLevelInformation>, AvifError> {
    let has_clli = stream.read_bool()?; // bit(1) clli_flag;
    let has_mdcv = stream.read_bool()?; // bit(1) mdcv_flag;
    let has_cclv = stream.read_bool()?; // bit(1) cclv_flag;
    let has_amve = stream.read_bool()?; // bit(1) amve_flag;
    let has_reve = stream.read_bool()?; // bit(1) reve_flag;
    let has_ndwt = stream.read_bool()?; // bit(1) ndwt_flag;
    let clli = if has_clli {
        Some(ContentLightLevelInformation::parse(stream)?) // ContentLightLevel clli;
    } else {
        None
    };
    if has_mdcv {
        skip_mastering_display_colour_volume(stream)?; // MasteringDisplayColourVolume mdcv;
    }
    if has_cclv {
        skip_content_colour_volume(stream)?; // ContentColourVolume cclv;
    }
    if has_amve {
        skip_ambient_viewing_environment(stream)?; // AmbientViewingEnvironment amve;
    }
    if has_reve {
        skip_reference_viewing_environment(stream)?; // ReferenceViewingEnvironment reve;
    }
    if has_ndwt {
        skip_nominal_diffuse_white(stream)?; // NominalDiffuseWhite ndwt;
    }
    Ok(clli)
}

fn chroma_subsampling_to_pixel_format(chroma_subsampling: u32) -> PixelFormat {
    assert!(chroma_subsampling < 4);
    match chroma_subsampling {
        0 => PixelFormat::Yuv400,
        1 => PixelFormat::Yuv420,
        2 => PixelFormat::Yuv422,
        _ => PixelFormat::Yuv444,
    }
}

fn pixel_format_and_centered_to_chroma_sample_position(
    pixel_format: PixelFormat,
    chroma_is_horizontally_centered: bool,
    chroma_is_vertically_centered: bool,
) -> Result<ChromaSamplePosition, AvifError> {
    match (
        pixel_format,
        chroma_is_horizontally_centered,
        chroma_is_vertically_centered,
    ) {
        (PixelFormat::Yuv420, false, false) => Ok(ChromaSamplePosition::Colocated),
        (PixelFormat::Yuv420, false, true) => Ok(ChromaSamplePosition::Vertical),
        // There is no way to describe this with AV1's chroma_sample_position enum besides CSP_UNKNOWN.
        // There is a proposal to assign the reserved value 3 (CSP_RESERVED) to the center chroma sample position.
        (PixelFormat::Yuv420, true, _) => Ok(ChromaSamplePosition::Unknown),

        // chroma_is_vertically_centered is ignored unless chroma_subsampling is 1.
        // In AV1, the chroma_sample_position syntax element is not present for the YUV 4:2:2 format.
        // Assume that AV1 uses the same 4:2:2 chroma sample location as HEVC and VVC (colocated).
        (PixelFormat::Yuv422, false, _) => Ok(ChromaSamplePosition::Unknown),
        (PixelFormat::Yuv422, true, _) => {
            AvifError::bmff_parse_failed("chroma_is_horizontally_centered should be false")
        }

        // chroma_is_horizontally_centered is ignored unless chroma_subsampling is 1 or 2.
        // chroma_is_vertically_centered is ignored unless chroma_subsampling is 1.
        (PixelFormat::Yuv400 | PixelFormat::Yuv444, _, _) => Ok(ChromaSamplePosition::Unknown),

        // Should not happen.
        (
            PixelFormat::None
            | PixelFormat::AndroidNv12
            | PixelFormat::AndroidNv21
            | PixelFormat::AndroidP010,
            _,
            _,
        ) => AvifError::bmff_parse_failed(format!("Unexpected pixel format {pixel_format:?}")),
    }
}

fn check_subsampling(
    chroma_subsampling: u32,
    chroma_is_horizontally_centered: bool,
    chroma_is_vertically_centered: bool,
    codec_config: &Av1CodecConfiguration,
) -> Result<(), AvifError> {
    let pixel_format = chroma_subsampling_to_pixel_format(chroma_subsampling);
    let chroma_sample_position = pixel_format_and_centered_to_chroma_sample_position(
        pixel_format,
        chroma_is_horizontally_centered,
        chroma_is_vertically_centered,
    )?;
    if pixel_format != CodecConfiguration::Av1(codec_config.clone()).pixel_format()
        || chroma_sample_position != codec_config.chroma_sample_position
    {
        return AvifError::bmff_parse_failed("Mismatch between mini and AV1 codec config");
    }
    Ok(())
}
