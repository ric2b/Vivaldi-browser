// Copyright 2024 Google LLC
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

use crate::decoder::gainmap::GainMapMetadata;
use crate::decoder::track::*;
use crate::decoder::Extent;
use crate::decoder::GenericIO;
use crate::image::YuvRange;
use crate::image::MAX_PLANE_COUNT;
use crate::internal_utils::stream::*;
use crate::internal_utils::*;
use crate::utils::clap::CleanAperture;
use crate::*;

#[derive(Debug, PartialEq)]
pub enum BoxSize {
    FixedSize(usize), // In bytes, header exclusive.
    UntilEndOfStream, // The box goes on until the end of the input stream.
}

#[derive(Debug)]
struct BoxHeader {
    size: BoxSize,
    box_type: String,
}

impl BoxHeader {
    fn size(&self) -> usize {
        match self.size {
            BoxSize::FixedSize(size) => size, // not reached.
            BoxSize::UntilEndOfStream => 0,
        }
    }
}

#[derive(Debug)]
pub struct FileTypeBox {
    pub major_brand: String,
    // minor_version "is informative only" (section 4.3.1 of ISO/IEC 14496-12)
    compatible_brands: Vec<String>,
}

impl FileTypeBox {
    fn has_brand(&self, brand: &str) -> bool {
        // As of 2024, section 4.3.1 of ISO/IEC 14496-12 does not explictly say that the file is
        // compliant with the specification defining the major brand, but "the major_brand should be
        // repeated in the compatible_brands". Later versions of the specification may explicitly
        // consider the major brand as one of the compatible brands, even if not repeated.
        if self.major_brand.as_str() == brand {
            return true;
        }
        self.compatible_brands.iter().any(|x| x.as_str() == brand)
    }

    fn has_brand_any(&self, brands: &[&str]) -> bool {
        brands.iter().any(|brand| self.has_brand(brand))
    }

    pub fn is_avif(&self) -> bool {
        // "avio" also exists but does not identify the file as AVIF on its own. See
        // https://aomediacodec.github.io/av1-avif/v1.1.0.html#image-and-image-collection-brand
        self.has_brand_any(&[
            "avif",
            "avis",
            #[cfg(feature = "heic")]
            "heic",
        ])
    }

    pub fn needs_meta(&self) -> bool {
        self.has_brand_any(&[
            "avif",
            #[cfg(feature = "heic")]
            "heic",
        ])
    }

    pub fn needs_moov(&self) -> bool {
        self.has_brand_any(&[
            "avis",
            #[cfg(feature = "heic")]
            "hevc",
            #[cfg(feature = "heic")]
            "msf1",
        ])
    }

    pub fn has_tmap(&self) -> bool {
        self.has_brand("tmap")
    }
}

#[derive(Debug, Default)]
pub struct ItemLocationEntry {
    pub item_id: u32,
    pub construction_method: u8,
    pub base_offset: u64,
    pub extent_count: u16,
    pub extents: Vec<Extent>,
}

#[derive(Debug, Default)]
pub struct ItemLocationBox {
    offset_size: u8,
    length_size: u8,
    base_offset_size: u8,
    index_size: u8,
    pub items: Vec<ItemLocationEntry>,
}

#[derive(Clone, Debug)]
pub struct ImageSpatialExtents {
    pub width: u32,
    pub height: u32,
}

#[derive(Clone, Debug, Default)]
pub struct PixelInformation {
    pub plane_depths: Vec<u8>,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Av1CodecConfiguration {
    pub seq_profile: u8,
    pub seq_level_idx0: u8,
    pub seq_tier0: u8,
    pub high_bitdepth: bool,
    pub twelve_bit: bool,
    pub monochrome: bool,
    pub chroma_subsampling_x: u8,
    pub chroma_subsampling_y: u8,
    pub chroma_sample_position: ChromaSamplePosition,
    pub raw_data: Vec<u8>,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct HevcCodecConfiguration {
    pub bitdepth: u8,
    pub nal_length_size: u8,
    pub vps: Vec<u8>,
    pub sps: Vec<u8>,
    pub pps: Vec<u8>,
}

impl CodecConfiguration {
    pub fn depth(&self) -> u8 {
        match self {
            Self::Av1(config) => match config.twelve_bit {
                true => 12,
                false => match config.high_bitdepth {
                    true => 10,
                    false => 8,
                },
            },
            Self::Hevc(config) => config.bitdepth,
        }
    }

    pub fn pixel_format(&self) -> PixelFormat {
        match self {
            Self::Av1(config) => {
                if config.monochrome {
                    PixelFormat::Yuv400
                } else if config.chroma_subsampling_x == 1 && config.chroma_subsampling_y == 1 {
                    PixelFormat::Yuv420
                } else if config.chroma_subsampling_x == 1 {
                    PixelFormat::Yuv422
                } else {
                    PixelFormat::Yuv444
                }
            }
            Self::Hevc(_) => {
                // It is okay to always return Yuv420 here since that is the only format that
                // android_mediacodec returns.
                // TODO: b/370549923 - Identify the correct YUV subsampling type from the codec
                // configuration data.
                PixelFormat::Yuv420
            }
        }
    }

    pub fn chroma_sample_position(&self) -> ChromaSamplePosition {
        match self {
            Self::Av1(config) => config.chroma_sample_position,
            Self::Hevc(_) => {
                // It is okay to always return ChromaSamplePosition::default() here since that is
                // the only format that android_mediacodec returns.
                // TODO: b/370549923 - Identify the correct chroma sample position from the codec
                // configuration data.
                ChromaSamplePosition::default()
            }
        }
    }

    pub fn raw_data(&self) -> Vec<u8> {
        match self {
            Self::Av1(config) => config.raw_data.clone(),
            Self::Hevc(config) => {
                // For HEVC, the codec specific data consists of the following 3 NAL units in
                // order: VPS, SPS and PPS. Each unit should be preceded by a start code of
                // "\x00\x00\x00\x01".
                // https://developer.android.com/reference/android/media/MediaCodec#CSD
                let mut data: Vec<u8> = Vec::new();
                for nal_unit in [&config.vps, &config.sps, &config.pps] {
                    // Start code.
                    data.extend_from_slice(&[0, 0, 0, 1]);
                    // Data.
                    data.extend_from_slice(&nal_unit[..]);
                }
                data
            }
        }
    }

    pub fn profile(&self) -> u8 {
        match self {
            Self::Av1(config) => config.seq_profile,
            Self::Hevc(_) => {
                // TODO: b/370549923 - Identify the correct profile from the codec configuration
                // data.
                0
            }
        }
    }

    pub fn nal_length_size(&self) -> u8 {
        match self {
            Self::Av1(_) => 0, // Unused. This function is only used for HEVC.
            Self::Hevc(config) => config.nal_length_size,
        }
    }

    pub fn is_avif(&self) -> bool {
        matches!(self, Self::Av1(_))
    }

    pub fn is_heic(&self) -> bool {
        matches!(self, Self::Hevc(_))
    }
}

#[derive(Clone, Debug, Default)]
pub struct Nclx {
    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,
    pub yuv_range: YuvRange,
}

#[derive(Clone, Debug)]
pub enum ColorInformation {
    Icc(Vec<u8>),
    Nclx(Nclx),
    Unknown,
}

/// cbindgen:rename-all=CamelCase
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[repr(C)]
pub struct PixelAspectRatio {
    pub h_spacing: u32,
    pub v_spacing: u32,
}

/// cbindgen:field-names=[maxCLL, maxPALL]
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ContentLightLevelInformation {
    pub max_cll: u16,
    pub max_pall: u16,
}

#[derive(Clone, Debug, PartialEq)]
pub enum CodecConfiguration {
    Av1(Av1CodecConfiguration),
    Hevc(HevcCodecConfiguration),
}

impl Default for CodecConfiguration {
    fn default() -> Self {
        Self::Av1(Av1CodecConfiguration::default())
    }
}

#[derive(Clone, Debug)]
pub enum ItemProperty {
    ImageSpatialExtents(ImageSpatialExtents),
    PixelInformation(PixelInformation),
    CodecConfiguration(CodecConfiguration),
    ColorInformation(ColorInformation),
    PixelAspectRatio(PixelAspectRatio),
    AuxiliaryType(String),
    CleanAperture(CleanAperture),
    ImageRotation(u8),
    ImageMirror(u8),
    OperatingPointSelector(u8),
    LayerSelector(u16),
    AV1LayeredImageIndexing([usize; 3]),
    ContentLightLevelInformation(ContentLightLevelInformation),
    Unknown(String),
}

// Section 8.11.14 of ISO/IEC 14496-12.
#[derive(Debug, Default)]
pub struct ItemPropertyAssociation {
    pub item_id: u32,
    pub associations: Vec<(
        u16,  // 1-based property_index
        bool, // essential
    )>,
}

#[derive(Debug, Default)]
pub struct ItemInfo {
    pub item_id: u32,
    item_protection_index: u16,
    pub item_type: String,
    item_name: String,
    pub content_type: String,
}

#[derive(Debug, Default)]
pub struct ItemPropertyBox {
    pub properties: Vec<ItemProperty>,
    pub associations: Vec<ItemPropertyAssociation>,
}

#[derive(Debug)]
pub struct ItemReference {
    // Read this reference as "{from_item_id} is a {reference_type} for {to_item_id}"
    // (except for dimg where it is in the opposite direction).
    pub from_item_id: u32,
    pub to_item_id: u32,
    pub reference_type: String,
    pub index: u32, // 0-based index of the reference within the iref type.
}

#[derive(Debug, Default)]
pub struct MetaBox {
    pub iinf: Vec<ItemInfo>,
    pub iloc: ItemLocationBox,
    pub primary_item_id: u32, // pitm
    pub iprp: ItemPropertyBox,
    pub iref: Vec<ItemReference>,
    pub idat: Vec<u8>,
}

#[derive(Debug)]
pub struct AvifBoxes {
    pub ftyp: FileTypeBox,
    pub meta: MetaBox,
    pub tracks: Vec<Track>,
}

fn parse_header(stream: &mut IStream, top_level: bool) -> AvifResult<BoxHeader> {
    // Section 4.2.2 of ISO/IEC 14496-12.
    let start_offset = stream.offset;
    // unsigned int(32) size;
    let mut size = stream.read_u32()? as u64;
    // unsigned int(32) type = boxtype;
    let box_type = stream.read_string(4)?;
    if size == 1 {
        // unsigned int(64) largesize;
        size = stream.read_u64()?;
    }
    if box_type == "uuid" {
        // unsigned int(8) usertype[16] = extended_type;
        stream.skip(16)?;
    }
    if size == 0 {
        // Section 4.2.2 of ISO/IEC 14496-12.
        //   if size is 0, then this box shall be in a top-level box (i.e. not contained in another
        //   box), and be the last box in its 'file', and its payload extends to the end of that
        //   enclosing 'file'. This is normally only used for a MediaDataBox.
        if !top_level {
            return Err(AvifError::BmffParseFailed(
                "non-top-level box with size 0".into(),
            ));
        }
        return Ok(BoxHeader {
            box_type,
            size: BoxSize::UntilEndOfStream,
        });
    }
    checked_decr!(size, u64_from_usize(stream.offset - start_offset)?);
    let size = usize_from_u64(size)?;
    if !top_level && size > stream.bytes_left()? {
        return Err(AvifError::BmffParseFailed("possibly truncated box".into()));
    }
    Ok(BoxHeader {
        box_type,
        size: BoxSize::FixedSize(size),
    })
}

fn parse_ftyp(stream: &mut IStream) -> AvifResult<FileTypeBox> {
    // Section 4.3.2 of ISO/IEC 14496-12.
    let major_brand = stream.read_string(4)?;
    // unsigned int(4) minor_version;
    stream.skip_u32()?;
    if stream.bytes_left()? % 4 != 0 {
        return Err(AvifError::BmffParseFailed(format!(
            "Box[ftyp] contains a compatible brands section that isn't divisible by 4 {}",
            stream.bytes_left()?
        )));
    }
    let mut compatible_brands: Vec<String> = create_vec_exact(stream.bytes_left()? / 4)?;
    while stream.has_bytes_left()? {
        compatible_brands.push(stream.read_string(4)?);
    }
    Ok(FileTypeBox {
        major_brand,
        compatible_brands,
    })
}

fn parse_hdlr(stream: &mut IStream) -> AvifResult<()> {
    // Section 8.4.3.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) pre_defined = 0;
    let predefined = stream.read_u32()?;
    if predefined != 0 {
        return Err(AvifError::BmffParseFailed(
            "Box[hdlr] contains a pre_defined value that is nonzero".into(),
        ));
    }
    // unsigned int(32) handler_type;
    let handler_type = stream.read_string(4)?;
    if handler_type != "pict" {
        // Section 6.2 of ISO/IEC 23008-12:
        //   The handler type for the MetaBox shall be 'pict'.
        // https://aomediacodec.github.io/av1-avif/v1.1.0.html#image-sequences does not apply
        // because this function is only called for the MetaBox but it would work too:
        //   The track handler for an AV1 Image Sequence shall be pict.
        return Err(AvifError::BmffParseFailed(
            "Box[hdlr] handler_type is not 'pict'".into(),
        ));
    }
    // const unsigned int(32)[3] reserved = 0;
    if stream.read_u32()? != 0 || stream.read_u32()? != 0 || stream.read_u32()? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Box[hdlr] contains invalid reserved bits".into(),
        ));
    }
    // string name;
    // Verify that a valid string is here, but don't bother to store it:
    //   name gives a human-readable name for the track type (for debugging and inspection
    //   purposes).
    stream.read_c_string()?;
    Ok(())
}

