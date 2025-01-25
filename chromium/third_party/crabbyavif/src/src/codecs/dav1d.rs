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

use dav1d_sys::bindings::*;

use std::mem::MaybeUninit;

#[derive(Debug, Default)]
pub struct Dav1d {
    context: Option<*mut Dav1dContext>,
    picture: Option<Dav1dPicture>,
}

unsafe extern "C" fn avif_dav1d_free_callback(
    _buf: *const u8,
    _cookie: *mut ::std::os::raw::c_void,
) {
    // Do nothing. The buffers are owned by the decoder.
}

// See https://code.videolan.org/videolan/dav1d/-/blob/9849ede1304da1443cfb4a86f197765081034205/include/dav1d/common.h#L55-59
const DAV1D_EAGAIN: i32 = if libc::EPERM > 0 { -libc::EAGAIN } else { libc::EAGAIN };

// The type of the fields from dav1d_sys::bindings::* are dependent on the
// compiler that is used to generate the bindings, version of dav1d, etc.
// So allow clippy to ignore unnecessary cast warnings.
#[allow(clippy::unnecessary_cast)]
impl Decoder for Dav1d {
    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()> {
        if self.context.is_some() {
            return Ok(());
        }
        let mut settings_uninit: MaybeUninit<Dav1dSettings> = MaybeUninit::uninit();
        unsafe { dav1d_default_settings(settings_uninit.as_mut_ptr()) };
        let mut settings = unsafe { settings_uninit.assume_init() };
        settings.max_frame_delay = 1;
        settings.n_threads = i32::try_from(config.max_threads).unwrap_or(1);
        settings.operating_point = config.operating_point as i32;
        settings.all_layers = if config.all_layers { 1 } else { 0 };

        let mut dec = MaybeUninit::uninit();
        let ret = unsafe { dav1d_open(dec.as_mut_ptr(), (&settings) as *const _) };
        if ret != 0 {
            return Err(AvifError::UnknownError(format!(
                "dav1d_open returned {ret}"
            )));
        }
        self.context = Some(unsafe { dec.assume_init() });

        Ok(())
    }

