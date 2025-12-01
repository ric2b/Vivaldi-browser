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

use crate::gainmap::GainMapMetadata;
use crate::internal_utils::stream::OStream;
use crate::internal_utils::*;
use crate::*;

pub(crate) const UNITY_MATRIX: [u8; 9 * 4] = [
    0x00, 0x01, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x00, 0x01, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, 0x00, //
    0x40, 0x00, 0x00, 0x00, //
];

pub(crate) fn write_hdlr(stream: &mut OStream, handler_type: &str) -> AvifResult<()> {
    stream.start_full_box("hdlr", (0, 0))?;
    // unsigned int(32) pre_defined = 0;
    stream.write_u32(0)?;
    // unsigned int(32) handler_type;
    stream.write_str(handler_type)?;
    // const unsigned int(32)[3] reserved = 0;
    stream.write_u32(0)?;
    stream.write_u32(0)?;
    stream.write_u32(0)?;
    // string name;
    stream.write_string_with_nul(&String::from(""))?;
    stream.finish_box()
}

pub(crate) fn write_pitm(stream: &mut OStream, item_id: u16) -> AvifResult<()> {
    stream.start_full_box("pitm", (0, 0))?;
    //  unsigned int(16) item_ID;
    stream.write_u16(item_id)?;
    stream.finish_box()
}

pub(crate) fn write_grid(stream: &mut OStream, grid: &Grid) -> AvifResult<()> {
    // ISO/IEC 23008-12 6.6.2.3.2
    // aligned(8) class ImageGrid {
    //     unsigned int(8) version = 0;
    //     unsigned int(8) flags;
    //     FieldLength = ((flags & 1) + 1) * 16;
    //     unsigned int(8) rows_minus_one;
    //     unsigned int(8) columns_minus_one;
    //     unsigned int(FieldLength) output_width;
    //     unsigned int(FieldLength) output_height;
    // }
    let flags = if grid.width > 65535 || grid.height > 65535 { 1 } else { 0 };
    // unsigned int(8) version = 0;
    stream.write_u8(0)?;
    // unsigned int(8) flags;
    stream.write_u8(flags)?;
    // unsigned int(8) rows_minus_one;
    stream.write_u8(grid.rows as u8 - 1)?;
    // unsigned int(8) columns_minus_one;
    stream.write_u8(grid.columns as u8 - 1)?;
    // unsigned int(FieldLength) output_width;
    // unsigned int(FieldLength) output_height;
    if flags == 1 {
        stream.write_u32(grid.width)?;
        stream.write_u32(grid.height)?;
    } else {
        stream.write_u16(grid.width as u16)?;
        stream.write_u16(grid.height as u16)?;
    }
    Ok(())
}

pub(crate) fn write_tmap(metadata: &GainMapMetadata) -> AvifResult<Vec<u8>> {
    let mut stream = OStream::default();
    // ToneMapImage syntax as per section 6.6.2.4.2 of ISO/IEC 23008-12:2024
    // amendment "Support for tone map derived image items and other improvements".
    // unsigned int(8) version = 0;
    stream.write_u8(0)?;
    // GainMapMetadata syntax as per clause C.2.2 of ISO 21496-1
    // unsigned int(16) minimum_version;
    stream.write_u16(0)?;
    // unsigned int(16) writer_version;
    stream.write_u16(0)?;
    // unsigned int(1) is_multichannel;
    stream.write_bool(metadata.channel_count() == 3)?;
    // unsigned int(1) use_base_colour_space;
    stream.write_bool(metadata.use_base_color_space)?;
    // unsigned int(6) reserved;
    stream.write_bits(0, 6)?;
    // unsigned int(32) base_hdr_headroom_numerator;
    // unsigned int(32) base_hdr_headroom_denominator;
    stream.write_ufraction(metadata.base_hdr_headroom)?;
    // unsigned int(32) alternate_hdr_headroom_numerator;
    // unsigned int(32) alternate_hdr_headroom_denominator;
    stream.write_ufraction(metadata.alternate_hdr_headroom)?;
    for i in 0..metadata.channel_count() as usize {
        // int(32) gain_map_min_numerator;
        // unsigned int(32) gain_map_min_denominator
        stream.write_fraction(metadata.min[i])?;
        // int(32) gain_map_max_numerator;
        // unsigned int(32) gain_map_max_denominator;
        stream.write_fraction(metadata.max[i])?;
        // unsigned int(32) gamma_numerator;
        // unsigned int(32) gamma_denominator;
        stream.write_ufraction(metadata.gamma[i])?;
        // int(32) base_offset_numerator;
        // unsigned int(32) base_offset_denominator;
        stream.write_fraction(metadata.base_offset[i])?;
        // int(32) alternate_offset_numerator;
        // unsigned int(32) alternate_offset_denominator;
        stream.write_fraction(metadata.alternate_offset[i])?;
    }
    Ok(stream.data)
}

