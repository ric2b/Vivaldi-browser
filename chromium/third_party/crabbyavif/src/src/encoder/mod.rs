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

pub mod item;
pub mod mini;
pub mod mp4box;
mod sampletransform;

use crate::encoder::item::*;
use crate::encoder::mp4box::*;

use crate::codecs::EncoderConfig;
use crate::gainmap::GainMap;
use crate::image::*;
use crate::internal_utils::stream::IStream;
use crate::internal_utils::stream::OStream;
use crate::internal_utils::*;
use crate::parser::exif;
use crate::parser::mp4box::*;
use crate::parser::obu::Av1SequenceHeader;
use crate::utils::clap::CropRect;
use crate::utils::IFraction;
use crate::*;

#[cfg(feature = "aom")]
use crate::codecs::aom::Aom;

use std::collections::HashMap;
use std::fmt;
use std::time::SystemTime;
use std::time::UNIX_EPOCH;

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub struct ScalingMode {
    pub horizontal: IFraction,
    pub vertical: IFraction,
}

impl Default for ScalingMode {
    fn default() -> Self {
        Self {
            horizontal: IFraction(1, 1),
            vertical: IFraction(1, 1),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum TilingMode {
    Auto,
    Manual(i32, i32), // tile_rows_log2, tile_columns_log2
}

impl Default for TilingMode {
    fn default() -> Self {
        Self::Manual(0, 0)
    }
}

impl TilingMode {
    fn log2(&self, width: u32, height: u32) -> (i32, i32) {
        match *self {
            Self::Auto => {
                let image_area = width * height;
                let tiles_log2 =
                    floor_log2(std::cmp::min(image_area.div_ceil(512 * 512), 8)) as i32;
                let (dim1, dim2) = if width >= height { (width, height) } else { (height, width) };
                let diff_log2 = floor_log2(dim1 / dim2) as i32;
                let diff = std::cmp::max(0, tiles_log2 - diff_log2);
                let dim2_log2 = diff / 2;
                let dim1_log2 = tiles_log2 - dim2_log2;
                if width >= height {
                    (dim2_log2, dim1_log2)
                } else {
                    (dim1_log2, dim2_log2)
                }
            }
            Self::Manual(rows_log2, columns_log2) => (rows_log2, columns_log2),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct MutableSettings {
    pub quality: i32,
    pub quality_alpha: i32,
    pub quality_gainmap: i32,
    pub tiling_mode: TilingMode,
    pub scaling_mode: ScalingMode,
}

impl Default for MutableSettings {
    fn default() -> Self {
        Self {
            quality: 60,
            quality_alpha: 60,
            quality_gainmap: 60,
            tiling_mode: Default::default(),
            scaling_mode: Default::default(),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Settings {
    pub threads: u32,
    pub speed: Option<u32>,
    pub header_format: HeaderFormat,
    pub keyframe_interval: i32,
    pub timescale: u64,
    pub repetition_count: RepetitionCount,
    pub extra_layer_count: u32,
    pub sample_transform_recipe: SampleTransformRecipe,
    pub mutable: MutableSettings,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            threads: 1,
            speed: None,
            header_format: HeaderFormat::default(),
            keyframe_interval: 0,
            timescale: 1,
            repetition_count: RepetitionCount::Infinite,
            extra_layer_count: 0,
            sample_transform_recipe: SampleTransformRecipe::None,
            mutable: Default::default(),
        }
    }
}

impl Settings {
    pub(crate) fn is_valid(&self) -> bool {
        self.extra_layer_count < MAX_AV1_LAYER_COUNT as u32 && self.timescale > 0
    }
}

#[derive(Debug, Default)]
pub(crate) struct Sample {
    pub data: Vec<u8>,
    pub sync: bool,
}

impl Sample {
    // This function is not used in all configurations.
    #[allow(dead_code)]
    pub(crate) fn create_from(data: &[u8], sync: bool) -> AvifResult<Self> {
        let mut copied_data: Vec<u8> = create_vec_exact(data.len())?;
        copied_data.extend_from_slice(data);
        Ok(Sample {
            data: copied_data,
            sync,
        })
    }
}

pub(crate) type Codec = Box<dyn crate::codecs::Encoder>;

// If Category is None, the option applies to all categories. If Category is some, it only
// applies to that category.
pub(crate) type CodecSpecificOptions = HashMap<(Option<Category>, String), String>;

#[derive(Default)]
#[allow(dead_code)]
pub struct Encoder {
    settings: Settings,
    items: Vec<Item>,
    image_metadata: Image,
    gainmap_image_metadata: Image,
    alt_image_metadata: Image,
    quantizer: i32,
    primary_item_id: u16,
    alternative_item_ids: Vec<u16>,
    single_image: bool,
    alpha_present: bool,
    image_item_type: String,
    config_property_name: String,
    duration_in_timescales: Vec<u64>,
    codec_specific_options: CodecSpecificOptions,
}

impl Encoder {
    pub fn create_with_settings(settings: &Settings) -> AvifResult<Self> {
        if !settings.is_valid() {
            return AvifError::invalid_argument();
        }
        Ok(Self {
            settings: *settings,
            ..Default::default()
        })
    }

    pub fn update_settings(&mut self, mutable: &MutableSettings) -> AvifResult<()> {
        self.settings.mutable = *mutable;
        Ok(())
    }

    pub fn set_codec_specific_option(
        &mut self,
        category: Option<Category>,
        key: String,
        value: String,
    ) {
        self.codec_specific_options.insert((category, key), value);
    }

    pub(crate) fn is_sequence(&self) -> bool {
        self.settings.extra_layer_count == 0 && self.duration_in_timescales.len() > 1
    }

    fn add_tmap_item(&mut self, gainmap: &GainMap) -> AvifResult<u16> {
        let item = Item {
            id: u16_from_usize(self.items.len() + 1)?,
            item_type: "tmap".into(),
            infe_name: Category::Gainmap.infe_name(),
            category: Category::Color,
            metadata_payload: write_tmap(&gainmap.metadata)?,
            ..Default::default()
        };
        let item_id = item.id;
        self.items.push(item);
        Ok(item_id)
    }

    fn add_items(&mut self, grid: &Grid, category: Category, hidden: bool) -> AvifResult<u16> {
        let cell_count = usize_from_u32(grid.rows * grid.columns)?;
        let mut top_level_item_id = 0;
        if cell_count > 1 {
            let mut stream = OStream::default();
            write_grid(&mut stream, grid)?;
            let grid_item = Item {
                id: u16_from_usize(self.items.len() + 1)?,
                item_type: "grid".into(),
                infe_name: category.infe_name(),
                category,
                grid: Some(*grid),
                metadata_payload: stream.data,
                hidden_image: hidden,
                ..Default::default()
            };
            top_level_item_id = grid_item.id;
            self.items.push(grid_item);
        }
        for cell_index in 0..cell_count {
            let item = Item {
                id: u16_from_usize(self.items.len() + 1)?,
                item_type: "av01".into(),
                infe_name: category.infe_name(),
                cell_index,
                category,
                dimg_from_id: if cell_count > 1 { Some(top_level_item_id) } else { None },
                hidden_image: hidden || cell_count > 1,
                extra_layer_count: self.settings.extra_layer_count,
                #[cfg(feature = "aom")]
                codec: Some(Box::<Aom>::default()),
                ..Default::default()
            };
            if cell_count == 1 {
                top_level_item_id = item.id;
            }
            self.items.push(item);
        }
        Ok(top_level_item_id)
    }

    fn add_exif_item(&mut self) -> AvifResult<()> {
        if self.image_metadata.exif.is_empty() {
            return Ok(());
        }
        let mut stream = IStream::create(&self.image_metadata.exif);
        let tiff_header_offset = exif::parse_exif_tiff_header_offset(&mut stream)?;
        let mut metadata_payload: Vec<u8> = create_vec_exact(4 + self.image_metadata.exif.len())?;
        metadata_payload.extend_from_slice(&tiff_header_offset.to_be_bytes());
        metadata_payload.extend_from_slice(&self.image_metadata.exif);
        self.items.push(Item {
            id: u16_from_usize(self.items.len() + 1)?,
            item_type: "Exif".into(),
            infe_name: "Exif".into(),
            iref_to_id: Some(self.primary_item_id),
            iref_type: Some("cdsc".into()),
            metadata_payload,
            ..Default::default()
        });
        Ok(())
    }

    fn add_xmp_item(&mut self) -> AvifResult<()> {
        if self.image_metadata.xmp.is_empty() {
            return Ok(());
        }
        self.items.push(Item {
            id: u16_from_usize(self.items.len() + 1)?,
            item_type: "mime".into(),
            infe_name: "XMP".into(),
            infe_content_type: "application/rdf+xml".into(),
            iref_to_id: Some(self.primary_item_id),
            iref_type: Some("cdsc".into()),
            metadata_payload: self.image_metadata.xmp.clone(),
            ..Default::default()
        });
        Ok(())
    }

    fn copy_alt_image_metadata(&mut self, gainmap: &GainMap, grid: &Grid) {
        self.alt_image_metadata.width = grid.width;
        self.alt_image_metadata.height = grid.height;
        self.alt_image_metadata.icc = gainmap.alt_icc.clone();
        self.alt_image_metadata.color_primaries = gainmap.alt_color_primaries;
        self.alt_image_metadata.transfer_characteristics = gainmap.alt_transfer_characteristics;
        self.alt_image_metadata.matrix_coefficients = gainmap.alt_matrix_coefficients;
        self.alt_image_metadata.yuv_range = gainmap.alt_yuv_range;
        self.alt_image_metadata.depth = if gainmap.alt_plane_depth > 0 {
            gainmap.alt_plane_depth
        } else {
            std::cmp::max(self.image_metadata.depth, gainmap.image.depth)
        };
        self.alt_image_metadata.yuv_format = if gainmap.alt_plane_count == 1 {
            PixelFormat::Yuv400
        } else {
            PixelFormat::Yuv444
        };
        self.alt_image_metadata.clli = Some(gainmap.alt_clli);
    }

    fn validate_image_grid(
        grid: &Grid,
        images: &[&Image],
        recipe: SampleTransformRecipe,
    ) -> AvifResult<()> {
        let first_image = images[0];
        let last_image = images.last().unwrap();
        for (index, image) in images.iter().enumerate() {
            if !matches!(
                (image.depth, recipe),
                (8 | 10 | 12, SampleTransformRecipe::None)
                    | (16, SampleTransformRecipe::BitDepthExtension8b8b)
            ) {
                return AvifError::invalid_argument();
            }
            let expected_width = if grid.is_last_column(index as u32) {
                last_image.width
            } else {
                first_image.width
            };
            let expected_height = if grid.is_last_row(index as u32) {
                last_image.height
            } else {
                first_image.height
            };
            if image.width != expected_width
                || image.height != expected_height
                || !image.has_same_cicp(first_image)
                || image.has_alpha() != first_image.has_alpha()
                || image.alpha_premultiplied != first_image.alpha_premultiplied
            {
                return AvifError::invalid_image_grid("all cells do not have the same properties");
            }
            if image.matrix_coefficients == MatrixCoefficients::Identity
                && image.yuv_format != PixelFormat::Yuv444
            {
                return AvifError::invalid_argument();
            }
            if !image.has_plane(Plane::Y) {
                return AvifError::no_content();
            }
        }
        if last_image.width > first_image.width || last_image.height > first_image.height {
            return AvifError::invalid_image_grid("last cell was larger than the first cell");
        }
        if images.len() > 1 {
            validate_grid_image_dimensions(first_image, grid)?;
        }
        if let Some(clap) = &first_image.clap {
            if !CropRect::create_from(clap, grid.width, grid.height, first_image.yuv_format)?
                .is_valid(grid.width, grid.height, first_image.yuv_format)
            {
                return AvifError::invalid_argument();
            }
        }
        Ok(())
    }

    fn validate_gainmap_grid(grid: &Grid, gainmaps: &[&GainMap]) -> AvifResult<()> {
        for gainmap in &gainmaps[1..] {
            if gainmaps[0] != *gainmap {
                return AvifError::invalid_image_grid(
                    "all cells should have the same gain map metadata",
                );
            }
        }
        if gainmaps[0].image.color_primaries != ColorPrimaries::Unspecified
            || gainmaps[0].image.transfer_characteristics != TransferCharacteristics::Unspecified
        {
            return AvifError::invalid_argument();
        }
        let gainmap_images: Vec<_> = gainmaps.iter().map(|x| &x.image).collect();
        Self::validate_image_grid(grid, &gainmap_images, SampleTransformRecipe::None)?;
        // Ensure that the gainmap image does not have alpha. validate_image_grid() ensures that
        // either all the cell images have alpha or all of them don't. So it is sufficient to check
        // if the first cell image does not have alpha.
        if gainmap_images[0].has_alpha() {
            return AvifError::invalid_argument();
        }
        Ok(())
    }

    fn add_image_impl(
        &mut self,
        grid_columns: u32,
        grid_rows: u32,
        cell_images: &[&Image],
        mut duration: u64,
        is_single_image: bool,
        gainmaps: Option<&[&GainMap]>,
    ) -> AvifResult<()> {
        let cell_count: usize = usize_from_u32(grid_rows * grid_columns)?;
        if cell_count == 0 || cell_images.len() != cell_count {
            return AvifError::invalid_argument();
        }
        if duration == 0 {
            duration = 1;
        }
        if self.items.is_empty() {
            let first_image = cell_images[0];
            let last_image = cell_images.last().unwrap();
            let grid = Grid {
                rows: grid_rows,
                columns: grid_columns,
                width: (grid_columns - 1) * first_image.width + last_image.width,
                height: (grid_rows - 1) * first_image.height + last_image.height,
            };
            Self::validate_image_grid(&grid, cell_images, self.settings.sample_transform_recipe)?;
            self.image_metadata = first_image.shallow_clone();
            if let Some(gainmaps) = gainmaps {
                self.gainmap_image_metadata = gainmaps[0].image.shallow_clone();
                self.copy_alt_image_metadata(gainmaps[0], &grid);
            }
            let color_item_id = self.add_items(&grid, Category::Color, /*hidden=*/ false)?;
            self.primary_item_id = color_item_id;
            self.alpha_present = first_image.has_plane(Plane::A)
                && if is_single_image {
                    // When encoding a single image in which the alpha plane exists but is entirely
                    // opaque, skip writing an alpha AV1 payload. This does not apply to image
                    // sequences since subsequent frames may have a non-opaque alpha channel.
                    !cell_images.iter().all(|image| image.is_opaque())
                } else {
                    true
                };

            if self.alpha_present {
                let alpha_item_id =
                    self.add_items(&grid, Category::Alpha, /*hidden=*/ false)?;
                let alpha_item = &mut self.items[alpha_item_id as usize - 1];
                alpha_item.iref_type = Some(String::from("auxl"));
                alpha_item.iref_to_id = Some(color_item_id);
                if self.image_metadata.alpha_premultiplied {
                    let color_item = &mut self.items[color_item_id as usize - 1];
                    color_item.iref_type = Some(String::from("prem"));
                    color_item.iref_to_id = Some(alpha_item_id);
                }
            }
            if let Some(gainmaps) = gainmaps {
                if gainmaps.len() != cell_images.len() {
                    return AvifError::invalid_image_grid("invalid number of gainmap images");
                }
                let first_gainmap_image = &gainmaps[0].image;
                let last_gainmap_image = &gainmaps.last().unwrap().image;
                let gainmap_grid = Grid {
                    rows: grid_rows,
                    columns: grid_columns,
                    width: (grid_columns - 1) * first_gainmap_image.width
                        + last_gainmap_image.width,
                    height: (grid_rows - 1) * first_gainmap_image.height
                        + last_gainmap_image.height,
                };
                Self::validate_gainmap_grid(&gainmap_grid, gainmaps)?;
                let tonemap_item_id = self.add_tmap_item(gainmaps[0])?;
                if !self.alternative_item_ids.is_empty() {
                    return AvifError::unknown_error("");
                }
                self.alternative_item_ids.push(tonemap_item_id);
                self.alternative_item_ids.push(color_item_id);
                let gainmap_item_id =
                    self.add_items(&gainmap_grid, Category::Gainmap, /*hidden=*/ true)?;
                for item_id in [color_item_id, gainmap_item_id] {
                    self.items[item_id as usize - 1].dimg_from_id = Some(tonemap_item_id);
                }
            }

            match self.settings.sample_transform_recipe {
                SampleTransformRecipe::None => {}
                SampleTransformRecipe::BitDepthExtension8b8b => {
                    if first_image.depth != 16 || gainmaps.is_some() {
                        return AvifError::not_implemented();
                    }
                    self.create_bit_depth_extension_items(&grid)?;
                }
            }

            self.add_exif_item()?;
            self.add_xmp_item()?;
        } else {
            if gainmaps.is_some() {
                return AvifError::not_implemented();
            }
            // Another frame in an image sequence, or layer in a layered image.
            let first_image = cell_images[0];
            if !first_image.has_same_cicp(&self.image_metadata)
                || first_image.alpha_premultiplied != self.image_metadata.alpha_premultiplied
                // If the previously added image had an alpha channel, then this image should have
                // it too. The reverse need not be true as we will simply ignore the alpha channel
                // of the current image in that case.
                || (self.image_metadata.alpha_present && !first_image.alpha_present)
            {
                return AvifError::invalid_argument();
            }
        }

        let (tile_rows_log2, tile_columns_log2) = self
            .settings
            .mutable
            .tiling_mode
            .log2(cell_images[0].width, cell_images[0].height);
        // Encode the AV1 OBUs.
        for item in &mut self.items {
            if item.codec.is_none() {
                continue;
            }
            let mut image = match item.category {
                Category::Gainmap => &gainmaps.unwrap()[item.cell_index].image,
                _ => cell_images[item.cell_index],
            };
            let first_image = match item.category {
                Category::Gainmap => &gainmaps.unwrap()[0].image,
                _ => cell_images[0],
            };
            let mut padded_image;
            if image.width != first_image.width || image.height != first_image.height {
                // Pad the right-most and/or bottom-most tiles so that all tiles share the same dimensions.
                padded_image = first_image.shallow_clone();
                padded_image.copy_and_pad(image)?;
                image = &padded_image;
            }
            let mut quality = match item.category {
                Category::Color => self.settings.mutable.quality,
                Category::Alpha => self.settings.mutable.quality_alpha,
                Category::Gainmap => self.settings.mutable.quality_gainmap,
            };

            // If used, contains the most or least significiant bits of the image.
            let bit_depth_extension_image;
            match self.settings.sample_transform_recipe {
                SampleTransformRecipe::None => assert!(!item.is_sato_least_significant_input),
                SampleTransformRecipe::BitDepthExtension8b8b => {
                    if !item.is_sato_least_significant_input {
                        // Encoding the least significant bits of a sample does not
                        // make any sense if the other bits are lossily compressed.
                        // Encode the most significant bits losslessly.
                        quality = 100;
                    }
                    bit_depth_extension_image =
                        Self::create_bit_depth_extension_image(image, item)?;
                    image = &bit_depth_extension_image;
                }
            }

            let encoder_config = EncoderConfig {
                tile_rows_log2,
                tile_columns_log2,
                quantizer: ((100 - quality) * 63 + 50) / 100,
                disable_lagged_output: self.alpha_present,
                is_single_image,
                speed: self.settings.speed,
                extra_layer_count: self.settings.extra_layer_count,
                threads: self.settings.threads,
                scaling_mode: self.settings.mutable.scaling_mode,
                codec_specific_options: self.codec_specific_options.clone(),
            };
            item.codec.unwrap_mut().encode_image(
                image,
                item.category,
                &encoder_config,
                &mut item.samples,
            )?;
        }
        self.duration_in_timescales.push(duration);
        Ok(())
    }

    pub fn add_image(&mut self, image: &Image) -> AvifResult<()> {
        self.add_image_impl(
            1,
            1,
            &[image],
            0,
            self.settings.extra_layer_count == 0,
            None,
        )
    }

    pub fn add_image_for_sequence(&mut self, image: &Image, duration: u64) -> AvifResult<()> {
        if self.settings.extra_layer_count != 0 {
            return AvifError::invalid_argument();
        }
        // TODO: this and add_image cannot be used on the same instance.
        self.add_image_impl(1, 1, &[image], duration, false, None)
    }

    pub fn add_image_grid(
        &mut self,
        grid_columns: u32,
        grid_rows: u32,
        images: &[&Image],
    ) -> AvifResult<()> {
        if grid_columns == 0 || grid_columns > 256 || grid_rows == 0 || grid_rows > 256 {
            return AvifError::invalid_image_grid("");
        }
        self.add_image_impl(
            grid_columns,
            grid_rows,
            images,
            0,
            self.settings.extra_layer_count == 0,
            None,
        )
    }

    pub fn add_image_gainmap(&mut self, image: &Image, gainmap: &GainMap) -> AvifResult<()> {
        if self.settings.extra_layer_count != 0 {
            return AvifError::not_implemented();
        }
        self.add_image_impl(1, 1, &[image], 0, true, Some(&[gainmap]))
    }

    pub fn add_image_gainmap_grid(
        &mut self,
        grid_columns: u32,
        grid_rows: u32,
        images: &[&Image],
        gainmaps: &[&GainMap],
    ) -> AvifResult<()> {
        if grid_columns == 0 || grid_columns > 256 || grid_rows == 0 || grid_rows > 256 {
            return AvifError::invalid_image_grid("");
        }
        if self.settings.extra_layer_count != 0 {
            return AvifError::not_implemented();
        }
        self.add_image_impl(grid_columns, grid_rows, images, 0, true, Some(gainmaps))
    }

    pub fn finish(&mut self) -> AvifResult<Vec<u8>> {
        if self.items.is_empty() {
            return AvifError::no_content();
        }
        for item in &mut self.items {
            if item.codec.is_none() {
                continue;
            }
            item.codec.unwrap_mut().finish(&mut item.samples)?;
            if item.extra_layer_count > 0
                && item.samples.len() != 1 + item.extra_layer_count as usize
            {
                return AvifError::invalid_argument();
            }
            // TODO: check if sample count == duration count.

            if !item.samples.is_empty() {
                // Harvest codec configuration from sequence header.
                let sequence_header = Av1SequenceHeader::parse_from_obus(&item.samples[0].data)?;
                item.codec_configuration = CodecConfiguration::Av1(sequence_header.config);
            }
        }
        let mut stream = OStream::default();

        if self.settings.header_format == HeaderFormat::Mini && mini::is_mini_compatible(self) {
            self.write_ftyp_and_mini(&mut stream)?;
            return Ok(stream.data);
        }

        self.write_ftyp(&mut stream)?;
        self.write_meta(&mut stream)?;
        self.write_moov(&mut stream)?;
        self.write_mdat(&mut stream)?;
        Ok(stream.data)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_case::test_case;

    #[test_case(256, 144, 0, 0 ; "144p")]
    #[test_case(426, 240, 0, 0 ; "240p")]
    #[test_case(640, 360, 0, 0 ; "360p")]
    #[test_case(854, 480, 0, 1 ; "480p")]
    #[test_case(1280, 720, 1, 1 ; "720p")]
    #[test_case(1920, 1080, 1, 2 ; "1080p")]
    #[test_case(2560, 1440, 1, 2 ; "2k")]
    #[test_case(3840, 2160, 1, 2 ; "4k")]
    #[test_case(7680, 4320, 1, 2 ; "8k")]
    #[test_case(768, 512, 0, 1 ; "case 1")]
    #[test_case(16384, 64, 0, 2 ; "case 2")]
    fn auto_tiling(
        width: u32,
        height: u32,
        expected_tile_rows_log2: i32,
        expected_tile_columns_log2: i32,
    ) {
        let tiling_mode = TilingMode::Auto;
        assert_eq!(
            tiling_mode.log2(width, height),
            (expected_tile_rows_log2, expected_tile_columns_log2)
        );
        assert_eq!(
            tiling_mode.log2(height, width),
            (expected_tile_columns_log2, expected_tile_rows_log2)
        );
    }
}
