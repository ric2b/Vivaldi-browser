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

use crate::codecs::Decoder;
use crate::codecs::DecoderConfig;
use crate::decoder::Category;
use crate::image::Image;
use crate::image::YuvRange;
use crate::internal_utils::pixels::*;
use crate::internal_utils::stream::IStream;
use crate::internal_utils::*;
use crate::*;

use ndk_sys::bindings::*;

use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr;

#[cfg(android_soong)]
include!(concat!(env!("OUT_DIR"), "/mediaimage2_bindgen.rs"));

#[derive(Debug)]
struct MediaFormat {
    format: *mut AMediaFormat,
}

macro_rules! c_str {
    ($var: ident, $var_tmp:ident, $str:expr) => {
        let $var_tmp = CString::new($str).unwrap();
        let $var = $var_tmp.as_ptr();
    };
}

#[derive(Debug, Default)]
struct PlaneInfo {
    color_format: i32,
    offset: [isize; 3],
    row_stride: [u32; 3],
    column_stride: [u32; 3],
}

impl PlaneInfo {
    fn pixel_format(&self) -> PixelFormat {
        match self.color_format {
            MediaCodec::YUV_P010 => PixelFormat::AndroidP010,
            _ => {
                let u_before_v = self.offset[2] == self.offset[1] + 1;
                let v_before_u = self.offset[1] == self.offset[2] + 1;
                let is_nv_format = self.column_stride == [1, 2, 2] && (u_before_v || v_before_u);
                match (is_nv_format, u_before_v) {
                    (true, true) => PixelFormat::AndroidNv12,
                    (true, false) => PixelFormat::AndroidNv21,
                    (false, _) => PixelFormat::Yuv420,
                }
            }
        }
    }

    fn depth(&self) -> u8 {
        match self.color_format {
            MediaCodec::YUV_P010 => 10,
            _ => 8,
        }
    }
}

impl MediaFormat {
    fn get_i32(&self, key: *const c_char) -> Option<i32> {
        let mut value: i32 = 0;
        match unsafe { AMediaFormat_getInt32(self.format, key, &mut value as *mut _) } {
            true => Some(value),
            false => None,
        }
    }

    fn get_i32_from_str(&self, key: &str) -> Option<i32> {
        c_str!(key_str, key_str_tmp, key);
        self.get_i32(key_str)
    }

    fn width(&self) -> AvifResult<i32> {
        self.get_i32(unsafe { AMEDIAFORMAT_KEY_WIDTH })
            .ok_or(AvifError::UnknownError("".into()))
    }

    fn height(&self) -> AvifResult<i32> {
        self.get_i32(unsafe { AMEDIAFORMAT_KEY_HEIGHT })
            .ok_or(AvifError::UnknownError("".into()))
    }

    fn slice_height(&self) -> AvifResult<i32> {
        self.get_i32(unsafe { AMEDIAFORMAT_KEY_SLICE_HEIGHT })
            .ok_or(AvifError::UnknownError("".into()))
    }

    fn stride(&self) -> AvifResult<i32> {
        self.get_i32(unsafe { AMEDIAFORMAT_KEY_STRIDE })
            .ok_or(AvifError::UnknownError("".into()))
    }

    fn color_format(&self) -> AvifResult<i32> {
        self.get_i32(unsafe { AMEDIAFORMAT_KEY_COLOR_FORMAT })
            .ok_or(AvifError::UnknownError("".into()))
    }

    fn color_range(&self) -> YuvRange {
        // color-range is documented but isn't exposed as a constant in the NDK:
        // https://developer.android.com/reference/android/media/MediaFormat#KEY_COLOR_RANGE
        let color_range = self.get_i32_from_str("color-range").unwrap_or(2);
        if color_range == 0 {
            YuvRange::Limited
        } else {
            YuvRange::Full
        }
    }

