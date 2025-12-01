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

// The type of the fields from dav1d_sys::bindings::* are dependent on the
// compiler that is used to generate the bindings, version of dav1d, etc.
// So allow clippy to ignore unnecessary cast warnings.
#![allow(clippy::unnecessary_cast)]

use crate::codecs::Decoder;
use crate::codecs::DecoderConfig;
use crate::decoder::CodecChoice;
use crate::decoder::GridImageHelper;
use crate::image::Image;
use crate::image::YuvRange;
use crate::utils::pixels::*;
use crate::*;

use dav1d_sys::bindings::*;

use std::mem::MaybeUninit;

#[derive(Default)]
pub struct Dav1d {
    context: Option<*mut Dav1dContext>,
    picture: Option<Dav1dPictureWrapper>,
    config: Option<DecoderConfig>,
}

unsafe extern "C" fn avif_dav1d_free_callback(
    _buf: *const u8,
    _cookie: *mut ::std::os::raw::c_void,
) {
    // Do nothing. The buffers are owned by the decoder.
}

// See https://code.videolan.org/videolan/dav1d/-/blob/9849ede1304da1443cfb4a86f197765081034205/include/dav1d/common.h#L55-59
const DAV1D_EAGAIN: i32 = if libc::EPERM > 0 { -libc::EAGAIN } else { libc::EAGAIN };

struct Dav1dPictureWrapper {
    picture: Dav1dPicture,
}

impl Default for Dav1dPictureWrapper {
    fn default() -> Self {
        Self {
            picture: unsafe { std::mem::zeroed() },
        }
    }
}

impl Dav1dPictureWrapper {
    fn mut_ptr(&mut self) -> *mut Dav1dPicture {
        (&mut self.picture) as *mut _
    }

    fn get(&self) -> &Dav1dPicture {
        &self.picture
    }

    fn use_layer(&self, spatial_id: u8) -> bool {
        spatial_id == 0xFF || spatial_id == unsafe { (*self.get().frame_hdr).spatial_id as u8 }
    }
}

impl Drop for Dav1dPictureWrapper {
    fn drop(&mut self) {
        unsafe {
            dav1d_picture_unref(self.mut_ptr());
        }
    }
}

struct Dav1dDataWrapper {
    data: Dav1dData,
}

impl Default for Dav1dDataWrapper {
    fn default() -> Self {
        Self {
            data: unsafe { std::mem::zeroed() },
        }
    }
}

impl Dav1dDataWrapper {
    fn mut_ptr(&mut self) -> *mut Dav1dData {
        (&mut self.data) as *mut _
    }

    fn has_data(&self) -> bool {
        self.data.sz > 0 && !self.data.data.is_null()
    }

    fn wrap(&mut self, payload: &[u8]) -> AvifResult<()> {
        match unsafe {
            dav1d_data_wrap(
                self.mut_ptr(),
                payload.as_ptr(),
                payload.len(),
                Some(avif_dav1d_free_callback),
                /*cookie=*/ std::ptr::null_mut(),
            )
        } {
            0 => Ok(()),
            res => Err(AvifError::UnknownError(format!(
                "dav1d_data_wrap returned {res}"
            ))),
        }
    }
}

impl Drop for Dav1dDataWrapper {
    fn drop(&mut self) {
        if self.has_data() {
            unsafe {
                dav1d_data_unref(self.mut_ptr());
            }
        }
    }
}