impl Encoder {
    pub(crate) fn write_ftyp(&self, stream: &mut OStream) -> AvifResult<()> {
        let mut compatible_brands = vec![
            String::from("avif"),
            String::from("mif1"),
            String::from("miaf"),
        ];
        // TODO: check if avio brand is necessary.
        if self.is_sequence() {
            compatible_brands.extend_from_slice(&[
                String::from("avis"),
                String::from("msf1"),
                String::from("iso8"),
            ]);
        }
        if self.items.iter().any(|x| x.is_tmap()) {
            compatible_brands.push(String::from("tmap"));
        }
        match self.image_metadata.depth {
            8 | 10 => match self.image_metadata.yuv_format {
                PixelFormat::Yuv420 => compatible_brands.push(String::from("MA1B")),
                PixelFormat::Yuv444 => compatible_brands.push(String::from("MA1A")),
                _ => {}
            },
            _ => {}
        }

        stream.start_box("ftyp")?;
        // unsigned int(32) major_brand;
        stream.write_string(&String::from(if self.is_sequence() {
            "avis"
        } else {
            "avif"
        }))?;
        // unsigned int(32) minor_version;
        stream.write_u32(0)?;
        // unsigned int(32) compatible_brands[];
        for compatible_brand in &compatible_brands {
            stream.write_string(compatible_brand)?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_iloc(stream: &mut OStream, items: &mut Vec<&mut Item>) -> AvifResult<()> {
        stream.start_full_box("iloc", (0, 0))?;
        // unsigned int(4) offset_size;
        // unsigned int(4) length_size;
        stream.write_u8(0x44)?;
        // unsigned int(4) base_offset_size;
        // unsigned int(4) reserved;
        stream.write_u8(0)?;
        // unsigned int(16) item_count;
        stream.write_u16(u16_from_usize(items.len())?)?;

        for item in items {
            // unsigned int(16) item_ID;
            stream.write_u16(item.id)?;
            // unsigned int(16) data_reference_index;
            stream.write_u16(0)?;

            if item.extra_layer_count > 0 {
                let layer_count = item.extra_layer_count as u16 + 1;
                // unsigned int(16) extent_count;
                stream.write_u16(layer_count)?;
                for i in 0..layer_count as usize {
                    item.mdat_offset_locations.push(stream.offset());
                    // unsigned int(offset_size*8) extent_offset;
                    stream.write_u32(0)?;
                    // unsigned int(length_size*8) extent_length;
                    stream.write_u32(u32_from_usize(item.samples[i].data.len())?)?;
                }
            } else {
                // unsigned int(16) extent_count;
                stream.write_u16(1)?;
                item.mdat_offset_locations.push(stream.offset());
                // unsigned int(offset_size*8) extent_offset;
                stream.write_u32(0)?;
                let extent_length = if item.samples.is_empty() {
                    u32_from_usize(item.metadata_payload.len())?
                } else {
                    u32_from_usize(item.samples[0].data.len())?
                };
                // unsigned int(length_size*8) extent_length;
                stream.write_u32(extent_length)?;
            }
        }

        stream.finish_box()
    }

    pub(crate) fn write_iinf(stream: &mut OStream, items: &Vec<&mut Item>) -> AvifResult<()> {
        stream.start_full_box("iinf", (0, 0))?;

        // unsigned int(16) entry_count;
        stream.write_u16(u16_from_usize(items.len())?)?;

        for item in items {
            let flags = if item.hidden_image { 1 } else { 0 };
            stream.start_full_box("infe", (2, flags))?;
            // unsigned int(16) item_ID;
            stream.write_u16(item.id)?;
            // unsigned int(16) item_protection_index;
            stream.write_u16(0)?;
            // unsigned int(32) item_type;
            stream.write_string(&item.item_type)?;
            // utf8string item_name;
            stream.write_string_with_nul(&item.infe_name)?;
            match item.item_type.as_str() {
                "mime" => {
                    // utf8string content_type;
                    stream.write_string_with_nul(&item.infe_content_type)?
                    // utf8string content_encoding; //optional
                }
                "uri " => {
                    // utf8string item_uri_type;
                    return AvifError::not_implemented();
                }
                _ => {}
            }
            stream.finish_box()?;
        }

        stream.finish_box()
    }

    pub(crate) fn write_iref(&self, stream: &mut OStream) -> AvifResult<()> {
        let mut box_started = false;
        for item in &self.items {
            let dimg_item_ids: Vec<_> = self
                .items
                .iter()
                .filter(|dimg_item| dimg_item.dimg_from_id.unwrap_or_default() == item.id)
                .map(|dimg_item| dimg_item.id)
                .collect();
            if !dimg_item_ids.is_empty() {
                if !box_started {
                    stream.start_full_box("iref", (0, 0))?;
                    box_started = true;
                }
                stream.start_box("dimg")?;
                // unsigned int(16) from_item_ID;
                stream.write_u16(item.id)?;
                // unsigned int(16) reference_count;
                stream.write_u16(u16_from_usize(dimg_item_ids.len())?)?;
                for dimg_item_id in dimg_item_ids {
                    // unsigned int(16) to_item_ID;
                    stream.write_u16(dimg_item_id)?;
                }
                stream.finish_box()?;
            }
            if let Some(iref_to_id) = item.iref_to_id {
                if !box_started {
                    stream.start_full_box("iref", (0, 0))?;
                    box_started = true;
                }
                stream.start_box(item.iref_type.as_ref().unwrap().as_str())?;
                // unsigned int(16) from_item_ID;
                stream.write_u16(item.id)?;
                // unsigned int(16) reference_count;
                stream.write_u16(1)?;
                // unsigned int(16) to_item_ID;
                stream.write_u16(iref_to_id)?;
                stream.finish_box()?;
            }
        }
        if box_started {
            stream.finish_box()?;
        }
        Ok(())
    }

    pub(crate) fn write_grpl(&mut self, stream: &mut OStream) -> AvifResult<()> {
        if self.alternative_item_ids.is_empty() {
            return Ok(());
        }
        stream.start_box("grpl")?;

        stream.start_full_box("altr", (0, 0))?;
        // Section 8.18.3.3 of ISO 14496-12 (ISOBMFF) says:
        //   group_id is a non-negative integer assigned to the particular grouping that shall not
        //   be equal to any group_id value of any other EntityToGroupBox, any item_ID value of the
        //   hierarchy level (file, movie. or track) that contains the GroupsListBox, or any
        //   track_ID value (when theGroupsListBox is contained in the file level).
        let group_id = (self.items.iter().map(|item| item.id).max().unwrap_or(0) as u32) + 1;
        stream.write_u32(group_id)?;
        stream.write_u32(u32_from_usize(self.alternative_item_ids.len())?)?;
        for item_id in self.alternative_item_ids.iter() {
            stream.write_u32((*item_id).into())?;
        }
        stream.finish_box()?;
        // end of altr

        stream.finish_box()
    }

    pub(crate) fn write_iprp(&mut self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_box("iprp")?;
        // ipco
        stream.start_box("ipco")?;
        let mut property_streams = Vec::new();
        for item in &mut self.items {
            let mut bit_depth_extension_metadata;
            let item_metadata = if item.is_tmap() {
                &self.alt_image_metadata
            } else if item.category == Category::Gainmap {
                &self.gainmap_image_metadata
            } else {
                match self.settings.sample_transform_recipe {
                    SampleTransformRecipe::None => &self.image_metadata,
                    SampleTransformRecipe::BitDepthExtension8b8b => {
                        if item.is_sato() {
                            &self.image_metadata
                        } else {
                            bit_depth_extension_metadata = self.image_metadata.shallow_clone();
                            bit_depth_extension_metadata.depth = 8;
                            &bit_depth_extension_metadata
                        }
                    }
                }
            };
            item.get_property_streams(&self.image_metadata, item_metadata, &mut property_streams)?;
        }
        // Deduplicate the property streams.
        let mut property_index_map = Vec::new();
        let mut last_written_property_index = 0u8;
        for i in 0..property_streams.len() {
            let current_data = &property_streams[i].data;
            match property_streams[0..i]
                .iter()
                .position(|x| x.data == *current_data)
            {
                Some(property_index) => {
                    // A duplicate stream was already written. Simply store the index of that
                    // stream.
                    property_index_map.push(property_index_map[property_index]);
                }
                None => {
                    // No duplicate streams were found. Write this stream and store its index.
                    stream.write_slice(current_data)?;
                    last_written_property_index += 1;
                    property_index_map.push(last_written_property_index);
                }
            }
        }
        stream.finish_box()?;
        // end of ipco

        // ipma
        stream.start_full_box("ipma", (0, 0))?;
        let entry_count = u32_from_usize(
            self.items
                .iter()
                .filter(|&item| !item.associations.is_empty())
                .count(),
        )?;
        // unsigned int(32) entry_count;
        stream.write_u32(entry_count)?;
        for item in &self.items {
            if item.associations.is_empty() {
                continue;
            }
            // unsigned int(16) item_ID;
            stream.write_u16(item.id)?;
            // unsigned int(8) association_count;
            stream.write_u8(u8_from_usize(item.associations.len())?)?;
            for (property_index, essential) in &item.associations {
                // bit(1) essential;
                stream.write_bool(*essential)?;
                // property_index_map is 0-indexed whereas the index stored in item.associations is
                // 1-indexed.
                let index = property_index_map[*property_index as usize - 1];
                if index >= (1 << 7) {
                    return AvifError::unknown_error("");
                }
                // unsigned int(7) property_index;
                stream.write_bits(index.into(), 7)?;
            }
        }
        stream.finish_box()?;
        // end of ipma

        stream.finish_box()
    }

    pub(crate) fn write_mvhd(
        &mut self,
        stream: &mut OStream,
        duration: u64,
        timestamp: u64,
    ) -> AvifResult<()> {
        stream.start_full_box("mvhd", (1, 0))?;
        // unsigned int(64) creation_time;
        stream.write_u64(timestamp)?;
        // unsigned int(64) modification_time;
        stream.write_u64(timestamp)?;
        // unsigned int(32) timescale;
        stream.write_u32(u32_from_u64(self.settings.timescale)?)?;
        // unsigned int(64) duration;
        stream.write_u64(duration)?;
        // template int(32) rate = 0x00010000; // typically 1.0
        stream.write_u32(0x00010000)?;
        // template int(16) volume = 0x0100; // typically, full volume
        stream.write_u16(0x0100)?;
        // const bit(16) reserved = 0;
        stream.write_u16(0)?;
        // const unsigned int(32)[2] reserved = 0;
        stream.write_u32(0)?;
        stream.write_u32(0)?;
        // template int(32)[9] matrix
        stream.write_slice(&UNITY_MATRIX)?;
        // bit(32)[6] pre_defined = 0;
        for _ in 0..6 {
            stream.write_u32(0)?;
        }
        // unsigned int(32) next_track_ID;
        stream.write_u32(u32_from_usize(self.items.len())?)?;
        stream.finish_box()
    }

    pub(crate) fn write_track_meta(&mut self, stream: &mut OStream) -> AvifResult<()> {
        let mut metadata_items: Vec<_> =
            self.items.iter_mut().filter(|x| x.is_metadata()).collect();
        if metadata_items.is_empty() {
            return Ok(());
        }
        stream.start_full_box("meta", (0, 0))?;
        write_hdlr(stream, "pict")?;
        Self::write_iloc(stream, &mut metadata_items)?;
        Self::write_iinf(stream, &metadata_items)?;
        stream.finish_box()
    }

    pub(crate) fn write_tracks(
        &mut self,
        stream: &mut OStream,
        duration: u64,
        total_duration: u64,
        timestamp: u64,
    ) -> AvifResult<()> {
        for index in 0..self.items.len() {
            let item = &self.items[index];
            if item.samples.is_empty() {
                continue;
            }
            stream.start_box("trak")?;
            item.write_tkhd(stream, &self.image_metadata, total_duration, timestamp)?;
            item.write_tref(stream)?;
            item.write_edts(
                stream,
                self.settings.repetition_count.loop_count(),
                duration,
            )?;
            if item.category == Category::Color {
                self.write_track_meta(stream)?;
            }
            let item = &self.items[index];
            // mdia
            {
                stream.start_box("mdia")?;
                // mdhd
                {
                    stream.start_full_box("mdhd", (1, 0))?;
                    // unsigned int(64) creation_time;
                    stream.write_u64(timestamp)?;
                    // unsigned int(64) modification_time;
                    stream.write_u64(timestamp)?;
                    // unsigned int(32) timescale;
                    stream.write_u32(u32_from_u64(self.settings.timescale)?)?;
                    // unsigned int(64) duration;
                    stream.write_u64(duration)?;
                    // bit(1) pad = 0; unsigned int(5)[3] language; ("und")
                    stream.write_u16(21956)?;
                    // unsigned int(16) pre_defined = 0;
                    stream.write_u16(0)?;
                    stream.finish_box()?;
                }
                write_hdlr(
                    stream,
                    if item.category == Category::Alpha { "auxv" } else { "pict" },
                )?;
                // minf
                {
                    stream.start_box("minf")?;
                    item.write_vmhd(stream)?;
                    item.write_dinf(stream)?;
                    let item_mut = &mut self.items[index];
                    item_mut.write_stbl(
                        stream,
                        &self.image_metadata,
                        &self.duration_in_timescales,
                    )?;
                    stream.finish_box()?;
                }
                stream.finish_box()?;
            }
            stream.finish_box()?;
        }
        Ok(())
    }

    pub(crate) fn write_mdat(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_box("mdat")?;
        let mut layered_item_ids = [Vec::new(), Vec::new()];
        // Use multiple passes to pack the items in the following order:
        //   * Pass 0: metadata (Exif/XMP/gain map metadata)
        //   * Pass 1: alpha, gain map image (AV1)
        //   * Pass 2: all other item data (AV1 color)
        //
        // See here for the discussion on alpha coming before color:
        // https://github.com/AOMediaCodec/libavif/issues/287
        //
        // Exif and XMP are packed first as they're required to be fully available by
        // Decoder::parse() before it returns AVIF_RESULT_OK, unless ignore_xmp and ignore_exif are
        // enabled.
        let mdat_start_offset = stream.offset();
        for pass in 0..=2 {
            for item in &self.items {
                if pass == 0
                    && item.item_type != "mime"
                    && item.item_type != "Exif"
                    && item.item_type != "tmap"
                {
                    continue;
                }
                if pass == 1 && !matches!(item.category, Category::Alpha | Category::Gainmap) {
                    continue;
                }
                if pass == 2 && item.category != Category::Color {
                    continue;
                }
                if self.settings.extra_layer_count > 0 && !item.samples.is_empty() {
                    if item.category == Category::Color {
                        layered_item_ids[1].push(item.id);
                    } else if item.category == Category::Alpha {
                        layered_item_ids[0].push(item.id);
                    }
                    continue;
                }

                let mut chunk_offset = stream.offset();
                if !item.samples.is_empty() {
                    if item.samples.len() > 1 {
                        // If there is more than 1 sample, then we do not de-duplicate the chunks.
                        for sample in &item.samples {
                            stream.write_slice(&sample.data)?;
                        }
                    } else {
                        chunk_offset =
                            stream.write_slice_dedupe(mdat_start_offset, &item.samples[0].data)?;
                    }
                } else if !item.metadata_payload.is_empty() {
                    chunk_offset =
                        stream.write_slice_dedupe(mdat_start_offset, &item.metadata_payload)?;
                } else {
                    // Empty item, ignore it.
                    continue;
                }
                for mdat_offset_location in &item.mdat_offset_locations {
                    stream.write_u32_at_offset(
                        u32_from_usize(chunk_offset)?,
                        *mdat_offset_location,
                    )?;
                }
            }
        }
        // TODO: simplify this code.
        for layered_item_id in &layered_item_ids {
            if layered_item_id.is_empty() {
                continue;
            }
            let mut layer_index = 0;
            loop {
                let mut has_more_samples = false;
                for item_id in layered_item_id {
                    let item = &self.items[*item_id as usize - 1];

                    if item.samples.len() <= layer_index {
                        // Already written all samples for this item.
                        continue;
                    } else if item.samples.len() > layer_index + 1 {
                        has_more_samples = true;
                    }

                    let chunk_offset = stream.offset();
                    stream.write_slice(&item.samples[layer_index].data)?;
                    stream.write_u32_at_offset(
                        u32_from_usize(chunk_offset)?,
                        item.mdat_offset_locations[layer_index],
                    )?;
                }
                layer_index += 1;
                if !has_more_samples {
                    break;
                }
            }
        }
        stream.finish_box()
    }

    pub(crate) fn write_meta(&mut self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("meta", (0, 0))?;
        write_hdlr(stream, "pict")?;
        write_pitm(stream, self.primary_item_id)?;
        let mut items_ref: Vec<_> = self.items.iter_mut().collect();
        Self::write_iloc(stream, &mut items_ref)?;
        Self::write_iinf(stream, &items_ref)?;
        self.write_iref(stream)?;
        self.write_iprp(stream)?;
        self.write_grpl(stream)?;
        stream.finish_box()
    }

    pub(crate) fn write_moov(&mut self, stream: &mut OStream) -> AvifResult<()> {
        if !self.is_sequence() {
            return Ok(());
        }
        let frames_duration_in_timescales = self
            .duration_in_timescales
            .iter()
            .try_fold(0u64, |acc, &x| acc.checked_add(x))
            .ok_or(AvifError::UnknownError("".into()))?;
        let timestamp: u64 = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs();
        let total_duration_in_timescales = if self.settings.repetition_count.is_infinite() {
            u64::MAX
        } else {
            let loop_count = self.settings.repetition_count.loop_count();
            if frames_duration_in_timescales == 0 {
                return AvifError::invalid_argument();
            }
            checked_mul!(frames_duration_in_timescales, loop_count)?
        };
        stream.start_box("moov")?;
        self.write_mvhd(stream, total_duration_in_timescales, timestamp)?;
        self.write_tracks(
            stream,
            frames_duration_in_timescales,
            total_duration_in_timescales,
            timestamp,
        )?;
        stream.finish_box()
    }
}
