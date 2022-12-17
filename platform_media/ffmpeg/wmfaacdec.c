// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "platform_media/ffmpeg/wmfaacdec.h"

#include <Windows.h>

#include <mfapi.h>

#include <VersionHelpers.h>
#include <mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#include "config.h"
#include "third_party/ffmpeg/libavcodec/aac_ac3_parser.h"
#include "third_party/ffmpeg/libavcodec/ac3_parser_internal.h"
#include "third_party/ffmpeg/libavcodec/adts_header.h"
#include "third_party/ffmpeg/libavcodec/avcodec.h"
#include "third_party/ffmpeg/libavcodec/bytestream.h"
#include "third_party/ffmpeg/libavcodec/codec_internal.h"
#include "third_party/ffmpeg/libavcodec/decode.h"
#include "third_party/ffmpeg/libavcodec/get_bits.h"
#include "third_party/ffmpeg/libavcodec/internal.h"
#include "third_party/ffmpeg/libavcodec/mpegaudiodecheader.h"
#include "third_party/ffmpeg/libavutil/avassert.h"
#include "third_party/ffmpeg/libavutil/log.h"
#include "third_party/ffmpeg/libavutil/opt.h"

typedef struct WMFDecodeContext {
  AVClass* av_class;

  IMFTransform* decoder;
  IMFSample* input_sample;
  IMFSample* output_sample;
  uint8_t* extradata;
  int extradata_size;
  int output_samples_per_second;
  int output_channel_count;
  int output_bits_per_sample;
  uint32_t input_stream_alignment;
  bool doing_output;
  bool adts_mode;
  bool after_eof;
} WMFDecodeContext;

typedef HRESULT(STDAPICALLTYPE* ffwmf_DllGetClassObject_ptr)(REFCLSID rclsid,
                                                             REFIID riid,
                                                             LPVOID* ppv);

static ffwmf_DllGetClassObject_ptr ffwmf_get_class_object = NULL;

static struct FFWMF_LogInfo ffwmf_log_info = {
    .max_verbosity = 0,  // no logging by default
    .file_path = __FILE__,
};

#define FFWMF_DEFAULT_STREAM_ID ((DWORD)0)

// IMFSample's timestamp is expressed in hundreds of nanoseconds.
#define FFWMF_SAMPLE_TIME_UNITS_PER_SECOND ((double)1.0e7)

// Do not bloat an official build with function names
#if defined(OFFICIAL_BUILD)
#define FFWMF_FUNCTION_NAME NULL
#else
#define FFWMF_FUNCTION_NAME __FUNCTION__
#endif

#define FFWMF_DO_LOG(avctx, verbosity_level, ...)                 \
  do {                                                            \
    if ((verbosity_level) <= ffwmf_log_info.max_verbosity) {      \
      ffwmf_print_log(avctx, verbosity_level, __FILE__, __LINE__, \
                      FFWMF_FUNCTION_NAME, __VA_ARGS__);          \
    }                                                             \
  } while (0)

#define FFWMF_LOG_ERROR(avctx, ...) FFWMF_DO_LOG(avctx, 1, __VA_ARGS__)

// Do not bloat an official build with debug logs
#if defined(OFFICIAL_BUILD)
#define FFWMF_DEBUG_LOG(avctx, verbosity_level, ...) ((void)0)
#else
#define FFWMF_DEBUG_LOG(avctx, verbosity_level, ...) \
  FFWMF_DO_LOG(avctx, verbosity_level, __VA_ARGS__)
#endif

struct FFWMF_LogInfo* ffwmf_get_log_info() {
  return &ffwmf_log_info;
}