fn parse_iloc(stream: &mut IStream) -> AvifResult<ItemLocationBox> {
    // Section 8.11.3.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version > 2 {
        return Err(AvifError::BmffParseFailed(format!(
            "Box[iloc] has an unsupported version: {version}"
        )));
    }
    let mut iloc = ItemLocationBox::default();
    let mut bits = stream.sub_bit_stream(2)?;
    // unsigned int(4) offset_size;
    iloc.offset_size = bits.read(4)? as u8;
    // unsigned int(4) length_size;
    iloc.length_size = bits.read(4)? as u8;
    // unsigned int(4) base_offset_size;
    iloc.base_offset_size = bits.read(4)? as u8;
    iloc.index_size = if version == 1 || version == 2 {
        // unsigned int(4) index_size;
        bits.read(4)? as u8
    } else {
        // unsigned int(4) reserved;
        bits.skip(4)?;
        0
    };
    assert_eq!(bits.remaining_bits()?, 0);

    // Section 8.11.3.3 of ISO/IEC 14496-12.
    for size in [
        iloc.offset_size,
        iloc.length_size,
        iloc.base_offset_size,
        iloc.index_size,
    ] {
        if ![0u8, 4, 8].contains(&size) {
            return Err(AvifError::BmffParseFailed(format!(
                "Box[iloc] has invalid size: {size}"
            )));
        }
    }

    let item_count: u32 = if version < 2 {
        // unsigned int(16) item_count;
        stream.read_u16()? as u32
    } else {
        // unsigned int(32) item_count;
        stream.read_u32()?
    };
    for _i in 0..item_count {
        let mut entry = ItemLocationEntry {
            item_id: if version < 2 {
                // unsigned int(16) item_ID;
                stream.read_u16()? as u32
            } else {
                // unsigned int(32) item_ID;
                stream.read_u32()?
            },
            ..ItemLocationEntry::default()
        };
        if entry.item_id == 0 {
            return Err(AvifError::BmffParseFailed(format!(
                "Box[iloc] has invalid item id: {}",
                entry.item_id
            )));
        }
        if version == 1 || version == 2 {
            let mut bits = stream.sub_bit_stream(2)?;
            // unsigned int(12) reserved = 0;
            if bits.read(12)? != 0 {
                return Err(AvifError::BmffParseFailed(
                    "Box[iloc] has invalid reserved bits".into(),
                ));
            }
            // unsigned int(4) construction_method;
            entry.construction_method = bits.read(4)? as u8;
            // 0: file offset, 1: idat offset, 2: item offset.
            if entry.construction_method != 0 && entry.construction_method != 1 {
                return Err(AvifError::BmffParseFailed(format!(
                    "Box[iloc] has unknown construction_method: {}",
                    entry.construction_method
                )));
            }
        }
        // unsigned int(16) data_reference_index;
        stream.skip(2)?;
        // unsigned int(base_offset_size*8) base_offset;
        entry.base_offset = stream.read_uxx(iloc.base_offset_size)?;
        // unsigned int(16) extent_count;
        entry.extent_count = stream.read_u16()?;
        for _j in 0..entry.extent_count {
            // unsigned int(index_size*8) item_reference_index;
            stream.skip(iloc.index_size as usize)?; // Only used for construction_method 2.
            let extent = Extent {
                // unsigned int(offset_size*8) extent_offset;
                offset: stream.read_uxx(iloc.offset_size)?,
                // unsigned int(length_size*8) extent_length;
                size: usize_from_u64(stream.read_uxx(iloc.length_size)?)?,
            };
            entry.extents.push(extent);
        }
        iloc.items.push(entry);
    }
    Ok(iloc)
}

