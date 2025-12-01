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

use crate::decoder::*;
use crate::internal_utils::sampletransform::*;
use crate::*;

use std::num::NonZero;

#[derive(Debug, Default)]
pub struct DecodeSample {
    pub item_id: u32, // 1-based. 0 if it comes from a track.
    pub offset: u64,
    pub size: usize,
    pub spatial_id: u8,
    pub sync: bool,
}

impl DecodeSample {
    pub(crate) fn partial_data<'a>(
        &'a self,
        io: &'a mut Box<impl decoder::IO + ?Sized>,
        buffer: &'a Option<Vec<u8>>,
        size: usize,
    ) -> AvifResult<&'a [u8]> {
        match buffer {
            Some(x) => {
                let start_offset = usize_from_u64(self.offset)?;
                let end_offset = checked_add!(start_offset, size)?;
                let range = start_offset..end_offset;
                check_slice_range(x.len(), &range)?;
                Ok(&x[range])
            }
            None => {
                let data = io.read(self.offset, size)?;
                if data.len() != size {
                    AvifError::truncated_data()
                } else {
                    Ok(data)
                }
            }
        }
    }

    pub(crate) fn data<'a>(
        &'a self,
        io: &'a mut Box<impl decoder::IO + ?Sized>,
        buffer: &'a Option<Vec<u8>>,
    ) -> AvifResult<&'a [u8]> {
        self.partial_data(io, buffer, self.size)
    }
}

#[derive(Debug, Default)]
pub struct DecodeInput {
    pub samples: Vec<DecodeSample>,
    pub all_layers: bool,
    pub decoding_item: DecodingItem,
}

#[derive(Debug, Default)]
pub struct Overlay {
    pub canvas_fill_value: [u16; 4],
    pub width: u32,
    pub height: u32,
    pub horizontal_offsets: Vec<i32>,
    pub vertical_offsets: Vec<i32>,
}

#[derive(Debug, Default)]
pub(crate) struct TileInfo {
    pub tile_count: u32,
    pub decoded_tile_count: u32,
    pub grid: Grid,
    pub overlay: Overlay,
    pub gainmap_metadata: GainMapMetadata,
    pub sample_transform: SampleTransform,
}

impl TileInfo {
    pub(crate) fn is_grid(&self) -> bool {
        self.grid.rows > 0 && self.grid.columns > 0
    }

    pub(crate) fn is_overlay(&self) -> bool {
        !self.overlay.horizontal_offsets.is_empty() && !self.overlay.vertical_offsets.is_empty()
    }

    pub(crate) fn is_sample_transform(&self) -> bool {
        !self.sample_transform.tokens.is_empty()
    }

    pub(crate) fn is_derived_image(&self) -> bool {
        self.is_grid() || self.is_overlay() || self.is_sample_transform()
    }

    pub(crate) fn grid_tile_count(&self) -> AvifResult<u32> {
        if self.is_grid() {
            checked_mul!(self.grid.rows, self.grid.columns)
        } else {
            Ok(1)
        }
    }

    pub(crate) fn decoded_row_count(&self, image_height: u32, tile_height: u32) -> u32 {
        if self.decoded_tile_count == 0 {
            return 0;
        }
        if self.decoded_tile_count == self.tile_count || !self.is_grid() {
            return image_height;
        }
        std::cmp::min(
            (self.decoded_tile_count / self.grid.columns) * tile_height,
            image_height,
        )
    }

    pub(crate) fn is_fully_decoded(&self) -> bool {
        self.tile_count == self.decoded_tile_count
    }
}

#[derive(Default)]
pub struct Tile {
    pub width: u32,
    pub height: u32,
    pub operating_point: u8,
    pub image: Image,
    pub input: DecodeInput,
    pub codec_index: usize,
    pub codec_config: CodecConfiguration,
}

