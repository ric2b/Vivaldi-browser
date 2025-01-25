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

use crate::image::YuvRange;
use crate::internal_utils::stream::*;
use crate::internal_utils::*;
use crate::parser::mp4box::Av1CodecConfiguration;
use crate::*;

#[derive(Debug)]
struct ObuHeader {
    obu_type: u8,
    size: u32,
}

#[derive(Debug, Default)]
pub struct Av1SequenceHeader {
    reduced_still_picture_header: bool,
    max_width: u32,
    max_height: u32,
    bit_depth: u8,
    yuv_format: PixelFormat,
    #[allow(unused)]
    chroma_sample_position: ChromaSamplePosition,
    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,
    pub yuv_range: YuvRange,
    config: Av1CodecConfiguration,
}

impl Av1SequenceHeader {
    fn parse_profile(&mut self, bits: &mut IBitStream) -> AvifResult<()> {
        self.config.seq_profile = bits.read(3)? as u8;
        if self.config.seq_profile > 2 {
            return Err(AvifError::BmffParseFailed("invalid seq_profile".into()));
        }
        let still_picture = bits.read_bool()?;
        self.reduced_still_picture_header = bits.read_bool()?;
        if self.reduced_still_picture_header && !still_picture {
            return Err(AvifError::BmffParseFailed(
                "invalid reduced_still_picture_header".into(),
            ));
        }
        if self.reduced_still_picture_header {
            self.config.seq_level_idx0 = bits.read(5)? as u8;
        } else {
            let mut buffer_delay_length = 0;
            let mut decoder_model_info_present_flag = false;
            let timing_info_present_flag = bits.read_bool()?;
            if timing_info_present_flag {
                // num_units_in_display_tick
                bits.skip(32)?;
                // time_scale
                bits.skip(32)?;
                let equal_picture_interval = bits.read_bool()?;
                if equal_picture_interval {
                    // num_ticks_per_picture_minus_1
                    bits.skip_uvlc()?;
                }
                decoder_model_info_present_flag = bits.read_bool()?;
                if decoder_model_info_present_flag {
                    let buffer_delay_length_minus_1 = bits.read(5)?;
                    buffer_delay_length = buffer_delay_length_minus_1 + 1;
                    // num_units_in_decoding_tick
                    bits.skip(32)?;
                    // buffer_removal_time_length_minus_1
                    bits.skip(5)?;
                    // frame_presentation_time_length_minus_1
                    bits.skip(5)?;
                }
            }
            let initial_display_delay_present_flag = bits.read_bool()?;
            let operating_points_cnt_minus_1 = bits.read(5)?;
            let operating_points_cnt = operating_points_cnt_minus_1 + 1;
            for i in 0..operating_points_cnt {
                // operating_point_idc
                bits.skip(12)?;
                let seq_level_idx = bits.read(5)?;
                if i == 0 {
                    self.config.seq_level_idx0 = seq_level_idx as u8;
                }
                if seq_level_idx > 7 {
                    let seq_tier = bits.read(1)?;
                    if i == 0 {
                        self.config.seq_tier0 = seq_tier as u8;
                    }
                }
                if decoder_model_info_present_flag {
                    let decoder_model_present_for_this_op = bits.read_bool()?;
                    if decoder_model_present_for_this_op {
                        // decoder_buffer_delay
                        bits.skip(buffer_delay_length as usize)?;
                        // encoder_buffer_delay
                        bits.skip(buffer_delay_length as usize)?;
                        // low_delay_mode_flag
                        bits.skip(1)?;
                    }
                }
                if initial_display_delay_present_flag {
                    let initial_display_delay_present_for_this_op = bits.read_bool()?;
                    if initial_display_delay_present_for_this_op {
                        // initial_display_delay_minus_1
                        bits.skip(4)?;
                    }
                }
            }
        }
        Ok(())
    }