    fn guess_plane_info(&self) -> AvifResult<PlaneInfo> {
        let height = self.height()?;
        let slice_height = self.slice_height().unwrap_or(height);
        let stride = self.stride()?;
        let color_format = self.color_format()?;
        let mut plane_info = PlaneInfo {
            color_format,
            ..Default::default()
        };
        match color_format {
            MediaCodec::YUV_P010 => {
                plane_info.row_stride = [
                    u32_from_i32(stride)?,
                    u32_from_i32(stride)?,
                    0, // V plane is not used for P010.
                ];
                plane_info.column_stride = [
                    2, 2, 0, // V plane is not used for P010.
                ];
                plane_info.offset = [
                    0,
                    isize_from_i32(stride * slice_height)?,
                    0, // V plane is not used for P010.
                ];
            }
            _ => {
                plane_info.row_stride = [
                    u32_from_i32(stride)?,
                    u32_from_i32((stride + 1) / 2)?,
                    u32_from_i32((stride + 1) / 2)?,
                ];
                plane_info.column_stride = [1, 1, 1];
                plane_info.offset[0] = 0;
                plane_info.offset[1] = isize_from_i32(stride * slice_height)?;
                let u_plane_size = isize_from_i32(((stride + 1) / 2) * ((height + 1) / 2))?;
                // When color format is YUV_420_FLEXIBLE, the V plane comes before the U plane.
                plane_info.offset[2] = plane_info.offset[1] - u_plane_size;
            }
        }
        Ok(plane_info)
    }

    fn get_plane_info(&self) -> AvifResult<PlaneInfo> {
        // When not building for the Android platform, image-data is not available, so simply try to
        // guess the buffer format based on the available keys in the format.
        #[cfg(not(android_soong))]
        return self.guess_plane_info();

        #[cfg(android_soong)]
        {
            c_str!(key_str, key_str_tmp, "image-data");
            let mut data: *mut std::ffi::c_void = ptr::null_mut();
            let mut size: usize = 0;
            if !unsafe {
                AMediaFormat_getBuffer(
                    self.format,
                    key_str,
                    &mut data as *mut _,
                    &mut size as *mut _,
                )
            } {
                return self.guess_plane_info();
            }
            if size != std::mem::size_of::<android_MediaImage2>() {
                return self.guess_plane_info();
            }
            let image_data = unsafe { *(data as *const android_MediaImage2) };
            if image_data.mType != android_MediaImage2_Type_MEDIA_IMAGE_TYPE_YUV {
                return self.guess_plane_info();
            }
            let planes = unsafe { ptr::read_unaligned(ptr::addr_of!(image_data.mPlane)) };
            let mut plane_info = PlaneInfo {
                color_format: self.color_format()?,
                ..Default::default()
            };
            for plane_index in 0usize..3 {
                plane_info.offset[plane_index] = isize_from_u32(planes[plane_index].mOffset)?;
                plane_info.row_stride[plane_index] = u32_from_i32(planes[plane_index].mRowInc)?;
                plane_info.column_stride[plane_index] = u32_from_i32(planes[plane_index].mColInc)?;
            }
            return Ok(plane_info);
        }
    }
}

enum CodecInitializer {
    ByName(String),
    ByMimeType(String),
}

#[cfg(android_soong)]
fn prefer_hardware_decoder(config: &DecoderConfig) -> bool {
    let prefer_hw = rustutils::system_properties::read_bool(
        "media.stagefright.thumbnail.prefer_hw_codecs",
        false,
    )
    .unwrap_or(false);
    // We will return true when all of the below conditions are true:
    // 1) prefer_hw is true.
    // 2) category is not Alpha. We do not prefer hardware for decoding the alpha plane since
    //    they generally tend to be monochrome images and using hardware for that is
    //    unreliable.
    // 3) profile is 0. As of Sep 2024, there are no AV1 hardware decoders that support
    //    anything other than profile 0.
    prefer_hw && config.category != Category::Alpha && config.codec_config.profile() == 0
}

