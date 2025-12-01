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
use crate::internal_utils::stream::*;
use crate::utils::clap::CleanAperture;
use crate::*;

#[derive(Default)]
pub(crate) struct Item {
    pub id: u16,
    pub item_type: String,
    pub category: Category,
    pub codec: Option<Codec>,
    pub samples: Vec<Sample>,
    pub codec_configuration: CodecConfiguration,
    pub cell_index: usize,
    pub hidden_image: bool,
    pub infe_name: String,
    pub infe_content_type: String,
    pub mdat_offset_locations: Vec<usize>,
    pub iref_to_id: Option<u16>, // If some, then make an iref from this id to iref_to_id.
    pub iref_type: Option<String>,
    pub grid: Option<Grid>,
    pub associations: Vec<(
        u8,   // 1-based property_index
        bool, // essential
    )>,
    pub extra_layer_count: u32,
    pub dimg_from_id: Option<u16>, // If some, then make an iref from dimg_from_id to this id.
    pub metadata_payload: Vec<u8>,
}

impl fmt::Debug for Item {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        write!(
            f,
            "Item: {{ id: {}, item_type: {}, has_codec: {} }}",
            self.id,
            self.item_type,
            self.codec.is_some()
        )
    }
}

impl Item {
    pub(crate) fn has_ipma(&self) -> bool {
        self.grid.is_some() || self.codec.is_some() || self.is_tmap()
    }

    pub(crate) fn is_metadata(&self) -> bool {
        self.item_type != "av01"
    }

    pub(crate) fn is_tmap(&self) -> bool {
        self.item_type == "tmap"
    }

    pub(crate) fn write_ispe(
        &mut self,
        stream: &mut OStream,
        image_metadata: &Image,
    ) -> AvifResult<()> {
        stream.start_full_box("ispe", (0, 0))?;
        let width = match self.grid {
            Some(grid) => grid.width,
            None => image_metadata.width,
        };
        // unsigned int(32) image_width;
        stream.write_u32(width)?;
        let height = match self.grid {
            Some(grid) => grid.height,
            None => image_metadata.height,
        };
        // unsigned int(32) image_height;
        stream.write_u32(height)?;
        stream.finish_box()
    }

