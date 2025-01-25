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
use crate::*;

use libgav1_sys::bindings::*;

use std::mem::MaybeUninit;

#[derive(Debug, Default)]
pub struct Libgav1 {
    decoder: Option<*mut Libgav1Decoder>,
    image: Option<Libgav1DecoderBuffer>,
}

#[allow(non_upper_case_globals)]
// The type of the fields from from dav1d_sys::bindings::* are dependent on the compiler that
// is used to generate the bindings, version of dav1d, etc. So allow clippy to ignore
// unnecessary cast warnings.
#[allow(clippy::unnecessary_cast)]
impl Decoder for Libgav1 {
    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()> {
        if self.decoder.is_some() {
            return Ok(()); // Already initialized.
        }
        let mut settings_uninit: MaybeUninit<Libgav1DecoderSettings> = MaybeUninit::uninit();
        unsafe {
            Libgav1DecoderSettingsInitDefault(settings_uninit.as_mut_ptr());
        }
        let mut settings = unsafe { settings_uninit.assume_init() };
        settings.threads = i32::try_from(config.max_threads).unwrap_or(1);
        settings.operating_point = config.operating_point as i32;
        settings.output_all_layers = if config.all_layers { 1 } else { 0 };
        unsafe {
            let mut dec = MaybeUninit::uninit();
            let ret = Libgav1DecoderCreate(&settings, dec.as_mut_ptr());
            if ret != Libgav1StatusCode_kLibgav1StatusOk {
                return Err(AvifError::UnknownError(format!(
                    "Libgav1DecoderCreate returned {ret}"
                )));
            }
            self.decoder = Some(dec.assume_init());
        }
        Ok(())
    }

    fn get_next_image(
        &mut self,
        av1_payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()> {
        if self.decoder.is_none() {
            self.initialize(&DecoderConfig::default())?;
        }
        unsafe {
            let ret = Libgav1DecoderEnqueueFrame(
                self.decoder.unwrap(),
                av1_payload.as_ptr(),
                av1_payload.len(),
                0,
                std::ptr::null_mut(),
            );
            if ret != Libgav1StatusCode_kLibgav1StatusOk {
                return Err(AvifError::UnknownError(format!(
                    "Libgav1DecoderEnqueueFrame returned {ret}"
                )));
            }
            self.image = None;
            let mut next_frame: *const Libgav1DecoderBuffer = std::ptr::null_mut();
            loop {
                let ret = Libgav1DecoderDequeueFrame(self.decoder.unwrap(), &mut next_frame);
                if ret != Libgav1StatusCode_kLibgav1StatusOk {
                    return Err(AvifError::UnknownError(format!(
                        "Libgav1DecoderDequeueFrame returned {ret}"
                    )));
                }
                if !next_frame.is_null()
                    && spatial_id != 0xFF
                    && (*next_frame).spatial_id as u8 != spatial_id
                {
                    next_frame = std::ptr::null_mut();
                } else {
                    break;
                }
            }
            // Got an image.
            if next_frame.is_null() {
                if category == Category::Alpha {
                    // Special case for alpha, re-use last frame.
                } else {
                    return Err(AvifError::UnknownError("".into()));
                }
            } else {
                self.image = Some(*next_frame);
            }

            let gav1_image = &self.image.unwrap();
            match category {
                Category::Alpha => {
                    if image.width > 0
                        && image.height > 0
                        && (image.width != (gav1_image.displayed_width[0] as u32)
                            || image.height != (gav1_image.displayed_height[0] as u32)
                            || image.depth != (gav1_image.bitdepth as u8))
                    {
                        // Alpha plane does not match the previous alpha plane.
                        return Err(AvifError::UnknownError("".into()));
                    }
                    image.width = gav1_image.displayed_width[0] as u32;
                    image.height = gav1_image.displayed_height[0] as u32;
                    image.depth = gav1_image.bitdepth as u8;
                    image.row_bytes[3] = gav1_image.stride[0] as u32;
                    image.planes[3] = Some(Pixels::from_raw_pointer(
                        gav1_image.plane[0],
                        image.depth as u32,
                        image.height,
                        image.row_bytes[3],
                    )?);
                    image.image_owns_planes[3] = false;
                    image.yuv_range =
                        if gav1_image.color_range == Libgav1ColorRange_kLibgav1ColorRangeStudio {
                            YuvRange::Limited
                        } else {
                            YuvRange::Full
                        };
                }
                _ => {
                    image.width = gav1_image.displayed_width[0] as u32;
                    image.height = gav1_image.displayed_height[0] as u32;
                    image.depth = gav1_image.bitdepth as u8;

                    image.yuv_format = match gav1_image.image_format {
                        Libgav1ImageFormat_kLibgav1ImageFormatMonochrome400 => PixelFormat::Yuv400,
                        Libgav1ImageFormat_kLibgav1ImageFormatYuv420 => PixelFormat::Yuv420,
                        Libgav1ImageFormat_kLibgav1ImageFormatYuv422 => PixelFormat::Yuv422,
                        Libgav1ImageFormat_kLibgav1ImageFormatYuv444 => PixelFormat::Yuv444,
                        _ => PixelFormat::Yuv420, // not reached.
                    };
                    image.yuv_range =
                        if gav1_image.color_range == Libgav1ColorRange_kLibgav1ColorRangeStudio {
                            YuvRange::Limited
                        } else {
                            YuvRange::Full
                        };
                    image.chroma_sample_position =
                        (gav1_image.chroma_sample_position as u32).into();

                    image.color_primaries = (gav1_image.color_primary as u16).into();
                    image.transfer_characteristics =
                        (gav1_image.transfer_characteristics as u16).into();
                    image.matrix_coefficients = (gav1_image.matrix_coefficients as u16).into();

                    for plane in 0usize..image.yuv_format.plane_count() {
                        image.row_bytes[plane] = gav1_image.stride[plane] as u32;
                        image.planes[plane] = Some(Pixels::from_raw_pointer(
                            gav1_image.plane[plane],
                            image.depth as u32,
                            image.height,
                            image.row_bytes[plane],
                        )?);
                        image.image_owns_planes[plane] = false;
                    }
                    if image.yuv_format == PixelFormat::Yuv400 {
                        // Clear left over chroma planes from previous frames.
                        image.clear_chroma_planes();
                    }
                }
            }
        }
        Ok(())
    }
}

impl Drop for Libgav1 {
    fn drop(&mut self) {
        if self.decoder.is_some() {
            unsafe { Libgav1DecoderDestroy(self.decoder.unwrap()) };
        }
    }
}