// Returns the primary item ID.
fn parse_pitm(stream: &mut IStream) -> AvifResult<u32> {
    // Section 8.11.4.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version == 0 {
        // unsigned int(16) item_ID;
        Ok(stream.read_u16()? as u32)
    } else {
        // unsigned int(32) item_ID;
        Ok(stream.read_u32()?)
    }
}

fn parse_ispe(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.3.2 of ISO/IEC 23008-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    let ispe = ImageSpatialExtents {
        // unsigned int(32) image_width;
        width: stream.read_u32()?,
        // unsigned int(32) image_height;
        height: stream.read_u32()?,
    };
    Ok(ItemProperty::ImageSpatialExtents(ispe))
}

fn parse_pixi(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.6.2 of ISO/IEC 23008-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int (8) num_channels;
    let num_channels = stream.read_u8()? as usize;
    if num_channels == 0 || num_channels > MAX_PLANE_COUNT {
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid plane count {num_channels} in pixi box"
        )));
    }
    let mut pixi = PixelInformation {
        plane_depths: create_vec_exact(num_channels)?,
    };
    for _ in 0..num_channels {
        // unsigned int (8) bits_per_channel;
        pixi.plane_depths.push(stream.read_u8()?);
        if pixi.plane_depths.last().unwrap() != pixi.plane_depths.first().unwrap() {
            return Err(AvifError::UnsupportedDepth);
        }
    }
    Ok(ItemProperty::PixelInformation(pixi))
}

#[allow(non_snake_case)]
fn parse_av1C(stream: &mut IStream) -> AvifResult<ItemProperty> {
    let raw_data = stream.get_immutable_vec(stream.bytes_left()?)?;
    // See https://aomediacodec.github.io/av1-isobmff/v1.2.0.html#av1codecconfigurationbox-syntax.
    let mut bits = stream.sub_bit_stream(4)?;
    // unsigned int (1) marker = 1;
    let marker = bits.read(1)?;
    if marker != 1 {
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid marker ({marker}) in av1C"
        )));
    }
    // unsigned int (7) version = 1;
    let version = bits.read(7)?;
    if version != 1 {
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid version ({version}) in av1C"
        )));
    }
    let av1C = Av1CodecConfiguration {
        // unsigned int(3) seq_profile;
        // unsigned int(5) seq_level_idx_0;
        seq_profile: bits.read(3)? as u8,
        seq_level_idx0: bits.read(5)? as u8,
        // unsigned int(1) seq_tier_0;
        // unsigned int(1) high_bitdepth;
        // unsigned int(1) twelve_bit;
        // unsigned int(1) monochrome;
        // unsigned int(1) chroma_subsampling_x;
        // unsigned int(1) chroma_subsampling_y;
        // unsigned int(2) chroma_sample_position;
        seq_tier0: bits.read(1)? as u8,
        high_bitdepth: bits.read_bool()?,
        twelve_bit: bits.read_bool()?,
        monochrome: bits.read_bool()?,
        chroma_subsampling_x: bits.read(1)? as u8,
        chroma_subsampling_y: bits.read(1)? as u8,
        chroma_sample_position: bits.read(2)?.into(),
        raw_data,
    };

    // unsigned int(3) reserved = 0;
    if bits.read(3)? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in av1C".into(),
        ));
    }
    // unsigned int(1) initial_presentation_delay_present;
    if bits.read(1)? == 1 {
        // unsigned int(4) initial_presentation_delay_minus_one;
        bits.read(4)?;
    } else {
        // unsigned int(4) reserved = 0;
        if bits.read(4)? != 0 {
            return Err(AvifError::BmffParseFailed(
                "Invalid reserved bits in av1C".into(),
            ));
        }
    }
    assert_eq!(bits.remaining_bits()?, 0);

    // https://aomediacodec.github.io/av1-avif/v1.1.0.html#av1-configuration-item-property:
    //   - Sequence Header OBUs should not be present in the AV1CodecConfigurationBox.
    // This is ignored.
    //   - If a Sequence Header OBU is present in the AV1CodecConfigurationBox, it shall match the
    //     Sequence Header OBU in the AV1 Image Item Data.
    // This is not enforced.
    //   - The values of the fields in the AV1CodecConfigurationBox shall match those of the
    //     Sequence Header OBU in the AV1 Image Item Data.
    // This is not enforced (?).
    //   - Metadata OBUs, if present, shall match the values given in other item properties, such as
    //     the PixelInformationProperty or ColourInformationBox.
    // This is not enforced.

    // unsigned int(8) configOBUs[];

    Ok(ItemProperty::CodecConfiguration(CodecConfiguration::Av1(
        av1C,
    )))
}

#[allow(non_snake_case)]
#[cfg(feature = "heic")]
fn parse_hvcC(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // unsigned int(8) configurationVersion;
    let configuration_version = stream.read_u8()?;
    if configuration_version != 0 && configuration_version != 1 {
        return Err(AvifError::BmffParseFailed(format!(
            "Unknown configurationVersion({configuration_version}) in hvcC. Expected 0 or 1."
        )));
    }
    let mut bits = stream.sub_bit_stream(21)?;
    // unsigned int(2) general_profile_space;
    // unsigned int(1) general_tier_flag;
    // unsigned int(5) general_profile_idc;
    // unsigned int(32) general_profile_compatibility_flags;
    // unsigned int(48) general_constraint_indicator_flags;
    // unsigned int(8) general_level_idc;
    // bit(4) reserved = '1111'b;
    // unsigned int(12) min_spatial_segmentation_idc;
    // bit(6) reserved = '111111'b;
    // unsigned int(2) parallelismType;
    // bit(6) reserved = '111111'b;
    // unsigned int(2) chroma_format_idc;
    // bit(5) reserved = '11111'b;
    bits.skip(2 + 1 + 5 + 32 + 48 + 8 + 4 + 12 + 6 + 2 + 6 + 2 + 5)?;
    // unsigned int(3) bit_depth_luma_minus8;
    let bitdepth = bits.read(3)? as u8 + 8;
    // bit(5) reserved = '11111'b;
    // unsigned int(3) bit_depth_chroma_minus8;
    // unsigned int(16) avgFrameRate;
    // unsigned int(2) constantFrameRate;
    // unsigned int(3) numTemporalLayers;
    // unsigned int(1) temporalIdNested;
    bits.skip(5 + 3 + 16 + 2 + 3 + 1)?;
    // unsigned int(2) lengthSizeMinusOne;
    let nal_length_size = 1 + bits.read(2)? as u8;
    assert!(bits.remaining_bits()? == 0);

    // unsigned int(8) numOfArrays;
    let num_of_arrays = stream.read_u8()?;
    let mut vps: Vec<u8> = Vec::new();
    let mut sps: Vec<u8> = Vec::new();
    let mut pps: Vec<u8> = Vec::new();
    for _i in 0..num_of_arrays {
        // unsigned int(1) array_completeness;
        // bit(1) reserved = 0;
        // unsigned int(6) NAL_unit_type;
        stream.skip(1)?;
        // unsigned int(16) numNalus;
        let num_nalus = stream.read_u16()?;
        for _j in 0..num_nalus {
            // unsigned int(16) nalUnitLength;
            let nal_unit_length = stream.read_u16()?;
            let nal_unit = stream.get_slice(nal_unit_length as usize)?;
            let nal_unit_type = (nal_unit[0] >> 1) & 0x3f;
            match nal_unit_type {
                32 => vps = nal_unit.to_vec(),
                33 => sps = nal_unit.to_vec(),
                34 => pps = nal_unit.to_vec(),
                _ => {}
            }
        }
    }
    Ok(ItemProperty::CodecConfiguration(CodecConfiguration::Hevc(
        HevcCodecConfiguration {
            bitdepth,
            nal_length_size,
            vps,
            pps,
            sps,
        },
    )))
}