fn get_codec_initializers(config: &DecoderConfig) -> Vec<CodecInitializer> {
    #[cfg(android_soong)]
    {
        // Use a specific decoder if it is requested.
        if let Ok(Some(decoder)) =
            rustutils::system_properties::read("media.crabbyavif.debug.decoder")
        {
            if !decoder.is_empty() {
                return vec![CodecInitializer::ByName(decoder)];
            }
        }
    }
    let dav1d = String::from("c2.android.av1-dav1d.decoder");
    let gav1 = String::from("c2.android.av1.decoder");
    let hevc = String::from("c2.android.hevc.decoder");
    // As of Sep 2024, c2.android.av1.decoder is the only known decoder to support 12-bit AV1. So
    // prefer that for 12 bit images.
    let prefer_gav1 = config.depth == 12;
    let is_avif = config.codec_config.is_avif();
    let mime_type = if is_avif { MediaCodec::AV1_MIME } else { MediaCodec::HEVC_MIME };
    let prefer_hw = false;
    #[cfg(android_soong)]
    let prefer_hw = prefer_hardware_decoder(config);
    match (prefer_hw, is_avif, prefer_gav1) {
        (true, false, _) => vec![
            CodecInitializer::ByMimeType(mime_type.to_string()),
            CodecInitializer::ByName(hevc),
        ],
        (false, false, _) => vec![
            CodecInitializer::ByName(hevc),
            CodecInitializer::ByMimeType(mime_type.to_string()),
        ],
        (true, true, true) => vec![
            CodecInitializer::ByName(gav1),
            CodecInitializer::ByMimeType(mime_type.to_string()),
            CodecInitializer::ByName(dav1d),
        ],
        (true, true, false) => vec![
            CodecInitializer::ByMimeType(mime_type.to_string()),
            CodecInitializer::ByName(dav1d),
            CodecInitializer::ByName(gav1),
        ],
        (false, true, true) => vec![
            CodecInitializer::ByName(gav1),
            CodecInitializer::ByName(dav1d),
            CodecInitializer::ByMimeType(mime_type.to_string()),
        ],
        (false, true, false) => vec![
            CodecInitializer::ByName(dav1d),
            CodecInitializer::ByName(gav1),
            CodecInitializer::ByMimeType(mime_type.to_string()),
        ],
    }
}

#[derive(Default)]
pub struct MediaCodec {
    codec: Option<*mut AMediaCodec>,
    format: Option<MediaFormat>,
    output_buffer_index: Option<usize>,
    config: Option<DecoderConfig>,
}

impl MediaCodec {
    // Flexible YUV 420 format used for 8-bit images:
    // https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities#COLOR_FormatYUV420Flexible
    const YUV_420_FLEXIBLE: i32 = 2135033992;
    // YUV P010 format used for 10-bit images:
    // https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities#COLOR_FormatYUVP010
    const YUV_P010: i32 = 54;
    const AV1_MIME: &str = "video/av01";
    const HEVC_MIME: &str = "video/hevc";
}

