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

#![allow(non_upper_case_globals)]

use crate::codecs::*;
use crate::encoder::Sample;
use crate::encoder::ScalingMode;
use crate::image::Image;
use crate::image::YuvRange;
use crate::utils::IFraction;
use crate::*;

use aom_sys::bindings::*;

use std::cmp;
use std::mem::MaybeUninit;

#[derive(Default)]
pub struct Aom {
    encoder: Option<aom_codec_ctx_t>,
    aom_config: Option<aom_codec_enc_cfg>,
    config: Option<EncoderConfig>,
    current_layer: u32,
}

fn aom_format(image: &Image, category: Category) -> AvifResult<aom_img_fmt_t> {
    let format = match category {
        Category::Alpha => aom_img_fmt_AOM_IMG_FMT_I420,
        _ => match image.yuv_format {
            PixelFormat::Yuv420 | PixelFormat::Yuv400 => aom_img_fmt_AOM_IMG_FMT_I420,
            PixelFormat::Yuv422 => aom_img_fmt_AOM_IMG_FMT_I422,
            PixelFormat::Yuv444 => aom_img_fmt_AOM_IMG_FMT_I444,
            _ => return Err(AvifError::InvalidArgument),
        },
    };
    Ok(if image.depth > 8 { format | AOM_IMG_FMT_HIGHBITDEPTH } else { format })
}

fn aom_bps(format: aom_img_fmt_t) -> i32 {
    match format {
        aom_img_fmt_AOM_IMG_FMT_I420 => 12,
        aom_img_fmt_AOM_IMG_FMT_I422 => 16,
        aom_img_fmt_AOM_IMG_FMT_I444 => 24,
        aom_img_fmt_AOM_IMG_FMT_I42016 => 24,
        aom_img_fmt_AOM_IMG_FMT_I42216 => 32,
        aom_img_fmt_AOM_IMG_FMT_I44416 => 48,
        _ => 16,
    }
}

fn aom_seq_profile(image: &Image, category: Category) -> AvifResult<u32> {
    if image.depth == 12 {
        // 12 bit is always profile 2.
        return Ok(2);
    }
    if category == Category::Alpha {
        // Alpha is monochrome, so it is always profile 0.
        return Ok(0);
    }
    match image.yuv_format {
        PixelFormat::Yuv420 | PixelFormat::Yuv400 => Ok(0),
        PixelFormat::Yuv422 => Ok(2),
        PixelFormat::Yuv444 => Ok(1),
        _ => Err(AvifError::InvalidArgument),
    }
}

fn get_aom_scaling_mode_1d(mut fraction: IFraction) -> AvifResult<aom_scaling_mode_1d> {
    fraction.is_valid()?;
    fraction.simplify();
    Ok(match fraction {
        IFraction(1, 1) => aom_scaling_mode_1d_AOME_NORMAL,
        IFraction(1, 2) => aom_scaling_mode_1d_AOME_ONETWO,
        IFraction(1, 3) => aom_scaling_mode_1d_AOME_ONETHREE,
        IFraction(1, 4) => aom_scaling_mode_1d_AOME_ONEFOUR,
        IFraction(1, 8) => aom_scaling_mode_1d_AOME_ONEEIGHT,
        IFraction(2, 3) => aom_scaling_mode_1d_AOME_TWOTHREE,
        IFraction(3, 4) => aom_scaling_mode_1d_AOME_THREEFOUR,
        IFraction(3, 5) => aom_scaling_mode_1d_AOME_THREEFIVE,
        IFraction(4, 5) => aom_scaling_mode_1d_AOME_FOURFIVE,
        _ => return Err(AvifError::NotImplemented),
    })
}

fn aom_scaling_mode(scaling_mode: &ScalingMode) -> AvifResult<aom_scaling_mode_t> {
    Ok(aom_scaling_mode_t {
        h_scaling_mode: get_aom_scaling_mode_1d(scaling_mode.horizontal)?,
        v_scaling_mode: get_aom_scaling_mode_1d(scaling_mode.vertical)?,
    })
}

macro_rules! codec_control {
    ($self: expr, $key: expr, $value: expr) => {
        if unsafe { aom_codec_control($self.encoder.unwrap_mut() as *mut _, $key as _, $value) }
            != aom_codec_err_t_AOM_CODEC_OK
        {
            return Err(AvifError::UnknownError("".into()));
        }
    };
}