fn parse_colr(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 12.1.5.2 of ISO/IEC 14496-12.

    // unsigned int(32) colour_type;
    let color_type = stream.read_string(4)?;
    if color_type == "rICC" || color_type == "prof" {
        if stream.bytes_left()? == 0 {
            // Section 12.1.5.3 of ISO/IEC 14496-12:
            //   ICC_profile: an ICC profile as defined in ISO 15076-1 or ICC.1 is supplied.
            // Section 7.2.1 of ICC.1:2010:
            //   The profile header is 128 bytes in length and contains 18 fields.
            // So an empty ICC profile is invalid.
            return Err(AvifError::BmffParseFailed(format!(
                "colr box contains 0 bytes of {color_type}"
            )));
        }
        // ICC_profile; // restricted ("rICC") or unrestricted ("prof") ICC profile
        return Ok(ItemProperty::ColorInformation(ColorInformation::Icc(
            stream.get_slice(stream.bytes_left()?)?.to_vec(),
        )));
    }
    if color_type == "nclx" {
        let mut nclx = Nclx {
            // unsigned int(16) colour_primaries;
            color_primaries: stream.read_u16()?.into(),
            // unsigned int(16) transfer_characteristics;
            transfer_characteristics: stream.read_u16()?.into(),
            // unsigned int(16) matrix_coefficients;
            matrix_coefficients: stream.read_u16()?.into(),
            ..Nclx::default()
        };
        let mut bits = stream.sub_bit_stream(1)?;
        // unsigned int(1) full_range_flag;
        nclx.yuv_range = if bits.read_bool()? { YuvRange::Full } else { YuvRange::Limited };
        // unsigned int(7) reserved = 0;
        if bits.read(7)? != 0 {
            return Err(AvifError::BmffParseFailed(
                "colr box contains invalid reserved bits".into(),
            ));
        }
        return Ok(ItemProperty::ColorInformation(ColorInformation::Nclx(nclx)));
    }
    Ok(ItemProperty::ColorInformation(ColorInformation::Unknown))
}

fn parse_pasp(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 12.1.4.2 of ISO/IEC 14496-12.
    let pasp = PixelAspectRatio {
        // unsigned int(32) hSpacing;
        h_spacing: stream.read_u32()?,
        // unsigned int(32) vSpacing;
        v_spacing: stream.read_u32()?,
    };
    Ok(ItemProperty::PixelAspectRatio(pasp))
}

#[allow(non_snake_case)]
fn parse_auxC(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.8.2 of ISO/IEC 23008-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // string aux_type;
    let auxiliary_type = stream.read_c_string()?;
    // template unsigned int(8) aux_subtype[];
    // until the end of the box, the semantics depend on the aux_type value
    Ok(ItemProperty::AuxiliaryType(auxiliary_type))
}

fn parse_clap(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 12.1.4.2 of ISO/IEC 14496-12.
    let clap = CleanAperture {
        // unsigned int(32) cleanApertureWidthN;
        // unsigned int(32) cleanApertureWidthD;
        width: stream.read_ufraction()?,
        // unsigned int(32) cleanApertureHeightN;
        // unsigned int(32) cleanApertureHeightD;
        height: stream.read_ufraction()?,
        // unsigned int(32) horizOffN;
        // unsigned int(32) horizOffD;
        horiz_off: stream.read_ufraction()?,
        // unsigned int(32) vertOffN;
        // unsigned int(32) vertOffD;
        vert_off: stream.read_ufraction()?,
    };
    Ok(ItemProperty::CleanAperture(clap))
}

fn parse_irot(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.10.2 of ISO/IEC 23008-12.
    let mut bits = stream.sub_bit_stream(1)?;
    // unsigned int (6) reserved = 0;
    if bits.read(6)? != 0 {
        return Err(AvifError::BmffParseFailed(
            "invalid reserved bits in irot".into(),
        ));
    }
    // unsigned int (2) angle;
    let angle = bits.read(2)? as u8;
    Ok(ItemProperty::ImageRotation(angle))
}

fn parse_imir(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.12.1 of ISO/IEC 23008-12.
    let mut bits = stream.sub_bit_stream(1)?;
    // unsigned int(7) reserved = 0;
    if bits.read(7)? != 0 {
        return Err(AvifError::BmffParseFailed(
            "invalid reserved bits in imir".into(),
        ));
    }
    // unsigned int(1) axis;
    let axis = bits.read(1)? as u8;
    Ok(ItemProperty::ImageMirror(axis))
}

fn parse_a1op(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // https://aomediacodec.github.io/av1-avif/v1.1.0.html#operating-point-selector-property-syntax

    // unsigned int(8) op_index;
    let op_index = stream.read_u8()?;
    if op_index > 31 {
        // 31 is AV1's maximum operating point value (operating_points_cnt_minus_1).
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid op_index ({op_index}) in a1op"
        )));
    }
    Ok(ItemProperty::OperatingPointSelector(op_index))
}

fn parse_lsel(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 6.5.11.1 of ISO/IEC 23008-12.

    // unsigned int(16) layer_id;
    let layer_id = stream.read_u16()?;

    // https://aomediacodec.github.io/av1-avif/v1.1.0.html#layer-selector-property:
    //   The layer_id indicates the value of the spatial_id to render. The value shall be between 0
    //   and 3, or the special value 0xFFFF.
    if layer_id != 0xFFFF && layer_id >= 4 {
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid layer_id ({layer_id}) in lsel"
        )));
    }
    Ok(ItemProperty::LayerSelector(layer_id))
}

fn parse_a1lx(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // https://aomediacodec.github.io/av1-avif/v1.1.0.html#layered-image-indexing-property-syntax
    let mut bits = stream.sub_bit_stream(1)?;
    // unsigned int(7) reserved = 0;
    if bits.read(7)? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in a1lx".into(),
        ));
    }
    // unsigned int(1) large_size;
    let large_size = bits.read_bool()?;
    let mut layer_sizes: [usize; 3] = [0; 3];
    for layer_size in &mut layer_sizes {
        if large_size {
            *layer_size = usize_from_u32(stream.read_u32()?)?;
        } else {
            *layer_size = usize_from_u16(stream.read_u16()?)?;
        }
    }
    Ok(ItemProperty::AV1LayeredImageIndexing(layer_sizes))
}

fn parse_clli(stream: &mut IStream) -> AvifResult<ItemProperty> {
    // Section 12.1.6.2 of ISO/IEC 14496-12.
    let clli = ContentLightLevelInformation {
        // unsigned int(16) max_content_light_level
        max_cll: stream.read_u16()?,
        // unsigned int(16) max_pic_average_light_level
        max_pall: stream.read_u16()?,
    };
    Ok(ItemProperty::ContentLightLevelInformation(clli))
}

fn parse_ipco(stream: &mut IStream) -> AvifResult<Vec<ItemProperty>> {
    // Section 8.11.14.2 of ISO/IEC 14496-12.
    let mut properties: Vec<ItemProperty> = Vec::new();
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "ispe" => properties.push(parse_ispe(&mut sub_stream)?),
            "pixi" => properties.push(parse_pixi(&mut sub_stream)?),
            "av1C" => properties.push(parse_av1C(&mut sub_stream)?),
            "colr" => properties.push(parse_colr(&mut sub_stream)?),
            "pasp" => properties.push(parse_pasp(&mut sub_stream)?),
            "auxC" => properties.push(parse_auxC(&mut sub_stream)?),
            "clap" => properties.push(parse_clap(&mut sub_stream)?),
            "irot" => properties.push(parse_irot(&mut sub_stream)?),
            "imir" => properties.push(parse_imir(&mut sub_stream)?),
            "a1op" => properties.push(parse_a1op(&mut sub_stream)?),
            "lsel" => properties.push(parse_lsel(&mut sub_stream)?),
            "a1lx" => properties.push(parse_a1lx(&mut sub_stream)?),
            "clli" => properties.push(parse_clli(&mut sub_stream)?),
            #[cfg(feature = "heic")]
            "hvcC" => properties.push(parse_hvcC(&mut sub_stream)?),
            _ => properties.push(ItemProperty::Unknown(header.box_type)),
        }
    }
    Ok(properties)
}

fn parse_ipma(stream: &mut IStream) -> AvifResult<Vec<ItemPropertyAssociation>> {
    // Section 8.11.14.2 of ISO/IEC 14496-12.
    let (version, flags) = stream.read_version_and_flags()?;
    // unsigned int(32) entry_count;
    let entry_count = stream.read_u32()?;
    let mut ipma: Vec<ItemPropertyAssociation> = create_vec_exact(usize_from_u32(entry_count)?)?;
    for _i in 0..entry_count {
        let mut entry = ItemPropertyAssociation::default();
        // ISO/IEC 23008-12, First edition, 2017-12, Section 9.3.1:
        //   Each ItemPropertyAssociation box shall be ordered by increasing item_ID, and there
        //   shall be at most one association box for each item_ID, in any
        //   ItemPropertyAssociation box.
        if version < 1 {
            // unsigned int(16) item_ID;
            entry.item_id = stream.read_u16()? as u32;
        } else {
            // unsigned int(32) item_ID;
            entry.item_id = stream.read_u32()?;
        }
        if entry.item_id == 0 {
            return Err(AvifError::BmffParseFailed(format!(
                "invalid item id ({}) in ipma",
                entry.item_id
            )));
        }
        if !ipma.is_empty() {
            let previous_item_id = ipma.last().unwrap().item_id;
            if entry.item_id <= previous_item_id {
                return Err(AvifError::BmffParseFailed(
                    "ipma item ids are not ordered by increasing id".into(),
                ));
            }
        }
        // unsigned int(8) association_count;
        let association_count = stream.read_u8()?;
        for _j in 0..association_count {
            let mut bits = stream.sub_bit_stream(if flags & 0x1 == 1 { 2 } else { 1 })?;
            // bit(1) essential;
            let essential = bits.read_bool()?;
            if flags & 0x1 == 1 {
                // unsigned int(15) property_index;
                entry.associations.push((bits.read(15)? as u16, essential));
            } else {
                //unsigned int(7) property_index;
                entry.associations.push((bits.read(7)? as u16, essential));
            }
        }
        ipma.push(entry);
    }
    Ok(ipma)
}