static void ffwmf_print_log(AVCodecContext* avctx,
                            int verbosity_level,
                            const char* file_name,
                            int line_number,
                            const char* function_name,
                            const char* format,
                            ...) {
  char message[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(message, sizeof message, format, ap);
  va_end(ap);
  ffwmf_log_info.log_function(verbosity_level, file_name, line_number,
                              function_name, message);
}

#define FFWMF_CHECK_STATUS() \
  do {                       \
    if (status < 0)          \
      goto cleanup;          \
  } while (0)

#define FFWMF_RETURN_STATUS() \
  do {                        \
    return status;            \
  } while (0)

#define FFWMF_LOG_HRESULT(hr, details) \
  FFWMF_LOG_ERROR(avctx, "FAILED %s, hr=0x%lX", details, hr)

#define FFWMF_CHECK_HRESULT(hr, details) \
  do {                                   \
    av_assert1(status >= 0);             \
    if (FAILED(hr)) {                    \
      FFWMF_LOG_HRESULT(hr, details);    \
      status = AVERROR_UNKNOWN;          \
      goto cleanup;                      \
    }                                    \
  } while (0)

HMODULE ffwmf_aac_dll = NULL;

// Permanently load WMF library.
static HMODULE ffwmf_load_audio_library(int internal) {
  if (!ffwmf_aac_dll) {
    // The name of the library changed with Windows-8.
    const wchar_t* dll_name =
        IsWindows8OrGreater() ? L"msauddecmft.dll" : L"msmpeg2adec.dll";

    // Query if the library was already loaded. This is useful in case the
    // application use a sandbox with a library preload when LoadLibrary() fails
    // but gettings its handle works.
    if (!GetModuleHandleEx(0, dll_name, &ffwmf_aac_dll)) {
      ffwmf_aac_dll = LoadLibrary(dll_name);
    }
  }

  return ffwmf_aac_dll;
}

static int ffwmf_copy_extra_data(AVCodecContext* avctx,
                                 const void* extradata,
                                 int extradata_size) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  if (extradata_size < 0) {
    // This may never happen according to FFmpeg sources, but lets be safe.
    extradata_size = 0;
  }

  if (wmf->extradata) {
    av_free(wmf->extradata);
    wmf->extradata = NULL;
  }
  wmf->extradata_size = extradata_size;
  if (extradata_size) {
    wmf->extradata = av_malloc(extradata_size);
    if (!wmf->extradata) {
      wmf->extradata_size = 0;
      status = AVERROR(ENOMEM);
    } else {
      memcpy(wmf->extradata, extradata, extradata_size);
    }
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_create_transformer(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IClassFactory* factory = NULL;

  av_assert1(!wmf->decoder);

  HMODULE library = ffwmf_load_audio_library(1);
  if (!library) {
    FFWMF_LOG_ERROR(avctx, "failed to load WMF audio DLL");
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

  // CoCreateInstance() may not be available due to sandbox restrictions, so
  // lookup the factory method directly.
  static const char get_class_object_name[] = "DllGetClassObject";
  if (!ffwmf_get_class_object) {
    ffwmf_get_class_object = (ffwmf_DllGetClassObject_ptr)GetProcAddress(
        library, get_class_object_name);
    if (!ffwmf_get_class_object) {
      FFWMF_LOG_ERROR(avctx, "failed to retrive %s", get_class_object_name);
      status = AVERROR_UNKNOWN;
      goto cleanup;
    }
  }

  av_assert1(avctx->codec_id == AV_CODEC_ID_AAC);
  GUID guid = CLSID_CMSAACDecMFT;

  HRESULT hr =
      ffwmf_get_class_object(&guid, &IID_IClassFactory, (void**)&factory);
  FFWMF_CHECK_HRESULT(hr, get_class_object_name);

  hr = factory->lpVtbl->CreateInstance(factory, NULL, &IID_IMFTransform,
                                       (void**)&wmf->decoder);
  FFWMF_CHECK_HRESULT(hr, "IClassFactory::CreateInstance()");

cleanup:
  if (factory) {
    factory->lpVtbl->Release(factory);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_set_input_media_type(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaType* media_type = NULL;
  HEAACWAVEFORMAT* aac_wave_format = NULL;
  HRESULT hr;

  // See Input Types in
  // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-decoder.
  // Another useful source is
  // https://searchfox.org/mozilla-central/source/dom/media/platforms/wmf/WMFAudioMFTManager.cpp
  hr = MFCreateMediaType(&media_type);
  FFWMF_CHECK_HRESULT(hr, "MFCreateMediaType()");

  hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE,
                                   &MFMediaType_Audio);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetGUID(MF_MT_MAJOR_TYPE)");

  hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE,
                                   &MFAudioFormat_AAC);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetGUID(MF_MT_SUBTYPE)");

  hr = media_type->lpVtbl->SetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS,
                                     avctx->channels);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetUINT32(MF_MT_AUDIO_NUM_CHANNELS)");

  hr = media_type->lpVtbl->SetUINT32(
      media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, avctx->sample_rate);
  FFWMF_CHECK_HRESULT(
      hr, "IMFMediaType::SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND)");

  if (wmf->adts_mode) {
    hr =
        media_type->lpVtbl->SetUINT32(media_type, &MF_MT_AAC_PAYLOAD_TYPE, 0x1);
    FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetUINT32(MF_MT_AAC_PAYLOAD_TYPE)");
  }
  size_t format_size = offsetof(HEAACWAVEFORMAT, pbAudioSpecificConfig);
  if (!wmf->adts_mode) {
    format_size += wmf->extradata_size;
  }
  aac_wave_format = av_malloc(format_size);
  if (!aac_wave_format) {
    status = AVERROR(ENOMEM);
    goto cleanup;
  }
  memset(aac_wave_format, 0, format_size);
  if (wmf->adts_mode) {
    aac_wave_format->wfInfo.wPayloadType = 1;
  } else {
    memcpy(aac_wave_format->pbAudioSpecificConfig, wmf->extradata,
           wmf->extradata_size);
  }
  // The blob must be set to the portion of HEAACWAVEFORMAT starting from
  // wfInfo.wPayloadType.
  hr = media_type->lpVtbl->SetBlob(
      media_type, &MF_MT_USER_DATA, &aac_wave_format->wfInfo.wPayloadType,
      format_size - offsetof(HEAACWAVEFORMAT, wfInfo.wPayloadType));
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetBlob(MF_MT_USER_DATA)");

  hr = wmf->decoder->lpVtbl->SetInputType(wmf->decoder, FFWMF_DEFAULT_STREAM_ID,
                                          media_type, /*fwFlags=*/0);
  if (FAILED(hr)) {
    const char* error_name;
    switch (hr) {
      case S_OK:
        error_name = "S_OK";
        break;
      case MF_E_INVALIDMEDIATYPE:
        error_name = "MF_E_INVALIDMEDIATYPE";
        break;
      case MF_E_INVALIDSTREAMNUMBER:
        error_name = "MF_E_INVALIDSTREAMNUMBER";
        break;
      case MF_E_INVALIDTYPE:
        error_name = "MF_E_INVALIDTYPE";
        break;
      case MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING:
        error_name = "MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING";
        break;
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        error_name = "MF_E_TRANSFORM_TYPE_NOT_SET";
        break;
      case MF_E_UNSUPPORTED_D3D_TYPE:
        error_name = "MF_E_UNSUPPORTED_D3D_TYPE";
        break;
      default:
        error_name = "other";
        break;
    }
    FFWMF_LOG_ERROR(avctx,
                    "failed IMFTransform::SetInputType(), error=%s(hr=0x%lX)",
                    error_name, hr);
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

cleanup:
  if (media_type) {
    media_type->lpVtbl->Release(media_type);
  }
  if (aac_wave_format) {
    av_free(aac_wave_format);
  }
  FFWMF_DEBUG_LOG(avctx, 3, "ADTS=%d return=%d", wmf->adts_mode, status);
  FFWMF_RETURN_STATUS();
}

// On entry |*sample_result| must be null. On a successfull return the caller is
// responsible for calling |Release()| on |*sample_result|. On errors
// |*sample_result| is kept at null.
static int ffwmf_create_sample(AVCodecContext* avctx,
                               DWORD buffer_size,
                               DWORD buffer_alignment_size,
                               IMFSample** sample_result) {
  IMFSample* sample = NULL;
  IMFMediaBuffer* buffer = NULL;
  int status = 0;

  av_assert1(!*sample_result);

  HRESULT hr = MFCreateSample(&sample);
  FFWMF_CHECK_HRESULT(hr, "MFCreateSample()");

  /*
   * Assume buffer_alignment_size is one of the sizes in
   * https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfcreatealignedmemorybuffer
   */
  DWORD alignment_param = buffer_alignment_size ? buffer_alignment_size - 1 : 0;
  hr = MFCreateAlignedMemoryBuffer(buffer_size, alignment_param, &buffer);
  FFWMF_CHECK_HRESULT(hr, "MFCreateAlignedMemoryBuffer()");

  hr = sample->lpVtbl->AddBuffer(sample, buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::AddBuffer()");

  // We successfully created and initialized the sample. Transfer the ownership
  // to the result to prevent releasing it in the cleanup.
  *sample_result = sample;
  sample = NULL;

cleanup:
  if (sample) {
    sample->lpVtbl->Release(sample);
  }
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_set_output_media_type(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaType* found_media_type = NULL;
  IMFMediaType* media_type = NULL;
  DWORD found_output_bits_per_sample = 0;

  // Find a PCM type with most bits.
  for (DWORD type_index = 0;; ++type_index) {
    if (media_type) {
      media_type->lpVtbl->Release(media_type);
      media_type = NULL;
    }
    HRESULT hr = wmf->decoder->lpVtbl->GetOutputAvailableType(
        wmf->decoder, FFWMF_DEFAULT_STREAM_ID, type_index, &media_type);
    if (hr == MF_E_NO_MORE_TYPES)
      break;
    FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetOutputAvailableType()");

    GUID subtype = {0};
    hr = media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &subtype);
    FFWMF_CHECK_HRESULT(hr, "IMFMediaType::GetGUID(MF_MT_SUBTYPE)");

    if (!IsEqualGUID(&subtype, &MFAudioFormat_PCM))
      continue;

    DWORD output_bits_per_sample = 0;
    hr = media_type->lpVtbl->GetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE,
                                       &output_bits_per_sample);
    FFWMF_CHECK_HRESULT(hr,
                        "IMFMediaType::GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE)");
    FFWMF_DEBUG_LOG(avctx, 4, "Found PCM output type bits_per_sample=%d",
                    (int)output_bits_per_sample);
    if (output_bits_per_sample != 16 && output_bits_per_sample != 32)
      continue;

    if (!found_media_type ||
        found_output_bits_per_sample < output_bits_per_sample) {
      if (found_media_type) {
        found_media_type->lpVtbl->Release(found_media_type);
      }
      found_media_type = media_type;
      media_type = NULL;
      found_output_bits_per_sample = output_bits_per_sample;
    }
  }
  if (!found_media_type) {
    FFWMF_LOG_ERROR(avctx, "failed to find PCM format among supported outputs");
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

  // Do not write directly into wmf->output_channel_count etc. Read first the
  // value into a temporary and then copy into wmf to ensure no change on
  // errors.
  DWORD output_samples_per_second = 0;
  DWORD output_channel_count = 0;

  HRESULT hr = found_media_type->lpVtbl->GetUINT32(
      found_media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND,
      &output_samples_per_second);
  FFWMF_CHECK_HRESULT(
      hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND)");

  hr = found_media_type->lpVtbl->GetUINT32(
      found_media_type, &MF_MT_AUDIO_NUM_CHANNELS, &output_channel_count);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_NUM_CHANNELS)");

  hr = wmf->decoder->lpVtbl->SetOutputType(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, found_media_type, /*flags=*/0);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::SetOutputType()");

  wmf->output_bits_per_sample = found_output_bits_per_sample;
  wmf->output_channel_count = output_channel_count;
  wmf->output_samples_per_second = output_samples_per_second;

  FFWMF_DEBUG_LOG(
      avctx, 3,
      "New output: bits_per_sample=%d samples_per_second=%d channels=%d",
      wmf->output_bits_per_sample, wmf->output_samples_per_second,
      wmf->output_channel_count);

  avctx->sample_fmt = (wmf->output_bits_per_sample == 32) ? AV_SAMPLE_FMT_S32
                                                          : AV_SAMPLE_FMT_S16;
  avctx->bits_per_raw_sample = wmf->output_bits_per_sample;
  avctx->sample_rate = wmf->output_samples_per_second;
  avctx->channels = wmf->output_channel_count;
  avctx->channel_layout = av_get_default_channel_layout(avctx->channels);

  MFT_OUTPUT_STREAM_INFO output_stream_info = {0};
  hr = wmf->decoder->lpVtbl->GetOutputStreamInfo(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, &output_stream_info);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetOutputStreamInfo()");

  // Create the output sample that is used for all output calls unless MFT
  // insists that it allocates the sample.
  if (wmf->output_sample) {
    wmf->output_sample->lpVtbl->Release(wmf->output_sample);
    wmf->output_sample = NULL;
  }
  if (!(output_stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
    status = ffwmf_create_sample(avctx, output_stream_info.cbSize,
                                 output_stream_info.cbAlignment,
                                 &wmf->output_sample);
    FFWMF_CHECK_STATUS();
  }

cleanup:
  if (found_media_type) {
    found_media_type->lpVtbl->Release(found_media_type);
  }
  if (media_type) {
    media_type->lpVtbl->Release(media_type);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_setup_transformer(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  status = ffwmf_create_transformer(avctx);
  FFWMF_CHECK_STATUS();

  status = ffwmf_set_input_media_type(avctx);
  FFWMF_CHECK_STATUS();

  status = ffwmf_set_output_media_type(avctx);
  FFWMF_CHECK_STATUS();

  /* This must be called after both input and output are configured. */
  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr = wmf->decoder->lpVtbl->GetInputStreamInfo(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, &input_stream_info);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetInputStreamInfo()");
  wmf->input_stream_alignment = input_stream_info.cbAlignment;

cleanup:
  FFWMF_DEBUG_LOG(avctx, 3, "return=%d", status);
  FFWMF_RETURN_STATUS();
}

static int ffwmf_check_adts_header(AVCodecContext* avctx,
                                   const uint8_t* data,
                                   size_t size,
                                   bool* with_adts_header) {
  int status = 0;
  AACADTSHeaderInfo adts_header = {0};
  GetBitContext gbc;

  av_assert1(!*with_adts_header);
  init_get_bits(&gbc, data, size);
  status = ff_adts_header_parse(&gbc, &adts_header);
  if (status >= 0) {
    status = 0;
    *with_adts_header = true;
    FFWMF_DEBUG_LOG(
        avctx, 8,
        "ADTS: object_type=%d sample_rate=%d(%d) samples=%d bit_rate=%d, "
        "sampling_index=%d, chan_config=%d num_aac_frames=%d",
        adts_header.object_type, adts_header.sample_rate, avctx->sample_rate,
        adts_header.samples, adts_header.bit_rate, adts_header.sampling_index,
        adts_header.chan_config, adts_header.num_aac_frames);
  } else if (status == AAC_AC3_PARSE_ERROR_SYNC) {
    // ff_adts_header_parse() did not detect ADTS header. So this is raw AAC.
    status = 0;
  } else {
    // Bad ADTS packet.
    goto cleanup;
  }

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_prepare_input(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;
  AVPacket pkt;
  bool has_packet = false;
  IMFSample* input_sample = NULL;
  IMFMediaBuffer* buffer = NULL;
  int status = 0;

  av_assert1(!wmf->doing_output);
  av_assert1(!wmf->after_eof);
  av_assert1(!wmf->input_sample);

  status = ff_decode_get_packet(avctx, &pkt);
  if (status == AVERROR(EAGAIN)) {
    // Propagate to FFmpeg that we need more packets.
    FFWMF_DEBUG_LOG(avctx, 7, "Waiting for more packets");
    goto cleanup;
  }
  if (status == AVERROR_EOF) {
    wmf->after_eof = true;
    // Drain the transformer so it can return the last sample which can be
    // incomplete or zero. We do it only for ADTS input as according to
    // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-decoder
    // and testing the raw AAC data always generate strictly one output per
    // input and there is nothing to drain.
    if (!wmf->adts_mode) {
      FFWMF_DEBUG_LOG(avctx, 3, "Exit after raw packet EOF");
      goto cleanup;
    }
    FFWMF_DEBUG_LOG(avctx, 3, "Draining on EOF");
    HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
        wmf->decoder, MFT_MESSAGE_COMMAND_DRAIN, 0);
    if (FAILED(hr)) {
      // Just log the error, do not report it to FFmpeg.
      FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
    }
    status = 0;
    wmf->doing_output = true;
    goto cleanup;
  }
  FFWMF_CHECK_STATUS();
  has_packet = true;

  if (!pkt.size) {
    // This should not happen as the decoder does not set
    // AV_CODEC_CAP_DELAY. But be defensive and treat as EOF.
    FFWMF_DEBUG_LOG(avctx, 2, "Unexpected empty packet");
    status = AVERROR_EOF;
    wmf->after_eof = true;
    goto cleanup;
  }

  FFWMF_DEBUG_LOG(avctx, 7, "Got new input packet, size=%d", pkt.size);

  // Check if the data include ADTS header.
  bool with_adts_header = false;
  status =
      ffwmf_check_adts_header(avctx, pkt.data, pkt.size, &with_adts_header);
  FFWMF_CHECK_STATUS();

  size_t new_extra_data_size = 0;
  uint8_t* new_extra_data = av_packet_get_side_data(
      &pkt, AV_PKT_DATA_NEW_EXTRADATA, &new_extra_data_size);
  if (new_extra_data) {
    FFWMF_DEBUG_LOG(avctx, 3, "extra_data_size=%zu", new_extra_data_size);
    status = ffwmf_copy_extra_data(avctx, new_extra_data, new_extra_data_size);
    FFWMF_CHECK_STATUS();
  }

  if (!wmf->decoder) {
    wmf->adts_mode = with_adts_header;
    status = ffwmf_setup_transformer(avctx);
    FFWMF_CHECK_STATUS();
  } else if (wmf->adts_mode != with_adts_header) {
    FFWMF_LOG_ERROR(avctx, "Mix of ADTS and non-adts packets is not supported");
    status = AVERROR_INVALIDDATA;
    goto cleanup;
  }

  status = ffwmf_create_sample(avctx, pkt.size, wmf->input_stream_alignment,
                               &input_sample);
  FFWMF_CHECK_STATUS();

  HRESULT hr = input_sample->lpVtbl->GetBufferByIndex(input_sample, 0, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::GetBufferByIndex()");

  uint8_t* buffer_ptr = NULL;
  hr = buffer->lpVtbl->Lock(buffer, &buffer_ptr, NULL, NULL);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  memcpy(buffer_ptr, pkt.data, pkt.size);

  hr = buffer->lpVtbl->Unlock(buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Unlock()");

  hr = buffer->lpVtbl->SetCurrentLength(buffer, pkt.size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength()");

  if (pkt.pts != AV_NOPTS_VALUE) {
    double presentation_time_in_seconds = pkt.pts * av_q2d(avctx->time_base);

    hr = input_sample->lpVtbl->SetSampleTime(
        input_sample, (int64_t)(presentation_time_in_seconds *
                                FFWMF_SAMPLE_TIME_UNITS_PER_SECOND));
    FFWMF_CHECK_HRESULT(hr, "IMFSample::SetSampleTime()");
  }

cleanup:
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  if (input_sample) {
    if (status < 0) {
      input_sample->lpVtbl->Release(input_sample);
    } else {
      wmf->input_sample = input_sample;
    }
  }
  if (has_packet) {
    av_packet_unref(&pkt);
  }
  FFWMF_RETURN_STATUS();
}

static int process_output_sample(AVCodecContext* avctx,
                                 IMFSample* sample,
                                 AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  bool locked_buffer = false;
  HRESULT hr;

  frame->sample_rate = wmf->output_samples_per_second;

  hr = sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::ConvertToContiguousBuffer()");

  uint8_t* data = NULL;
  DWORD data_size = 0;
  hr = buffer->lpVtbl->Lock(buffer, &data, NULL, &data_size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  locked_buffer = true;

  int sample_size =
      wmf->output_channel_count * (wmf->output_bits_per_sample / 8);
  frame->nb_samples = data_size / sample_size;
  data_size = frame->nb_samples * sample_size;

  if (frame->nb_samples == 0) {
    FFWMF_DEBUG_LOG(avctx, 7, "No samples in the output");
    status = AVERROR(EAGAIN);
    goto cleanup;
  }
  if (wmf->after_eof) {
    // We are draining the transformer to get the last frame. But for some
    // reasons AAC decoder may return all-zeros useless frame. Drop it if so and
    // return EOF.
    int i;
    bool all_zeros = true;
    for (i = 0; i < data_size; ++i) {
      if (data[i] != 0) {
        all_zeros = false;
        break;
      }
    }
    if (all_zeros) {
      FFWMF_DEBUG_LOG(avctx, 3, "Dropping zero frame on EOF");
      status = AVERROR_EOF;
      goto cleanup;
    }
  }
  status = ff_get_buffer(avctx, frame, 0);
  FFWMF_CHECK_STATUS();
  FFWMF_DEBUG_LOG(avctx, 7, "Got %zu decoded bytes", (size_t)data_size);

  // For the interleave formats like AV_SAMPLE_FMT_S32 and AV_SAMPLE_FMT_S16
  // where each sample contains the data for all channels the pointer to the
  // first and only audio buffer is in frame->data[0]. So we just copy there
  // what WMF returned.
  memcpy(frame->data[0], data, data_size);

cleanup:
  if (buffer) {
    if (locked_buffer) {
      hr = buffer->lpVtbl->Unlock(buffer);
      if (FAILED(hr)) {
        FFWMF_LOG_HRESULT(hr, "IMFMediaBuffer::Lock()");
        status = AVERROR_UNKNOWN;
      }
    }
    buffer->lpVtbl->Release(buffer);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_process_output(AVCodecContext* avctx, AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  HRESULT hr = S_OK;

  av_assert1(wmf->doing_output);
  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  for (;;) {
    // Make the whole buffer available for use by |decoder_| again after it was
    // filled with data by the previous call to ProcessOutput().
    if (wmf->output_sample) {
      hr = wmf->output_sample->lpVtbl->ConvertToContiguousBuffer(
          wmf->output_sample, &buffer);
      FFWMF_CHECK_HRESULT(hr, "IMFSample::ConvertToContiguousBuffer()");

      hr = buffer->lpVtbl->SetCurrentLength(buffer, 0);
      FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength(0)");

      buffer->lpVtbl->Release(buffer);
      buffer = NULL;
    } else if (output_data_buffer.pSample) {
      // The sample was allocated by MFT.
      output_data_buffer.pSample->lpVtbl->Release(output_data_buffer.pSample);
      output_data_buffer.pSample = NULL;
    }

    memset(&output_data_buffer, 0, sizeof output_data_buffer);
    output_data_buffer.pSample = wmf->output_sample;
    output_data_buffer.dwStreamID = FFWMF_DEFAULT_STREAM_ID;
    DWORD process_output_status = 0;
    hr = wmf->decoder->lpVtbl->ProcessOutput(
        wmf->decoder, /*dwFlags=*/0, /*cOutputBufferCount=*/1,
        &output_data_buffer, &process_output_status);
    if (output_data_buffer.pEvents) {
      // Even though we're not interested in events we have to clean them up.
      output_data_buffer.pEvents->lpVtbl->Release(output_data_buffer.pEvents);
      output_data_buffer.pEvents = NULL;
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // This is normal, switch to the input mode.
      FFWMF_DEBUG_LOG(avctx, 7, "Need more input");
      wmf->doing_output = false;
      goto cleanup;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      // Update the output media type and try to process the output again.
      status = ffwmf_set_output_media_type(avctx);
      FFWMF_CHECK_STATUS();
      wmf->doing_output = false;
      goto cleanup;
    }
    // Report any other non-successfull HRESULT.
    FFWMF_CHECK_HRESULT(hr, "IMFTransform::ProcessOutput()");

    status = process_output_sample(avctx, output_data_buffer.pSample, frame);
    if (status == AVERROR(EAGAIN)) {
      FFWMF_DEBUG_LOG(avctx, 7, "Repeating output attempt");
      status = 0;
      continue;
    }
    FFWMF_CHECK_STATUS();
    goto cleanup;
  }

cleanup:
  if (!wmf->output_sample && output_data_buffer.pSample) {
    // The sample was allocated by MFT.
    output_data_buffer.pSample->lpVtbl->Release(output_data_buffer.pSample);
  }
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  return status;
}

static av_cold int ffwmf_init_decoder(AVCodecContext* avctx) {
  int status;

  status =
      ffwmf_copy_extra_data(avctx, avctx->extradata, avctx->extradata_size);
  FFWMF_CHECK_STATUS();

cleanup:
  FFWMF_DEBUG_LOG(avctx, 2, "return=%d", status);
  FFWMF_RETURN_STATUS();
}

static av_cold int ffwmf_close_decoder(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  if (wmf->decoder) {
    wmf->decoder->lpVtbl->Release(wmf->decoder);
    wmf->decoder = NULL;
  }
  if (wmf->output_sample) {
    wmf->output_sample->lpVtbl->Release(wmf->output_sample);
    wmf->output_sample = NULL;
  }
  if (wmf->extradata) {
    av_free(wmf->extradata);
    wmf->extradata = NULL;
  }
  if (wmf->input_sample) {
    wmf->input_sample->lpVtbl->Release(wmf->input_sample);
    wmf->input_sample = NULL;
  }
  FFWMF_DEBUG_LOG(avctx, 2, "closed");
  return 0;
}

static int ffwmf_receive_frame(AVCodecContext* avctx, AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  for (;;) {
    if (wmf->doing_output) {
      status = ffwmf_process_output(avctx, frame);
      FFWMF_CHECK_STATUS();
      if (wmf->doing_output) {
        // We successfully output a decoded frame, return to FFmpeg.
        goto cleanup;
      }
    }
    if (wmf->after_eof) {
      // No more data to read and everything was output.
      FFWMF_DEBUG_LOG(avctx, 3, "Reporting EOF to the caller");
      status = AVERROR_EOF;
      goto cleanup;
    }

    if (!wmf->input_sample) {
      status = ffwmf_prepare_input(avctx);
      FFWMF_CHECK_STATUS();
    }

    if (wmf->input_sample) {
      HRESULT hr = wmf->decoder->lpVtbl->ProcessInput(
          wmf->decoder, FFWMF_DEFAULT_STREAM_ID, wmf->input_sample,
          /*dwFlags=*/0);
      if (hr == MF_E_NOTACCEPTING) {
        FFWMF_DEBUG_LOG(avctx, 7, "Full decoder, switching to output");
        wmf->doing_output = true;
      } else {
        FFWMF_CHECK_HRESULT(hr, "IMFSample::ProcessInput()");

        // Release the sample as we no longer need it.
        wmf->input_sample->lpVtbl->Release(wmf->input_sample);
        wmf->input_sample = NULL;
        FFWMF_DEBUG_LOG(avctx, 7, "Consumed input sample");

        // According to
        // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-processinput#remarks
        // after getting success here one should continue to call ProcessInput
        // until it returns  MF_E_NOTACCEPTING. However as AAC typically has one
        // output frame per input we start to call ProcessOutput on success as
        // well to get data ASAP. If this is wrong, then the latter returns
        // MF_E_TRANSFORM_NEED_MORE_INPUT and then we try to get more inout.
        wmf->doing_output = true;
      }
    }
  }

cleanup:
  FFWMF_RETURN_STATUS();
}

static void ffwmf_flush(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  if (!wmf || !wmf->decoder) {
    // We can be called before the decoder is lazy-initialized.
    return;
  }
  if (wmf->input_sample) {
    wmf->input_sample->lpVtbl->Release(wmf->input_sample);
    wmf->input_sample = NULL;
  }
  HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
      wmf->decoder, MFT_MESSAGE_COMMAND_FLUSH, 0);
  if (FAILED(hr)) {
    FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
  }
  wmf->after_eof = false;
  wmf->doing_output = false;

  FFWMF_DEBUG_LOG(avctx, 2, "flushed");
}

static const AVClass ffwmf_aac_decoder_class = {
    .class_name = "WMF AAC decoder",
    .version = LIBAVUTIL_VERSION_INT,
};

const FFCodec ffwmf_aac_decoder = {
    .p.name = "aac_wmf",
    .p.long_name = NULL_IF_CONFIG_SMALL("AAC (Windows Media Foundation)"),
    .p.type = AVMEDIA_TYPE_AUDIO,
    .p.id = AV_CODEC_ID_AAC,
    .priv_data_size = sizeof(WMFDecodeContext),
    .init = ffwmf_init_decoder,
    .close = ffwmf_close_decoder,
    .receive_frame = ffwmf_receive_frame,
    .p.sample_fmts =
        (const enum AVSampleFormat[]){AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S16,
                                      AV_SAMPLE_FMT_NONE},
    .p.wrapper_name = "wmf",
    .p.capabilities = AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DR1,
    .caps_internal = FF_CODEC_CAP_INIT_THREADSAFE | FF_CODEC_CAP_INIT_CLEANUP,
    .flush = ffwmf_flush,
    .p.priv_class = &ffwmf_aac_decoder_class,
};