    pub(crate) fn write_pixi(
        &mut self,
        stream: &mut OStream,
        image_metadata: &Image,
    ) -> AvifResult<()> {
        stream.start_full_box("pixi", (0, 0))?;
        let num_channels = if self.category == Category::Alpha {
            1
        } else {
            image_metadata.yuv_format.plane_count() as u8
        };
        // unsigned int (8) num_channels;
        stream.write_u8(num_channels)?;
        for _ in 0..num_channels {
            // unsigned int (8) bits_per_channel;
            stream.write_u8(image_metadata.depth)?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_codec_config(&self, stream: &mut OStream) -> AvifResult<()> {
        if let CodecConfiguration::Av1(config) = &self.codec_configuration {
            stream.start_box("av1C")?;
            // unsigned int (1) marker = 1;
            stream.write_bits(1, 1)?;
            // unsigned int (7) version = 1;
            stream.write_bits(1, 7)?;
            // unsigned int(3) seq_profile;
            stream.write_bits(config.seq_profile, 3)?;
            // unsigned int(5) seq_level_idx_0;
            stream.write_bits(config.seq_level_idx0, 5)?;
            // unsigned int(1) seq_tier_0;
            stream.write_bits(config.seq_tier0, 1)?;
            // unsigned int(1) high_bitdepth;
            stream.write_bits(config.high_bitdepth as u8, 1)?;
            // unsigned int(1) twelve_bit;
            stream.write_bits(config.twelve_bit as u8, 1)?;
            // unsigned int(1) monochrome;
            stream.write_bits(config.monochrome as u8, 1)?;
            // unsigned int(1) chroma_subsampling_x;
            stream.write_bits(config.chroma_subsampling_x, 1)?;
            // unsigned int(1) chroma_subsampling_y;
            stream.write_bits(config.chroma_subsampling_y, 1)?;
            // unsigned int(2) chroma_sample_position;
            stream.write_bits(config.chroma_sample_position as u8, 2)?;
            // unsigned int (3) reserved = 0;
            // unsigned int (1) initial_presentation_delay_present;
            // unsigned int (4) reserved = 0;
            stream.write_u8(0)?;
            stream.finish_box()?;
        }
        Ok(())
    }

    #[allow(non_snake_case)]
    pub(crate) fn write_auxC(&mut self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("auxC", (0, 0))?;
        stream
            .write_string_with_nul(&String::from("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"))?;
        stream.finish_box()
    }

    fn write_a1lx(&mut self, stream: &mut OStream) -> AvifResult<()> {
        let layer_sizes: Vec<_> = self.samples[0..self.extra_layer_count as usize]
            .iter()
            .map(|x| x.data.len())
            .collect();
        let has_large_size = layer_sizes.iter().any(|x| *x > 0xffff);
        stream.start_box("a1lx")?;
        // unsigned int(7) reserved = 0;
        stream.write_bits(0, 7)?;
        // unsigned int(1) large_size;
        stream.write_bits(has_large_size as u8, 1)?;
        // FieldLength = (large_size + 1) * 16;
        // unsigned int(FieldLength) layer_size[3];
        for i in 0..3 {
            let layer_size = *layer_sizes.get(i).unwrap_or(&0);
            if has_large_size {
                stream.write_u32(u32_from_usize(layer_size)?)?;
            } else {
                stream.write_u16(u16_from_usize(layer_size)?)?;
            }
        }
        stream.finish_box()
    }

    fn write_nclx(&self, stream: &mut OStream, image_metadata: &Image) -> AvifResult<()> {
        stream.start_box("colr")?;
        // unsigned int(32) colour_type;
        stream.write_str("nclx")?;
        // unsigned int(16) colour_primaries;
        stream.write_u16(image_metadata.color_primaries as u16)?;
        // unsigned int(16) transfer_characteristics;
        stream.write_u16(image_metadata.transfer_characteristics as u16)?;
        // unsigned int(16) matrix_coefficients;
        stream.write_u16(image_metadata.matrix_coefficients as u16)?;
        // unsigned int(1) full_range_flag;
        stream.write_bits(
            if image_metadata.yuv_range == YuvRange::Full { 1 } else { 0 },
            1,
        )?;
        // unsigned int(7) reserved = 0;
        stream.write_bits(0, 7)?;
        stream.finish_box()
    }

    fn write_pasp(&self, stream: &mut OStream, pasp: &PixelAspectRatio) -> AvifResult<()> {
        stream.start_box("pasp")?;
        // unsigned int(32) hSpacing;
        stream.write_u32(pasp.h_spacing)?;
        // unsigned int(32) vSpacing;
        stream.write_u32(pasp.v_spacing)?;
        stream.finish_box()
    }

    fn write_clli(
        &self,
        stream: &mut OStream,
        clli: &ContentLightLevelInformation,
    ) -> AvifResult<()> {
        stream.start_box("clli")?;
        // unsigned int(16) max_content_light_level
        stream.write_u16(clli.max_cll)?;
        // unsigned int(16) max_pic_average_light_level
        stream.write_u16(clli.max_pall)?;
        stream.finish_box()
    }

    fn write_clap(&self, stream: &mut OStream, clap: &CleanAperture) -> AvifResult<()> {
        stream.start_box("clap")?;
        // unsigned int(32) cleanApertureWidthN;
        // unsigned int(32) cleanApertureWidthD;
        stream.write_ufraction(clap.width)?;
        // unsigned int(32) cleanApertureHeightN;
        // unsigned int(32) cleanApertureHeightD;
        stream.write_ufraction(clap.height)?;
        // unsigned int(32) horizOffN;
        // unsigned int(32) horizOffD;
        stream.write_ufraction(clap.horiz_off)?;
        // unsigned int(32) vertOffN;
        // unsigned int(32) vertOffD;
        stream.write_ufraction(clap.vert_off)?;
        stream.finish_box()
    }

    fn write_irot(&self, stream: &mut OStream, angle: u8) -> AvifResult<()> {
        stream.start_box("irot")?;
        // unsigned int(6) reserved = 0;
        stream.write_bits(0, 6)?;
        // unsigned int(2) angle;
        stream.write_bits(angle & 0x03, 2)?;
        stream.finish_box()
    }

    fn write_imir(&self, stream: &mut OStream, axis: u8) -> AvifResult<()> {
        stream.start_box("imir")?;
        // unsigned int(7) reserved = 0;
        stream.write_bits(0, 7)?;
        // unsigned int(1) axis;
        stream.write_bits(axis & 0x01, 1)?;
        stream.finish_box()
    }

    fn write_icc(&self, stream: &mut OStream, image_metadata: &Image) -> AvifResult<()> {
        if image_metadata.icc.is_empty() {
            return Ok(());
        }
        stream.start_box("colr")?;
        // unsigned int(32) colour_type;
        stream.write_str("prof")?;
        stream.write_slice(&image_metadata.icc)?;
        stream.finish_box()
    }

    fn write_transformative_properties(
        &mut self,
        streams: &mut Vec<OStream>,
        metadata: &Image,
    ) -> AvifResult<()> {
        if let Some(clap) = metadata.clap {
            streams.push(OStream::default());
            self.write_clap(streams.last_mut().unwrap(), &clap)?;
            self.associations
                .push((u8_from_usize(streams.len())?, true));
        }
        if let Some(angle) = metadata.irot_angle {
            streams.push(OStream::default());
            self.write_irot(streams.last_mut().unwrap(), angle)?;
            self.associations
                .push((u8_from_usize(streams.len())?, true));
        }
        if let Some(axis) = metadata.imir_axis {
            streams.push(OStream::default());
            self.write_imir(streams.last_mut().unwrap(), axis)?;
            self.associations
                .push((u8_from_usize(streams.len())?, true));
        }
        Ok(())
    }

    pub(crate) fn get_property_streams(
        &mut self,
        image_metadata: &Image,
        item_metadata: &Image,
        streams: &mut Vec<OStream>,
    ) -> AvifResult<()> {
        if !self.has_ipma() {
            return Ok(());
        }

        streams.push(OStream::default());
        self.write_ispe(streams.last_mut().unwrap(), item_metadata)?;
        self.associations
            .push((u8_from_usize(streams.len())?, false));

        // TODO: check for is_tmap and alt_plane_depth.
        streams.push(OStream::default());
        self.write_pixi(streams.last_mut().unwrap(), item_metadata)?;
        self.associations
            .push((u8_from_usize(streams.len())?, false));

        if self.codec.is_some() {
            streams.push(OStream::default());
            self.write_codec_config(streams.last_mut().unwrap())?;
            self.associations
                .push((u8_from_usize(streams.len())?, true));
        }

        match self.category {
            Category::Color => {
                // Color properties.
                // Note the 'tmap' item when a gain map is present also has category set to
                // Category::Color.
                if !item_metadata.icc.is_empty() {
                    streams.push(OStream::default());
                    self.write_icc(streams.last_mut().unwrap(), item_metadata)?;
                    self.associations
                        .push((u8_from_usize(streams.len())?, false));
                }
                streams.push(OStream::default());
                self.write_nclx(streams.last_mut().unwrap(), item_metadata)?;
                self.associations
                    .push((u8_from_usize(streams.len())?, false));
                if let Some(pasp) = item_metadata.pasp {
                    streams.push(OStream::default());
                    self.write_pasp(streams.last_mut().unwrap(), &pasp)?;
                    self.associations
                        .push((u8_from_usize(streams.len())?, false));
                }
                // HDR properties.
                if let Some(clli) = item_metadata.clli {
                    streams.push(OStream::default());
                    self.write_clli(streams.last_mut().unwrap(), &clli)?;
                    self.associations
                        .push((u8_from_usize(streams.len())?, false));
                }
                self.write_transformative_properties(streams, item_metadata)?;
            }
            Category::Alpha => {
                streams.push(OStream::default());
                self.write_auxC(streams.last_mut().unwrap())?;
                self.associations
                    .push((u8_from_usize(streams.len())?, false));
            }
            Category::Gainmap => {
                streams.push(OStream::default());
                self.write_nclx(streams.last_mut().unwrap(), item_metadata)?;
                self.associations
                    .push((u8_from_usize(streams.len())?, false));
                if let Some(pasp) = image_metadata.pasp {
                    streams.push(OStream::default());
                    self.write_pasp(streams.last_mut().unwrap(), &pasp)?;
                    self.associations
                        .push((u8_from_usize(streams.len())?, false));
                }
                if item_metadata.clap.is_some()
                    || item_metadata.irot_angle.is_some()
                    || item_metadata.imir_axis.is_some()
                    || item_metadata.pasp.is_some()
                {
                    return Err(AvifError::UnknownError(
                        "transformative properties must be associated with the base image".into(),
                    ));
                }
                self.write_transformative_properties(streams, image_metadata)?;
            }
        }
        if self.extra_layer_count > 0 {
            streams.push(OStream::default());
            self.write_a1lx(streams.last_mut().unwrap())?;
            self.associations
                .push((u8_from_usize(streams.len())?, false));
            // We don't write 'lsel' property since many decoders do not support it and will reject
            // the image, see https://github.com/AOMediaCodec/libavif/pull/2429
        }
        Ok(())
    }

    pub(crate) fn write_tkhd(
        &self,
        stream: &mut OStream,
        image_metadata: &Image,
        duration: u64,
        timestamp: u64,
    ) -> AvifResult<()> {
        stream.start_full_box("tkhd", (1, 1))?;
        // unsigned int(64) creation_time;
        stream.write_u64(timestamp)?;
        // unsigned int(64) modification_time;
        stream.write_u64(timestamp)?;
        // unsigned int(32) track_ID;
        stream.write_u32(self.id as u32)?;
        // const unsigned int(32) reserved = 0;
        stream.write_u32(0)?;
        // unsigned int(64) duration;
        stream.write_u64(duration)?;
        // const unsigned int(32)[2] reserved = 0;
        stream.write_u32(0)?;
        stream.write_u32(0)?;
        // template int(16) layer = 0;
        stream.write_u16(0)?;
        // template int(16) alternate_group = 0;
        stream.write_u16(0)?;
        // template int(16) volume = {if track_is_audio 0x0100 else 0};
        stream.write_u16(0)?;
        // const unsigned int(16) reserved = 0;
        stream.write_u16(0)?;
        // template int(32)[9] matrix
        stream.write_slice(&mp4box::UNITY_MATRIX)?;
        // unsigned int(32) width;
        stream.write_u32(image_metadata.width << 16)?;
        // unsigned int(32) height;
        stream.write_u32(image_metadata.height << 16)?;
        stream.finish_box()
    }

    pub(crate) fn write_tref(&self, stream: &mut OStream) -> AvifResult<()> {
        if let Some(iref_to_id) = self.iref_to_id {
            stream.start_box("tref")?;
            {
                stream.start_box(self.iref_type.as_ref().unwrap().as_str())?;
                stream.write_u32(iref_to_id as u32)?;
                stream.finish_box()?;
            }
            stream.finish_box()?;
        }
        Ok(())
    }

    pub(crate) fn write_edts(
        &self,
        stream: &mut OStream,
        loop_count: u64,
        duration: u64,
    ) -> AvifResult<()> {
        stream.start_box("edts")?;
        {
            let elst_flags = if loop_count == 1 { 0 } else { 1 };
            stream.start_full_box("elst", (1, elst_flags))?;
            // unsigned int(32) entry_count;
            stream.write_u32(1)?;
            // unsigned int(64) segment_duration;
            stream.write_u64(duration)?;
            // int(64) media_time;
            stream.write_u64(0)?;
            // int(16) media_rate_integer;
            stream.write_u16(1)?;
            // int(16) media_rate_fraction = 0;
            stream.write_u16(0)?;
            stream.finish_box()?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_vmhd(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("vmhd", (0, 1))?;
        // template unsigned int(16) graphicsmode = 0; (copy over the existing image)
        stream.write_u16(0)?;
        // template unsigned int(16)[3] opcolor = {0, 0, 0};
        stream.write_u16(0)?;
        stream.write_u16(0)?;
        stream.write_u16(0)?;
        stream.finish_box()
    }

    pub(crate) fn write_dinf(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_box("dinf")?;
        {
            stream.start_full_box("dref", (0, 0))?;
            // unsigned int(32) entry_count
            stream.write_u32(1)?;
            {
                // flags:1 means data is in this file
                stream.start_full_box("url ", (0, 1))?;
                stream.finish_box()?;
            }
            stream.finish_box()?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_ccst(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("ccst", (0, 0))?;
        // unsigned int(1) all_ref_pics_intra;
        stream.write_bits(0, 1)?;
        // unsigned int(1) intra_pred_used;
        stream.write_bits(1, 1)?;
        // unsigned int(4) max_ref_per_pic;
        stream.write_bits(15, 4)?;
        // unsigned int(26) reserved;
        stream.write_bits(0, 2)?;
        stream.write_u8(0)?;
        stream.write_u8(0)?;
        stream.write_u8(0)?;
        stream.finish_box()
    }

    pub(crate) fn write_stsd(
        &self,
        stream: &mut OStream,
        image_metadata: &Image,
    ) -> AvifResult<()> {
        stream.start_full_box("stsd", (0, 0))?;
        // unsigned int(32) entry_count;
        stream.write_u32(1)?;
        {
            stream.start_box("av01")?;
            // const unsigned int(8)[6] reserved = 0;
            for _ in 0..6 {
                stream.write_u8(0)?;
            }
            // unsigned int(16) data_reference_index;
            stream.write_u16(1)?;
            // unsigned int(16) pre_defined = 0;
            stream.write_u16(0)?;
            // const unsigned int(16) reserved = 0;
            stream.write_u16(0)?;
            // unsigned int(32)[3] pre_defined = 0;
            stream.write_u32(0)?;
            stream.write_u32(0)?;
            stream.write_u32(0)?;
            // unsigned int(16) width;
            stream.write_u16(u16_from_u32(image_metadata.width)?)?;
            // unsigned int(16) height;
            stream.write_u16(u16_from_u32(image_metadata.height)?)?;
            // template unsigned int(32) horizresolution
            stream.write_u32(0x00480000)?;
            // template unsigned int(32) vertresolution
            stream.write_u32(0x00480000)?;
            // const unsigned int(32) reserved = 0;
            stream.write_u32(0)?;
            // template unsigned int(16) frame_count = 1;
            stream.write_u16(1)?;
            // string[32] compressorname;
            const COMPRESSOR_NAME: &str = "AOM Coding with CrabbyAvif      ";
            assert_eq!(COMPRESSOR_NAME.len(), 32);
            stream.write_str(COMPRESSOR_NAME)?;
            // template unsigned int(16) depth = 0x0018;
            stream.write_u16(0x0018)?;
            // int(16) pre_defined = -1
            stream.write_u16(0xffff)?;

            self.write_codec_config(stream)?;
            if self.category == Category::Color {
                self.write_icc(stream, image_metadata)?;
                self.write_nclx(stream, image_metadata)?;
                // TODO: Determine if HDR and transformative properties have to be written here or
                // not.
            }
            self.write_ccst(stream)?;

            stream.finish_box()?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_stts(
        &self,
        stream: &mut OStream,
        duration_in_timescales: &Vec<u64>,
    ) -> AvifResult<()> {
        let mut stts: Vec<(u64, u32)> = Vec::new();
        let mut current_value = None;
        let mut current_count = 0;
        for duration in duration_in_timescales {
            if let Some(current) = current_value {
                if *duration == current {
                    current_count += 1;
                } else {
                    stts.push((current, current_count));
                    current_value = Some(*duration);
                    current_count = 1;
                }
            } else {
                current_value = Some(*duration);
                current_count = 1;
            }
        }
        if let Some(current) = current_value {
            stts.push((current, current_count));
        }

        stream.start_full_box("stts", (0, 0))?;
        // unsigned int(32) entry_count;
        stream.write_u32(u32_from_usize(stts.len())?)?;
        for (sample_delta, sample_count) in stts {
            // unsigned int(32) sample_count;
            stream.write_u32(sample_count)?;
            // unsigned int(32) sample_delta;
            stream.write_u32(u32_from_u64(sample_delta)?)?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_stsc(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("stsc", (0, 0))?;
        // unsigned int(32) entry_count;
        stream.write_u32(1)?;
        // unsigned int(32) first_chunk;
        stream.write_u32(1)?;
        // unsigned int(32) samples_per_chunk;
        stream.write_u32(u32_from_usize(self.samples.len())?)?;
        // unsigned int(32) sample_description_index;
        stream.write_u32(1)?;
        stream.finish_box()
    }

    pub(crate) fn write_stsz(&self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("stsz", (0, 0))?;
        // unsigned int(32) sample_size;
        stream.write_u32(0)?;
        // unsigned int(32) sample_count;
        stream.write_u32(u32_from_usize(self.samples.len())?)?;
        for sample in &self.samples {
            // unsigned int(32) entry_size;
            stream.write_u32(u32_from_usize(sample.data.len())?)?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_stco(&mut self, stream: &mut OStream) -> AvifResult<()> {
        stream.start_full_box("stco", (0, 0))?;
        // unsigned int(32) entry_count;
        stream.write_u32(1)?;
        // unsigned int(32) chunk_offset;
        self.mdat_offset_locations.push(stream.offset());
        stream.write_u32(0)?;
        stream.finish_box()
    }

    pub(crate) fn write_stss(&mut self, stream: &mut OStream) -> AvifResult<()> {
        let sync_samples_count = self.samples.iter().filter(|x| x.sync).count();
        if sync_samples_count == self.samples.len() {
            // ISO/IEC 14496-12, Section 8.6.2.1:
            //   If the SyncSampleBox is not present, every sample is a sync sample.
            return Ok(());
        }
        stream.start_full_box("stss", (0, 0))?;
        // unsigned int(32) entry_count;
        stream.write_u32(u32_from_usize(sync_samples_count)?)?;
        for (index, sample) in self.samples.iter().enumerate() {
            if !sample.sync {
                continue;
            }
            // unsigned int(32) sample_number;
            stream.write_u32(u32_from_usize(index + 1)?)?;
        }
        stream.finish_box()
    }

    pub(crate) fn write_stbl(
        &mut self,
        stream: &mut OStream,
        image_metadata: &Image,
        duration_in_timescales: &Vec<u64>,
    ) -> AvifResult<()> {
        stream.start_box("stbl")?;
        self.write_stsd(stream, image_metadata)?;
        self.write_stts(stream, duration_in_timescales)?;
        self.write_stsc(stream)?;
        self.write_stsz(stream)?;
        self.write_stco(stream)?;
        self.write_stss(stream)?;
        stream.finish_box()
    }
}