fn parse_iprp(stream: &mut IStream) -> AvifResult<ItemPropertyBox> {
    // Section 8.11.14.2 of ISO/IEC 14496-12.
    let header = parse_header(stream, /*top_level=*/ false)?;
    if header.box_type != "ipco" {
        return Err(AvifError::BmffParseFailed(
            "First box in iprp is not ipco".into(),
        ));
    }
    let mut iprp = ItemPropertyBox::default();
    // Parse ipco box.
    {
        let mut sub_stream = stream.sub_stream(&header.size)?;
        iprp.properties = parse_ipco(&mut sub_stream)?;
    }
    // Parse ipma boxes.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        if header.box_type != "ipma" {
            return Err(AvifError::BmffParseFailed(
                "Found non ipma box in iprp".into(),
            ));
        }
        let mut sub_stream = stream.sub_stream(&header.size)?;
        iprp.associations.append(&mut parse_ipma(&mut sub_stream)?);
    }
    Ok(iprp)
}

fn parse_infe(stream: &mut IStream) -> AvifResult<ItemInfo> {
    // Section 8.11.6.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version != 2 && version != 3 {
        return Err(AvifError::BmffParseFailed(
            "infe box version 2 or 3 expected.".into(),
        ));
    }

    // TODO: check flags. ISO/IEC 23008-12:2017, Section 9.2 says:
    // The flags field of ItemInfoEntry with version greater than or equal to 2 is specified
    // as follows:
    //   (flags & 1) equal to 1 indicates that the item is not intended to be a part of the
    //   presentation. For example, when (flags & 1) is equal to 1 for an image item, the
    //   image item should not be displayed. (flags & 1) equal to 0 indicates that the item
    //   is intended to be a part of the presentation.
    //
    // See also Section 6.4.2.
    let mut entry = ItemInfo::default();
    if version == 2 {
        // unsigned int(16) item_ID;
        entry.item_id = stream.read_u16()? as u32;
    } else {
        // unsigned int(32) item_ID;
        entry.item_id = stream.read_u32()?;
    }
    if entry.item_id == 0 {
        return Err(AvifError::BmffParseFailed(format!(
            "Invalid item id ({}) found in infe",
            entry.item_id
        )));
    }
    // unsigned int(16) item_protection_index;
    entry.item_protection_index = stream.read_u16()?;
    // unsigned int(32) item_type;
    entry.item_type = stream.read_string(4)?;

    // utf8string item_name;
    entry.item_name = stream.read_c_string()?;

    if entry.item_type == "mime" {
        // utf8string content_type;
        entry.content_type = stream.read_c_string()?;
        // utf8string content_encoding; //optional
    }
    // if (item_type == 'uri ') {
    //  utf8string item_uri_type;
    // }
    Ok(entry)
}

fn parse_iinf(stream: &mut IStream) -> AvifResult<Vec<ItemInfo>> {
    // Section 8.11.6.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version > 1 {
        return Err(AvifError::BmffParseFailed(format!(
            "Unsupported version {} in iinf box",
            version
        )));
    }
    let entry_count: u32 = if version == 0 {
        // unsigned int(16) entry_count;
        stream.read_u16()? as u32
    } else {
        // unsigned int(32) entry_count;
        stream.read_u32()?
    };
    let mut iinf: Vec<ItemInfo> = create_vec_exact(usize_from_u32(entry_count)?)?;
    for _i in 0..entry_count {
        let header = parse_header(stream, /*top_level=*/ false)?;
        if header.box_type != "infe" {
            return Err(AvifError::BmffParseFailed(
                "Found non infe box in iinf".into(),
            ));
        }
        let mut sub_stream = stream.sub_stream(&header.size)?;
        iinf.push(parse_infe(&mut sub_stream)?);
    }
    Ok(iinf)
}

fn parse_iref(stream: &mut IStream) -> AvifResult<Vec<ItemReference>> {
    // Section 8.11.12.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    let mut iref: Vec<ItemReference> = Vec::new();
    // versions > 1 are not supported. ignore them.
    if version > 1 {
        return Ok(iref);
    }
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let from_item_id: u32 = if version == 0 {
            // unsigned int(16) from_item_ID;
            stream.read_u16()? as u32
        } else {
            // unsigned int(32) from_item_ID;
            stream.read_u32()?
        };
        if from_item_id == 0 {
            return Err(AvifError::BmffParseFailed(
                "invalid from_item_id (0) in iref".into(),
            ));
        }
        // unsigned int(16) reference_count;
        let reference_count = stream.read_u16()?;
        for index in 0..reference_count {
            let to_item_id: u32 = if version == 0 {
                // unsigned int(16) to_item_ID;
                stream.read_u16()? as u32
            } else {
                // unsigned int(32) to_item_ID;
                stream.read_u32()?
            };
            if to_item_id == 0 {
                return Err(AvifError::BmffParseFailed(
                    "invalid to_item_id (0) in iref".into(),
                ));
            }
            iref.push(ItemReference {
                from_item_id,
                to_item_id,
                reference_type: header.box_type.clone(),
                index: index as u32,
            });
        }
    }
    Ok(iref)
}

fn parse_idat(stream: &mut IStream) -> AvifResult<Vec<u8>> {
    // Section 8.11.11.2 of ISO/IEC 14496-12.
    if !stream.has_bytes_left()? {
        return Err(AvifError::BmffParseFailed("Invalid idat size (0)".into()));
    }
    let mut idat: Vec<u8> = Vec::with_capacity(stream.bytes_left()?);
    idat.extend_from_slice(stream.get_slice(stream.bytes_left()?)?);
    Ok(idat)
}

fn parse_meta(stream: &mut IStream) -> AvifResult<MetaBox> {
    // Section 8.11.1.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    let mut meta = MetaBox::default();

    // Parse the first hdlr box.
    {
        let header = parse_header(stream, /*top_level=*/ false)?;
        if header.box_type != "hdlr" {
            return Err(AvifError::BmffParseFailed(
                "first box in meta is not hdlr".into(),
            ));
        }
        parse_hdlr(&mut stream.sub_stream(&header.size)?)?;
    }

    let mut boxes_seen: HashSet<String> = HashSet::with_hasher(NonRandomHasherState);
    boxes_seen.insert(String::from("hdlr"));
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        match header.box_type.as_str() {
            "hdlr" | "iloc" | "pitm" | "iprp" | "iinf" | "iref" | "idat" => {
                if boxes_seen.contains(&header.box_type) {
                    return Err(AvifError::BmffParseFailed(format!(
                        "duplicate {} box in meta.",
                        header.box_type
                    )));
                }
                boxes_seen.insert(header.box_type.clone());
            }
            _ => {}
        }
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "iloc" => meta.iloc = parse_iloc(&mut sub_stream)?,
            "pitm" => meta.primary_item_id = parse_pitm(&mut sub_stream)?,
            "iprp" => meta.iprp = parse_iprp(&mut sub_stream)?,
            "iinf" => meta.iinf = parse_iinf(&mut sub_stream)?,
            "iref" => meta.iref = parse_iref(&mut sub_stream)?,
            "idat" => meta.idat = parse_idat(&mut sub_stream)?,
            _ => {}
        }
    }
    Ok(meta)
}