impl Tile {
    pub(crate) fn create_from_item(
        item: &mut Item,
        allow_progressive: bool,
        image_count_limit: Option<NonZero<u32>>,
        size_hint: u64,
    ) -> AvifResult<Tile> {
        if size_hint != 0 && item.size as u64 > size_hint {
            return AvifError::bmff_parse_failed("exceeded size_hint");
        }
        let mut tile = Tile {
            width: item.width,
            height: item.height,
            operating_point: item.operating_point(),
            image: Image::default(),
            codec_config: item
                .codec_config()
                .ok_or(AvifError::BmffParseFailed("missing av1C property".into()))?
                .clone(),
            ..Tile::default()
        };
        let mut layer_sizes: [usize; MAX_AV1_LAYER_COUNT] = [0; MAX_AV1_LAYER_COUNT];
        let mut layer_count: usize = 0;
        let a1lx = item.a1lx();
        let has_a1lx = a1lx.is_some();
        if let Some(a1lx) = a1lx {
            let mut remaining_size: usize = item.size;
            for i in 0usize..3 {
                layer_count += 1;
                if a1lx[i] > 0 {
                    // >= instead of > because there must be room for the last layer
                    if a1lx[i] >= remaining_size {
                        return AvifError::bmff_parse_failed(format!(
                            "a1lx layer index [{i}] does not fit in item size"
                        ));
                    }
                    layer_sizes[i] = a1lx[i];
                    remaining_size -= a1lx[i];
                } else {
                    layer_sizes[i] = remaining_size;
                    remaining_size = 0;
                    break;
                }
            }
            if remaining_size > 0 {
                assert!(layer_count == 3);
                layer_count += 1;
                layer_sizes[3] = remaining_size;
            }
        }
        let lsel;
        let has_lsel;
        match item.lsel() {
            Some(x) => {
                lsel = *x;
                has_lsel = true;
            }
            None => {
                lsel = 0;
                has_lsel = false;
            }
        }
        // Progressive images offer layers via the a1lxProp, but don't specify a layer selection with
        // lsel.
        item.progressive = has_a1lx && (!has_lsel || lsel == 0xFFFF);
        let base_item_offset = if item.extents.len() == 1 { item.extents[0].offset } else { 0 };
        if has_lsel && lsel != 0xFFFF {
            // Layer selection. This requires that the underlying AV1 codec decodes all layers, and
            // then only returns the requested layer as a single frame. To the user of libavif,
            // this appears to be a single frame.
            tile.input.all_layers = true;
            let mut sample_size: usize = 0;
            let layer_id = usize_from_u16(lsel)?;
            if layer_count > 0 {
                // Optimization: If we're selecting a layer that doesn't require the entire image's
                // payload (hinted via the a1lx box).
                if layer_id >= layer_count {
                    return AvifError::invalid_image_grid("lsel layer index not found in a1lx.");
                }
                let layer_id_plus_1 = layer_id + 1;
                for layer_size in layer_sizes.iter().take(layer_id_plus_1) {
                    checked_incr!(sample_size, *layer_size);
                }
            } else {
                // This layer payload subsection is not known. Use the whole payload.
                sample_size = item.size;
            }
            let sample = DecodeSample {
                item_id: item.id,
                offset: base_item_offset,
                size: sample_size,
                spatial_id: lsel as u8,
                sync: true,
            };
            tile.input.samples.push(sample);
        } else if item.progressive && allow_progressive {
            // Progressive image. Decode all layers and expose them all to the
            // user.
            if let Some(limit) = image_count_limit {
                if layer_count as u32 > limit.get() {
                    return AvifError::bmff_parse_failed(
                        "exceeded image_count_limit (progressive)",
                    );
                }
            }
            tile.input.all_layers = true;
            let mut offset = 0;
            for (i, layer_size) in layer_sizes.iter().take(layer_count).enumerate() {
                let sample = DecodeSample {
                    item_id: item.id,
                    offset: checked_add!(base_item_offset, offset)?,
                    size: *layer_size,
                    spatial_id: 0xff,
                    sync: i == 0, // Assume all layers depend on the first layer.
                };
                tile.input.samples.push(sample);
                offset = checked_add!(offset, *layer_size as u64)?;
            }
        } else {
            // Typical case: Use the entire item's payload for a single frame output
            let sample = DecodeSample {
                item_id: item.id,
                offset: base_item_offset,
                size: item.size,
                // Legal spatial_id values are [0,1,2,3], so this serves as a sentinel value for
                // "do not filter by spatial_id"
                spatial_id: 0xff,
                sync: true,
            };
            tile.input.samples.push(sample);
        }
        Ok(tile)
    }