    fn get_next_image(
        &mut self,
        av1_payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()> {
        if self.context.is_none() {
            self.initialize(&DecoderConfig::default())?;
        }
        unsafe {
            let mut data: Dav1dData = std::mem::zeroed();
            let res = dav1d_data_wrap(
                (&mut data) as *mut _,
                av1_payload.as_ptr(),
                av1_payload.len(),
                Some(avif_dav1d_free_callback),
                /*cookie=*/ std::ptr::null_mut(),
            );
            if res != 0 {
                return Err(AvifError::UnknownError(format!(
                    "dav1d_data_wrap returned {res}"
                )));
            }
            let mut next_frame: Dav1dPicture = std::mem::zeroed();
            let got_picture;
            loop {
                if !data.data.is_null() {
                    let res = dav1d_send_data(self.context.unwrap(), (&mut data) as *mut _);
                    if res < 0 && res != DAV1D_EAGAIN {
                        dav1d_data_unref((&mut data) as *mut _);
                        return Err(AvifError::UnknownError(format!(
                            "dav1d_send_data returned {res}"
                        )));
                    }
                }

                let res = dav1d_get_picture(self.context.unwrap(), (&mut next_frame) as *mut _);
                if res == DAV1D_EAGAIN {
                    // send more data.
                    if !data.data.is_null() {
                        continue;
                    }
                    return Err(AvifError::UnknownError("".into()));
                } else if res < 0 {
                    if !data.data.is_null() {
                        dav1d_data_unref((&mut data) as *mut _);
                    }
                    return Err(AvifError::UnknownError(format!(
                        "dav1d_send_picture returned {res}"
                    )));
                } else {
                    // Got a picture.
                    let frame_spatial_id = (*next_frame.frame_hdr).spatial_id as u8;
                    if spatial_id != 0xFF && spatial_id != frame_spatial_id {
                        // layer selection: skip this unwanted layer.
                        dav1d_picture_unref((&mut next_frame) as *mut _);
                    } else {
                        got_picture = true;
                        break;
                    }
                }
            }
            if !data.data.is_null() {
                dav1d_data_unref((&mut data) as *mut _);
            }

            // Drain all buffered frames in the decoder.
            //
            // The sample should have only one frame of the desired layer. If there are more frames
            // after that frame, we need to discard them so that they won't be mistakenly output
            // when the decoder is used to decode another sample.
            let mut buffered_frame: Dav1dPicture = std::mem::zeroed();
            loop {
                let res = dav1d_get_picture(self.context.unwrap(), (&mut buffered_frame) as *mut _);
                if res < 0 {
                    if res != DAV1D_EAGAIN {
                        if got_picture {
                            dav1d_picture_unref((&mut next_frame) as *mut _);
                        }
                        return Err(AvifError::UnknownError(format!(
                            "error draining buffered frames {res}"
                        )));
                    }
                } else {
                    dav1d_picture_unref((&mut buffered_frame) as *mut _);
                }
                if res != 0 {
                    break;
                }
            }

            if got_picture {
                // unref previous frame.
                if self.picture.is_some() {
                    let mut previous_picture = self.picture.unwrap();
                    dav1d_picture_unref((&mut previous_picture) as *mut _);
                }
                self.picture = Some(next_frame);
            } else if category == Category::Alpha && self.picture.is_some() {
                // Special case for alpha, re-use last frame.
            } else {
                return Err(AvifError::UnknownError("".into()));
            }
        }

        let dav1d_picture = self.picture.unwrap_ref();
        match category {
            Category::Alpha => {
                if image.width > 0
                    && image.height > 0
                    && (image.width != (dav1d_picture.p.w as u32)
                        || image.height != (dav1d_picture.p.h as u32)
                        || image.depth != (dav1d_picture.p.bpc as u8))
                {
                    // Alpha plane does not match the previous alpha plane.
                    return Err(AvifError::UnknownError("".into()));
                }
                image.width = dav1d_picture.p.w as u32;
                image.height = dav1d_picture.p.h as u32;
                image.depth = dav1d_picture.p.bpc as u8;
                image.row_bytes[3] = dav1d_picture.stride[0] as u32;
                image.planes[3] = Some(Pixels::from_raw_pointer(
                    dav1d_picture.data[0] as *mut u8,
                    image.depth as u32,
                    image.height,
                    image.row_bytes[3],
                )?);
                image.image_owns_planes[3] = false;
                let seq_hdr = unsafe { &(*dav1d_picture.seq_hdr) };
                image.yuv_range =
                    if seq_hdr.color_range == 0 { YuvRange::Limited } else { YuvRange::Full };
            }
            _ => {
                image.width = dav1d_picture.p.w as u32;
                image.height = dav1d_picture.p.h as u32;
                image.depth = dav1d_picture.p.bpc as u8;

                image.yuv_format = match dav1d_picture.p.layout {
                    0 => PixelFormat::Yuv400,
                    1 => PixelFormat::Yuv420,
                    2 => PixelFormat::Yuv422,
                    3 => PixelFormat::Yuv444,
                    _ => return Err(AvifError::UnknownError("".into())), // not reached.
                };
                let seq_hdr = unsafe { &(*dav1d_picture.seq_hdr) };
                image.yuv_range =
                    if seq_hdr.color_range == 0 { YuvRange::Limited } else { YuvRange::Full };
                image.chroma_sample_position = (seq_hdr.chr as u32).into();

                image.color_primaries = (seq_hdr.pri as u16).into();
                image.transfer_characteristics = (seq_hdr.trc as u16).into();
                image.matrix_coefficients = (seq_hdr.mtrx as u16).into();

                for plane in 0usize..image.yuv_format.plane_count() {
                    let stride_index = if plane == 0 { 0 } else { 1 };
                    image.row_bytes[plane] = dav1d_picture.stride[stride_index] as u32;
                    image.planes[plane] = Some(Pixels::from_raw_pointer(
                        dav1d_picture.data[plane] as *mut u8,
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
        Ok(())
    }
}

impl Drop for Dav1d {
    fn drop(&mut self) {
        if self.picture.is_some() {
            unsafe { dav1d_picture_unref(self.picture.unwrap_mut() as *mut _) };
        }
        if self.context.is_some() {
            unsafe { dav1d_close(&mut self.context.unwrap()) };
        }
    }
}