fn parse_tkhd(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.3.2.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version == 1 {
        // unsigned int(64) creation_time;
        stream.skip_u64()?;
        // unsigned int(64) modification_time;
        stream.skip_u64()?;
        // unsigned int(32) track_ID;
        track.id = stream.read_u32()?;
        // const unsigned int(32) reserved = 0;
        if stream.read_u32()? != 0 {
            return Err(AvifError::BmffParseFailed(
                "Invalid reserved bits in tkhd".into(),
            ));
        }
        // unsigned int(64) duration;
        track.track_duration = stream.read_u64()?;
    } else if version == 0 {
        // unsigned int(32) creation_time;
        stream.skip_u32()?;
        // unsigned int(32) modification_time;
        stream.skip_u32()?;
        // unsigned int(32) track_ID;
        track.id = stream.read_u32()?;
        // const unsigned int(32) reserved = 0;
        if stream.read_u32()? != 0 {
            return Err(AvifError::BmffParseFailed(
                "Invalid reserved bits in tkhd".into(),
            ));
        }
        // unsigned int(32) duration;
        track.track_duration = stream.read_u32()? as u64;
    } else {
        return Err(AvifError::BmffParseFailed(format!(
            "unsupported version ({version}) in trak"
        )));
    }

    // const unsigned int(32)[2] reserved = 0;
    if stream.read_u32()? != 0 || stream.read_u32()? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in tkhd".into(),
        ));
    }
    // The following fields should be 0 but are ignored instead.
    // template int(16) layer = 0;
    stream.skip(2)?;
    // template int(16) alternate_group = 0;
    stream.skip(2)?;
    // template int(16) volume = {if track_is_audio 0x0100 else 0};
    stream.skip(2)?;
    // const unsigned int(16) reserved = 0;
    if stream.read_u16()? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in tkhd".into(),
        ));
    }
    // template int(32)[9] matrix= { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 }; // unity matrix
    stream.skip(4 * 9)?;

    // unsigned int(32) width;
    track.width = stream.read_u32()? >> 16;
    // unsigned int(32) height;
    track.height = stream.read_u32()? >> 16;

    if track.width == 0 || track.height == 0 {
        return Err(AvifError::BmffParseFailed(
            "invalid track dimensions".into(),
        ));
    }
    Ok(())
}

fn parse_mdhd(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.4.2.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version == 1 {
        // unsigned int(64) creation_time;
        stream.skip_u64()?;
        // unsigned int(64) modification_time;
        stream.skip_u64()?;
        // unsigned int(32) timescale;
        track.media_timescale = stream.read_u32()?;
        // unsigned int(64) duration;
        track.media_duration = stream.read_u64()?;
    } else if version == 0 {
        // unsigned int(32) creation_time;
        stream.skip_u32()?;
        // unsigned int(32) modification_time;
        stream.skip_u32()?;
        // unsigned int(32) timescale;
        track.media_timescale = stream.read_u32()?;
        // unsigned int(32) duration;
        track.media_duration = stream.read_u32()? as u64;
    } else {
        return Err(AvifError::BmffParseFailed(format!(
            "unsupported version ({version}) in mdhd"
        )));
    }

    let mut bits = stream.sub_bit_stream(4)?;
    // bit(1) pad = 0;
    if bits.read(1)? != 0 {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in mdhd".into(),
        ));
    }
    // unsigned int(5)[3] language; // ISO-639-2/T language code
    bits.skip(5 * 3)?;
    // unsigned int(16) pre_defined = 0; ("Readers should expect any value")
    bits.skip(2)?;
    Ok(())
}

fn parse_stco(
    stream: &mut IStream,
    sample_table: &mut SampleTable,
    large_offset: bool,
) -> AvifResult<()> {
    // Section 8.7.5.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) entry_count;
    let entry_count = usize_from_u32(stream.read_u32()?)?;
    sample_table.chunk_offsets = create_vec_exact(entry_count)?;
    for _ in 0..entry_count {
        let chunk_offset: u64 = if large_offset {
            // unsigned int(64) chunk_offset;
            stream.read_u64()?
        } else {
            // unsigned int(32) chunk_offset;
            stream.read_u32()? as u64
        };
        sample_table.chunk_offsets.push(chunk_offset);
    }
    Ok(())
}

fn parse_stsc(stream: &mut IStream, sample_table: &mut SampleTable) -> AvifResult<()> {
    // Section 8.7.4.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) entry_count;
    let entry_count = usize_from_u32(stream.read_u32()?)?;
    sample_table.sample_to_chunk = create_vec_exact(entry_count)?;
    for i in 0..entry_count {
        let stsc = SampleToChunk {
            // unsigned int(32) first_chunk;
            first_chunk: stream.read_u32()?,
            // unsigned int(32) samples_per_chunk;
            samples_per_chunk: stream.read_u32()?,
            // unsigned int(32) sample_description_index;
            sample_description_index: stream.read_u32()?,
        };
        if i == 0 {
            if stsc.first_chunk != 1 {
                return Err(AvifError::BmffParseFailed(
                    "stsc does not begin with chunk 1.".into(),
                ));
            }
        } else if stsc.first_chunk <= sample_table.sample_to_chunk.last().unwrap().first_chunk {
            return Err(AvifError::BmffParseFailed(
                "stsc chunks are not strictly increasing.".into(),
            ));
        }
        if stsc.sample_description_index == 0 {
            return Err(AvifError::BmffParseFailed(format!(
                "sample_description_index is {} in stsc chunk.",
                stsc.sample_description_index
            )));
        }
        sample_table.sample_to_chunk.push(stsc);
    }
    Ok(())
}

fn parse_stsz(stream: &mut IStream, sample_table: &mut SampleTable) -> AvifResult<()> {
    // Section 8.7.3.2.1 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) sample_size;
    let sample_size = stream.read_u32()?;
    // unsigned int(32) sample_count;
    let sample_count = usize_from_u32(stream.read_u32()?)?;

    if sample_size > 0 {
        sample_table.sample_size = SampleSize::FixedSize(sample_size);
        return Ok(());
    }
    let mut sample_sizes: Vec<u32> = create_vec_exact(sample_count)?;
    for _ in 0..sample_count {
        // unsigned int(32) entry_size;
        sample_sizes.push(stream.read_u32()?);
    }
    sample_table.sample_size = SampleSize::Sizes(sample_sizes);
    Ok(())
}

fn parse_stss(stream: &mut IStream, sample_table: &mut SampleTable) -> AvifResult<()> {
    // Section 8.6.2.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) entry_count;
    let entry_count = usize_from_u32(stream.read_u32()?)?;
    sample_table.sync_samples = create_vec_exact(entry_count)?;
    for _ in 0..entry_count {
        // unsigned int(32) sample_number;
        sample_table.sync_samples.push(stream.read_u32()?);
    }
    Ok(())
}

fn parse_stts(stream: &mut IStream, sample_table: &mut SampleTable) -> AvifResult<()> {
    // Section 8.6.1.2.2 of ISO/IEC 14496-12.
    let (_version, _flags) = stream.read_and_enforce_version_and_flags(0)?;
    // unsigned int(32) entry_count;
    let entry_count = usize_from_u32(stream.read_u32()?)?;
    sample_table.time_to_sample = create_vec_exact(entry_count)?;
    for _ in 0..entry_count {
        let stts = TimeToSample {
            // unsigned int(32) sample_count;
            sample_count: stream.read_u32()?,
            // unsigned int(32) sample_delta;
            sample_delta: stream.read_u32()?,
        };
        sample_table.time_to_sample.push(stts);
    }
    Ok(())
}

fn parse_sample_entry(stream: &mut IStream, format: String) -> AvifResult<SampleDescription> {
    // Section 8.5.2.2 of ISO/IEC 14496-12.
    let mut sample_entry = SampleDescription {
        format,
        ..SampleDescription::default()
    };
    // const unsigned int(8) reserved[6] = 0;
    if stream.read_u8()? != 0
        || stream.read_u8()? != 0
        || stream.read_u8()? != 0
        || stream.read_u8()? != 0
        || stream.read_u8()? != 0
        || stream.read_u8()? != 0
    {
        return Err(AvifError::BmffParseFailed(
            "Invalid reserved bits in SampleEntry of stsd".into(),
        ));
    }
    // unsigned int(16) data_reference_index;
    stream.skip(2)?;

    if sample_entry.format == "av01" {
        // https://aomediacodec.github.io/av1-isobmff/v1.2.0.html#av1sampleentry-syntax:
        //   class AV1SampleEntry extends VisualSampleEntry('av01'){
        //     AV1CodecConfigurationBox config;
        //   }
        // https://aomediacodec.github.io/av1-isobmff/v1.2.0.html#av1codecconfigurationbox-syntax:
        //   class AV1CodecConfigurationBox extends Box('av1C'){
        //     AV1CodecConfigurationRecord av1Config;
        //   }

        // Section 12.1.3.2 of ISO/IEC 14496-12:
        //   class VisualSampleEntry(codingname) extends SampleEntry(codingname)

        // unsigned int(16) pre_defined = 0; ("Readers should expect any value")
        stream.skip(2)?;
        // const unsigned int(16) reserved = 0;
        if stream.read_u16()? != 0 {
            return Err(AvifError::BmffParseFailed(
                "Invalid reserved bits in VisualSampleEntry of stsd".into(),
            ));
        }
        // unsigned int(32) pre_defined[3] = 0;
        stream.skip(4 * 3)?;
        // unsigned int(16) width;
        stream.skip(2)?;
        // unsigned int(16) height;
        stream.skip(2)?;
        // template unsigned int(32) horizresolution = 0x00480000; // 72 dpi
        stream.skip_u32()?;
        // template unsigned int(32) vertresolution = 0x00480000; // 72 dpi
        stream.skip_u32()?;
        // const unsigned int(32) reserved = 0;
        if stream.read_u32()? != 0 {
            return Err(AvifError::BmffParseFailed(
                "Invalid reserved bits in VisualSampleEntry of stsd".into(),
            ));
        }
        // template unsigned int(16) frame_count;
        stream.skip(2)?;
        // uint(8) compressorname[32];
        stream.skip(32)?;
        // template unsigned int(16) depth = 0x0018;
        if stream.read_u16()? != 0x0018 {
            return Err(AvifError::BmffParseFailed(
                "Invalid depth in VisualSampleEntry of stsd".into(),
            ));
        }
        // unsigned int(16) pre_defined = 0; ("Readers should expect any value")
        stream.skip(2)?;

        // other boxes from derived specifications
        // CleanApertureBox clap; // optional
        // PixelAspectRatioBox pasp; // optional

        // Now read any of 'av1C', 'clap', 'pasp' etc.
        sample_entry.properties = parse_ipco(&mut stream.sub_stream(&BoxSize::UntilEndOfStream)?)?;

        if !sample_entry
            .properties
            .iter()
            .any(|p| matches!(p, ItemProperty::CodecConfiguration(_)))
        {
            return Err(AvifError::BmffParseFailed(
                "AV1SampleEntry must contain an AV1CodecConfigurationRecord".into(),
            ));
        }
    }
    Ok(sample_entry)
}