    fn parse_frame_max_dimensions(&mut self, bits: &mut IBitStream) -> AvifResult<()> {
        let frame_width_bits_minus_1 = bits.read(4)?;
        let frame_height_bits_minus_1 = bits.read(4)?;
        let max_frame_width_minus_1 = bits.read(frame_width_bits_minus_1 as usize + 1)?;
        let max_frame_height_minus_1 = bits.read(frame_height_bits_minus_1 as usize + 1)?;
        self.max_width = checked_add!(max_frame_width_minus_1, 1)?;
        self.max_height = checked_add!(max_frame_height_minus_1, 1)?;
        let frame_id_numbers_present_flag =
            if self.reduced_still_picture_header { false } else { bits.read_bool()? };
        if frame_id_numbers_present_flag {
            // delta_frame_id_length_minus_2
            bits.skip(4)?;
            // additional_frame_id_length_minus_1
            bits.skip(3)?;
        }
        Ok(())
    }

    fn parse_enabled_features(&mut self, bits: &mut IBitStream) -> AvifResult<()> {
        // use_128x128_superblock
        bits.skip(1)?;
        // enable_filter_intra
        bits.skip(1)?;
        // enable_intra_edge_filter
        bits.skip(1)?;
        if self.reduced_still_picture_header {
            return Ok(());
        }
        // enable_interintra_compound
        bits.skip(1)?;
        // enable_masked_compound
        bits.skip(1)?;
        // enable_warped_motion
        bits.skip(1)?;
        // enable_dual_filter
        bits.skip(1)?;
        let enable_order_hint = bits.read_bool()?;
        if enable_order_hint {
            // enable_jnt_comp
            bits.skip(1)?;
            // enable_ref_frame_mvs
            bits.skip(1)?;
        }
        let seq_choose_screen_content_tools = bits.read_bool()?;
        let seq_force_screen_content_tools = if seq_choose_screen_content_tools {
            2 // SELECT_SCREEN_CONTENT_TOOLS
        } else {
            bits.read(1)?
        };
        if seq_force_screen_content_tools > 0 {
            let seq_choose_integer_mv = bits.read_bool()?;
            if !seq_choose_integer_mv {
                // seq_force_integer_mv
                bits.skip(1)?;
            }
        }
        if enable_order_hint {
            // order_hint_bits_minus_1
            bits.skip(3)?;
        }
        Ok(())
    }

    fn parse_color_config(&mut self, bits: &mut IBitStream) -> AvifResult<()> {
        self.config.high_bitdepth = bits.read_bool()?;
        if self.config.seq_profile == 2 && self.config.high_bitdepth {
            self.config.twelve_bit = bits.read_bool()?;
            self.bit_depth = if self.config.twelve_bit { 12 } else { 10 };
        } else {
            self.bit_depth = if self.config.high_bitdepth { 10 } else { 8 };
        }
        if self.config.seq_profile != 1 {
            self.config.monochrome = bits.read_bool()?;
        }
        let color_description_present_flag = bits.read_bool()?;
        if color_description_present_flag {
            self.color_primaries = (bits.read(8)? as u16).into();
            self.transfer_characteristics = (bits.read(8)? as u16).into();
            self.matrix_coefficients = (bits.read(8)? as u16).into();
        } else {
            self.color_primaries = ColorPrimaries::Unspecified;
            self.transfer_characteristics = TransferCharacteristics::Unspecified;
            self.matrix_coefficients = MatrixCoefficients::Unspecified;
        }
        if self.config.monochrome {
            let color_range = bits.read_bool()?;
            self.yuv_range = if color_range { YuvRange::Full } else { YuvRange::Limited };
            self.config.chroma_subsampling_x = 1;
            self.config.chroma_subsampling_y = 1;
            self.yuv_format = PixelFormat::Yuv400;
            return Ok(());
        } else if self.color_primaries == ColorPrimaries::Bt709
            && self.transfer_characteristics == TransferCharacteristics::Srgb
            && self.matrix_coefficients == MatrixCoefficients::Identity
        {
            self.yuv_range = YuvRange::Full;
            self.yuv_format = PixelFormat::Yuv444;
        } else {
            let color_range = bits.read_bool()?;
            self.yuv_range = if color_range { YuvRange::Full } else { YuvRange::Limited };
            match self.config.seq_profile {
                0 => {
                    self.config.chroma_subsampling_x = 1;
                    self.config.chroma_subsampling_y = 1;
                    self.yuv_format = PixelFormat::Yuv420;
                }
                1 => {
                    self.yuv_format = PixelFormat::Yuv444;
                }
                2 => {
                    if self.bit_depth == 12 {
                        self.config.chroma_subsampling_x = bits.read(1)? as u8;
                        if self.config.chroma_subsampling_x == 1 {
                            self.config.chroma_subsampling_y = bits.read(1)? as u8;
                        }
                    } else {
                        self.config.chroma_subsampling_x = 1;
                    }
                    self.yuv_format = if self.config.chroma_subsampling_x == 1 {
                        if self.config.chroma_subsampling_y == 1 {
                            PixelFormat::Yuv420
                        } else {
                            PixelFormat::Yuv422
                        }
                    } else {
                        PixelFormat::Yuv444
                    };
                }
                _ => {} // Not reached.
            }
            if self.config.chroma_subsampling_x == 1 && self.config.chroma_subsampling_y == 1 {
                self.config.chroma_sample_position = bits.read(2)?.into();
            }
        }
        // separate_uv_delta_q
        bits.skip(1)?;
        Ok(())
    }