impl Dav1d {
    fn initialize_impl(&mut self, low_latency: bool) -> AvifResult<()> {
        if self.context.is_some() {
            return Ok(());
        }
        let config = self.config.unwrap_ref();
        let mut settings_uninit: MaybeUninit<Dav1dSettings> = MaybeUninit::uninit();
        unsafe { dav1d_default_settings(settings_uninit.as_mut_ptr()) };
        let mut settings = unsafe { settings_uninit.assume_init() };
        if low_latency {
            settings.max_frame_delay = 1;
        }
        settings.n_threads = i32::try_from(config.max_threads).unwrap_or(1);
        settings.operating_point = config.operating_point as i32;
        settings.all_layers = if config.all_layers { 1 } else { 0 };
        let frame_size_limit = match config.image_size_limit {
            Some(value) => value.get(),
            None => 0,
        };
        // Set a maximum frame size limit to avoid OOM'ing fuzzers. In 32-bit builds, if
        // frame_size_limit > 8192 * 8192, dav1d reduces frame_size_limit to 8192 * 8192 and logs
        // a message, so we set frame_size_limit to at most 8192 * 8192 to avoid the dav1d_log
        // message.
        settings.frame_size_limit = if cfg!(target_pointer_width = "32") {
            std::cmp::min(frame_size_limit, 8192 * 8192)
        } else {
            frame_size_limit
        };

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

    fn drop_impl(&mut self) {
        self.picture = None;
        if self.context.is_some() {
            unsafe { dav1d_close(&mut self.context.unwrap()) };
        }
        self.context = None;
    }

    fn picture_to_image(
        &self,
        dav1d_picture: &Dav1dPicture,
        image: &mut Image,
        category: Category,
    ) -> AvifResult<()> {
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

    fn get_next_image_grid_impl(
        &mut self,
        payloads: &[Vec<u8>],
        spatial_id: u8,
        grid_image_helper: &mut GridImageHelper,
    ) -> AvifResult<()> {
        if self.context.is_none() {
            self.initialize_impl(false)?;
        }
        let mut res;
        let context = self.context.unwrap();
        let mut payloads_iter = payloads.iter().peekable();
        unsafe {
            let mut data = Dav1dDataWrapper::default();
            let max_retries = 500;
            let mut retries = 0;
            while !grid_image_helper.is_grid_complete()? {
                if !data.has_data() && payloads_iter.peek().is_some() {
                    data.wrap(payloads_iter.next().unwrap())?;
                }
                if data.has_data() {
                    res = dav1d_send_data(context, data.mut_ptr());
                    if res != 0 && res != DAV1D_EAGAIN {
                        return Err(AvifError::UnknownError(format!(
                            "dav1d_send_data returned {res}"
                        )));
                    }
                }
                let mut picture = Dav1dPictureWrapper::default();
                res = dav1d_get_picture(context, picture.mut_ptr());
                if res != 0 && res != DAV1D_EAGAIN {
                    return Err(AvifError::UnknownError(format!(
                        "dav1d_get_picture returned {res}"
                    )));
                } else if res == 0 && picture.use_layer(spatial_id) {
                    let mut cell_image = Image::default();
                    self.picture_to_image(
                        picture.get(),
                        &mut cell_image,
                        grid_image_helper.category,
                    )?;
                    grid_image_helper.copy_from_cell_image(&mut cell_image)?;
                    retries = 0;
                } else {
                    retries += 1;
                    if retries > max_retries {
                        return Err(AvifError::UnknownError(format!(
                            "dav1d_get_picture never returned a frame after {max_retries} calls"
                        )));
                    }
                }
            }
            self.flush()?;
        }
        Ok(())
    }

    fn flush(&mut self) -> AvifResult<()> {
        unsafe {
            loop {
                let mut picture = Dav1dPictureWrapper::default();
                let res = dav1d_get_picture(self.context.unwrap(), picture.mut_ptr());
                if res < 0 && res != DAV1D_EAGAIN {
                    return Err(AvifError::UnknownError(format!(
                        "error draining buffered frames {res}"
                    )));
                }
                if res != 0 {
                    break;
                }
            }
        }
        Ok(())
    }
}

impl Decoder for Dav1d {
    fn codec(&self) -> CodecChoice {
        CodecChoice::Dav1d
    }

    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()> {
        self.config = Some(config.clone());
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
            self.initialize_impl(true)?;
        }
        unsafe {
            let mut data = Dav1dDataWrapper::default();
            data.wrap(av1_payload)?;
            let next_picture: Option<Dav1dPictureWrapper>;
            loop {
                if data.has_data() {
                    let res = dav1d_send_data(self.context.unwrap(), data.mut_ptr());
                    if res < 0 && res != DAV1D_EAGAIN {
                        return Err(AvifError::UnknownError(format!(
                            "dav1d_send_data returned {res}"
                        )));
                    }
                }

                let mut picture = Dav1dPictureWrapper::default();
                let res = dav1d_get_picture(self.context.unwrap(), picture.mut_ptr());
                if res == DAV1D_EAGAIN {
                    if data.has_data() {
                        continue;
                    }
                    return Err(AvifError::UnknownError("".into()));
                } else if res < 0 {
                    return Err(AvifError::UnknownError(format!(
                        "dav1d_send_picture returned {res}"
                    )));
                } else if picture.use_layer(spatial_id) {
                    // Got a picture.
                    next_picture = Some(picture);
                    break;
                }
            }
            self.flush()?;
            if next_picture.is_some() {
                self.picture = Some(next_picture.unwrap());
            } else if category == Category::Alpha && self.picture.is_some() {
                // Special case for alpha, re-use last frame.
            } else {
                return Err(AvifError::UnknownError("".into()));
            }
        }
        self.picture_to_image(self.picture.unwrap_ref().get(), image, category)?;
        Ok(())
    }

    fn get_next_image_grid(
        &mut self,
        payloads: &[Vec<u8>],
        spatial_id: u8,
        grid_image_helper: &mut GridImageHelper,
    ) -> AvifResult<()> {
        let res = self.get_next_image_grid_impl(payloads, spatial_id, grid_image_helper);
        if res.is_err() {
            self.drop_impl();
        }
        res
    }
}

impl Drop for Dav1d {
    fn drop(&mut self) {
        self.drop_impl();
    }
}