fn parse_stsd(stream: &mut IStream, sample_table: &mut SampleTable) -> AvifResult<()> {
    // Section 8.5.2.2 of ISO/IEC 14496-12.
    let (version, _flags) = stream.read_version_and_flags()?;
    if version != 0 && version != 1 {
        // Section 8.5.2.3 of ISO/IEC 14496-12:
        //   version is set to zero. A version number of 1 shall be treated as a version of 0.
        return Err(AvifError::BmffParseFailed(
            "stsd box version 0 or 1 expected.".into(),
        ));
    }
    // unsigned int(32) entry_count;
    let entry_count = usize_from_u32(stream.read_u32()?)?;
    sample_table.sample_descriptions = create_vec_exact(entry_count)?;
    for _ in 0..entry_count {
        // aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format)
        let header = parse_header(stream, /*top_level=*/ false)?;
        let sample_entry =
            parse_sample_entry(&mut stream.sub_stream(&header.size)?, header.box_type)?;
        sample_table.sample_descriptions.push(sample_entry);
    }
    Ok(())
}

fn parse_stbl(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.5.1.2 of ISO/IEC 14496-12.
    if track.sample_table.is_some() {
        return Err(AvifError::BmffParseFailed(
            "duplicate stbl for track.".into(),
        ));
    }
    let mut sample_table = SampleTable::default();
    let mut boxes_seen: HashSet<String> = HashSet::with_hasher(NonRandomHasherState);
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        if boxes_seen.contains(&header.box_type) {
            return Err(AvifError::BmffParseFailed(format!(
                "duplicate box in stbl: {}",
                header.box_type
            )));
        }
        let mut skipped_box = false;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "stco" => {
                if boxes_seen.contains("co64") {
                    return Err(AvifError::BmffParseFailed(
                        "exactly one of co64 or stco is allowed in stbl".into(),
                    ));
                }
                parse_stco(&mut sub_stream, &mut sample_table, false)?;
            }
            "co64" => {
                if boxes_seen.contains("stco") {
                    return Err(AvifError::BmffParseFailed(
                        "exactly one of co64 or stco is allowed in stbl".into(),
                    ));
                }
                parse_stco(&mut sub_stream, &mut sample_table, true)?;
            }
            "stsc" => parse_stsc(&mut sub_stream, &mut sample_table)?,
            "stsz" => parse_stsz(&mut sub_stream, &mut sample_table)?,
            "stss" => parse_stss(&mut sub_stream, &mut sample_table)?,
            "stts" => parse_stts(&mut sub_stream, &mut sample_table)?,
            "stsd" => parse_stsd(&mut sub_stream, &mut sample_table)?,
            _ => skipped_box = true,
        }
        // For boxes that are skipped, we do not need to validate if they occur exactly once or
        // not.
        if !skipped_box {
            boxes_seen.insert(header.box_type.clone());
        }
    }
    track.sample_table = Some(sample_table);
    Ok(())
}

fn parse_minf(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.4.4.2 of ISO/IEC 14496-12.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        if header.box_type == "stbl" {
            parse_stbl(&mut sub_stream, track)?;
        }
    }
    Ok(())
}

fn parse_mdia(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.4.1.2 of ISO/IEC 14496-12.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "mdhd" => parse_mdhd(&mut sub_stream, track)?,
            "minf" => parse_minf(&mut sub_stream, track)?,
            _ => {}
        }
    }
    Ok(())
}

fn parse_tref(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    // Section 8.3.3.2 of ISO/IEC 14496-12.

    // TrackReferenceTypeBox [];
    while stream.has_bytes_left()? {
        // aligned(8) class TrackReferenceTypeBox (reference_type) extends Box(reference_type)
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "auxl" => {
                // unsigned int(32) track_IDs[];
                // Use only the first one and skip the rest.
                track.aux_for_id = Some(sub_stream.read_u32()?);
            }
            "prem" => {
                // unsigned int(32) track_IDs[];
                // Use only the first one and skip the rest.
                track.prem_by_id = Some(sub_stream.read_u32()?);
            }
            _ => {}
        }
    }
    Ok(())
}

fn parse_elst(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    if track.elst_seen {
        return Err(AvifError::BmffParseFailed(
            "more than one elst box was found for track".into(),
        ));
    }
    track.elst_seen = true;

    // Section 8.6.6.2 of ISO/IEC 14496-12.
    let (version, flags) = stream.read_version_and_flags()?;

    // Section 8.6.6.3 of ISO/IEC 14496-12:
    //   flags - the following values are defined. The values of flags greater than 1 are reserved
    //     RepeatEdits 1
    if (flags & 1) == 0 {
        track.is_repeating = false;
        // TODO: This early return is not part of the spec, investigate
        return Ok(());
    }
    track.is_repeating = true;

    // unsigned int(32) entry_count;
    let entry_count = stream.read_u32()?;
    if entry_count != 1 {
        return Err(AvifError::BmffParseFailed(format!(
            "elst has entry_count ({entry_count}) != 1"
        )));
    }

    if version == 1 {
        // unsigned int(64) segment_duration;
        track.segment_duration = stream.read_u64()?;
        // int(64) media_time;
        stream.skip(8)?;
    } else if version == 0 {
        // unsigned int(32) segment_duration;
        track.segment_duration = stream.read_u32()? as u64;
        // int(32) media_time;
        stream.skip(4)?;
    } else {
        return Err(AvifError::BmffParseFailed(
            "unsupported version in elst".into(),
        ));
    }
    // int(16) media_rate_integer;
    stream.skip(2)?;
    // int(16) media_rate_fraction;
    stream.skip(2)?;

    if track.segment_duration == 0 {
        return Err(AvifError::BmffParseFailed(
            "invalid value for segment_duration (0)".into(),
        ));
    }
    Ok(())
}

fn parse_edts(stream: &mut IStream, track: &mut Track) -> AvifResult<()> {
    if track.elst_seen {
        // This function always exits with track.elst_seen set to true. So it is sufficient to
        // check track.elst_seen to verify the uniqueness of the edts box.
        return Err(AvifError::BmffParseFailed(
            "multiple edts boxes found for track.".into(),
        ));
    }

    // Section 8.6.5.2 of ISO/IEC 14496-12.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        if header.box_type == "elst" {
            parse_elst(&mut sub_stream, track)?;
        }
    }

    if !track.elst_seen {
        return Err(AvifError::BmffParseFailed(
            "elst box was not found in edts".into(),
        ));
    }
    Ok(())
}

fn parse_trak(stream: &mut IStream) -> AvifResult<Track> {
    let mut track = Track::default();
    let mut tkhd_seen = false;
    // Section 8.3.1.2 of ISO/IEC 14496-12.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        match header.box_type.as_str() {
            "tkhd" => {
                if tkhd_seen {
                    return Err(AvifError::BmffParseFailed(
                        "trak box contains multiple tkhd boxes".into(),
                    ));
                }
                parse_tkhd(&mut sub_stream, &mut track)?;
                tkhd_seen = true;
            }
            "mdia" => parse_mdia(&mut sub_stream, &mut track)?,
            "tref" => parse_tref(&mut sub_stream, &mut track)?,
            "edts" => parse_edts(&mut sub_stream, &mut track)?,
            "meta" => track.meta = Some(parse_meta(&mut sub_stream)?),
            _ => {}
        }
    }
    if !tkhd_seen {
        return Err(AvifError::BmffParseFailed(
            "trak box did not contain a tkhd box".into(),
        ));
    }
    Ok(track)
}