impl Decoder for MediaCodec {
    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()> {
        if self.codec.is_some() {
            return Ok(()); // Already initialized.
        }
        let format = unsafe { AMediaFormat_new() };
        if format.is_null() {
            return Err(AvifError::UnknownError("".into()));
        }
        c_str!(
            mime_type,
            mime_type_tmp,
            if config.codec_config.is_avif() { Self::AV1_MIME } else { Self::HEVC_MIME }
        );
        unsafe {
            AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime_type);
            AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, i32_from_u32(config.width)?);
            AMediaFormat_setInt32(
                format,
                AMEDIAFORMAT_KEY_HEIGHT,
                i32_from_u32(config.height)?,
            );
            AMediaFormat_setInt32(
                format,
                AMEDIAFORMAT_KEY_COLOR_FORMAT,
                if config.depth == 10 { Self::YUV_P010 } else { Self::YUV_420_FLEXIBLE },
            );
            // low-latency is documented but isn't exposed as a constant in the NDK:
            // https://developer.android.com/reference/android/media/MediaFormat#KEY_LOW_LATENCY
            c_str!(low_latency, low_latency_tmp, "low-latency");
            AMediaFormat_setInt32(format, low_latency, 1);
            AMediaFormat_setInt32(
                format,
                AMEDIAFORMAT_KEY_MAX_INPUT_SIZE,
                i32_from_usize(config.max_input_size)?,
            );
            let codec_specific_data = config.codec_config.raw_data();
            if !codec_specific_data.is_empty() {
                AMediaFormat_setBuffer(
                    format,
                    AMEDIAFORMAT_KEY_CSD_0,
                    codec_specific_data.as_ptr() as *const _,
                    codec_specific_data.len(),
                );
            }
        }

        let mut codec = ptr::null_mut();
        for codec_initializer in get_codec_initializers(config) {
            codec = match codec_initializer {
                CodecInitializer::ByName(name) => {
                    c_str!(codec_name, codec_name_tmp, name.as_str());
                    unsafe { AMediaCodec_createCodecByName(codec_name) }
                }
                CodecInitializer::ByMimeType(mime_type) => {
                    c_str!(codec_mime, codec_mime_tmp, mime_type.as_str());
                    unsafe { AMediaCodec_createDecoderByType(codec_mime) }
                }
            };
            if codec.is_null() {
                continue;
            }
            let status = unsafe {
                AMediaCodec_configure(codec, format, ptr::null_mut(), ptr::null_mut(), 0)
            };
            if status != media_status_t_AMEDIA_OK {
                unsafe {
                    AMediaCodec_delete(codec);
                }
                codec = ptr::null_mut();
                continue;
            }
            let status = unsafe { AMediaCodec_start(codec) };
            if status != media_status_t_AMEDIA_OK {
                unsafe {
                    AMediaCodec_delete(codec);
                }
                codec = ptr::null_mut();
                continue;
            }
            break;
        }
        if codec.is_null() {
            unsafe { AMediaFormat_delete(format) };
            return Err(AvifError::NoCodecAvailable);
        }
        self.codec = Some(codec);
        self.config = Some(config.clone());
        Ok(())
    }

    fn get_next_image(
        &mut self,
        payload: &[u8],
        _spatial_id: u8,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()> {
        if self.codec.is_none() {
            self.initialize(&DecoderConfig::default())?;
        }
        let codec = self.codec.unwrap();
        if self.output_buffer_index.is_some() {
            // Release any existing output buffer.
            unsafe {
                AMediaCodec_releaseOutputBuffer(codec, self.output_buffer_index.unwrap(), false);
            }
        }
        unsafe {
            let input_index = AMediaCodec_dequeueInputBuffer(codec, 0);
            if input_index >= 0 {
                let mut input_buffer_size: usize = 0;
                let input_buffer = AMediaCodec_getInputBuffer(
                    codec,
                    input_index as usize,
                    &mut input_buffer_size as *mut _,
                );
                if input_buffer.is_null() {
                    return Err(AvifError::UnknownError(format!(
                        "input buffer at index {input_index} was null"
                    )));
                }
                let hevc_whole_nal_units = self.hevc_whole_nal_units(payload)?;
                let codec_payload = match &hevc_whole_nal_units {
                    Some(hevc_payload) => hevc_payload,
                    None => payload,
                };
                if input_buffer_size < codec_payload.len() {
                    return Err(AvifError::UnknownError(format!(
                        "input buffer (size {input_buffer_size}) was not big enough. required size: {}",
                        codec_payload.len()
                    )));
                }
                ptr::copy_nonoverlapping(codec_payload.as_ptr(), input_buffer, codec_payload.len());

                if AMediaCodec_queueInputBuffer(
                    codec,
                    usize_from_isize(input_index)?,
                    /*offset=*/ 0,
                    codec_payload.len(),
                    /*pts=*/ 0,
                    /*flags=*/ 0,
                ) != media_status_t_AMEDIA_OK
                {
                    return Err(AvifError::UnknownError("".into()));
                }
            } else {
                return Err(AvifError::UnknownError(format!(
                    "got input index < 0: {input_index}"
                )));
            }
        }
        let mut buffer: Option<*mut u8> = None;
        let mut buffer_size: usize = 0;
        let mut retry_count = 0;
        let mut buffer_info = AMediaCodecBufferInfo::default();
        while retry_count < 100 {
            retry_count += 1;
            unsafe {
                let output_index =
                    AMediaCodec_dequeueOutputBuffer(codec, &mut buffer_info as *mut _, 10000);
                if output_index >= 0 {
                    let output_buffer = AMediaCodec_getOutputBuffer(
                        codec,
                        usize_from_isize(output_index)?,
                        &mut buffer_size as *mut _,
                    );
                    if output_buffer.is_null() {
                        return Err(AvifError::UnknownError("output buffer is null".into()));
                    }
                    buffer = Some(output_buffer);
                    self.output_buffer_index = Some(usize_from_isize(output_index)?);
                    break;
                } else if output_index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED as isize {
                    continue;
                } else if output_index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED as isize {
                    let format = AMediaCodec_getOutputFormat(codec);
                    if format.is_null() {
                        return Err(AvifError::UnknownError("output format was null".into()));
                    }
                    self.format = Some(MediaFormat { format });
                    continue;
                } else if output_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER as isize {
                    continue;
                } else {
                    return Err(AvifError::UnknownError(format!(
                        "mediacodec dequeue_output_buffer failed: {output_index}"
                    )));
                }
            }
        }
        if buffer.is_none() {
            return Err(AvifError::UnknownError(
                "did not get buffer from mediacodec".into(),
            ));
        }
        if self.format.is_none() {
            return Err(AvifError::UnknownError("format is none".into()));
        }
        let buffer = buffer.unwrap();
        let format = self.format.unwrap_ref();
        image.width = format.width()? as u32;
        image.height = format.height()? as u32;
        image.yuv_range = format.color_range();
        let plane_info = format.get_plane_info()?;
        image.depth = plane_info.depth();
        image.yuv_format = plane_info.pixel_format();
        match category {
            Category::Alpha => {
                // TODO: make sure alpha plane matches previous alpha plane.
                image.row_bytes[3] = plane_info.row_stride[0];
                image.planes[3] = Some(Pixels::from_raw_pointer(
                    unsafe { buffer.offset(plane_info.offset[0]) },
                    image.depth as u32,
                    image.height,
                    image.row_bytes[3],
                )?);
            }
            _ => {
                image.chroma_sample_position = ChromaSamplePosition::Unknown;
                image.color_primaries = ColorPrimaries::Unspecified;
                image.transfer_characteristics = TransferCharacteristics::Unspecified;
                image.matrix_coefficients = MatrixCoefficients::Unspecified;

                for i in 0usize..3 {
                    if i == 2
                        && matches!(
                            image.yuv_format,
                            PixelFormat::AndroidP010
                                | PixelFormat::AndroidNv12
                                | PixelFormat::AndroidNv21
                        )
                    {
                        // V plane is not needed for these formats.
                        break;
                    }
                    image.row_bytes[i] = plane_info.row_stride[i];
                    let plane_height = if i == 0 { image.height } else { (image.height + 1) / 2 };
                    image.planes[i] = Some(Pixels::from_raw_pointer(
                        unsafe { buffer.offset(plane_info.offset[i]) },
                        image.depth as u32,
                        plane_height,
                        image.row_bytes[i],
                    )?);
                }
            }
        }
        Ok(())
    }
}

