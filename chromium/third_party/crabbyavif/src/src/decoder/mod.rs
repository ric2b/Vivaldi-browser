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

pub mod item;
#[cfg(feature = "sample_transform")]
pub mod sampletransform;
pub mod tile;
pub mod track;

use crate::decoder::item::*;
use crate::decoder::tile::*;
use crate::decoder::track::*;

#[cfg(feature = "dav1d")]
use crate::codecs::dav1d::Dav1d;

#[cfg(feature = "libgav1")]
use crate::codecs::libgav1::Libgav1;

#[cfg(feature = "android_mediacodec")]
use crate::codecs::android_mediacodec::MediaCodec;

use crate::codecs::DecoderConfig;
use crate::gainmap::*;
use crate::image::*;
use crate::internal_utils::io::*;
use crate::internal_utils::*;
use crate::parser::exif;
use crate::parser::mp4box;
use crate::parser::mp4box::*;
use crate::parser::obu::Av1SequenceHeader;
use crate::*;

use std::cmp::max;
use std::cmp::min;
use std::num::NonZero;

pub trait IO {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]>;
    fn size_hint(&self) -> u64;
    fn persistent(&self) -> bool;
}

impl dyn IO {
    pub(crate) fn read_exact(&mut self, offset: u64, read_size: usize) -> AvifResult<&[u8]> {
        let result = self.read(offset, read_size)?;
        if result.len() < read_size {
            Err(AvifError::TruncatedData)
        } else {
            assert!(result.len() == read_size);
            Ok(result)
        }
    }
}

pub type GenericIO = Box<dyn IO>;
pub(crate) type Codec = Box<dyn crate::codecs::Decoder>;

#[derive(Debug, Default, PartialEq)]
pub enum CodecChoice {
    #[default]
    Auto,
    Dav1d,
    Libgav1,
    MediaCodec,
}