fn parse_moov(stream: &mut IStream) -> AvifResult<Vec<Track>> {
    let mut tracks: Vec<Track> = Vec::new();
    // Section 8.2.1.2 of ISO/IEC 14496-12.
    while stream.has_bytes_left()? {
        let header = parse_header(stream, /*top_level=*/ false)?;
        let mut sub_stream = stream.sub_stream(&header.size)?;
        if header.box_type == "trak" {
            tracks.push(parse_trak(&mut sub_stream)?);
        }
    }
    if tracks.is_empty() {
        return Err(AvifError::BmffParseFailed(
            "moov box does not contain any tracks".into(),
        ));
    }
    Ok(tracks)
}

pub fn parse(io: &mut GenericIO) -> AvifResult<AvifBoxes> {
    let mut ftyp: Option<FileTypeBox> = None;
    let mut meta: Option<MetaBox> = None;
    let mut tracks: Option<Vec<Track>> = None;
    let mut parse_offset: u64 = 0;
    loop {
        // Read just enough to get the longest possible valid box header (4+4+8+16 bytes).
        let header_data = io.read(parse_offset, 32)?;
        if header_data.is_empty() {
            // No error and size is 0. We have reached the end of the stream.
            break;
        }
        let mut header_stream = IStream::create(header_data);
        let header = parse_header(&mut header_stream, /*top_level=*/ true)?;
        parse_offset = parse_offset
            .checked_add(header_stream.offset as u64)
            .ok_or(AvifError::BmffParseFailed("invalid parse offset".into()))?;

        // Read the rest of the box if necessary.
        match header.box_type.as_str() {
            "ftyp" | "meta" | "moov" => {
                let box_data = match header.size {
                    BoxSize::UntilEndOfStream => io.read(parse_offset, usize::MAX)?,
                    BoxSize::FixedSize(size) => io.read_exact(parse_offset, size)?,
                };
                let mut box_stream = IStream::create(box_data);
                match header.box_type.as_str() {
                    "ftyp" => {
                        ftyp = Some(parse_ftyp(&mut box_stream)?);
                        if !ftyp.unwrap_ref().is_avif() {
                            return Err(AvifError::InvalidFtyp);
                        }
                    }
                    "meta" => meta = Some(parse_meta(&mut box_stream)?),
                    "moov" => {
                        tracks = Some(parse_moov(&mut box_stream)?);
                        // decoder.image_sequence_track_present = true;
                    }
                    _ => {} // Not reached.
                }
                if ftyp.is_some() {
                    let ftyp = ftyp.unwrap_ref();
                    if (!ftyp.needs_meta() || meta.is_some())
                        && (!ftyp.needs_moov() || tracks.is_some())
                    {
                        // Enough information has been parsed to consider parse a success.
                        break;
                    }
                }
            }
            _ => {}
        }
        if header.size == BoxSize::UntilEndOfStream {
            // There is no other box after this one because it goes till the end of the stream.
            break;
        }
        parse_offset = parse_offset
            .checked_add(header.size() as u64)
            .ok_or(AvifError::BmffParseFailed("invalid parse offset".into()))?;
    }
    if ftyp.is_none() {
        return Err(AvifError::InvalidFtyp);
    }
    let ftyp = ftyp.unwrap();
    if (ftyp.needs_meta() && meta.is_none()) || (ftyp.needs_moov() && tracks.is_none()) {
        return Err(AvifError::TruncatedData);
    }
    // TODO: Enforce 'ftyp' as first box seen, for consistency with peek_compatible_file_type()?
    Ok(AvifBoxes {
        ftyp,
        meta: meta.unwrap_or_default(),
        tracks: tracks.unwrap_or_default(),
    })
}

pub fn peek_compatible_file_type(data: &[u8]) -> AvifResult<bool> {
    let mut stream = IStream::create(data);
    let header = parse_header(&mut stream, /*top_level=*/ true)?;
    if header.box_type != "ftyp" {
        // Section 6.3.4 of ISO/IEC 14496-12:
        //   The FileTypeBox shall occur before any variable-length box.
        //   Only a fixed-size box such as a file signature, if required, may precede it.
        return Ok(false);
    }
    if header.size == BoxSize::UntilEndOfStream {
        // The 'ftyp' box goes on till the end of the file. Either there is no brand requiring
        // anything in the file but a FileTypebox (so not AVIF), or it is invalid.
        return Ok(false);
    }
    let mut header_stream = stream.sub_stream(&header.size)?;
    let ftyp = parse_ftyp(&mut header_stream)?;
    Ok(ftyp.is_avif())
}

pub fn parse_tmap(stream: &mut IStream) -> AvifResult<Option<GainMapMetadata>> {
    // Experimental, not yet specified.

    // unsigned int(8) version = 0;
    let version = stream.read_u8()?;
    if version != 0 {
        return Ok(None); // Unsupported version.
    }
    // unsigned int(16) minimum_version;
    let minimum_version = stream.read_u16()?;
    let supported_version = 0;
    if minimum_version > supported_version {
        return Ok(None); // Unsupported version.
    }
    // unsigned int(16) writer_version;
    let writer_version = stream.read_u16()?;

    let mut metadata = GainMapMetadata::default();
    let mut bits = stream.sub_bit_stream(1)?;
    // unsigned int(1) is_multichannel;
    let is_multichannel = bits.read_bool()?;
    let channel_count = if is_multichannel { 3 } else { 1 };
    // unsigned int(1) use_base_colour_space;
    metadata.use_base_color_space = bits.read_bool()?;
    // unsigned int(6) reserved;
    bits.skip(6)?;

    // unsigned int(32) base_hdr_headroom_numerator;
    // unsigned int(32) base_hdr_headroom_denominator;
    metadata.base_hdr_headroom = stream.read_ufraction()?;
    // unsigned int(32) alternate_hdr_headroom_numerator;
    // unsigned int(32) alternate_hdr_headroom_denominator;
    metadata.alternate_hdr_headroom = stream.read_ufraction()?;
    for i in 0..channel_count {
        // int(32) gain_map_min_numerator;
        // unsigned int(32) gain_map_min_denominator
        metadata.min[i] = stream.read_fraction()?;
        // int(32) gain_map_max_numerator;
        // unsigned int(32) gain_map_max_denominator;
        metadata.max[i] = stream.read_fraction()?;
        // unsigned int(32) gamma_numerator;
        // unsigned int(32) gamma_denominator;
        metadata.gamma[i] = stream.read_ufraction()?;
        // int(32) base_offset_numerator;
        // unsigned int(32) base_offset_denominator;
        metadata.base_offset[i] = stream.read_fraction()?;
        // int(32) alternate_offset_numerator;
        // unsigned int(32) alternate_offset_denominator;
        metadata.alternate_offset[i] = stream.read_fraction()?;
    }

    // Fill the remaining values by copying those from the first channel.
    for i in channel_count..3 {
        metadata.min[i] = metadata.min[0];
        metadata.max[i] = metadata.max[0];
        metadata.gamma[i] = metadata.gamma[0];
        metadata.base_offset[i] = metadata.base_offset[0];
        metadata.alternate_offset[i] = metadata.alternate_offset[0];
    }
    if writer_version <= supported_version && stream.has_bytes_left()? {
        return Err(AvifError::InvalidToneMappedImage(
            "invalid trailing bytes in tmap box".into(),
        ));
    }
    Ok(Some(metadata))
}

#[cfg(test)]
mod tests {
    use crate::parser::mp4box;
    use crate::AvifResult;

    #[test]
    fn peek_compatible_file_type() -> AvifResult<()> {
        let buf = [
            0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, //
            0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, //
            0x61, 0x76, 0x69, 0x66, 0x6d, 0x69, 0x66, 0x31, //
            0x6d, 0x69, 0x61, 0x66, 0x4d, 0x41, 0x31, 0x41, //
            0x00, 0x00, 0x00, 0xf2, 0x6d, 0x65, 0x74, 0x61, //
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, //
        ];
        let min_required_bytes = 32;
        for i in 0..buf.len() {
            let res = mp4box::peek_compatible_file_type(&buf[..i]);
            if i < min_required_bytes {
                // Not enough bytes.
                assert!(res.is_err());
            } else {
                assert!(res?);
            }
        }
        Ok(())
    }
}