    fn parse_obu_header(stream: &mut IStream) -> AvifResult<ObuHeader> {
        let mut bits = stream.sub_bit_stream(1)?;

        // Section 5.3.2 of AV1 specification.
        // https://aomediacodec.github.io/av1-spec/#obu-header-syntax
        let obu_forbidden_bit = bits.read(1)?;
        if obu_forbidden_bit != 0 {
            return Err(AvifError::BmffParseFailed(
                "invalid obu_forbidden_bit".into(),
            ));
        }
        let obu_type = bits.read(4)? as u8;
        let obu_extension_flag = bits.read_bool()?;
        let obu_has_size_field = bits.read_bool()?;
        // obu_reserved_1bit
        bits.skip(1)?; // "The value is ignored by a decoder."

        if obu_extension_flag {
            let mut bits = stream.sub_bit_stream(1)?;
            // temporal_id
            bits.skip(3)?;
            // spatial_id
            bits.skip(2)?;
            // extension_header_reserved_3bits
            bits.skip(3)?;
        }

        let size = if obu_has_size_field {
            stream.read_uleb128()?
        } else {
            u32_from_usize(stream.bytes_left()?)? // sz - 1 - obu_extension_flag
        };

        Ok(ObuHeader { obu_type, size })
    }

    pub fn parse_from_obus(data: &[u8]) -> AvifResult<Self> {
        let mut stream = IStream::create(data);

        while stream.has_bytes_left()? {
            let obu = Self::parse_obu_header(&mut stream)?;
            if obu.obu_type != /*OBU_SEQUENCE_HEADER=*/1 {
                // Not a sequence header. Skip this obu.
                stream.skip(usize_from_u32(obu.size)?)?;
                continue;
            }
            let mut bits = stream.sub_bit_stream(usize_from_u32(obu.size)?)?;
            let mut sequence_header = Av1SequenceHeader::default();
            sequence_header.parse_profile(&mut bits)?;
            sequence_header.parse_frame_max_dimensions(&mut bits)?;
            sequence_header.parse_enabled_features(&mut bits)?;
            // enable_superres
            bits.skip(1)?;
            // enable_cdef
            bits.skip(1)?;
            // enable_restoration
            bits.skip(1)?;
            sequence_header.parse_color_config(&mut bits)?;
            // film_grain_params_present
            bits.skip(1)?;
            return Ok(sequence_header);
        }
        Err(AvifError::BmffParseFailed(
            "could not parse sequence header".into(),
        ))
    }
}