    pub(crate) fn create_from_track(
        track: &Track,
        image_count_limit: Option<NonZero<u32>>,
        size_hint: u64,
        decoding_item: DecodingItem,
    ) -> AvifResult<Tile> {
        let properties = track
            .get_properties()
            .ok_or(AvifError::BmffParseFailed("".into()))?;
        let codec_config = find_property!(properties, CodecConfiguration)
            .ok_or(AvifError::BmffParseFailed("".into()))?
            .clone();
        let mut tile = Tile {
            width: track.width,
            height: track.height,
            operating_point: 0, // No way to set operating point via tracks
            input: DecodeInput {
                decoding_item,
                ..DecodeInput::default()
            },
            codec_config,
            ..Tile::default()
        };
        let sample_table = &track.sample_table.unwrap_ref();

        if let Some(limit) = image_count_limit {
            let mut limit = limit.get();
            for (chunk_index, _chunk_offset) in sample_table.chunk_offsets.iter().enumerate() {
                // Figure out how many samples are in this chunk.
                let sample_count = sample_table.get_sample_count_of_chunk(chunk_index as u32);
                if sample_count == 0 {
                    return AvifError::bmff_parse_failed("chunk with 0 samples found");
                }
                if sample_count > limit {
                    return AvifError::bmff_parse_failed("exceeded image_count_limit");
                }
                limit -= sample_count;
            }
        }

        let mut sample_size_index: usize = 0;
        for (chunk_index, chunk_offset) in sample_table.chunk_offsets.iter().enumerate() {
            // Figure out how many samples are in this chunk.
            let sample_count = sample_table.get_sample_count_of_chunk(chunk_index as u32);
            if sample_count == 0 {
                return AvifError::bmff_parse_failed("chunk with 0 samples found");
            }

            let mut sample_offset = *chunk_offset;
            for _ in 0..sample_count {
                let sample_size = sample_table.sample_size(sample_size_index)?;
                let sample_size_hint = checked_add!(sample_offset, sample_size as u64)?;
                if size_hint != 0 && sample_size_hint > size_hint {
                    return AvifError::bmff_parse_failed("exceeded size_hint");
                }
                let sample = DecodeSample {
                    item_id: 0,
                    offset: sample_offset,
                    size: sample_size,
                    // Legal spatial_id values are [0,1,2,3], so this serves as a sentinel value for "do
                    // not filter by spatial_id"
                    spatial_id: 0xff,
                    // Assume first sample is always sync (in case stss box was missing).
                    sync: tile.input.samples.is_empty(),
                };
                tile.input.samples.push(sample);
                checked_incr!(sample_offset, sample_size as u64);
                checked_incr!(sample_size_index, 1);
            }
        }
        for sync_sample_number in &sample_table.sync_samples {
            let index = usize_from_u32(*sync_sample_number)?;
            // sample_table.sync_samples is 1-based.
            if index == 0 || index > tile.input.samples.len() {
                return AvifError::bmff_parse_failed(format!("invalid sync sample number {index}"));
            }
            tile.input.samples[index - 1].sync = true;
        }
        Ok(tile)
    }

    pub(crate) fn max_sample_size(&self) -> usize {
        match self.input.samples.iter().max_by_key(|sample| sample.size) {
            Some(sample) => sample.size,
            None => 0,
        }
    }
}
