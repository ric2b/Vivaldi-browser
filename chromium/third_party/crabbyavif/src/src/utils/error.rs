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

use crate::*;

// To be used instead of direct AvifError enum variants in order to debug
// unexpected Err propagations as early as possible in the call stack.
#[allow(dead_code)]
impl AvifError {
    fn on_error() {
        // Use std::intrinsics::breakpoint() or manually add a breakpoint here.
        // Alternatively, uncomment the following to print the stack trace.
        // println!("{}", std::backtrace::Backtrace::force_capture());
    }

    pub(crate) fn invalid_ftyp<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::InvalidFtyp)
    }
    pub(crate) fn no_content<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::NoContent)
    }
    pub(crate) fn no_yuv_format_selected<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::NoYuvFormatSelected)
    }
    pub(crate) fn reformat_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::ReformatFailed)
    }
    pub(crate) fn unsupported_depth<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::UnsupportedDepth)
    }
    pub(crate) fn encode_color_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::EncodeColorFailed)
    }
    pub(crate) fn encode_alpha_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::EncodeAlphaFailed)
    }

    pub(crate) fn missing_image_item<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::MissingImageItem)
    }
    pub(crate) fn decode_color_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::DecodeColorFailed)
    }
    pub(crate) fn decode_alpha_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::DecodeAlphaFailed)
    }
    pub(crate) fn color_alpha_size_mismatch<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::ColorAlphaSizeMismatch)
    }
    pub(crate) fn ispe_size_mismatch<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::IspeSizeMismatch)
    }
    pub(crate) fn no_codec_available<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::NoCodecAvailable)
    }
    pub(crate) fn no_images_remaining<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::NoImagesRemaining)
    }
    pub(crate) fn invalid_exif_payload<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::InvalidExifPayload)
    }

    pub(crate) fn invalid_codec_specific_option<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::InvalidCodecSpecificOption)
    }
    pub(crate) fn truncated_data<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::TruncatedData)
    }
    pub(crate) fn io_not_set<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::IoNotSet)
    }
    pub(crate) fn io_error<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::IoError)
    }
    pub(crate) fn waiting_on_io<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::WaitingOnIo)
    }
    pub(crate) fn invalid_argument<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::InvalidArgument)
    }
    pub(crate) fn not_implemented<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::NotImplemented)
    }
    pub(crate) fn out_of_memory<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::OutOfMemory)
    }
    pub(crate) fn cannot_change_setting<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::CannotChangeSetting)
    }
    pub(crate) fn incompatible_image<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::IncompatibleImage)
    }
    pub(crate) fn encode_gain_map_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::EncodeGainMapFailed)
    }
    pub(crate) fn decode_gain_map_failed<T>() -> Result<T, AvifError> {
        AvifError::on_error();
        Err(AvifError::DecodeGainMapFailed)
    }

    pub(crate) fn unknown_error<T, O>(object: O) -> Result<T, AvifError>
    where
        O: std::fmt::Display,
    {
        AvifError::on_error();
        Err(AvifError::UnknownError(object.to_string()))
    }
    pub(crate) fn bmff_parse_failed<T, O>(object: O) -> Result<T, AvifError>
    where
        O: std::fmt::Display,
    {
        AvifError::on_error();
        Err(AvifError::BmffParseFailed(object.to_string()))
    }
    pub(crate) fn invalid_image_grid<T, O>(object: O) -> Result<T, AvifError>
    where
        O: std::fmt::Display,
    {
        AvifError::on_error();
        Err(AvifError::InvalidImageGrid(object.to_string()))
    }
    pub(crate) fn invalid_tone_mapped_image<T, O>(object: O) -> Result<T, AvifError>
    where
        O: std::fmt::Display,
    {
        AvifError::on_error();
        Err(AvifError::InvalidToneMappedImage(object.to_string()))
    }

    pub(crate) fn map_unknown_error<E>(error: E) -> AvifError
    where
        E: std::fmt::Display,
    {
        AvifError::on_error();
        AvifError::UnknownError(error.to_string())
    }
    pub(crate) fn map_io_error<E>(_: E) -> AvifError {
        AvifError::on_error();
        AvifError::IoError
    }
    pub(crate) fn map_out_of_memory<E>(_: E) -> AvifError {
        AvifError::on_error();
        AvifError::OutOfMemory
    }
    pub(crate) fn map_invalid_exif_payload<E>(_: E) -> AvifError {
        AvifError::on_error();
        AvifError::InvalidExifPayload
    }
}