impl CodecChoice {
    fn get_codec(&self, is_avif: bool) -> AvifResult<Codec> {
        match self {
            CodecChoice::Auto => {
                // Preferred order of codecs in Auto mode: Android MediaCodec, Dav1d, Libgav1.
                CodecChoice::MediaCodec
                    .get_codec(is_avif)
                    .or_else(|_| CodecChoice::Dav1d.get_codec(is_avif))
                    .or_else(|_| CodecChoice::Libgav1.get_codec(is_avif))
            }
            CodecChoice::Dav1d => {
                if !is_avif {
                    return Err(AvifError::NoCodecAvailable);
                }
                #[cfg(feature = "dav1d")]
                return Ok(Box::<Dav1d>::default());
                #[cfg(not(feature = "dav1d"))]
                return Err(AvifError::NoCodecAvailable);
            }
            CodecChoice::Libgav1 => {
                if !is_avif {
                    return Err(AvifError::NoCodecAvailable);
                }
                #[cfg(feature = "libgav1")]
                return Ok(Box::<Libgav1>::default());
                #[cfg(not(feature = "libgav1"))]
                return Err(AvifError::NoCodecAvailable);
            }
            CodecChoice::MediaCodec => {
                #[cfg(feature = "android_mediacodec")]
                return Ok(Box::<MediaCodec>::default());
                #[cfg(not(feature = "android_mediacodec"))]
                return Err(AvifError::NoCodecAvailable);
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Source {
    #[default]
    Auto = 0,
    PrimaryItem = 1,
    Tracks = 2,
    // TODO: Thumbnail,
}

pub const DEFAULT_IMAGE_SIZE_LIMIT: u32 = 16384 * 16384;
pub const DEFAULT_IMAGE_DIMENSION_LIMIT: u32 = 32768;
pub const DEFAULT_IMAGE_COUNT_LIMIT: u32 = 12 * 3600 * 60;

#[derive(Debug, PartialEq)]
pub enum ImageContentType {
    None,
    ColorAndAlpha,
    GainMap,
    All,
}

impl ImageContentType {
    pub(crate) fn decoding_items(&self) -> Vec<DecodingItem> {
        let categories = match self {
            Self::None => vec![],
            Self::ColorAndAlpha => vec![Category::Color, Category::Alpha],
            Self::GainMap => vec![Category::Gainmap],
            Self::All => Category::ALL.to_vec(),
        };
        DecodingItem::all_for_categories(&categories)
    }

    pub(crate) fn gainmap(&self) -> bool {
        matches!(self, Self::GainMap | Self::All)
    }
}

#[derive(Debug)]
pub struct Settings {
    pub source: Source,
    pub ignore_exif: bool,
    pub ignore_xmp: bool,
    pub strictness: Strictness,
    pub allow_progressive: bool,
    pub allow_incremental: bool,
    pub image_content_to_decode: ImageContentType,
    pub codec_choice: CodecChoice,
    pub image_size_limit: Option<NonZero<u32>>,
    pub image_dimension_limit: Option<NonZero<u32>>,
    pub image_count_limit: Option<NonZero<u32>>,
    pub max_threads: u32,
    pub android_mediacodec_output_color_format: AndroidMediaCodecOutputColorFormat,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            source: Default::default(),
            ignore_exif: false,
            ignore_xmp: false,
            strictness: Default::default(),
            allow_progressive: false,
            allow_incremental: false,
            image_content_to_decode: ImageContentType::ColorAndAlpha,
            codec_choice: Default::default(),
            image_size_limit: NonZero::new(DEFAULT_IMAGE_SIZE_LIMIT),
            image_dimension_limit: NonZero::new(DEFAULT_IMAGE_DIMENSION_LIMIT),
            image_count_limit: NonZero::new(DEFAULT_IMAGE_COUNT_LIMIT),
            max_threads: 1,
            android_mediacodec_output_color_format: AndroidMediaCodecOutputColorFormat::default(),
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(C)]
pub struct Extent {
    pub offset: u64,
    pub size: usize,
}

impl Extent {
    fn merge(&mut self, extent: &Extent) -> AvifResult<()> {
        if self.size == 0 {
            *self = *extent;
            return Ok(());
        }
        if extent.size == 0 {
            return Ok(());
        }
        let max_extent_1 = checked_add!(self.offset, u64_from_usize(self.size)?)?;
        let max_extent_2 = checked_add!(extent.offset, u64_from_usize(extent.size)?)?;
        self.offset = min(self.offset, extent.offset);
        // The extents may not be contiguous. It does not matter for nth_image_max_extent().
        self.size = usize_from_u64(checked_sub!(max(max_extent_1, max_extent_2), self.offset)?)?;
        Ok(())
    }
}

#[derive(Debug)]
pub enum StrictnessFlag {
    PixiRequired,
    ClapValid,
    AlphaIspeRequired,
}

#[derive(Debug, Default)]
pub enum Strictness {
    None,
    #[default]
    All,
    SpecificInclude(Vec<StrictnessFlag>),
    SpecificExclude(Vec<StrictnessFlag>),
}

impl Strictness {
    pub(crate) fn pixi_required(&self) -> bool {
        match self {
            Strictness::All => true,
            Strictness::SpecificInclude(flags) => flags
                .iter()
                .any(|x| matches!(x, StrictnessFlag::PixiRequired)),
            Strictness::SpecificExclude(flags) => !flags
                .iter()
                .any(|x| matches!(x, StrictnessFlag::PixiRequired)),
            _ => false,
        }
    }

    pub(crate) fn alpha_ispe_required(&self) -> bool {
        match self {
            Strictness::All => true,
            Strictness::SpecificInclude(flags) => flags
                .iter()
                .any(|x| matches!(x, StrictnessFlag::AlphaIspeRequired)),
            Strictness::SpecificExclude(flags) => !flags
                .iter()
                .any(|x| matches!(x, StrictnessFlag::AlphaIspeRequired)),
            _ => false,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub enum ProgressiveState {
    #[default]
    Unavailable = 0,
    Available = 1,
    Active = 2,
}

#[derive(Default, PartialEq)]
enum ParseState {
    #[default]
    None,
    AwaitingSequenceHeader,
    Complete,
}

/// cbindgen:field-names=[colorOBUSize,alphaOBUSize]
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct IOStats {
    pub color_obu_size: usize,
    pub alpha_obu_size: usize,
}

#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub struct DecodingItem {
    pub category: Category,
    // 0 for the main image, 1 to MAX_EXTRA_INPUTS for extra input images.
    pub item_idx: usize,
}

impl DecodingItem {
    const COUNT: usize = 3 + Self::MAX_EXTRA_INPUTS * 2;
    // Max supported number of inputs for derived image items.
    const MAX_EXTRA_INPUTS: usize = 3;
    const ALL: [DecodingItem; Self::COUNT] = [
        Self::COLOR,
        Self::color(1),
        Self::color(2),
        Self::color(3),
        Self::ALPHA,
        Self::alpha(1),
        Self::alpha(2),
        Self::alpha(3),
        Self::GAINMAP,
    ];
    const ALL_USIZE: [usize; Self::COUNT] = [0, 1, 2, 3, 4, 5, 6, 7, 8];

    const COLOR: DecodingItem = Self::color(0);
    const ALPHA: DecodingItem = Self::alpha(0);
    const GAINMAP: DecodingItem = DecodingItem {
        category: Category::Gainmap,
        item_idx: 0,
    };

    const fn color(item_idx: usize) -> DecodingItem {
        DecodingItem {
            category: Category::Color,
            item_idx,
        }
    }

    const fn alpha(item_idx: usize) -> DecodingItem {
        DecodingItem {
            category: Category::Alpha,
            item_idx,
        }
    }

    fn all_for_categories(categories: &[Category]) -> Vec<DecodingItem> {
        Self::ALL
            .iter()
            .filter(|x| categories.contains(&x.category))
            .cloned()
            .collect()
    }

    fn usize(self) -> usize {
        match self.category {
            Category::Color => self.item_idx,
            Category::Alpha => 1 + Self::MAX_EXTRA_INPUTS + self.item_idx,
            Category::Gainmap => (1 + Self::MAX_EXTRA_INPUTS) * 2,
        }
    }
}

#[derive(Default)]
pub struct Decoder {
    pub settings: Settings,
    image_count: u32,
    image_index: i32,
    image_timing: ImageTiming,
    timescale: u64,
    duration_in_timescales: u64,
    duration: f64,
    repetition_count: RepetitionCount,
    gainmap: GainMap,
    gainmap_present: bool,
    image: Image,
    extra_inputs: [Image; DecodingItem::MAX_EXTRA_INPUTS],
    source: Source,
    tile_info: [TileInfo; DecodingItem::COUNT],
    tiles: [Vec<Tile>; DecodingItem::COUNT],
    items: Items,
    tracks: Vec<Track>,
    // To replicate the C-API, we need to keep this optional. Otherwise this
    // could be part of the initialization.
    io: Option<GenericIO>,
    codecs: Vec<Codec>,
    color_track_id: Option<u32>,
    parse_state: ParseState,
    io_stats: IOStats,
    compression_format: CompressionFormat,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum CompressionFormat {
    #[default]
    Avif = 0,
    Heic = 1,
}

pub(crate) struct GridImageHelper<'a> {
    grid: &'a Grid,
    image: &'a mut Image,
    pub category: Category,
    cell_index: usize,
    expected_cell_count: usize,
    codec_config: &'a CodecConfiguration,
    first_cell_image: Option<Image>,
    tile_width: u32,
    tile_height: u32,
}

// These functions are not used in all configurations.
#[allow(unused)]
impl GridImageHelper<'_> {
    pub(crate) fn is_grid_complete(&self) -> AvifResult<bool> {
        Ok(self.cell_index == self.expected_cell_count)
    }

    pub(crate) fn copy_from_cell_image(&mut self, cell_image: &mut Image) -> AvifResult<()> {
        if self.is_grid_complete()? {
            return Ok(());
        }
        if self.category == Category::Alpha && cell_image.yuv_range == YuvRange::Limited {
            cell_image.alpha_to_full_range()?;
        }
        cell_image.scale(self.tile_width, self.tile_height, self.category)?;
        if self.cell_index == 0 {
            validate_grid_image_dimensions(cell_image, self.grid)?;
            if self.category != Category::Alpha {
                self.image.width = self.grid.width;
                self.image.height = self.grid.height;
                self.image
                    .copy_properties_from(cell_image, self.codec_config);
            }
            self.image.allocate_planes(self.category)?;
        } else if self.first_cell_image.is_some()
            && !cell_image.has_same_properties_and_cicp(self.first_cell_image.unwrap_ref())
        {
            return Err(AvifError::InvalidImageGrid(
                "grid image contains mismatched tiles".into(),
            ));
        }
        self.image
            .copy_from_tile(cell_image, self.grid, self.cell_index as u32, self.category)?;
        if self.cell_index == 0 {
            self.first_cell_image = Some(cell_image.shallow_clone());
        }
        self.cell_index += 1;
        Ok(())
    }
}

impl Decoder {
    pub fn image_count(&self) -> u32 {
        self.image_count
    }
    pub fn image_index(&self) -> i32 {
        self.image_index
    }
    pub fn image_timing(&self) -> ImageTiming {
        self.image_timing
    }
    pub fn timescale(&self) -> u64 {
        self.timescale
    }
    pub fn duration_in_timescales(&self) -> u64 {
        self.duration_in_timescales
    }
    pub fn duration(&self) -> f64 {
        self.duration
    }
    pub fn repetition_count(&self) -> RepetitionCount {
        self.repetition_count
    }
    pub fn gainmap(&self) -> &GainMap {
        &self.gainmap
    }
    pub fn gainmap_present(&self) -> bool {
        self.gainmap_present
    }
    pub fn io_stats(&self) -> IOStats {
        self.io_stats
    }
    pub fn compression_format(&self) -> CompressionFormat {
        self.compression_format
    }

    fn parsing_complete(&self) -> bool {
        self.parse_state == ParseState::Complete
    }

    pub fn set_io_file(&mut self, filename: &String) -> AvifResult<()> {
        self.io = Some(Box::new(DecoderFileIO::create(filename)?));
        self.parse_state = ParseState::None;
        Ok(())
    }

    pub fn set_io_vec(&mut self, data: Vec<u8>) {
        self.io = Some(Box::new(DecoderMemoryIO { data }));
        self.parse_state = ParseState::None;
    }

    /// # Safety
    ///
    /// This function is intended for use only from the C API. The assumption is that the caller
    /// will always pass in a valid pointer and size.
    pub unsafe fn set_io_raw(&mut self, data: *const u8, size: usize) -> AvifResult<()> {
        self.io = Some(Box::new(unsafe { DecoderRawIO::create(data, size) }));
        self.parse_state = ParseState::None;
        Ok(())
    }

    pub fn set_io(&mut self, io: GenericIO) {
        self.io = Some(io);
        self.parse_state = ParseState::None;
    }

    fn find_alpha_item(&mut self, color_item_index: u32) -> AvifResult<Option<u32>> {
        let color_item = self.items.get(&color_item_index).unwrap();
        if let Some(item) = self.items.iter().find(|x| {
            !x.1.should_skip() && x.1.aux_for_id == color_item.id && x.1.is_auxiliary_alpha()
        }) {
            return Ok(Some(*item.0));
        }
        if !color_item.is_grid_item() || color_item.source_item_ids.is_empty() {
            return Ok(None);
        }
        // If color item is a grid, check if there is an alpha channel which is represented as an
        // auxl item to each color tile item.
        let mut alpha_item_indices: Vec<u32> = create_vec_exact(color_item.source_item_ids.len())?;
        for color_grid_item_id in &color_item.source_item_ids {
            match self
                .items
                .iter()
                .find(|x| x.1.aux_for_id == *color_grid_item_id && x.1.is_auxiliary_alpha())
            {
                Some(item) => alpha_item_indices.push(*item.0),
                None => {
                    if alpha_item_indices.is_empty() {
                        return Ok(None);
                    } else {
                        return Err(AvifError::BmffParseFailed(
                            "Some tiles but not all have an alpha auxiliary image item".into(),
                        ));
                    }
                }
            }
        }

        // Make up an alpha item for convenience. For the item_id, choose the first id that is not
        // found in the actual image. In the very unlikely case that all the item ids are used,
        // treat this as an image without alpha channel.
        let alpha_item_id = match (1..u32::MAX).find(|&id| !self.items.contains_key(&id)) {
            Some(id) => id,
            None => return Ok(None),
        };
        let first_item = self.items.get(&alpha_item_indices[0]).unwrap();
        let properties = match first_item.codec_config() {
            Some(config) => vec![ItemProperty::CodecConfiguration(config.clone())],
            None => return Ok(None),
        };
        let alpha_item = Item {
            id: alpha_item_id,
            item_type: String::from("grid"),
            width: color_item.width,
            height: color_item.height,
            source_item_ids: alpha_item_indices,
            properties,
            is_made_up: true,
            ..Item::default()
        };
        self.tile_info[DecodingItem::ALPHA.usize()].grid =
            self.tile_info[DecodingItem::COLOR.usize()].grid;
        self.items.insert(alpha_item_id, alpha_item);
        Ok(Some(alpha_item_id))
    }

    fn validate_gainmap_item(
        &mut self,
        gainmap_id: u32,
        tonemap_id: u32,
        #[allow(unused)] color_item_id: u32, // This parameter is unused in some configurations.
    ) -> AvifResult<()> {
        let gainmap_item = self
            .items
            .get(&gainmap_id)
            .ok_or(AvifError::InvalidToneMappedImage("".into()))?;
        // Find and adopt all colr boxes "at most one for a given value of colour type"
        // (HEIF 6.5.5.1, from Amendment 3). Accept one of each type, and bail out if more than one
        // of a given type is provided.
        if let Some(nclx) = find_nclx(&gainmap_item.properties)? {
            self.gainmap.image.color_primaries = nclx.color_primaries;
            self.gainmap.image.transfer_characteristics = nclx.transfer_characteristics;
            self.gainmap.image.matrix_coefficients = nclx.matrix_coefficients;
            self.gainmap.image.yuv_range = nclx.yuv_range;
        }
        if tonemap_id == 0 {
            return Ok(());
        }
        // Find and adopt all colr boxes "at most one for a given value of colour type"
        // (HEIF 6.5.5.1, from Amendment 3). Accept one of each type, and bail out if more than one
        // of a given type is provided.
        let tonemap_item = self
            .items
            .get(&tonemap_id)
            .ok_or(AvifError::InvalidToneMappedImage("".into()))?;
        if let Some(nclx) = find_nclx(&tonemap_item.properties)? {
            self.gainmap.alt_color_primaries = nclx.color_primaries;
            self.gainmap.alt_transfer_characteristics = nclx.transfer_characteristics;
            self.gainmap.alt_matrix_coefficients = nclx.matrix_coefficients;
            self.gainmap.alt_yuv_range = nclx.yuv_range;
        }
        if let Some(icc) = find_icc(&tonemap_item.properties)? {
            self.gainmap.alt_icc.clone_from(icc);
        }
        if let Some(clli) = tonemap_item.clli() {
            self.gainmap.alt_clli = *clli;
        }
        if let Some(pixi) = tonemap_item.pixi() {
            self.gainmap.alt_plane_count = pixi.plane_depths.len() as u8;
            self.gainmap.alt_plane_depth = pixi.plane_depths[0];
        }
        // HEIC files created by Apple have some of these properties set in the Tonemap item. So do
        // not perform this validation when HEIC is enabled.
        #[cfg(not(feature = "heic"))]
        {
            if let Some(ispe) = find_property!(tonemap_item.properties, ImageSpatialExtents) {
                let color_item = self
                    .items
                    .get(&color_item_id)
                    .ok_or(AvifError::InvalidToneMappedImage("".into()))?;
                if ispe.width != color_item.width || ispe.height != color_item.height {
                    return Err(AvifError::InvalidToneMappedImage(
                        "Box[tmap] ispe property width/height does not match base image".into(),
                    ));
                }
            } else {
                return Err(AvifError::InvalidToneMappedImage(
                    "Box[tmap] missing mandatory ispe property".into(),
                ));
            }
            if find_property!(tonemap_item.properties, PixelAspectRatio).is_some()
                || find_property!(tonemap_item.properties, CleanAperture).is_some()
                || find_property!(tonemap_item.properties, ImageRotation).is_some()
                || find_property!(tonemap_item.properties, ImageMirror).is_some()
            {
                return Err(AvifError::InvalidToneMappedImage("".into()));
            }
        }
        Ok(())
    }

    fn search_exif_or_xmp_metadata(
        items: &mut Items,
        color_item_index: Option<u32>,
        settings: &Settings,
        io: &mut GenericIO,
        image: &mut Image,
    ) -> AvifResult<()> {
        if !settings.ignore_exif {
            if let Some(exif) = items.iter_mut().rfind(|x| x.1.is_exif(color_item_index)) {
                let mut stream = exif.1.stream(io)?;
                exif::parse(&mut stream)?;
                image
                    .exif
                    .extend_from_slice(stream.get_slice(stream.bytes_left()?)?);
            }
        }
        if !settings.ignore_xmp {
            if let Some(xmp) = items.iter_mut().rfind(|x| x.1.is_xmp(color_item_index)) {
                let mut stream = xmp.1.stream(io)?;
                image
                    .xmp
                    .extend_from_slice(stream.get_slice(stream.bytes_left()?)?);
            }
        }
        Ok(())
    }

    fn generate_tiles(
        &mut self,
        item_id: u32,
        decoding_item: DecodingItem,
    ) -> AvifResult<Vec<Tile>> {
        let item = self
            .items
            .get(&item_id)
            .ok_or(AvifError::MissingImageItem)?;
        let mut tiles: Vec<Tile> = Vec::new();
        if item.is_sample_transform_item() {
            return Ok(tiles);
        }
        if item.source_item_ids.is_empty() {
            if item.size == 0 {
                return Err(AvifError::MissingImageItem);
            }
            let mut tile = Tile::create_from_item(
                self.items.get_mut(&item_id).unwrap(),
                self.settings.allow_progressive,
                self.settings.image_count_limit,
                self.io.unwrap_ref().size_hint(),
            )?;
            tile.input.decoding_item = decoding_item;
            tiles.push(tile);
        } else {
            if !self.tile_info[decoding_item.usize()].is_derived_image() {
                return Err(AvifError::InvalidImageGrid(
                    "dimg items were found but image is not a derived image.".into(),
                ));
            }
            let mut progressive = true;
            for derived_item_id in item.source_item_ids.clone() {
                let derived_item = self
                    .items
                    .get_mut(&derived_item_id)
                    .ok_or(AvifError::InvalidImageGrid("missing derived item".into()))?;
                let mut tile = Tile::create_from_item(
                    derived_item,
                    self.settings.allow_progressive,
                    self.settings.image_count_limit,
                    self.io.unwrap_ref().size_hint(),
                )?;
                tile.input.decoding_item = decoding_item;
                tiles.push(tile);
                progressive = progressive && derived_item.progressive;
            }

            if decoding_item == DecodingItem::COLOR && progressive {
                // Propagate the progressive status to the top-level item.
                self.items.get_mut(&item_id).unwrap().progressive = true;
            }
        }
        self.tile_info[decoding_item.usize()].tile_count = u32_from_usize(tiles.len())?;
        Ok(tiles)
    }

    fn harvest_cicp_from_sequence_header(&mut self) -> AvifResult<()> {
        let decoding_item = DecodingItem::COLOR;
        if self.tiles[decoding_item.usize()].is_empty() {
            return Ok(());
        }
        let mut search_size = 64;
        while search_size < 4096 {
            let tile_index = 0;
            self.prepare_sample(
                /*image_index=*/ 0,
                decoding_item,
                tile_index,
                Some(search_size),
            )?;
            let io = &mut self.io.unwrap_mut();
            let sample = &self.tiles[decoding_item.usize()][tile_index].input.samples[0];
            let item_data_buffer = if sample.item_id == 0 {
                &None
            } else {
                &self.items.get(&sample.item_id).unwrap().data_buffer
            };
            if let Ok(sequence_header) = Av1SequenceHeader::parse_from_obus(sample.partial_data(
                io,
                item_data_buffer,
                min(search_size, sample.size),
            )?) {
                self.image.color_primaries = sequence_header.color_primaries;
                self.image.transfer_characteristics = sequence_header.transfer_characteristics;
                self.image.matrix_coefficients = sequence_header.matrix_coefficients;
                self.image.yuv_range = sequence_header.yuv_range;
                break;
            }
            search_size += 64;
        }
        Ok(())
    }

    // Populates the source item ids for a derived image item.
    // These are the ids that are in the item's `dimg` box.
    fn populate_source_item_ids(&mut self, item_id: u32) -> AvifResult<()> {
        if !self.items.get(&item_id).unwrap().is_derived_image_item() {
            return Ok(());
        }

        let mut source_item_ids: Vec<u32> = vec![];
        let mut first_codec_config: Option<CodecConfiguration> = None;
        let mut first_icc: Option<Vec<u8>> = None;
        let mut first_nclx: Option<Nclx> = None;
        // Collect all the dimg items.
        for dimg_item_id in self.items.keys() {
            if *dimg_item_id == item_id {
                continue;
            }
            let dimg_item = self
                .items
                .get(dimg_item_id)
                .ok_or(AvifError::InvalidImageGrid("".into()))?;
            if dimg_item.dimg_for_id != item_id {
                continue;
            }
            if dimg_item.should_skip() {
                return Err(AvifError::NotImplemented);
            }
            if dimg_item.is_image_codec_item() {
                if first_codec_config.is_none() {
                    first_codec_config = Some(
                        dimg_item
                            .codec_config()
                            .ok_or(AvifError::BmffParseFailed(
                                "missing codec config property".into(),
                            ))?
                            .clone(),
                    );
                }
                if first_icc.is_none() {
                    first_icc = find_icc(&dimg_item.properties)?.cloned();
                }
                if first_nclx.is_none() {
                    first_nclx = find_nclx(&dimg_item.properties)?.cloned();
                }
            }
            source_item_ids.push(*dimg_item_id);
        }
        if source_item_ids.is_empty() {
            return Ok(());
        }
        // The order of derived item ids matters: sort them by dimg_index, which is the order that
        // items appear in the 'iref' box.
        source_item_ids.sort_by_key(|k| self.items.get(k).unwrap().dimg_index);
        let item = self.items.get_mut(&item_id).unwrap();
        item.source_item_ids = source_item_ids;
        if let Some(first_codec_config) = first_codec_config {
            // Adopt the configuration property of the first tile.
            // validate_properties() later makes sure they are all equal.
            item.properties
                .push(ItemProperty::CodecConfiguration(first_codec_config));
        }
        if item.is_grid_item() || item.is_overlay_item() {
            // For grid and overlay items, adopt the icc color profile and the nclx of the first
            // tile if it is not explicitly specified for the overall grid.
            if first_icc.is_some() && find_icc(&item.properties)?.is_none() {
                item.properties
                    .push(ItemProperty::ColorInformation(ColorInformation::Icc(
                        first_icc.unwrap(),
                    )));
            }
            if first_nclx.is_some() && find_nclx(&item.properties)?.is_none() {
                item.properties
                    .push(ItemProperty::ColorInformation(ColorInformation::Nclx(
                        first_nclx.unwrap(),
                    )));
            }
        }
        Ok(())
    }

    fn validate_source_items(&self, item_id: u32, tile_info: &TileInfo) -> AvifResult<()> {
        let item = self.items.get(&item_id).unwrap();
        let source_items: Vec<_> = item
            .source_item_ids
            .iter()
            .map(|id| self.items.get(id).unwrap())
            .collect();
        if item.is_grid_item() {
            let tile_count = tile_info.grid_tile_count()? as usize;
            if source_items.len() != tile_count {
                return Err(AvifError::InvalidImageGrid(
                    "expected number of tiles not found".into(),
                ));
            }
            if !source_items.iter().all(|item| item.is_image_codec_item()) {
                return Err(AvifError::InvalidImageGrid("invalid grid items".into()));
            }
        } else if item.is_overlay_item() {
            if source_items.is_empty() {
                return Err(AvifError::BmffParseFailed(
                    "no dimg items found for iovl".into(),
                ));
            }
            // MIAF allows overlays of grid but we don't support them.
            // See ISO/IEC 23000-12:2025, section 7.3.11.1.
            if source_items.iter().any(|item| item.is_grid_item()) {
                return Err(AvifError::NotImplemented);
            }
            if !source_items.iter().all(|item| item.is_image_codec_item()) {
                return Err(AvifError::InvalidImageGrid("invalid overlay items".into()));
            }
        } else if item.is_tone_mapped_item() {
            if source_items.len() != 2 {
                return Err(AvifError::InvalidToneMappedImage(
                    "expected tmap to have 2 dimg items".into(),
                ));
            }
            if !source_items
                .iter()
                .all(|item| item.is_image_codec_item() || item.is_grid_item())
            {
                return Err(AvifError::InvalidImageGrid("invalid tmap items".into()));
            }
        } else if item.is_sample_transform_item() {
            if source_items.len() > 32 {
                return Err(AvifError::InvalidImageGrid(
                    "expected sato to between 0 and 32 dimg items".into(),
                ));
            }
            if source_items.len() > DecodingItem::MAX_EXTRA_INPUTS {
                return Err(AvifError::NotImplemented);
            }
            if !source_items
                .iter()
                .all(|item| item.is_image_codec_item() || item.is_grid_item())
            {
                return Err(AvifError::InvalidImageGrid("invalid sato items".into()));
            }
        }
        Ok(())
    }

    // Finds the best item corresponding to the given item_id using the altr group if present
    // (finds the first supported alternative in the altr group). Parses the item and returns its
    // id, which may be different from the passed item_id if an altr group was used.
    fn find_and_parse_item(
        &mut self,
        item_id: u32,
        decoding_item: DecodingItem,
        ftyp: &FileTypeBox,
        meta: &MetaBox,
    ) -> AvifResult<u32> {
        let altr_group = meta
            .grpl
            .iter()
            .find(|g| g.grouping_type == "altr" && g.entity_ids.contains(&item_id));
        let item_ids = match altr_group {
            Some(altr_group) => &altr_group.entity_ids,
            None => &vec![item_id],
        };
        for item_id in item_ids {
            if let Some(item) = self.items.get(item_id) {
                if item.should_skip()
                    || !item.is_image_item()
                    || (item.is_tone_mapped_item() && !ftyp.has_tmap())
                {
                    continue;
                }
                match self.read_and_parse_item(*item_id, decoding_item) {
                    Ok(()) => return Ok(*item_id),
                    Err(AvifError::NotImplemented) => continue,
                    Err(err) => return Err(err),
                }
            }
        }
        Err(AvifError::NoContent)
    }

    fn reset(&mut self) {
        let decoder = Decoder::default();
        // Reset all fields to default except the following: settings, io, source.
        /* Do not reset 'settings' */
        self.image_count = decoder.image_count;
        self.image_index = decoder.image_index;
        self.image_timing = decoder.image_timing;
        self.timescale = decoder.timescale;
        self.duration_in_timescales = decoder.duration_in_timescales;
        self.duration = decoder.duration;
        self.repetition_count = decoder.repetition_count;
        self.gainmap = decoder.gainmap;
        self.gainmap_present = decoder.gainmap_present;
        self.image = decoder.image;
        self.extra_inputs = decoder.extra_inputs;
        /* Do not reset 'source' */
        self.tile_info = decoder.tile_info;
        self.tiles = decoder.tiles;
        self.items = decoder.items;
        self.tracks = decoder.tracks;
        /* Do not reset 'io' */
        self.codecs = decoder.codecs;
        self.color_track_id = decoder.color_track_id;
        self.parse_state = decoder.parse_state;
        self.io_stats = decoder.io_stats;
        self.compression_format = decoder.compression_format;
    }

    pub fn parse(&mut self) -> AvifResult<()> {
        if self.parsing_complete() {
            // Parse was called again. Reset the data and start over.
            self.parse_state = ParseState::None;
        }
        if self.io.is_none() {
            return Err(AvifError::IoNotSet);
        }

        if self.parse_state == ParseState::None {
            self.reset();
            let avif_boxes = mp4box::parse(self.io.unwrap_mut())?;
            self.tracks = avif_boxes.tracks;
            if !self.tracks.is_empty() {
                self.image.image_sequence_track_present = true;
                for track in &self.tracks {
                    if track.is_video_handler()
                        && !track.check_limits(
                            self.settings.image_size_limit,
                            self.settings.image_dimension_limit,
                        )
                    {
                        return Err(AvifError::BmffParseFailed(
                            "track dimension too large".into(),
                        ));
                    }
                }
            }
            self.items = construct_items(&avif_boxes.meta)?;
            if avif_boxes.ftyp.has_tmap() && !self.items.values().any(|x| x.item_type == "tmap") {
                return Err(AvifError::BmffParseFailed(
                    "tmap was required but not found".into(),
                ));
            }
            for item in self.items.values_mut() {
                item.harvest_ispe(
                    self.settings.strictness.alpha_ispe_required(),
                    self.settings.image_size_limit,
                    self.settings.image_dimension_limit,
                )?;
            }

            self.source = match self.settings.source {
                // Decide the source based on the major brand.
                Source::Auto => match avif_boxes.ftyp.major_brand.as_str() {
                    "avis" => Source::Tracks,
                    "avif" => Source::PrimaryItem,
                    _ => {
                        if self.tracks.is_empty() {
                            Source::PrimaryItem
                        } else {
                            Source::Tracks
                        }
                    }
                },
                Source::Tracks => Source::Tracks,
                Source::PrimaryItem => Source::PrimaryItem,
            };

            let color_properties: &Vec<ItemProperty>;
            let gainmap_properties: Option<&Vec<ItemProperty>>;
            let mut is_sample_transform = false;
            if self.source == Source::Tracks {
                let color_track = self
                    .tracks
                    .iter()
                    .find(|x| x.is_color())
                    .ok_or(AvifError::NoContent)?;
                if let Some(meta) = &color_track.meta {
                    let mut color_track_items = construct_items(meta)?;
                    Self::search_exif_or_xmp_metadata(
                        &mut color_track_items,
                        None,
                        &self.settings,
                        self.io.unwrap_mut(),
                        &mut self.image,
                    )?;
                }
                self.color_track_id = Some(color_track.id);
                color_properties = color_track
                    .get_properties()
                    .ok_or(AvifError::BmffParseFailed("".into()))?;
                gainmap_properties = None;

                self.tiles[DecodingItem::COLOR.usize()].push(Tile::create_from_track(
                    color_track,
                    self.settings.image_count_limit,
                    self.io.unwrap_ref().size_hint(),
                    DecodingItem::COLOR,
                )?);
                self.tile_info[DecodingItem::COLOR.usize()].tile_count = 1;

                if let Some(alpha_track) = self
                    .tracks
                    .iter()
                    .find(|x| x.is_aux(color_track.id) && x.is_auxiliary_alpha())
                {
                    self.tiles[DecodingItem::ALPHA.usize()].push(Tile::create_from_track(
                        alpha_track,
                        self.settings.image_count_limit,
                        self.io.unwrap_ref().size_hint(),
                        DecodingItem::ALPHA,
                    )?);
                    self.tile_info[DecodingItem::ALPHA.usize()].tile_count = 1;
                    self.image.alpha_present = true;
                    self.image.alpha_premultiplied = color_track.prem_by_id == Some(alpha_track.id);
                }

                self.image_index = -1;
                self.image_count = self.tiles[DecodingItem::COLOR.usize()][0]
                    .input
                    .samples
                    .len() as u32;
                self.timescale = color_track.media_timescale as u64;
                self.duration_in_timescales = color_track.media_duration;
                if self.timescale != 0 {
                    self.duration = (self.duration_in_timescales as f64) / (self.timescale as f64);
                } else {
                    self.duration = 0.0;
                }
                self.repetition_count = color_track.repetition_count()?;
                self.image_timing = Default::default();

                self.image.width = color_track.width;
                self.image.height = color_track.height;
            } else {
                assert_eq!(self.source, Source::PrimaryItem);
                let mut item_ids: [u32; DecodingItem::COUNT] = [0; DecodingItem::COUNT];

                // Mandatory color item (primary item).
                let primary_item_id = self.find_and_parse_item(
                    avif_boxes.meta.primary_item_id,
                    DecodingItem::COLOR,
                    &avif_boxes.ftyp,
                    &avif_boxes.meta,
                )?;
                item_ids[DecodingItem::COLOR.usize()] = primary_item_id;

                let primary_item = self.items.get(&primary_item_id).unwrap();
                if primary_item.is_tone_mapped_item() {
                    // validate_source_items() guarantees that tmap has two source item ids.
                    let base_item_id = primary_item.source_item_ids[0];
                    let gainmap_id = primary_item.source_item_ids[1];

                    // Set the color item it to the base image and reparse it.
                    item_ids[DecodingItem::COLOR.usize()] = base_item_id;
                    self.read_and_parse_item(base_item_id, DecodingItem::COLOR)?;

                    // Parse the gainmap making sure it's valid.
                    self.read_and_parse_item(gainmap_id, DecodingItem::GAINMAP)?;

                    self.validate_gainmap_item(
                        gainmap_id,
                        primary_item_id,
                        item_ids[DecodingItem::COLOR.usize()],
                    )?;
                    self.gainmap.metadata = self.tile_info[DecodingItem::COLOR.usize()]
                        .gainmap_metadata
                        .clone();
                    self.gainmap_present = true;

                    if self.settings.image_content_to_decode.gainmap() {
                        item_ids[DecodingItem::GAINMAP.usize()] = gainmap_id;
                    }
                }

                let mut alpha_present = false;
                let mut alpha_premultiplied = false;

                let primary_item = self.items.get(&primary_item_id).unwrap();
                if primary_item.is_sample_transform_item() {
                    let source_item_ids = primary_item.source_item_ids.clone();
                    for (idx, item_id) in source_item_ids.iter().enumerate() {
                        let decoding_item = DecodingItem::color(idx + 1);
                        item_ids[decoding_item.usize()] = *item_id;
                        self.read_and_parse_item(*item_id, decoding_item)?;
                        // Optional alpha auxiliary item
                        if let Some(alpha_item_id) = self.find_alpha_item(*item_id)? {
                            let alpha_decoding_item = DecodingItem::alpha(idx + 1);
                            if !self.items.get(&alpha_item_id).unwrap().is_made_up {
                                self.read_and_parse_item(alpha_item_id, alpha_decoding_item)?;
                            }
                            item_ids[alpha_decoding_item.usize()] = alpha_item_id;
                            let is_premultiplied =
                                self.items.get(item_id).unwrap().prem_by_id == alpha_item_id;
                            if idx > 0 && !alpha_present {
                                return Err(AvifError::InvalidImageGrid("input images for sato derived image item must either all have alpha or all not have alpha".into()));
                            }
                            if alpha_present && alpha_premultiplied != is_premultiplied {
                                return Err(AvifError::InvalidImageGrid("alpha for sato input images must all have the same premultiplication".into()));
                            }
                            alpha_present = true;
                            alpha_premultiplied = is_premultiplied;
                        } else if alpha_present {
                            return Err(AvifError::InvalidImageGrid("input images for sato derived image item must either all have alpha or all not have alpha".into()));
                        }
                        let item = self.items.get(item_id).unwrap();
                        self.extra_inputs[idx].width = item.width;
                        self.extra_inputs[idx].height = item.height;
                        let codec_config = item
                            .codec_config()
                            .ok_or(AvifError::BmffParseFailed("".into()))?;
                        self.extra_inputs[idx].depth = codec_config.depth();
                        self.extra_inputs[idx].yuv_format = codec_config.pixel_format();
                        self.extra_inputs[idx].chroma_sample_position =
                            codec_config.chroma_sample_position();
                    }
                    is_sample_transform = true;
                }

                // Find exif/xmp from meta if any.
                Self::search_exif_or_xmp_metadata(
                    &mut self.items,
                    Some(item_ids[DecodingItem::COLOR.usize()]),
                    &self.settings,
                    self.io.unwrap_mut(),
                    &mut self.image,
                )?;

                // Optional alpha auxiliary item
                if let Some(alpha_item_id) =
                    self.find_alpha_item(item_ids[DecodingItem::COLOR.usize()])?
                {
                    if !self.items.get(&alpha_item_id).unwrap().is_made_up {
                        self.read_and_parse_item(alpha_item_id, DecodingItem::ALPHA)?;
                    }
                    item_ids[DecodingItem::ALPHA.usize()] = alpha_item_id;
                    alpha_present = true;
                    alpha_premultiplied = self
                        .items
                        .get(&item_ids[DecodingItem::COLOR.usize()])
                        .unwrap()
                        .prem_by_id
                        == alpha_item_id
                }

                self.image_index = -1;
                self.image_count = 1;
                self.timescale = 1;
                self.duration = 1.0;
                self.duration_in_timescales = 1;
                self.image_timing.timescale = 1;
                self.image_timing.duration = 1.0;
                self.image_timing.duration_in_timescales = 1;

                for decoding_item in DecodingItem::ALL {
                    let item_id = item_ids[decoding_item.usize()];
                    if item_id == 0 {
                        continue;
                    }

                    let item = self.items.get(&item_id).unwrap();
                    if decoding_item == DecodingItem::ALPHA && item.width == 0 && item.height == 0 {
                        // NON-STANDARD: Alpha subimage does not have an ispe property; adopt
                        // width/height from color item.
                        assert!(!self.settings.strictness.alpha_ispe_required());
                        let color_item = self
                            .items
                            .get(&item_ids[DecodingItem::COLOR.usize()])
                            .unwrap();
                        let width = color_item.width;
                        let height = color_item.height;
                        let alpha_item = self.items.get_mut(&item_id).unwrap();
                        // Note: We cannot directly use color_item.width here because borrow
                        // checker won't allow that.
                        alpha_item.width = width;
                        alpha_item.height = height;
                    }

                    self.tiles[decoding_item.usize()] =
                        self.generate_tiles(item_id, decoding_item)?;
                    let item = self.items.get(&item_id).unwrap();
                    // Made up alpha item does not contain the pixi property. So do not try to
                    // validate it.
                    // Sample transforms can modify the bit depth of an item so it must be
                    // explicitly signalled.
                    let pixi_required = self.settings.strictness.pixi_required()
                        && !item.is_made_up
                        || item.is_sample_transform_item();
                    item.validate_properties(&self.items, pixi_required)?;
                }

                let color_item = self
                    .items
                    .get(&item_ids[DecodingItem::COLOR.usize()])
                    .unwrap();
                self.image.width = color_item.width;
                self.image.height = color_item.height;
                self.image.alpha_present = alpha_present;
                self.image.alpha_premultiplied = alpha_premultiplied;

                if color_item.progressive {
                    self.image.progressive_state = ProgressiveState::Available;
                    let sample_count = self.tiles[DecodingItem::COLOR.usize()][0]
                        .input
                        .samples
                        .len();
                    if sample_count > 1 {
                        self.image.progressive_state = ProgressiveState::Active;
                        self.image_count = sample_count as u32;
                    }
                }

                if item_ids[DecodingItem::GAINMAP.usize()] != 0 {
                    let gainmap_item = self
                        .items
                        .get(&item_ids[DecodingItem::GAINMAP.usize()])
                        .unwrap();
                    self.gainmap.image.width = gainmap_item.width;
                    self.gainmap.image.height = gainmap_item.height;
                    let codec_config = gainmap_item
                        .codec_config()
                        .ok_or(AvifError::BmffParseFailed("".into()))?;
                    self.gainmap.image.depth = codec_config.depth();
                    self.gainmap.image.yuv_format = codec_config.pixel_format();
                    self.gainmap.image.chroma_sample_position =
                        codec_config.chroma_sample_position();
                }

                // This borrow has to be in the end of this branch.
                color_properties = &self
                    .items
                    .get(&item_ids[DecodingItem::COLOR.usize()])
                    .unwrap()
                    .properties;
                gainmap_properties = if item_ids[DecodingItem::GAINMAP.usize()] != 0 {
                    Some(
                        &self
                            .items
                            .get(&item_ids[DecodingItem::GAINMAP.usize()])
                            .unwrap()
                            .properties,
                    )
                } else {
                    None
                };
            }

            // Check validity of samples.
            for tiles in &self.tiles {
                for tile in tiles {
                    for sample in &tile.input.samples {
                        if sample.size == 0 {
                            return Err(AvifError::BmffParseFailed(
                                "sample has invalid size.".into(),
                            ));
                        }
                        // The item_idx checks is to try to mimic libavif's behavior
                        // which only takes into account the size of the item whose id
                        // is in the pitm box.
                        if tile.input.decoding_item.item_idx <= 1 {
                            match tile.input.decoding_item.category {
                                Category::Color => {
                                    checked_incr!(self.io_stats.color_obu_size, sample.size)
                                }
                                Category::Alpha => {
                                    checked_incr!(self.io_stats.alpha_obu_size, sample.size)
                                }
                                _ => {}
                            }
                        }
                    }
                }
            }

            // Find and adopt all colr boxes "at most one for a given value of colour type"
            // (HEIF 6.5.5.1, from Amendment 3) Accept one of each type, and bail out if more than one
            // of a given type is provided.
            let mut cicp_set = false;

            if let Some(nclx) = find_nclx(color_properties)? {
                self.image.color_primaries = nclx.color_primaries;
                self.image.transfer_characteristics = nclx.transfer_characteristics;
                self.image.matrix_coefficients = nclx.matrix_coefficients;
                self.image.yuv_range = nclx.yuv_range;
                cicp_set = true;
            }
            if let Some(icc) = find_icc(color_properties)? {
                self.image.icc.clone_from(icc);
            }

            self.image.clli = find_property!(color_properties, ContentLightLevelInformation);
            self.image.pasp = find_property!(color_properties, PixelAspectRatio);
            self.image.clap = find_property!(color_properties, CleanAperture);
            self.image.irot_angle = find_property!(color_properties, ImageRotation);
            self.image.imir_axis = find_property!(color_properties, ImageMirror);

            if let Some(gainmap_properties) = gainmap_properties {
                // Ensure that the bitstream contains the same 'pasp', 'clap', 'irot and 'imir'
                // properties for both the base and gain map image items.
                if self.image.pasp != find_property!(gainmap_properties, PixelAspectRatio)
                    || self.image.clap != find_property!(gainmap_properties, CleanAperture)
                    || self.image.irot_angle != find_property!(gainmap_properties, ImageRotation)
                    || self.image.imir_axis != find_property!(gainmap_properties, ImageMirror)
                {
                    return Err(AvifError::DecodeGainMapFailed);
                }
            }

            let codec_config = find_property!(color_properties, CodecConfiguration)
                .ok_or(AvifError::BmffParseFailed("".into()))?;
            self.image.depth = codec_config.depth();
            // A sample transform item can have a depth different from its input images (which is where
            // the codec config comes from). The depth from the pixi property should be used instead.
            if is_sample_transform {
                if let Some(pixi) = find_property!(color_properties, PixelInformation) {
                    self.image.depth = pixi.plane_depths[0];
                }
            }

            self.image.yuv_format = codec_config.pixel_format();
            self.image.chroma_sample_position = codec_config.chroma_sample_position();
            self.compression_format = if codec_config.is_avif() {
                CompressionFormat::Avif
            } else {
                CompressionFormat::Heic
            };

            if cicp_set {
                self.parse_state = ParseState::Complete;
                return Ok(());
            }
            self.parse_state = ParseState::AwaitingSequenceHeader;
        }

        // If cicp was not set, try to harvest it from the sequence header.
        self.harvest_cicp_from_sequence_header()?;
        self.parse_state = ParseState::Complete;

        Ok(())
    }

    fn read_and_parse_item(&mut self, item_id: u32, decoding_item: DecodingItem) -> AvifResult<()> {
        if item_id == 0 {
            return Ok(());
        }
        self.populate_source_item_ids(item_id)?;
        self.items.get_mut(&item_id).unwrap().read_and_parse(
            self.io.unwrap_mut(),
            &mut self.tile_info[decoding_item.usize()],
            self.settings.image_size_limit,
            self.settings.image_dimension_limit,
        )?;
        self.validate_source_items(item_id, &self.tile_info[decoding_item.usize()])
    }

    fn can_use_single_codec(&self) -> AvifResult<bool> {
        let mut total_tile_count: usize = 0;
        for tiles in &self.tiles {
            total_tile_count = checked_add!(total_tile_count, tiles.len())?;
        }
        if total_tile_count == 1 {
            return Ok(true);
        }
        if self.image_count != 1 {
            return Ok(false);
        }
        let mut image_buffers = 0;
        let mut stolen_image_buffers = 0;
        for decoding_item in DecodingItem::ALL_USIZE {
            if self.tile_info[decoding_item].tile_count > 0 {
                image_buffers += 1;
            }
            if self.tile_info[decoding_item].tile_count == 1 {
                stolen_image_buffers += 1;
            }
        }
        if stolen_image_buffers > 0 && image_buffers > 1 {
            // Stealing will cause problems. So we need separate codec instances.
            return Ok(false);
        }
        let operating_point = self.tiles[0][0].operating_point;
        let all_layers = self.tiles[0][0].input.all_layers;
        for tiles in &self.tiles {
            for tile in tiles {
                if tile.operating_point != operating_point || tile.input.all_layers != all_layers {
                    return Ok(false);
                }
            }
        }
        Ok(true)
    }

    fn create_codec(&mut self, decoding_item: DecodingItem, tile_index: usize) -> AvifResult<()> {
        let tile = &self.tiles[decoding_item.usize()][tile_index];
        let mut codec: Codec = self
            .settings
            .codec_choice
            .get_codec(tile.codec_config.is_avif())?;
        let config = DecoderConfig {
            operating_point: tile.operating_point,
            all_layers: tile.input.all_layers,
            width: tile.width,
            height: tile.height,
            depth: self.image.depth,
            max_threads: self.settings.max_threads,
            image_size_limit: self.settings.image_size_limit,
            max_input_size: tile.max_sample_size(),
            codec_config: tile.codec_config.clone(),
            category: decoding_item.category,
            android_mediacodec_output_color_format: self
                .settings
                .android_mediacodec_output_color_format,
        };
        codec.initialize(&config)?;
        self.codecs.push(codec);
        Ok(())
    }

    fn create_codecs(&mut self) -> AvifResult<()> {
        if !self.codecs.is_empty() {
            return Ok(());
        }
        if matches!(self.source, Source::Tracks) || cfg!(feature = "android_mediacodec") {
            // In this case, there are two possibilities in the following order:
            //  1) If source is Tracks, then we will use at most two codec instances (one each for
            //     Color and Alpha). Gainmap will always be empty.
            //  2) If android_mediacodec is true, then we will use at most three codec instances
            //     (one for each category).
            self.codecs = create_vec_exact(3)?;
            for decoding_item in self.settings.image_content_to_decode.decoding_items() {
                if self.tiles[decoding_item.usize()].is_empty() {
                    continue;
                }
                self.create_codec(decoding_item, 0)?;
                for tile in &mut self.tiles[decoding_item.usize()] {
                    tile.codec_index = self.codecs.len() - 1;
                }
            }
        } else if self.can_use_single_codec()? {
            self.codecs = create_vec_exact(1)?;
            self.create_codec(DecodingItem::COLOR, 0)?;
            for tiles in &mut self.tiles {
                for tile in tiles {
                    tile.codec_index = 0;
                }
            }
        } else {
            self.codecs = create_vec_exact(self.tiles.iter().map(|tiles| tiles.len()).sum())?;
            for decoding_item in self.settings.image_content_to_decode.decoding_items() {
                for tile_index in 0..self.tiles[decoding_item.usize()].len() {
                    self.create_codec(decoding_item, tile_index)?;
                    self.tiles[decoding_item.usize()][tile_index].codec_index =
                        self.codecs.len() - 1;
                }
            }
        }
        Ok(())
    }

    fn prepare_sample(
        &mut self,
        image_index: usize,
        decoding_item: DecodingItem,
        tile_index: usize,
        max_num_bytes: Option<usize>, // Bytes read past that size will be ignored.
    ) -> AvifResult<()> {
        let tile = &mut self.tiles[decoding_item.usize()][tile_index];
        if tile.input.samples.len() <= image_index {
            return Err(AvifError::NoImagesRemaining);
        }
        let sample = &tile.input.samples[image_index];
        if sample.item_id == 0 {
            // Data comes from a track. Nothing to prepare.
            return Ok(());
        }
        // Data comes from an item.
        let item = self
            .items
            .get_mut(&sample.item_id)
            .ok_or(AvifError::BmffParseFailed("".into()))?;
        if item.extents.len() == 1 {
            if !item.idat.is_empty() {
                item.data_buffer = Some(item.idat.clone());
            }
            return Ok(());
        }
        if let Some(data) = &item.data_buffer {
            if data.len() == item.size {
                return Ok(()); // All extents have already been merged.
            }
            if max_num_bytes.is_some_and(|max_num_bytes| data.len() >= max_num_bytes) {
                return Ok(()); // Some sufficient extents have already been merged.
            }
        }
        // Item has multiple extents, merge them into a contiguous buffer.
        if item.data_buffer.is_none() {
            item.data_buffer = Some(create_vec_exact(item.size)?);
        }
        let data = item.data_buffer.unwrap_mut();
        let mut bytes_to_skip = data.len(); // These extents were already merged.
        for extent in &item.extents {
            if bytes_to_skip != 0 {
                checked_decr!(bytes_to_skip, extent.size);
                continue;
            }
            if item.idat.is_empty() {
                let io = self.io.unwrap_mut();
                data.extend_from_slice(io.read_exact(extent.offset, extent.size)?);
            } else {
                let offset = usize_from_u64(extent.offset)?;
                let end_offset = checked_add!(offset, extent.size)?;
                let range = offset..end_offset;
                check_slice_range(item.idat.len(), &range)?;
                data.extend_from_slice(&item.idat[range]);
            }
            if max_num_bytes.is_some_and(|max_num_bytes| data.len() >= max_num_bytes) {
                return Ok(()); // There are enough merged extents to satisfy max_num_bytes.
            }
        }
        assert_eq!(bytes_to_skip, 0);
        assert_eq!(data.len(), item.size);
        Ok(())
    }

    fn prepare_samples(&mut self, image_index: usize) -> AvifResult<()> {
        for decoding_item in self.settings.image_content_to_decode.decoding_items() {
            for tile_index in 0..self.tiles[decoding_item.usize()].len() {
                match (
                    self.settings.allow_progressive,
                    self.prepare_sample(image_index, decoding_item, tile_index, None),
                ) {
                    (_, Ok(_)) | (true, Err(AvifError::WaitingOnIo)) => continue,
                    (_, Err(err)) => return Err(err),
                }
            }
        }
        Ok(())
    }

    fn decode_tile(
        &mut self,
        image_index: usize,
        decoding_item: DecodingItem,
        tile_index: usize,
    ) -> AvifResult<()> {
        // Split the tiles array into two mutable arrays so that we can validate the
        // properties of tiles with index > 0 with that of the first tile.
        let (tiles_slice1, tiles_slice2) =
            self.tiles[decoding_item.usize()].split_at_mut(tile_index);
        let tile = &mut tiles_slice2[0];
        let sample = &tile.input.samples[image_index];
        let io = &mut self.io.unwrap_mut();
        let category = decoding_item.category;

        let codec = &mut self.codecs[tile.codec_index];
        let item_data_buffer = if sample.item_id == 0 {
            &None
        } else {
            &self.items.get(&sample.item_id).unwrap().data_buffer
        };
        let data = match (
            self.settings.allow_progressive,
            sample.data(io, item_data_buffer),
        ) {
            (_, Ok(data)) => data,
            (true, Err(AvifError::TruncatedData) | Err(AvifError::NoContent)) => {
                return Err(AvifError::WaitingOnIo)
            }
            (_, Err(err)) => return Err(err),
        };
        let next_image_result =
            codec.get_next_image(data, sample.spatial_id, &mut tile.image, category);
        if next_image_result.is_err() {
            if cfg!(feature = "android_mediacodec")
                && cfg!(feature = "heic")
                && tile.codec_config.is_heic()
                && category == Category::Alpha
            {
                // When decoding HEIC on Android, if the alpha channel decoding fails, simply
                // ignore it and return the rest of the image.
                checked_incr!(self.tile_info[decoding_item.usize()].decoded_tile_count, 1);
                return Ok(());
            } else {
                return next_image_result;
            }
        }

        checked_incr!(self.tile_info[decoding_item.usize()].decoded_tile_count, 1);

        if category == Category::Alpha && tile.image.yuv_range == YuvRange::Limited {
            tile.image.alpha_to_full_range()?;
        }
        tile.image.scale(tile.width, tile.height, category)?;

        let dst_image = match category {
            Category::Color | Category::Alpha if (decoding_item.item_idx == 0) => &mut self.image,
            Category::Color | Category::Alpha => &mut self.extra_inputs[decoding_item.item_idx - 1],
            Category::Gainmap => &mut self.gainmap.image,
        };

        if self.tile_info[decoding_item.usize()].is_grid() {
            if tile_index == 0 {
                let grid = &self.tile_info[decoding_item.usize()].grid;
                validate_grid_image_dimensions(&tile.image, grid)?;
                match category {
                    Category::Color | Category::Gainmap => {
                        dst_image.width = grid.width;
                        dst_image.height = grid.height;
                        dst_image.copy_properties_from(&tile.image, &tile.codec_config);
                        dst_image.allocate_planes(category)?;
                    }
                    Category::Alpha => {
                        // Alpha is always just one plane and the depth has been validated
                        // to be the same as the color planes' depth.
                        dst_image.allocate_planes(category)?;
                    }
                }
            }
            if !tiles_slice1.is_empty()
                && !tile
                    .image
                    .has_same_properties_and_cicp(&tiles_slice1[0].image)
            {
                return Err(AvifError::InvalidImageGrid(
                    "grid image contains mismatched tiles".into(),
                ));
            }

            dst_image.copy_from_tile(
                &tile.image,
                &self.tile_info[decoding_item.usize()].grid,
                tile_index as u32,
                category,
            )?;
        } else if self.tile_info[decoding_item.usize()].is_overlay() {
            if tile_index == 0 {
                let overlay = &self.tile_info[decoding_item.usize()].overlay;
                let canvas_fill_values =
                    dst_image.convert_rgba16_to_yuva(overlay.canvas_fill_value);
                match category {
                    Category::Color | Category::Gainmap => {
                        dst_image.width = overlay.width;
                        dst_image.height = overlay.height;
                        dst_image.copy_properties_from(&tile.image, &tile.codec_config);
                        dst_image
                            .allocate_planes_with_default_values(category, canvas_fill_values)?;
                    }
                    Category::Alpha => {
                        // Alpha is always just one plane and the depth has been validated
                        // to be the same as the color planes' depth.
                        dst_image
                            .allocate_planes_with_default_values(category, canvas_fill_values)?;
                    }
                }
            }
            if !tiles_slice1.is_empty() {
                let first_tile_image = &tiles_slice1[0].image;
                if tile.image.width != first_tile_image.width
                    || tile.image.height != first_tile_image.height
                    || tile.image.depth != first_tile_image.depth
                    || tile.image.yuv_format != first_tile_image.yuv_format
                    || tile.image.yuv_range != first_tile_image.yuv_range
                    || tile.image.color_primaries != first_tile_image.color_primaries
                    || tile.image.transfer_characteristics
                        != first_tile_image.transfer_characteristics
                    || tile.image.matrix_coefficients != first_tile_image.matrix_coefficients
                {
                    return Err(AvifError::InvalidImageGrid(
                        "overlay image contains mismatched tiles".into(),
                    ));
                }
            }
            dst_image.copy_and_overlay_from_tile(
                &tile.image,
                &self.tile_info[decoding_item.usize()],
                tile_index as u32,
                category,
            )?;
        } else {
            // Non grid/overlay path, steal or copy planes from the only tile.
            match category {
                Category::Color | Category::Gainmap => {
                    dst_image.width = tile.image.width;
                    dst_image.height = tile.image.height;
                    dst_image.copy_properties_from(&tile.image, &tile.codec_config);
                    dst_image.steal_or_copy_planes_from(&tile.image, category)?;
                }
                Category::Alpha => {
                    if !dst_image.has_same_properties(&tile.image) {
                        return Err(AvifError::DecodeAlphaFailed);
                    }
                    dst_image.steal_or_copy_planes_from(&tile.image, category)?;
                }
            }
        }
        Ok(())
    }

    fn decode_grid(&mut self, image_index: usize, decoding_item: DecodingItem) -> AvifResult<()> {
        let tile_count = self.tiles[decoding_item.usize()].len();
        if tile_count == 0 {
            return Ok(());
        }
        let previous_decoded_tile_count =
            self.tile_info[decoding_item.usize()].decoded_tile_count as usize;
        let mut payloads = vec![];
        let mut pending_read = false;
        for tile_index in previous_decoded_tile_count..tile_count {
            let tile = &self.tiles[decoding_item.usize()][tile_index];
            let sample = &tile.input.samples[image_index];
            let item_data_buffer = if sample.item_id == 0 {
                &None
            } else {
                &self.items.get(&sample.item_id).unwrap().data_buffer
            };
            let io = &mut self.io.unwrap_mut();
            let data = match sample.data(io, item_data_buffer) {
                Ok(data) => data,
                Err(AvifError::WaitingOnIo) => {
                    if self.settings.allow_incremental {
                        if payloads.is_empty() {
                            // No cells have been read. Nothing to decode.
                            return Err(AvifError::WaitingOnIo);
                        } else {
                            // One or more cells have been read. Decode them.
                            pending_read = true;
                            break;
                        }
                    } else {
                        return Err(AvifError::WaitingOnIo);
                    }
                }
                Err(err) => return Err(err),
            };
            payloads.push(data.to_vec());
        }
        let grid = &self.tile_info[decoding_item.usize()].grid;
        // If we are not doing incremental decode, all the cells must have been read.
        if !self.settings.allow_incremental
            && checked_mul!(grid.rows, grid.columns)? != payloads.len() as u32
        {
            return Err(AvifError::InvalidArgument);
        }
        let first_tile = &self.tiles[decoding_item.usize()][previous_decoded_tile_count];
        let category = decoding_item.category;
        let mut grid_image_helper = GridImageHelper {
            grid,
            image: if category == Category::Gainmap {
                &mut self.gainmap.image
            } else {
                &mut self.image
            },
            category,
            cell_index: previous_decoded_tile_count,
            expected_cell_count: previous_decoded_tile_count + payloads.len(),
            codec_config: &first_tile.codec_config,
            first_cell_image: None,
            tile_width: first_tile.width,
            tile_height: first_tile.height,
        };
        let codec = &mut self.codecs[first_tile.codec_index];
        let next_image_result = codec.get_next_image_grid(
            &payloads,
            first_tile.input.samples[image_index].spatial_id,
            &mut grid_image_helper,
        );
        if next_image_result.is_err() {
            if cfg!(feature = "android_mediacodec")
                && cfg!(feature = "heic")
                && first_tile.codec_config.is_heic()
                && category == Category::Alpha
            {
                // When decoding HEIC on Android, if the alpha channel decoding fails, simply
                // ignore it and return the rest of the image.
            } else {
                return next_image_result;
            }
        }
        if !grid_image_helper.is_grid_complete()? {
            return Err(AvifError::UnknownError(
                "codec did not decode all cells".into(),
            ));
        }
        checked_incr!(
            self.tile_info[decoding_item.usize()].decoded_tile_count,
            u32_from_usize(payloads.len())?
        );
        if pending_read {
            Err(AvifError::WaitingOnIo)
        } else {
            Ok(())
        }
    }

    fn apply_sample_transform(&mut self) -> AvifResult<()> {
        #[cfg(feature = "sample_transform")]
        return self.tile_info[DecodingItem::COLOR.usize()]
            .sample_transform
            .allocate_planes_and_apply(&self.extra_inputs, &mut self.image);
        #[cfg(not(feature = "sample_transform"))]
        return Err(AvifError::NotImplemented);
    }

    fn can_use_decode_grid(&self, decoding_item: DecodingItem) -> bool {
        let first_tile = &self.tiles[decoding_item.usize()][0];
        let codec = self.codecs[first_tile.codec_index].codec();
        // Has to be a grid.
        self.tile_info[decoding_item.usize()].is_grid()
            // Has to be one of the supported codecs.
            && matches!(codec, CodecChoice::MediaCodec | CodecChoice::Dav1d)
            // All the tiles must use the same codec instance.
            && self.tiles[decoding_item.usize()][1..]
                .iter()
                .all(|x| x.codec_index == first_tile.codec_index)
    }

    fn decode_tiles(&mut self, image_index: usize) -> AvifResult<()> {
        let mut decoded_something = false;
        for decoding_item in self.settings.image_content_to_decode.decoding_items() {
            let tile_count = self.tiles[decoding_item.usize()].len();
            if tile_count == 0 {
                continue;
            }
            if self.can_use_decode_grid(decoding_item) {
                self.decode_grid(image_index, decoding_item)?;
                decoded_something = true;
            } else {
                let previous_decoded_tile_count =
                    self.tile_info[decoding_item.usize()].decoded_tile_count as usize;
                for tile_index in previous_decoded_tile_count..tile_count {
                    self.decode_tile(image_index, decoding_item, tile_index)?;
                    decoded_something = true;
                }
            }
        }
        if decoded_something {
            Ok(())
        } else {
            Err(AvifError::NoContent)
        }
    }

    pub fn next_image(&mut self) -> AvifResult<()> {
        if self.io.is_none() {
            return Err(AvifError::IoNotSet);
        }
        if !self.parsing_complete() {
            return Err(AvifError::NoContent);
        }
        if self.is_current_frame_fully_decoded() {
            for decoding_item in DecodingItem::ALL_USIZE {
                self.tile_info[decoding_item].decoded_tile_count = 0;
            }
        }

        let next_image_index = checked_add!(self.image_index, 1)?;
        self.create_codecs()?;
        match (
            self.settings.allow_progressive,
            self.prepare_samples(next_image_index as usize),
        ) {
            (_, Ok(_)) | (true, Err(AvifError::WaitingOnIo)) => {}
            (_, Err(err)) => return Err(err),
        }
        self.decode_tiles(next_image_index as usize)?;

        if !self.tile_info[DecodingItem::COLOR.usize()]
            .sample_transform
            .tokens
            .is_empty()
        {
            self.apply_sample_transform()?;
        }

        self.image_index = next_image_index;
        self.image_timing = self.nth_image_timing(self.image_index as u32)?;
        Ok(())
    }

    fn is_current_frame_fully_decoded(&self) -> bool {
        if !self.parsing_complete() {
            return false;
        }
        for decoding_item in self.settings.image_content_to_decode.decoding_items() {
            if !self.tile_info[decoding_item.usize()].is_fully_decoded() {
                return false;
            }
        }
        true
    }

    pub fn nth_image(&mut self, index: u32) -> AvifResult<()> {
        if !self.parsing_complete() {
            return Err(AvifError::NoContent);
        }
        if index >= self.image_count {
            return Err(AvifError::NoImagesRemaining);
        }
        let requested_index = i32_from_u32(index)?;
        if requested_index == checked_add!(self.image_index, 1)? {
            return self.next_image();
        }
        if requested_index == self.image_index && self.is_current_frame_fully_decoded() {
            // Current frame which is already fully decoded has been requested. Do nothing.
            return Ok(());
        }
        let nearest_keyframe = i32_from_u32(self.nearest_keyframe(index))?;
        if nearest_keyframe > checked_add!(self.image_index, 1)?
            || requested_index <= self.image_index
        {
            // Start decoding from the nearest keyframe.
            self.image_index = nearest_keyframe - 1;
        }
        loop {
            self.next_image()?;
            if requested_index == self.image_index {
                break;
            }
        }
        Ok(())
    }

    pub fn image(&self) -> Option<&Image> {
        if self.parsing_complete() {
            Some(&self.image)
        } else {
            None
        }
    }

    pub fn nth_image_timing(&self, n: u32) -> AvifResult<ImageTiming> {
        if !self.parsing_complete() {
            return Err(AvifError::NoContent);
        }
        if let Some(limit) = self.settings.image_count_limit {
            if n > limit.get() {
                return Err(AvifError::NoImagesRemaining);
            }
        }
        if self.color_track_id.is_none() {
            return Ok(self.image_timing);
        }
        let color_track_id = self.color_track_id.unwrap();
        let color_track = self
            .tracks
            .iter()
            .find(|x| x.id == color_track_id)
            .ok_or(AvifError::NoContent)?;
        if color_track.sample_table.is_none() {
            return Ok(self.image_timing);
        }
        color_track.image_timing(n)
    }

    // When next_image() or nth_image() returns AvifResult::WaitingOnIo, this function can be called
    // next to retrieve the number of top rows that can be immediately accessed from the luma plane
    // of decoder->image, and alpha if any. The corresponding rows from the chroma planes,
    // if any, can also be accessed (half rounded up if subsampled, same number of rows otherwise).
    // If a gain map is present, and image_content_to_decode contains ImageContentType::GainMap,
    // the gain map's planes can also be accessed in the same way.
    // The number of available gain map rows is at least:
    //   decoder.decoded_row_count() * decoder.gainmap.image.height / decoder.image.height
    // When gain map scaling is needed, callers might choose to use a few less rows depending on how
    // many rows are needed by the scaling algorithm, to avoid the last row(s) changing when more
    // data becomes available. allow_incremental must be set to true before calling next_image() or
    // nth_image(). Returns decoder.image.height when the last call to next_image() or nth_image()
    // returned AvifResult::Ok. Returns 0 in all other cases.
    pub fn decoded_row_count(&self) -> u32 {
        let mut min_row_count = self.image.height;
        for decoding_item in DecodingItem::ALL {
            let decoding_item_usize = decoding_item.usize();
            if self.tiles[decoding_item_usize].is_empty() {
                continue;
            }
            let first_tile_height = self.tiles[decoding_item_usize][0].height;
            let row_count = if decoding_item.category == Category::Gainmap
                && self.gainmap_present()
                && self.settings.image_content_to_decode.gainmap()
                && self.gainmap.image.height != 0
                && self.gainmap.image.height != self.image.height
            {
                if self.tile_info[decoding_item_usize].is_fully_decoded() {
                    self.image.height
                } else {
                    let gainmap_row_count = self.tile_info[decoding_item_usize]
                        .decoded_row_count(self.gainmap.image.height, first_tile_height);
                    // row_count fits for sure in 32 bits because heights do.
                    let row_count = (gainmap_row_count as u64 * self.image.height as u64
                        / self.gainmap.image.height as u64)
                        as u32;

                    // Make sure it satisfies the C API guarantee.
                    assert!(
                        gainmap_row_count
                            >= (row_count as f32 / self.image.height as f32
                                * self.gainmap.image.height as f32)
                                .round() as u32
                    );
                    row_count
                }
            } else {
                self.tile_info[decoding_item_usize]
                    .decoded_row_count(self.image.height, first_tile_height)
            };
            min_row_count = std::cmp::min(min_row_count, row_count);
        }
        min_row_count
    }

    pub fn is_keyframe(&self, index: u32) -> bool {
        if !self.parsing_complete() {
            return false;
        }
        let index = index as usize;
        // All the tiles for the requested index must be a keyframe.
        for decoding_item in DecodingItem::ALL_USIZE {
            for tile in &self.tiles[decoding_item] {
                if index >= tile.input.samples.len() || !tile.input.samples[index].sync {
                    return false;
                }
            }
        }
        true
    }

    pub fn nearest_keyframe(&self, mut index: u32) -> u32 {
        if !self.parsing_complete() {
            return 0;
        }
        while index != 0 {
            if self.is_keyframe(index) {
                return index;
            }
            index -= 1;
        }
        assert!(self.is_keyframe(0));
        0
    }

    pub fn nth_image_max_extent(&self, index: u32) -> AvifResult<Extent> {
        if !self.parsing_complete() {
            return Err(AvifError::NoContent);
        }
        let mut extent = Extent::default();
        let start_index = self.nearest_keyframe(index) as usize;
        let end_index = index as usize;
        for current_index in start_index..=end_index {
            for decoding_item in DecodingItem::ALL_USIZE {
                for tile in &self.tiles[decoding_item] {
                    if current_index >= tile.input.samples.len() {
                        return Err(AvifError::NoImagesRemaining);
                    }
                    let sample = &tile.input.samples[current_index];
                    let sample_extent = if sample.item_id != 0 {
                        let item = self.items.get(&sample.item_id).unwrap();
                        item.max_extent(sample)?
                    } else {
                        Extent {
                            offset: sample.offset,
                            size: sample.size,
                        }
                    };
                    extent.merge(&sample_extent)?;
                }
            }
        }
        Ok(extent)
    }

    pub fn peek_compatible_file_type(data: &[u8]) -> bool {
        mp4box::peek_compatible_file_type(data).unwrap_or(false)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_case::test_case;

    #[test_case(10, 20, 50, 100, 10, 140 ; "case 1")]
    #[test_case(100, 20, 50, 100, 50, 100 ; "case 2")]
    fn merge_extents(
        offset1: u64,
        size1: usize,
        offset2: u64,
        size2: usize,
        expected_offset: u64,
        expected_size: usize,
    ) {
        let mut e1 = Extent {
            offset: offset1,
            size: size1,
        };
        let e2 = Extent {
            offset: offset2,
            size: size2,
        };
        assert!(e1.merge(&e2).is_ok());
        assert_eq!(e1.offset, expected_offset);
        assert_eq!(e1.size, expected_size);
    }

    #[test]
    fn decoding_item_usize() {
        assert_eq!(
            DecodingItem::ALL.map(|c| c.usize()),
            DecodingItem::ALL_USIZE
        );
    }
}