impl Encoder for Aom {
    fn encode_image(
        &mut self,
        image: &Image,
        category: Category,
        config: &EncoderConfig,
        output_samples: &mut Vec<Sample>,
    ) -> AvifResult<()> {
        if self.encoder.is_none() {
            let encoder_iface = unsafe { aom_codec_av1_cx() };
            let aom_usage = if config.is_single_image {
                AOM_USAGE_ALL_INTRA
            } else if config.speed.unwrap_or(0) >= 7 {
                AOM_USAGE_REALTIME
            } else {
                AOM_USAGE_GOOD_QUALITY
            };
            let mut cfg_uninit: MaybeUninit<aom_codec_enc_cfg> = MaybeUninit::uninit();
            let err = unsafe {
                aom_codec_enc_config_default(encoder_iface, cfg_uninit.as_mut_ptr(), aom_usage)
            };
            if err != aom_codec_err_t_AOM_CODEC_OK {
                return Err(AvifError::UnknownError("".into()));
            }
            let mut aom_config = unsafe { cfg_uninit.assume_init() };
            aom_config.rc_end_usage = match aom_usage {
                AOM_USAGE_REALTIME => aom_rc_mode_AOM_CBR,
                _ => aom_rc_mode_AOM_Q,
            };
            aom_config.g_profile = aom_seq_profile(image, category)?;
            aom_config.g_bit_depth = image.depth as _;
            aom_config.g_input_bit_depth = image.depth as _;
            aom_config.g_w = image.width;
            aom_config.g_h = image.height;

            if config.is_single_image {
                aom_config.g_limit = 1;
                aom_config.g_lag_in_frames = 0;
                aom_config.kf_mode = aom_kf_mode_AOM_KF_DISABLED;
                aom_config.kf_max_dist = 0;
            }
            if config.disable_lagged_output {
                aom_config.g_lag_in_frames = 0;
            }
            if config.extra_layer_count > 0 {
                aom_config.g_lag_in_frames = 0;
                aom_config.g_limit = config.extra_layer_count + 1;
            }
            if config.threads > 1 {
                aom_config.g_threads = cmp::min(config.threads, 64);
            }

            aom_config.monochrome =
                (category == Category::Alpha || image.yuv_format == PixelFormat::Yuv400).into();
            // TODO: Aom options pre init.
            aom_config.rc_min_quantizer = config.quantizer as u32;
            aom_config.rc_max_quantizer = config.quantizer as u32;

            let mut encoder_uninit: MaybeUninit<aom_codec_ctx_t> = MaybeUninit::uninit();
            let err = unsafe {
                aom_codec_enc_init_ver(
                    encoder_uninit.as_mut_ptr(),
                    encoder_iface,
                    &aom_config as *const _,
                    if image.depth > 8 { AOM_CODEC_USE_HIGHBITDEPTH } else { 0 } as _,
                    AOM_ENCODER_ABI_VERSION as _,
                )
            };
            if err != aom_codec_err_t_AOM_CODEC_OK {
                return Err(AvifError::UnknownError(format!(
                    "aom_codec_enc_init failed. err: {err}"
                )));
            }
            self.encoder = Some(unsafe { encoder_uninit.assume_init() });

            if aom_config.rc_end_usage == aom_rc_mode_AOM_CQ
                || aom_config.rc_end_usage == aom_rc_mode_AOM_Q
            {
                codec_control!(
                    self,
                    aome_enc_control_id_AOME_SET_CQ_LEVEL,
                    config.quantizer
                );
            }
            if config.quantizer == 0 {
                codec_control!(self, aome_enc_control_id_AV1E_SET_LOSSLESS, 1);
            }
            if config.tile_rows_log2 != 0 {
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_TILE_ROWS,
                    config.tile_rows_log2
                );
            }
            if config.tile_columns_log2 != 0 {
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_TILE_COLUMNS,
                    config.tile_columns_log2
                );
            }
            if config.extra_layer_count > 0 {
                codec_control!(
                    self,
                    aome_enc_control_id_AOME_SET_NUMBER_SPATIAL_LAYERS,
                    config.extra_layer_count + 1
                );
            }
            if let Some(speed) = config.speed {
                codec_control!(
                    self,
                    aome_enc_control_id_AOME_SET_CPUUSED,
                    cmp::min(speed, 9)
                );
            }
            match category {
                Category::Alpha => {
                    // AVIF specification, Section 4 "Auxiliary Image Items and Sequences":
                    //   The color_range field in the Sequence Header OBU shall be set to 1.
                    codec_control!(
                        self,
                        aome_enc_control_id_AV1E_SET_COLOR_RANGE,
                        aom_color_range_AOM_CR_FULL_RANGE
                    )
                    // Keep the default AOM_CSP_UNKNOWN value.

                    // CICP (CP/TC/MC) does not apply to the alpha auxiliary image.
                    // Keep default Unspecified (2) colour primaries, transfer characteristics,
                    // and matrix coefficients.
                }
                _ => {
                    // libaom's defaults are AOM_CSP_UNKNOWN and 0 (studio/limited range).
                    // Call aom_codec_control() only if the values are not the defaults.
                    // AV1-ISOBMFF specification, Section 2.3.4:
                    //   The value of full_range_flag in the 'colr' box SHALL match the color_range
                    //   flag in the Sequence Header OBU.
                    if image.yuv_range != YuvRange::Limited {
                        codec_control!(
                            self,
                            aome_enc_control_id_AV1E_SET_COLOR_RANGE,
                            aom_color_range_AOM_CR_FULL_RANGE
                        );
                    }
                    // Section 2.3.4 of AV1-ISOBMFF says 'colr' with 'nclx' should be present and
                    // shall match CICP values in the Sequence Header OBU, unless the latter has
                    // 2/2/2 (Unspecified). So set CICP values to 2/2/2 (Unspecified) in the
                    // Sequence Header OBU for simplicity. libaom's defaults are
                    // AOM_CICP_CP_UNSPECIFIED, AOM_CICP_TC_UNSPECIFIED, and
                    // AOM_CICP_MC_UNSPECIFIED. No need to call aom_codec_control().
                }
            }
            if aom_config.g_usage == AOM_USAGE_ALL_INTRA {
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_SKIP_POSTPROC_FILTERING,
                    1
                );
            }
            // TODO: Aom options post init.
            // TODO: tuning?
            self.aom_config = Some(aom_config);
            self.config = Some(*config);
        } else if self.config.unwrap_ref() != config {
            let aom_config = self.aom_config.unwrap_mut();
            if aom_config.g_w != image.width || aom_config.g_h != image.height {
                // Dimension changes aren't allowed.
                return Err(AvifError::NotImplemented);
            }
            let last_config = self.config.unwrap_ref();
            if last_config.quantizer != config.quantizer {
                if aom_config.rc_end_usage == aom_rc_mode_AOM_VBR
                    || aom_config.rc_end_usage == aom_rc_mode_AOM_CBR
                {
                    aom_config.rc_min_quantizer = config.quantizer as u32;
                    aom_config.rc_max_quantizer = config.quantizer as u32;
                    let err = unsafe {
                        aom_codec_enc_config_set(
                            self.encoder.unwrap_mut() as *mut _,
                            self.aom_config.unwrap_ref() as *const _,
                        )
                    };
                    if err != aom_codec_err_t_AOM_CODEC_OK {
                        return Err(AvifError::UnknownError(format!(
                            "aom_codec_enc_config_set failed. err: {err}"
                        )));
                    }
                } else if aom_config.rc_end_usage == aom_rc_mode_AOM_CQ
                    || aom_config.rc_end_usage == aom_rc_mode_AOM_Q
                {
                    codec_control!(
                        self,
                        aome_enc_control_id_AOME_SET_CQ_LEVEL,
                        config.quantizer
                    );
                }
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_LOSSLESS,
                    if config.quantizer == 0 { 1 } else { 0 }
                );
            }
            if last_config.tile_rows_log2 != config.tile_rows_log2 {
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_TILE_ROWS,
                    config.tile_rows_log2
                );
            }
            if last_config.tile_columns_log2 != config.tile_columns_log2 {
                codec_control!(
                    self,
                    aome_enc_control_id_AV1E_SET_TILE_COLUMNS,
                    config.tile_columns_log2
                );
            }
            self.config = Some(*config);
        }
        if self.current_layer > config.extra_layer_count {
            return Err(AvifError::InvalidArgument);
        }
        if config.extra_layer_count > 0 {
            codec_control!(
                self,
                aome_enc_control_id_AOME_SET_SPATIAL_LAYER_ID,
                self.current_layer
            );
        }
        let scaling_mode = aom_scaling_mode(&self.config.unwrap_ref().scaling_mode)?;
        if scaling_mode.h_scaling_mode != aom_scaling_mode_1d_AOME_NORMAL
            || scaling_mode.v_scaling_mode != aom_scaling_mode_1d_AOME_NORMAL
        {
            codec_control!(
                self,
                aome_enc_control_id_AOME_SET_SCALEMODE,
                &scaling_mode as *const _
            );
        }
        let mut aom_image: aom_image_t = unsafe { std::mem::zeroed() };
        aom_image.fmt = aom_format(image, category)?;
        aom_image.bit_depth = if image.depth > 8 { 16 } else { 8 };
        aom_image.w = image.width;
        aom_image.h = image.height;
        aom_image.d_w = image.width;
        aom_image.d_h = image.height;
        aom_image.bps = aom_bps(aom_image.fmt);
        aom_image.x_chroma_shift = image.yuv_format.chroma_shift_x().0;
        aom_image.y_chroma_shift = image.yuv_format.chroma_shift_y();
        match category {
            Category::Alpha => {
                aom_image.range = aom_color_range_AOM_CR_FULL_RANGE;
                aom_image.monochrome = 1;
                aom_image.x_chroma_shift = 1;
                aom_image.y_chroma_shift = 1;
                aom_image.planes[0] = image.planes[3].unwrap_ref().ptr_generic() as *mut _;
                aom_image.stride[0] = image.row_bytes[3] as i32;
            }
            _ => {
                aom_image.range = image.yuv_range as u32;
                if image.yuv_format == PixelFormat::Yuv400 {
                    aom_image.monochrome = 1;
                    aom_image.x_chroma_shift = 1;
                    aom_image.y_chroma_shift = 1;
                    aom_image.planes[0] = image.planes[0].unwrap_ref().ptr_generic() as *mut _;
                    aom_image.stride[0] = image.row_bytes[0] as i32;
                } else {
                    aom_image.monochrome = 0;
                    for i in 0..=2 {
                        aom_image.planes[i] = image.planes[i].unwrap_ref().ptr_generic() as *mut _;
                        aom_image.stride[i] = image.row_bytes[i] as i32;
                    }
                }
            }
        }
        aom_image.cp = image.color_primaries as u32;
        aom_image.tc = image.transfer_characteristics as u32;
        aom_image.mc = image.matrix_coefficients as u32;
        // TODO: b/392112497 - force keyframes when necessary.
        let mut encode_flags = 0i64;
        if self.current_layer > 0 {
            encode_flags |= AOM_EFLAG_NO_REF_GF as i64
                | AOM_EFLAG_NO_REF_ARF as i64
                | AOM_EFLAG_NO_REF_BWD as i64
                | AOM_EFLAG_NO_REF_ARF2 as i64
                | AOM_EFLAG_NO_UPD_GF as i64
                | AOM_EFLAG_NO_UPD_ARF as i64;
        }
        let err = unsafe {
            aom_codec_encode(
                self.encoder.unwrap_mut() as *mut _,
                &aom_image as *const _,
                0,
                1,
                encode_flags,
            )
        };
        if err != aom_codec_err_t_AOM_CODEC_OK {
            return Err(AvifError::UnknownError(format!("err: {err}")));
        }
        let mut iter: aom_codec_iter_t = std::ptr::null_mut();
        loop {
            let pkt = unsafe {
                aom_codec_get_cx_data(self.encoder.unwrap_mut() as *mut _, &mut iter as *mut _)
            };
            if pkt.is_null() {
                break;
            }
            let pkt = unsafe { *pkt };
            if pkt.kind == aom_codec_cx_pkt_kind_AOM_CODEC_CX_FRAME_PKT {
                unsafe {
                    let encoded_data = std::slice::from_raw_parts(
                        pkt.data.frame.buf as *const u8,
                        pkt.data.frame.sz,
                    );
                    let sync = (pkt.data.frame.flags & AOM_FRAME_IS_KEY) != 0;
                    output_samples.push(Sample::create_from(encoded_data, sync)?);
                }
            }
        }
        if config.is_single_image
            || (config.extra_layer_count > 0 && config.extra_layer_count == self.current_layer)
        {
            self.finish(output_samples)?;
            unsafe {
                aom_codec_destroy(self.encoder.unwrap_mut() as *mut _);
            }
            self.encoder = None;
        }
        if config.extra_layer_count > 0 {
            self.current_layer += 1;
        }
        Ok(())
    }

    fn finish(&mut self, output_samples: &mut Vec<crate::encoder::Sample>) -> AvifResult<()> {
        if self.encoder.is_none() {
            return Ok(());
        }
        loop {
            // Flush the encoder.
            let err = unsafe {
                aom_codec_encode(
                    self.encoder.unwrap_mut() as *mut _,
                    std::ptr::null(),
                    0,
                    1,
                    0,
                )
            };
            if err != aom_codec_err_t_AOM_CODEC_OK {
                return Err(AvifError::UnknownError("".into()));
            }
            let mut got_packet = false;
            let mut iter: aom_codec_iter_t = std::ptr::null_mut();
            loop {
                let pkt = unsafe {
                    aom_codec_get_cx_data(self.encoder.unwrap_mut() as *mut _, &mut iter as *mut _)
                };
                if pkt.is_null() {
                    break;
                }
                let pkt = unsafe { *pkt };
                if pkt.kind == aom_codec_cx_pkt_kind_AOM_CODEC_CX_FRAME_PKT {
                    got_packet = true;
                    unsafe {
                        let encoded_data = std::slice::from_raw_parts(
                            pkt.data.frame.buf as *const u8,
                            pkt.data.frame.sz,
                        );
                        let sync = (pkt.data.frame.flags & AOM_FRAME_IS_KEY) != 0;
                        output_samples.push(Sample::create_from(encoded_data, sync)?);
                    }
                }
            }
            if !got_packet {
                break;
            }
        }
        Ok(())
    }
}

impl Drop for Aom {
    fn drop(&mut self) {
        if self.encoder.is_some() {
            unsafe {
                aom_codec_destroy(self.encoder.unwrap_mut() as *mut _);
            }
        }
    }
}