impl MediaCodec {
    fn hevc_whole_nal_units(&self, payload: &[u8]) -> AvifResult<Option<Vec<u8>>> {
        if !self.config.unwrap_ref().codec_config.is_heic() {
            return Ok(None);
        }
        // For HEVC, MediaCodec expects whole NAL units with each unit prefixed with a start code
        // of "\x00\x00\x00\x01".
        let nal_length_size = self.config.unwrap_ref().codec_config.nal_length_size() as usize;
        let mut offset = 0;
        let mut hevc_payload = Vec::new();
        while offset < payload.len() {
            let payload_slice = &payload[offset..];
            let mut stream = IStream::create(payload_slice);
            let nal_length = usize_from_u64(stream.read_uxx(nal_length_size as u8)?)?;
            let nal_unit_end = checked_add!(nal_length, nal_length_size)?;
            let nal_unit_range = nal_length_size..nal_unit_end;
            check_slice_range(payload_slice.len(), &nal_unit_range)?;
            // Start code.
            hevc_payload.extend_from_slice(&[0, 0, 0, 1]);
            // NAL Unit.
            hevc_payload.extend_from_slice(&payload_slice[nal_unit_range]);
            offset = checked_add!(offset, nal_unit_end)?;
        }
        Ok(Some(hevc_payload))
    }
}

impl Drop for MediaFormat {
    fn drop(&mut self) {
        unsafe { AMediaFormat_delete(self.format) };
    }
}

impl Drop for MediaCodec {
    fn drop(&mut self) {
        if self.codec.is_some() {
            if self.output_buffer_index.is_some() {
                unsafe {
                    AMediaCodec_releaseOutputBuffer(
                        self.codec.unwrap(),
                        self.output_buffer_index.unwrap(),
                        false,
                    );
                }
                self.output_buffer_index = None;
            }
            unsafe {
                AMediaCodec_stop(self.codec.unwrap());
                AMediaCodec_delete(self.codec.unwrap());
            }
            self.codec = None;
        }
        self.format = None;
    }
}
