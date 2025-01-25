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

pub mod gainmap;
pub mod item;
pub mod tile;
pub mod track;

use crate::decoder::gainmap::*;
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

pub trait IO {
    fn read(&mut self, offset: u64, max_read_size: usize) -> AvifResult<&[u8]>;
    fn size_hint(&self) -> u64;
    fn persistent(&self) -> bool;
}

impl dyn IO {
    pub fn read_exact(&mut self, offset: u64, read_size: usize) -> AvifResult<&[u8]> {
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
pub type Codec = Box<dyn crate::codecs::Decoder>;

#[derive(Debug, Default)]
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
    pub fn color_and_alpha(&self) -> bool {
        matches!(self, Self::ColorAndAlpha | Self::All)
    }
    pub fn gainmap(&self) -> bool {
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
    pub image_size_limit: u32,
    pub image_dimension_limit: u32,
    pub image_count_limit: u32,
    pub max_threads: u32,
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
            image_size_limit: DEFAULT_IMAGE_SIZE_LIMIT,
            image_dimension_limit: DEFAULT_IMAGE_DIMENSION_LIMIT,
            image_count_limit: DEFAULT_IMAGE_COUNT_LIMIT,
            max_threads: 1,
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
    pub fn pixi_required(&self) -> bool {
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

    pub fn alpha_ispe_required(&self) -> bool {
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
    source: Source,
    tile_info: [TileInfo; Category::COUNT],
    tiles: [Vec<Tile>; Category::COUNT],
    items: Items,
    tracks: Vec<Track>,
    // To replicate the C-API, we need to keep this optional. Otherwise this
    // could be part of the initialization.
    io: Option<GenericIO>,
    codecs: Vec<Codec>,
    color_track_id: Option<u32>,
    parse_state: ParseState,
    io_stats: IOStats,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Category {
    #[default]
    Color,
    Alpha,
    Gainmap,
}

impl Category {
    const COUNT: usize = 3;
    const ALL: [Category; Category::COUNT] = [Self::Color, Self::Alpha, Self::Gainmap];
    const ALL_USIZE: [usize; Category::COUNT] = [0, 1, 2];

    pub fn usize(self) -> usize {
        match self {
            Category::Color => 0,
            Category::Alpha => 1,
            Category::Gainmap => 2,
        }
    }

    pub fn planes(&self) -> &[Plane] {
        match self {
            Category::Alpha => &A_PLANE,
            _ => &YUV_PLANES,
        }
    }
}

macro_rules! find_property {
    ($properties:expr, $property_name:ident) => {
        $properties.iter().find_map(|p| match p {
            ItemProperty::$property_name(value) => Some(value.clone()),
            _ => None,
        })
    };
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
        if color_item.item_type != "grid" || color_item.grid_item_ids.is_empty() {
            return Ok(None);
        }
        // If color item is a grid, check if there is an alpha channel which is represented as an
        // auxl item to each color tile item.
        let mut alpha_item_indices: Vec<u32> = create_vec_exact(color_item.grid_item_ids.len())?;
        for color_grid_item_id in &color_item.grid_item_ids {
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

        // Make up an alpha item for convenience.
        // TODO(yguyon): Find another item id if max is used.
        let alpha_item_id = self
            .items
            .keys()
            .max()
            .unwrap()
            .checked_add(1)
            .ok_or(AvifError::NotImplemented)?;
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
            grid_item_ids: alpha_item_indices,
            properties,
            is_made_up: true,
            ..Item::default()
        };
        self.tile_info[Category::Alpha.usize()].grid = self.tile_info[Category::Color.usize()].grid;
        self.items.insert(alpha_item_id, alpha_item);
        Ok(Some(alpha_item_id))
    }

    // returns (tone_mapped_image_item_id, gain_map_item_id) if found
    fn find_tone_mapped_image_item(&self, color_item_id: u32) -> AvifResult<Option<(u32, u32)>> {
        let tmap_items: Vec<_> = self.items.values().filter(|x| x.is_tmap()).collect();
        for item in tmap_items {
            let dimg_items: Vec<_> = self
                .items
                .values()
                .filter(|x| x.dimg_for_id == item.id)
                .collect();
            if dimg_items.len() != 2 {
                return Err(AvifError::InvalidToneMappedImage(
                    "Expected tmap to have 2 dimg items".into(),
                ));
            }
            let item0 = if dimg_items[0].dimg_index == 0 { dimg_items[0] } else { dimg_items[1] };
            if item0.id != color_item_id {
                continue;
            }
            let item1 = if dimg_items[0].dimg_index == 0 { dimg_items[1] } else { dimg_items[0] };
            return Ok(Some((item.id, item1.id)));
        }
        Ok(None)
    }

    // returns (tone_mapped_image_item_id, gain_map_item_id) if found
    fn find_gainmap_item(&self, color_item_id: u32) -> AvifResult<Option<(u32, u32)>> {
        if let Some((tonemap_id, gainmap_id)) = self.find_tone_mapped_image_item(color_item_id)? {
            let gainmap_item = self
                .items
                .get(&gainmap_id)
                .ok_or(AvifError::InvalidToneMappedImage("".into()))?;
            if gainmap_item.should_skip() {
                return Err(AvifError::InvalidToneMappedImage("".into()));
            }
            Ok(Some((tonemap_id, gainmap_id)))
        } else {
            Ok(None)
        }
    }

    fn validate_gainmap_item(&mut self, gainmap_id: u32, tonemap_id: u32) -> AvifResult<()> {
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
        if find_property!(tonemap_item.properties, PixelAspectRatio).is_some()
            || find_property!(tonemap_item.properties, CleanAperture).is_some()
            || find_property!(tonemap_item.properties, ImageRotation).is_some()
            || find_property!(tonemap_item.properties, ImageMirror).is_some()
        {
            return Err(AvifError::InvalidToneMappedImage("".into()));
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

    fn generate_tiles(&mut self, item_id: u32, category: Category) -> AvifResult<Vec<Tile>> {
        let mut tiles: Vec<Tile> = Vec::new();
        let item = self
            .items
            .get(&item_id)
            .ok_or(AvifError::MissingImageItem)?;
        if item.grid_item_ids.is_empty() {
            if item.size == 0 {
                return Err(AvifError::MissingImageItem);
            }
            let mut tile = Tile::create_from_item(
                self.items.get_mut(&item_id).unwrap(),
                self.settings.allow_progressive,
                self.settings.image_count_limit,
                self.io.unwrap_ref().size_hint(),
            )?;
            tile.input.category = category;
            tiles.push(tile);
        } else {
            if !self.tile_info[category.usize()].is_grid() {
                return Err(AvifError::InvalidImageGrid(
                    "dimg items were found but image is not grid.".into(),
                ));
            }
            let mut progressive = true;
            for grid_item_id in item.grid_item_ids.clone() {
                let grid_item = self
                    .items
                    .get_mut(&grid_item_id)
                    .ok_or(AvifError::InvalidImageGrid("missing grid item".into()))?;
                let mut tile = Tile::create_from_item(
                    grid_item,
                    self.settings.allow_progressive,
                    self.settings.image_count_limit,
                    self.io.unwrap_ref().size_hint(),
                )?;
                tile.input.category = category;
                tiles.push(tile);
                progressive = progressive && grid_item.progressive;
            }

            if category == Category::Color && progressive {
                // Propagate the progressive status to the top-level grid item.
                self.items.get_mut(&item_id).unwrap().progressive = true;
            }
        }
        self.tile_info[category.usize()].tile_count = u32_from_usize(tiles.len())?;
        Ok(tiles)
    }

    fn harvest_cicp_from_sequence_header(&mut self) -> AvifResult<()> {
        let category = Category::Color;
        if self.tiles[category.usize()].is_empty() {
            return Ok(());
        }
        let mut search_size = 64;
        while search_size < 4096 {
            let tile_index = 0;
            self.prepare_sample(
                /*image_index=*/ 0,
                category,
                tile_index,
                Some(search_size),
            )?;
            let io = &mut self.io.unwrap_mut();
            let sample = &self.tiles[category.usize()][tile_index].input.samples[0];
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

    fn populate_grid_item_ids(&mut self, item_id: u32, category: Category) -> AvifResult<()> {
        if self.items.get(&item_id).unwrap().item_type != "grid" {
            return Ok(());
        }
        let tile_count = self.tile_info[category.usize()].grid_tile_count()? as usize;
        let mut grid_item_ids: Vec<u32> = create_vec_exact(tile_count)?;
        let mut first_codec_config: Option<CodecConfiguration> = None;
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
            if dimg_item.item_type != "av01" || dimg_item.has_unsupported_essential_property {
                return Err(AvifError::InvalidImageGrid(
                    "invalid input item in dimg grid".into(),
                ));
            }
            if first_codec_config.is_none() {
                // Adopt the configuration property of the first tile.
                // validate_properties() makes sure they are all equal.
                first_codec_config = Some(
                    dimg_item
                        .codec_config()
                        .ok_or(AvifError::BmffParseFailed(
                            "missing codec config property".into(),
                        ))?
                        .clone(),
                );
            }
            if grid_item_ids.len() >= tile_count {
                return Err(AvifError::InvalidImageGrid(
                    "Expected number of tiles not found".into(),
                ));
            }
            grid_item_ids.push(*dimg_item_id);
        }
        if grid_item_ids.len() != tile_count {
            return Err(AvifError::InvalidImageGrid(
                "Expected number of tiles not found".into(),
            ));
        }
        // ISO/IEC 23008-12: The input images are inserted in row-major order,
        // top-row first, left to right, in the order of SingleItemTypeReferenceBox of type 'dimg'
        // for this derived image item within the ItemReferenceBox.
        // Sort the grid items by dimg_index. dimg_index is the order in which the items appear in
        // the 'iref' box.
        grid_item_ids.sort_by_key(|k| self.items.get(k).unwrap().dimg_index);
        let item = self.items.get_mut(&item_id).unwrap();
        item.properties.push(ItemProperty::CodecConfiguration(
            first_codec_config.unwrap(),
        ));
        item.grid_item_ids = grid_item_ids;
        Ok(())
    }

    fn reset(&mut self) {
        let decoder = Decoder::default();
        // Reset all fields to default except the following: settings, io, source.
        self.image_count = decoder.image_count;
        self.image_timing = decoder.image_timing;
        self.timescale = decoder.timescale;
        self.duration_in_timescales = decoder.duration_in_timescales;
        self.duration = decoder.duration;
        self.repetition_count = decoder.repetition_count;
        self.gainmap = decoder.gainmap;
        self.gainmap_present = decoder.gainmap_present;
        self.image = decoder.image;
        self.tile_info = decoder.tile_info;
        self.tiles = decoder.tiles;
        self.image_index = decoder.image_index;
        self.items = decoder.items;
        self.tracks = decoder.tracks;
        self.codecs = decoder.codecs;
        self.color_track_id = decoder.color_track_id;
        self.parse_state = decoder.parse_state;
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
                    if !track.check_limits(
                        self.settings.image_size_limit,
                        self.settings.image_dimension_limit,
                    ) {
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

                self.tiles[Category::Color.usize()].push(Tile::create_from_track(
                    color_track,
                    self.settings.image_count_limit,
                    self.io.unwrap_ref().size_hint(),
                    Category::Color,
                )?);
                self.tile_info[Category::Color.usize()].tile_count = 1;

                if let Some(alpha_track) = self.tracks.iter().find(|x| x.is_aux(color_track.id)) {
                    self.tiles[Category::Alpha.usize()].push(Tile::create_from_track(
                        alpha_track,
                        self.settings.image_count_limit,
                        self.io.unwrap_ref().size_hint(),
                        Category::Alpha,
                    )?);
                    self.tile_info[Category::Alpha.usize()].tile_count = 1;
                    self.image.alpha_present = true;
                    self.image.alpha_premultiplied = color_track.prem_by_id == Some(alpha_track.id);
                }

                self.image_index = -1;
                self.image_count =
                    self.tiles[Category::Color.usize()][0].input.samples.len() as u32;
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
                let mut item_ids: [u32; Category::COUNT] = [0; Category::COUNT];

                // Mandatory color item (primary item).
                let color_item_id = self
                    .items
                    .iter()
                    .find(|x| {
                        !x.1.should_skip()
                            && x.1.id != 0
                            && x.1.id == avif_boxes.meta.primary_item_id
                    })
                    .map(|it| *it.0);

                item_ids[Category::Color.usize()] = color_item_id.ok_or(AvifError::NoContent)?;
                self.read_and_parse_item(item_ids[Category::Color.usize()], Category::Color)?;
                self.populate_grid_item_ids(item_ids[Category::Color.usize()], Category::Color)?;

                // Find exif/xmp from meta if any.
                Self::search_exif_or_xmp_metadata(
                    &mut self.items,
                    Some(item_ids[Category::Color.usize()]),
                    &self.settings,
                    self.io.unwrap_mut(),
                    &mut self.image,
                )?;

                // Optional alpha auxiliary item
                if let Some(alpha_item_id) =
                    self.find_alpha_item(item_ids[Category::Color.usize()])?
                {
                    if !self.items.get(&alpha_item_id).unwrap().is_made_up {
                        self.read_and_parse_item(alpha_item_id, Category::Alpha)?;
                        self.populate_grid_item_ids(alpha_item_id, Category::Alpha)?;
                    }
                    item_ids[Category::Alpha.usize()] = alpha_item_id;
                }

                // Optional gainmap item
                if avif_boxes.ftyp.has_tmap() {
                    if let Some((tonemap_id, gainmap_id)) =
                        self.find_gainmap_item(item_ids[Category::Color.usize()])?
                    {
                        let tonemap_item = self
                            .items
                            .get_mut(&tonemap_id)
                            .ok_or(AvifError::InvalidToneMappedImage("".into()))?;
                        let mut stream = tonemap_item.stream(self.io.unwrap_mut())?;
                        if let Some(metadata) = mp4box::parse_tmap(&mut stream)? {
                            self.gainmap.metadata = metadata;
                            self.read_and_parse_item(gainmap_id, Category::Gainmap)?;
                            self.populate_grid_item_ids(gainmap_id, Category::Gainmap)?;
                            self.validate_gainmap_item(gainmap_id, tonemap_id)?;
                            self.gainmap_present = true;
                            if self.settings.image_content_to_decode.gainmap() {
                                item_ids[Category::Gainmap.usize()] = gainmap_id;
                            }
                        }
                    }
                }

                self.image_index = -1;
                self.image_count = 1;
                self.timescale = 1;
                self.duration = 1.0;
                self.duration_in_timescales = 1;
                self.image_timing.timescale = 1;
                self.image_timing.duration = 1.0;
                self.image_timing.duration_in_timescales = 1;

                for category in Category::ALL {
                    let item_id = item_ids[category.usize()];
                    if item_id == 0 {
                        continue;
                    }

                    let item = self.items.get(&item_id).unwrap();
                    if category == Category::Alpha && item.width == 0 && item.height == 0 {
                        // NON-STANDARD: Alpha subimage does not have an ispe property; adopt
                        // width/height from color item.
                        assert!(!self.settings.strictness.alpha_ispe_required());
                        let color_item =
                            self.items.get(&item_ids[Category::Color.usize()]).unwrap();
                        let width = color_item.width;
                        let height = color_item.height;
                        let alpha_item = self.items.get_mut(&item_id).unwrap();
                        // Note: We cannot directly use color_item.width here because borrow
                        // checker won't allow that.
                        alpha_item.width = width;
                        alpha_item.height = height;
                    }

                    self.tiles[category.usize()] = self.generate_tiles(item_id, category)?;
                    let item = self.items.get(&item_id).unwrap();
                    // Made up alpha item does not contain the pixi property. So do not try to
                    // validate it.
                    let pixi_required =
                        self.settings.strictness.pixi_required() && !item.is_made_up;
                    item.validate_properties(&self.items, pixi_required)?;
                }

                let color_item = self.items.get(&item_ids[Category::Color.usize()]).unwrap();
                self.image.width = color_item.width;
                self.image.height = color_item.height;
                self.image.alpha_present = item_ids[Category::Alpha.usize()] != 0;
                // alphapremultiplied.

                if color_item.progressive {
                    self.image.progressive_state = ProgressiveState::Available;
                    let sample_count = self.tiles[Category::Color.usize()][0].input.samples.len();
                    if sample_count > 1 {
                        self.image.progressive_state = ProgressiveState::Active;
                        self.image_count = sample_count as u32;
                    }
                }

                if item_ids[Category::Gainmap.usize()] != 0 {
                    let gainmap_item = self
                        .items
                        .get(&item_ids[Category::Gainmap.usize()])
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
                    .get(&item_ids[Category::Color.usize()])
                    .unwrap()
                    .properties;
                gainmap_properties = if item_ids[Category::Gainmap.usize()] != 0 {
                    Some(
                        &self
                            .items
                            .get(&item_ids[Category::Gainmap.usize()])
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
                        match tile.input.category {
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
            self.image.yuv_format = codec_config.pixel_format();
            self.image.chroma_sample_position = codec_config.chroma_sample_position();

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

    fn read_and_parse_item(&mut self, item_id: u32, category: Category) -> AvifResult<()> {
        if item_id == 0 {
            return Ok(());
        }
        self.items.get_mut(&item_id).unwrap().read_and_parse(
            self.io.unwrap_mut(),
            &mut self.tile_info[category.usize()].grid,
            self.settings.image_size_limit,
            self.settings.image_dimension_limit,
        )
    }

    fn can_use_single_codec(&self) -> AvifResult<bool> {
        let total_tile_count = checked_add!(
            checked_add!(self.tiles[0].len(), self.tiles[1].len())?,
            self.tiles[2].len()
        )?;
        if total_tile_count == 1 {
            return Ok(true);
        }
        if self.image_count != 1 {
            return Ok(false);
        }
        let mut image_buffers = 0;
        let mut stolen_image_buffers = 0;
        for category in Category::ALL_USIZE {
            if self.tile_info[category].tile_count > 0 {
                image_buffers += 1;
            }
            if self.tile_info[category].tile_count == 1 {
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

    fn create_codec(&mut self, category: Category, tile_index: usize) -> AvifResult<()> {
        let tile = &self.tiles[category.usize()][tile_index];
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
            max_input_size: tile.max_sample_size(),
            codec_config: tile.codec_config.clone(),
            category,
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
            for category in Category::ALL {
                if self.tiles[category.usize()].is_empty() {
                    continue;
                }
                self.create_codec(category, 0)?;
                for tile in &mut self.tiles[category.usize()] {
                    tile.codec_index = self.codecs.len() - 1;
                }
            }
        } else if self.can_use_single_codec()? {
            self.codecs = create_vec_exact(1)?;
            self.create_codec(Category::Color, 0)?;
            for tiles in &mut self.tiles {
                for tile in tiles {
                    tile.codec_index = 0;
                }
            }
        } else {
            self.codecs = create_vec_exact(self.tiles.iter().map(|tiles| tiles.len()).sum())?;
            for category in Category::ALL {
                for tile_index in 0..self.tiles[category.usize()].len() {
                    self.create_codec(category, tile_index)?;
                    self.tiles[category.usize()][tile_index].codec_index = self.codecs.len() - 1;
                }
            }
        }
        Ok(())
    }

    fn prepare_sample(
        &mut self,
        image_index: usize,
        category: Category,
        tile_index: usize,
        max_num_bytes: Option<usize>, // Bytes read past that size will be ignored.
    ) -> AvifResult<()> {
        let tile = &mut self.tiles[category.usize()][tile_index];
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
            // Item has only one extent. Nothing to prepare.
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
            let io = self.io.unwrap_mut();
            data.extend_from_slice(io.read_exact(extent.offset, extent.size)?);
            if max_num_bytes.is_some_and(|max_num_bytes| data.len() >= max_num_bytes) {
                return Ok(()); // There are enough merged extents to satisfy max_num_bytes.
            }
        }
        assert_eq!(bytes_to_skip, 0);
        assert_eq!(data.len(), item.size);
        Ok(())
    }

    fn prepare_samples(&mut self, image_index: usize) -> AvifResult<()> {
        for category in Category::ALL {
            for tile_index in 0..self.tiles[category.usize()].len() {
                self.prepare_sample(image_index, category, tile_index, None)?;
            }
        }
        Ok(())
    }

    fn validate_grid_image_dimensions(image: &Image, grid: &Grid) -> AvifResult<()> {
        if checked_mul!(image.width, grid.columns)? < grid.width
            || checked_mul!(image.height, grid.rows)? < grid.height
        {
            return Err(AvifError::InvalidImageGrid(
                        "Grid image tiles do not completely cover the image (HEIF (ISO/IEC 23008-12:2017), Section 6.6.2.3.1)".into(),
                    ));
        }
        if checked_mul!(image.width, grid.columns)? < grid.width
            || checked_mul!(image.height, grid.rows)? < grid.height
        {
            return Err(AvifError::InvalidImageGrid(
                "Grid image tiles do not completely cover the image (HEIF (ISO/IEC 23008-12:2017), \
                    Section 6.6.2.3.1)"
                    .into(),
            ));
        }
        if checked_mul!(image.width, grid.columns - 1)? >= grid.width
            || checked_mul!(image.height, grid.rows - 1)? >= grid.height
        {
            return Err(AvifError::InvalidImageGrid(
                "Grid image tiles in the rightmost column and bottommost row do not overlap the \
                     reconstructed image grid canvas. See MIAF (ISO/IEC 23000-22:2019), Section \
                     7.3.11.4.2, Figure 2"
                    .into(),
            ));
        }
        // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
        //   - the tile_width shall be greater than or equal to 64, and should be a multiple of 64
        //   - the tile_height shall be greater than or equal to 64, and should be a multiple of 64
        // The "should" part is ignored here.
        if image.width < 64 || image.height < 64 {
            return Err(AvifError::InvalidImageGrid(format!(
                "Grid image tile width ({}) or height ({}) cannot be smaller than 64. See MIAF \
                     (ISO/IEC 23000-22:2019), Section 7.3.11.4.2",
                image.width, image.height
            )));
        }
        // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
        //   - when the images are in the 4:2:2 chroma sampling format the horizontal tile offsets
        //     and widths, and the output width, shall be even numbers;
        //   - when the images are in the 4:2:0 chroma sampling format both the horizontal and
        //     vertical tile offsets and widths, and the output width and height, shall be even
        //     numbers.
        if ((image.yuv_format == PixelFormat::Yuv420 || image.yuv_format == PixelFormat::Yuv422)
            && (grid.width % 2 != 0 || image.width % 2 != 0))
            || (image.yuv_format == PixelFormat::Yuv420
                && (grid.height % 2 != 0 || image.height % 2 != 0))
        {
            return Err(AvifError::InvalidImageGrid(format!(
                "Grid image width ({}) or height ({}) or tile width ({}) or height ({}) shall be \
                    even if chroma is subsampled in that dimension. See MIAF \
                    (ISO/IEC 23000-22:2019), Section 7.3.11.4.2",
                grid.width, grid.height, image.width, image.height
            )));
        }
        Ok(())
    }

    fn decode_tile(
        &mut self,
        image_index: usize,
        category: Category,
        tile_index: usize,
    ) -> AvifResult<()> {
        // Split the tiles array into two mutable arrays so that we can validate the
        // properties of tiles with index > 0 with that of the first tile.
        let (tiles_slice1, tiles_slice2) = self.tiles[category.usize()].split_at_mut(tile_index);
        let tile = &mut tiles_slice2[0];
        let sample = &tile.input.samples[image_index];
        let io = &mut self.io.unwrap_mut();

        let codec = &mut self.codecs[tile.codec_index];
        let item_data_buffer = if sample.item_id == 0 {
            &None
        } else {
            &self.items.get(&sample.item_id).unwrap().data_buffer
        };
        let data = sample.data(io, item_data_buffer)?;
        codec.get_next_image(data, sample.spatial_id, &mut tile.image, category)?;
        checked_incr!(self.tile_info[category.usize()].decoded_tile_count, 1);

        if category == Category::Alpha && tile.image.yuv_range == YuvRange::Limited {
            tile.image.alpha_to_full_range()?;
        }
        tile.image.scale(tile.width, tile.height, category)?;

        if self.tile_info[category.usize()].is_grid() {
            if tile_index == 0 {
                let grid = &self.tile_info[category.usize()].grid;
                Self::validate_grid_image_dimensions(&tile.image, grid)?;
                match category {
                    Category::Color => {
                        self.image.width = grid.width;
                        self.image.height = grid.height;
                        // Adopt the yuv_format and depth.
                        self.image.yuv_format = tile.image.yuv_format;
                        self.image.depth = tile.image.depth;
                        self.image.allocate_planes(category)?;
                    }
                    Category::Alpha => {
                        // Alpha is always just one plane and the depth has been validated
                        // to be the same as the color planes' depth.
                        self.image.allocate_planes(category)?;
                    }
                    Category::Gainmap => {
                        self.gainmap.image.width = grid.width;
                        self.gainmap.image.height = grid.height;
                        // Adopt the yuv_format and depth.
                        self.gainmap.image.yuv_format = tile.image.yuv_format;
                        self.gainmap.image.depth = tile.image.depth;
                        self.gainmap.image.allocate_planes(category)?;
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
                        "grid image contains mismatched tiles".into(),
                    ));
                }
            }
            match category {
                Category::Gainmap => self.gainmap.image.copy_from_tile(
                    &tile.image,
                    &self.tile_info[category.usize()],
                    tile_index as u32,
                    category,
                )?,
                _ => {
                    self.image.copy_from_tile(
                        &tile.image,
                        &self.tile_info[category.usize()],
                        tile_index as u32,
                        category,
                    )?;
                }
            }
        } else {
            // Non grid path, steal or copy planes from the only tile.
            match category {
                Category::Color => {
                    self.image.width = tile.image.width;
                    self.image.height = tile.image.height;
                    self.image.depth = tile.image.depth;
                    self.image.yuv_format = tile.image.yuv_format;
                    self.image.steal_or_copy_from(&tile.image, category)?;
                }
                Category::Alpha => {
                    if !self.image.has_same_properties(&tile.image) {
                        return Err(AvifError::DecodeAlphaFailed);
                    }
                    self.image.steal_or_copy_from(&tile.image, category)?;
                }
                Category::Gainmap => {
                    self.gainmap.image.width = tile.image.width;
                    self.gainmap.image.height = tile.image.height;
                    self.gainmap.image.depth = tile.image.depth;
                    self.gainmap.image.yuv_format = tile.image.yuv_format;
                    self.gainmap
                        .image
                        .steal_or_copy_from(&tile.image, category)?;
                }
            }
        }
        Ok(())
    }

    fn decode_tiles(&mut self, image_index: usize) -> AvifResult<()> {
        let mut decoded_something = false;
        for category in Category::ALL {
            if !self.settings.image_content_to_decode.color_and_alpha()
                && (category == Category::Color || category == Category::Alpha)
            {
                continue;
            }

            let previous_decoded_tile_count =
                self.tile_info[category.usize()].decoded_tile_count as usize;
            let tile_count = self.tiles[category.usize()].len();
            for tile_index in previous_decoded_tile_count..tile_count {
                self.decode_tile(image_index, category, tile_index)?;
                decoded_something = true;
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
            for category in Category::ALL_USIZE {
                self.tile_info[category].decoded_tile_count = 0;
            }
        }

        let next_image_index = checked_add!(self.image_index, 1)?;
        self.create_codecs()?;
        self.prepare_samples(next_image_index as usize)?;
        self.decode_tiles(next_image_index as usize)?;
        self.image_index = next_image_index;
        self.image_timing = self.nth_image_timing(self.image_index as u32)?;
        Ok(())
    }

    fn is_current_frame_fully_decoded(&self) -> bool {
        if !self.parsing_complete() {
            return false;
        }
        for category in Category::ALL_USIZE {
            if !self.tile_info[category].is_fully_decoded() {
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
        if n > self.settings.image_count_limit {
            return Err(AvifError::NoImagesRemaining);
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
        for category in Category::ALL_USIZE {
            if self.tiles[category].is_empty() {
                continue;
            }
            let first_tile_height = self.tiles[category][0].height;
            let row_count = if category == Category::Gainmap.usize()
                && self.gainmap_present()
                && self.settings.image_content_to_decode.gainmap()
                && self.gainmap.image.height != 0
                && self.gainmap.image.height != self.image.height
            {
                if self.tile_info[category].is_fully_decoded() {
                    self.image.height
                } else {
                    let gainmap_row_count = self.tile_info[category]
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
                self.tile_info[category].decoded_row_count(self.image.height, first_tile_height)
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
        for category in Category::ALL_USIZE {
            for tile in &self.tiles[category] {
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
            for category in Category::ALL_USIZE {
                for tile in &self.tiles[category] {
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
}
