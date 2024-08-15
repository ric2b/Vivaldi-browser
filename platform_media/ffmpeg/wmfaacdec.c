// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

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
#include "third_party/ffmpeg/libavcodec/adts_parser.h"
#include "third_party/ffmpeg/libavcodec/avcodec.h"
#include "third_party/ffmpeg/libavcodec/bytestream.h"
#include "third_party/ffmpeg/libavcodec/codec_internal.h"
#include "third_party/ffmpeg/libavcodec/decode.h"
#include "third_party/ffmpeg/libavcodec/get_bits.h"
#include "third_party/ffmpeg/libavcodec/internal.h"
#include "third_party/ffmpeg/libavcodec/mpeg4audio.h"
#include "third_party/ffmpeg/libavcodec/mpegaudiodecheader.h"
#include "third_party/ffmpeg/libavutil/avassert.h"
#include "third_party/ffmpeg/libavutil/log.h"
#include "third_party/ffmpeg/libavutil/mem.h"
#include "third_party/ffmpeg/libavutil/opt.h"

#include "platform_media/ffmpeg/ffviv_audio_kit.h"

typedef struct {
  // All decoder-private structures must begin with this field.
  AVClass* av_class;

  IMFTransform* decoder;
  IMFSample* input_sample;
  IMFSample* output_sample;

  FFVIV_AdtsConverter adts_converter;

  // The presentation timestamp of the last incoming FFmpeg packet with data to
  // decode or AV_NOPTS_VALUE if unknown.
  int64_t last_pts;

  // Audio parameters supplied by FFmpeg or deduced from an ADTS header or
  // packet side data.

  int object_type;
  int sample_rate;
  int channels;

  // Packed audio configuration data also known as AudioSpecificConfig in AAC
  // specs.
  uint8_t* extradata;
  int extradata_size;

  // Audio parameters for the decoded audio.

  int output_sample_rate;
  int output_channels;
  int output_bits_per_sample;

  // Alignment for input sample data to pass to the decoder.
  uint32_t input_stream_alignment;

  // With SBR (Spectral band Replication) sound the WMF AAC decoder may notify
  // about a new output media type with sampling frequency 2 times less than the
  // real. Then after consuming more input without producing any decoded data in
  // between it will change to the type with the real rate. To ignore the first
  // bogus change we use this flag to forward to FFmpeg the format change only
  // after we receive new decoded data.
  bool pending_output_change;

  // We are draining the decoder to squeeze the last frame from it after eof.
  bool eof_drain;

  // True when `decoder->ProcessOutput()` returned success and we must continue
  // to call it without calling `decoder->ProcessInput()` until it returns
  // MF_E_TRANSFORM_NEED_MORE_INPUT. This is implied by MS documentation and
  // confirmed via testing.
  bool after_successful_output;

  // Number of successful `decoder->ProcessInput()` calls minus number of
  // successful `decoder->ProcessOutput()` calls to monitor how far WMF decoder
  // delays the output.
  int input_output_delta;

  // Number of calls to `ffwmf_decode_frame()`.
  unsigned decode_frame_calls;

} WMFDecodeContext;

typedef HRESULT(STDAPICALLTYPE* ffwmf_DllGetClassObject_ptr)(REFCLSID rclsid,
                                                             REFIID riid,
                                                             LPVOID* ppv);

static ffwmf_DllGetClassObject_ptr ffwmf_get_class_object = NULL;

// Firefox always converts ADTS headers if any into extra metadata and only
// feeds raw AAC frames to WMF decoder. The original Opera code feeds the
// decoder with ADTS. The constant selects the preferred behaviour.
#define FFWMF_FEED_ADTS_TO_DECODER (false)

#define FFWMF_DEFAULT_STREAM_ID ((DWORD)0)

// IMFSample's timestamp is expressed in hundreds of nanoseconds.
#define FFWMF_SAMPLE_TIME_UNITS_PER_SECOND ((double)1.0e7)

// When true, configure the decoder to use the so-called low-latency mode when
// it produces output immediately after feeding it with an encoded frame. This
// matches what the software FFmpeg and native MacOS decoders do. When false the
// decoder uses default settings when it typically delays the output by one
// frame.
#define FFWMF_USE_LOW_LATENCY_MODE 1

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
  av_log(avctx, AV_LOG_ERROR, "FAILED %s, hr=0x%lX\n", details, hr)

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
                                 const uint8_t* extradata,
                                 int extradata_size) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  if (extradata_size == 0)
    goto cleanup;

  // The extra data is AudioSpecificConfig, see section 1.6.2.1 in
  // ISO14496-3-2009. It must be at least 2 bytes long.
  if (extradata_size < 2) {
    av_log(avctx, AV_LOG_ERROR, "Invalid extra data size %d\n", extradata_size);
    status = AVERROR_INVALIDDATA;
    goto cleanup;
  }

  GetBitContext gbc;
  init_get_bits8(&gbc, extradata, extradata_size);

  int object_type = get_bits(&gbc, 5);
  if (object_type == 31) {
    object_type = 32 + get_bits(&gbc, 6);
  }

  int sample_rate = 0;
  int sampling_index = get_bits(&gbc, 4);
  if (sampling_index == 0xF) {
    // The sampling frequency is explicitly defined in the following 24 bits
    if (extradata_size < 5) {
      av_log(avctx, AV_LOG_ERROR,
             "Extra data with invalid sampling frequency\n");
      status = AVERROR_INVALIDDATA;
      goto cleanup;
    }
    sample_rate = get_bits(&gbc, 24);
  } else {
    // The frequency is the index into the table.
    sample_rate = ff_mpeg4audio_sample_rates[sampling_index];
  }
  if (!sample_rate) {
    // Keep the old value on unknown or invalid rate.
    sample_rate = wmf->sample_rate;
  }

  int channels = 0;
  int channel_config = get_bits(&gbc, 4);
  if (1 <= channel_config && channel_config <= 6) {
    channels = channel_config;
  } else if (channel_config == 7) {
    channels = 8;
  } else {
    // Preserve old channel on unknown channel config.
    channels = wmf->channels;
  }

  av_log(avctx, AV_LOG_DEBUG,
         "New extra: size=%d object_type=%d sample_rate=%d channels=%d "
         "data=(0x%02x 0x%02x)\n",
         extradata_size, object_type, sample_rate, channels, extradata[0],
         extradata[1]);

  status = ffviv_copy_data(extradata, extradata_size, &wmf->extradata,
                           &wmf->extradata_size);
  FFWMF_CHECK_STATUS();

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_create_transformer(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IClassFactory* factory = NULL;
  IMFAttributes* attributes = NULL;

  av_assert1(!wmf->decoder);

  HMODULE library = ffwmf_load_audio_library(1);
  if (!library) {
    av_log(avctx, AV_LOG_ERROR, "failed to load WMF audio DLL\n");
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
      av_log(avctx, AV_LOG_ERROR, "failed to retrive %s\n",
             get_class_object_name);
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

  if (FFWMF_USE_LOW_LATENCY_MODE) {
    hr = wmf->decoder->lpVtbl->GetAttributes(wmf->decoder, &attributes);
    FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetAttributes()");

    hr = attributes->lpVtbl->SetUINT32(attributes, &MF_LOW_LATENCY, 1);
    FFWMF_CHECK_HRESULT(hr, "IMFAttributes::SetUINT32(MF_LOW_LATENCY)");
  }

cleanup:
  if (attributes) {
    attributes->lpVtbl->Release(attributes);
  }
  if (factory) {
    factory->lpVtbl->Release(factory);
  }
  FFWMF_RETURN_STATUS();
}

// See Input Types in
// https://docs.microsoft.com/en-us/windows/win32/medfound/aac-decoder.
// Another useful source is
// https://searchfox.org/mozilla-central/source/dom/media/platforms/wmf/WMFAudioMFTManager.cpp
static int ffwmf_set_input_media_type(AVCodecContext* avctx,
                                      bool* media_type_error) {
  av_assert1(!*media_type_error);

  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaType* media_type = NULL;
  HEAACWAVEINFO* wave_info = NULL;

  HRESULT hr = MFCreateMediaType(&media_type);
  FFWMF_CHECK_HRESULT(hr, "MFCreateMediaType()");

  hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE,
                                   &MFMediaType_Audio);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetGUID(MF_MT_MAJOR_TYPE)");

  hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE,
                                   &MFAudioFormat_AAC);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::SetGUID(MF_MT_SUBTYPE)");

  bool adts_mode =
      FFWMF_FEED_ADTS_TO_DECODER && wmf->adts_converter.header_size != 0;
  size_t info_size = sizeof(*wave_info);
  if (!adts_mode) {
    info_size += wmf->extradata_size;
  }
  wave_info = av_malloc(info_size);
  if (!wave_info) {
    status = AVERROR(ENOMEM);
    goto cleanup;
  }
  memset(wave_info, 0, info_size);
  wave_info->wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;
  wave_info->wfx.nChannels = wmf->channels;
  wave_info->wfx.nSamplesPerSec = wmf->sample_rate;
  wave_info->wfx.nAvgBytesPerSec = 1;
  wave_info->wfx.nBlockAlign = 1;
  wave_info->wfx.wBitsPerSample = 16;
  wave_info->wfx.cbSize = (WORD)(info_size - sizeof(WAVEFORMATEX));

  wave_info->wPayloadType = adts_mode ? 1 : 0;

  // MS documentation suggests to set wAudioProfileLevelIndication to 0xFE when
  // unknown. Firefox sets this to extra->object_type when object_type comes
  // from ADTS header, but that is wrong as these are different things, see
  // table 1.1 and 1.14 in ISO14496-3-2009. Moreover, MS documentation at
  // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-decoder in an
  // example assigns 0x2A to this when the object type is 2. In general it seems
  // this does not influence anything. So use the unknown marker.
  wave_info->wAudioProfileLevelIndication = 0xFE;

  if (!adts_mode) {
    memcpy(wave_info + 1, wmf->extradata, wmf->extradata_size);
  }

  hr = MFInitMediaTypeFromWaveFormatEx(media_type, &wave_info->wfx, info_size);
  FFWMF_CHECK_HRESULT(hr, "MFInitMediaTypeFromWaveFormatEx()");

  hr = wmf->decoder->lpVtbl->SetInputType(wmf->decoder, FFWMF_DEFAULT_STREAM_ID,
                                          media_type, /*fwFlags=*/0);
  if (FAILED(hr)) {
    const char* error_name = NULL;
    switch (hr) {
      case S_OK:
        break;
      case MF_E_INVALIDMEDIATYPE:
        *media_type_error = true;
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
    if (error_name) {
      av_log(avctx, AV_LOG_ERROR,
             "failed IMFTransform::SetInputType(), error=%s(hr=0x%lX)\n",
             error_name, hr);
      status = AVERROR_UNKNOWN;
    }
    goto cleanup;
  }

cleanup:
  if (media_type) {
    media_type->lpVtbl->Release(media_type);
  }
  if (wave_info) {
    av_free(wave_info);
  }
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
    av_log(avctx, AV_LOG_ERROR,
           "failed to find PCM format among supported outputs\n");
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

  // Do not write directly into wmf->output_channels etc. Read first the
  // value into a temporary and then copy into wmf to ensure no change on
  // errors.
  DWORD output_sample_rate = 0;
  DWORD output_channels = 0;

  HRESULT hr = found_media_type->lpVtbl->GetUINT32(
      found_media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_sample_rate);
  FFWMF_CHECK_HRESULT(
      hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND)");

  hr = found_media_type->lpVtbl->GetUINT32(
      found_media_type, &MF_MT_AUDIO_NUM_CHANNELS, &output_channels);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_NUM_CHANNELS)");

  hr = wmf->decoder->lpVtbl->SetOutputType(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, found_media_type, /*flags=*/0);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::SetOutputType()");

  wmf->output_bits_per_sample = found_output_bits_per_sample;
  wmf->output_channels = output_channels;
  wmf->output_sample_rate = output_sample_rate;

  // We will inform FFmpeg about the new output format only after we receive
  // decoded data, see comments at the field definition.
  wmf->pending_output_change = true;

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
  av_log(avctx, AV_LOG_DEBUG,
         "[N=%u] New output type: bits_per_sample=%d samples_per_second=%d "
         "channels=%d preallocated_output_sample=%d\n",
         wmf->decode_frame_calls, wmf->output_bits_per_sample,
         wmf->output_sample_rate, wmf->output_channels, !!wmf->output_sample);

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

  // Try to set the input media type twice, first with the config from FFmpeg
  // and then with basic one if WMF decoder rejects the initial one.
  //
  // TODO(igor@vivaldi.com): Consider unconditionally removing extension bytes
  // that follows the initial 2 bytes of the config following Firefox, see
  // https://searchfox.org/mozilla-central/source/dom/media/platforms/wmf/WMFAudioMFTManager.cpp#32
  for (int attempt = 0;; ++attempt) {
    bool media_type_error = false;
    status = ffwmf_set_input_media_type(avctx, &media_type_error);
    FFWMF_CHECK_STATUS();
    if (!media_type_error)
      break;
    if (attempt > 0 || wmf->sample_rate == 0 || wmf->channels == 0) {
      av_log(avctx, AV_LOG_ERROR, "error=MF_E_INVALIDMEDIATYPE (0xC00D36B4)\n");
      status = AVERROR_UNKNOWN;
      goto cleanup;
    }
    av_log(avctx, AV_LOG_INFO,
           "Decoder rejected AAC config, trying simpler one\n");
    status = ffviv_construct_simple_aac_config(
        avctx, /*object_type=*/2, wmf->sample_rate, wmf->channels,
        &wmf->extradata, &wmf->extradata_size);
    FFWMF_CHECK_STATUS();
  }

  status = ffwmf_set_output_media_type(avctx);
  FFWMF_CHECK_STATUS();

  /* This must be called after both input and output are configured. */
  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr = wmf->decoder->lpVtbl->GetInputStreamInfo(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, &input_stream_info);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetInputStreamInfo()");
  wmf->input_stream_alignment = input_stream_info.cbAlignment;

cleanup:
  av_log(avctx, AV_LOG_INFO, "return=%d\n", status);
  FFWMF_RETURN_STATUS();
}

static int ffwmf_process_adts(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  av_assert1(wmf->adts_converter.header_size != 0);

  if (wmf->adts_converter.header.object_type == 1 &&
      wmf->adts_converter.header.chan_config == 2) {
    // This object type generates MF_E_INVALIDMEDIATYPE(hr=0xC00D36B4) with
    // IMFTransform::SetInputType() if we strip ADTS headers and gives no output
    // if feed ADTS to the decoders. The following replacement makes things to
    // work.
    wmf->adts_converter.header.object_type = 2;
  }

  int status = ffviv_convert_adts_to_aac_config(
      avctx, &wmf->adts_converter, &wmf->extradata, &wmf->extradata_size);
  FFWMF_CHECK_STATUS();

  wmf->object_type = wmf->adts_converter.header.object_type;
  wmf->sample_rate = wmf->adts_converter.header.sample_rate;
  int chan_config = wmf->adts_converter.header.chan_config;
  int channels = chan_config < 7 ? chan_config : (chan_config == 7 ? 8 : 0);
  if (channels) {
    wmf->channels = channels;
  }

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_prepare_input(AVCodecContext* avctx,
                               AVPacket* avpkt,
                               bool* consumed) {
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFSample* input_sample = NULL;
  IMFMediaBuffer* buffer = NULL;
  int status = 0;

  av_assert1(!*consumed);
  av_assert1(!wmf->input_sample);

  if (!avpkt->size) {
    if (wmf->eof_drain)
      goto cleanup;
    // We received eof the first time. Drain the transformer so it can
    // return the last sample which can be incomplete or zero.
    av_log(avctx, AV_LOG_INFO, "[N=%u] Draining on EOF\n",
           wmf->decode_frame_calls);
    HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
        wmf->decoder, MFT_MESSAGE_COMMAND_DRAIN, 0);
    if (FAILED(hr)) {
      // Just log the error, do not report it to FFmpeg.
      FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
    }
    wmf->eof_drain = true;
    goto cleanup;
  }

  av_log(avctx, AV_LOG_DEBUG, "[N=%u] Got new input packet, size=%d pts=%s\n",
         wmf->decode_frame_calls, avpkt->size,
         (avpkt->pts != AV_NOPTS_VALUE) ? "valid" : "none");

  // Check if the data include ADTS header.
  status = ffviv_check_adts_header(avctx, &wmf->adts_converter, avpkt->data,
                                   avpkt->size);
  FFWMF_CHECK_STATUS();
  if (wmf->adts_converter.header_size != 0) {
    status = ffwmf_process_adts(avctx);
    FFWMF_CHECK_STATUS();
  } else {
    size_t new_extra_data_size = 0;
    uint8_t* new_extra_data = av_packet_get_side_data(
        avpkt, AV_PKT_DATA_NEW_EXTRADATA, &new_extra_data_size);
    status = ffwmf_copy_extra_data(avctx, new_extra_data, new_extra_data_size);
    FFWMF_CHECK_STATUS();
  }

  if (!wmf->decoder) {
    status = ffwmf_setup_transformer(avctx);
    FFWMF_CHECK_STATUS();
  }

  int skip_size =
      FFWMF_FEED_ADTS_TO_DECODER ? 0 : wmf->adts_converter.header_size;
  size_t data_size = avpkt->size - skip_size;

  status = ffwmf_create_sample(avctx, data_size, wmf->input_stream_alignment,
                               &input_sample);
  FFWMF_CHECK_STATUS();

  HRESULT hr = input_sample->lpVtbl->GetBufferByIndex(input_sample, 0, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::GetBufferByIndex()");

  uint8_t* buffer_ptr = NULL;
  hr = buffer->lpVtbl->Lock(buffer, &buffer_ptr, NULL, NULL);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  memcpy(buffer_ptr, avpkt->data + skip_size, data_size);

  hr = buffer->lpVtbl->Unlock(buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Unlock()");

  hr = buffer->lpVtbl->SetCurrentLength(buffer, data_size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength()");

  wmf->last_pts = avpkt->pts;
  if (avpkt->pts != AV_NOPTS_VALUE) {
    double presentation_time_in_seconds = avpkt->pts * av_q2d(avctx->time_base);

    hr = input_sample->lpVtbl->SetSampleTime(
        input_sample, (int64_t)(presentation_time_in_seconds *
                                FFWMF_SAMPLE_TIME_UNITS_PER_SECOND));
    FFWMF_CHECK_HRESULT(hr, "IMFSample::SetSampleTime()");
  }

  *consumed = true;

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
  FFWMF_RETURN_STATUS();
}

static int process_output_sample(AVCodecContext* avctx,
                                 IMFSample* sample,
                                 AVFrame* frame,
                                 int* got_frame_ptr) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  bool locked_buffer = false;
  HRESULT hr;

  hr = sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::ConvertToContiguousBuffer()");

  uint8_t* data = NULL;
  DWORD data_size = 0;
  hr = buffer->lpVtbl->Lock(buffer, &data, NULL, &data_size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  locked_buffer = true;

  int sample_size = wmf->output_channels * (wmf->output_bits_per_sample / 8);
  int samples = data_size / sample_size;
  data_size = samples * sample_size;

  if (samples == 0) {
    av_log(avctx, AV_LOG_INFO, "[N=%u] No samples in the output\n",
           wmf->decode_frame_calls);
    status = AVERROR(EAGAIN);
    goto cleanup;
  }
  if (wmf->eof_drain) {
    // We are draining the transformer to get the last frame. But for some
    // reasons AAC decoder may return all-zeros useless frame. Drop it.
    int i;
    bool all_zeros = true;
    for (i = 0; i < data_size; ++i) {
      if (data[i] != 0) {
        all_zeros = false;
        break;
      }
    }
    if (all_zeros) {
      av_log(avctx, AV_LOG_INFO,
             "[N=%u] Dropping zero frame on EOF, io_delta=%d\n",
             wmf->decode_frame_calls, wmf->input_output_delta);
      goto cleanup;
    }
  }

  // We got decoded data, update FFmpeg about any pending format changes.
  if (wmf->pending_output_change) {
    wmf->pending_output_change = false;
    avctx->sample_fmt = (wmf->output_bits_per_sample == 32) ? AV_SAMPLE_FMT_S32
                                                            : AV_SAMPLE_FMT_S16;

    // TODO(igor@vivaldi.com): Properly convert WMF channel layout into FFmpeg.
    av_channel_layout_uninit(&avctx->ch_layout);
    av_channel_layout_default(&avctx->ch_layout, wmf->output_channels);

    avctx->sample_rate = wmf->output_sample_rate;
  }

  // ff_get_buffer uses nb_samples to get the allocation size.
  frame->nb_samples = samples;
  status = ff_get_buffer(avctx, frame, 0);
  if (status < 0) {
    av_log(avctx, AV_LOG_ERROR,
           "Failed ff_get_buffer(): status=%d data_size=%zu\n", status,
           (size_t)data_size);
    goto cleanup;
  }

  *got_frame_ptr = 1;

  // For the interleave formats like AV_SAMPLE_FMT_S32 and AV_SAMPLE_FMT_S16
  // where each sample contains the data for all channels the pointer to the
  // first and only audio buffer is in frame->data[0]. So we just copy there
  // what WMF returned.
  memcpy(frame->data[0], data, data_size);

  // avctx->frame_size must be set to a typical number of samples to use for AAC
  // timing accounting. For AAC that should be the standard 1024 or 2048, so
  // just use the number of decoded samples unless we after eof when the last
  // drained frame can be incomplete.
  if (!wmf->eof_drain) {
    avctx->frame_size = samples;
  }

  if (wmf->last_pts != AV_NOPTS_VALUE) {
    frame->pts = wmf->last_pts;
    wmf->last_pts = AV_NOPTS_VALUE;
  }

  // AudioFileReader in Chromium does not support sampling rate changes, but
  // WMF decoder may briefly for one or two frames reduce the rate by factor
  // of two for SBR AAC. As a workaround keep the initial decoded rate as is.
  //
  // TODO(igor@vivaldi.com): Consider a better workaround here like upscaling
  // of data.
  if (wmf->output_sample_rate * 2 != avctx->sample_rate) {
    frame->sample_rate = wmf->output_sample_rate;
  } else {
    frame->sample_rate = avctx->sample_rate;
  }

  av_log(avctx, AV_LOG_DEBUG,
         "[N=%u] Sent to FFmpeg %zu decoded bytes, samples=%d io_delta=%d\n",
         wmf->decode_frame_calls, (size_t)data_size, samples,
         wmf->input_output_delta);

cleanup:
  if (buffer) {
    if (locked_buffer) {
      hr = buffer->lpVtbl->Unlock(buffer);
      if (FAILED(hr)) {
        FFWMF_LOG_HRESULT(hr, "IMFMediaBuffer::Lock()");
        status = AVERROR_UNKNOWN;
      }
    }

    // Prepare the buffer for re-use as we copied its data.
    hr = buffer->lpVtbl->SetCurrentLength(buffer, 0);
    if (FAILED(hr)) {
      FFWMF_LOG_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength(0)");
      status = AVERROR_UNKNOWN;
    }
    buffer->lpVtbl->Release(buffer);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_process_output(AVCodecContext* avctx,
                                AVFrame* frame,
                                int* got_frame_ptr) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  HRESULT hr = S_OK;

  av_assert1(!*got_frame_ptr);
  wmf->after_successful_output = false;
  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  int stream_change_count = 0;
  for (;;) {
    if (!wmf->output_sample && output_data_buffer.pSample) {
      // The sample was allocated by MFT on the previous iteration.
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
      av_log(avctx, AV_LOG_DEBUG, "[N=%u] Need more input\n",
             wmf->decode_frame_calls);
      goto cleanup;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      // Try to detect an infinite loop via repeated
      // MF_E_TRANSFORM_STREAM_CHANGE result. The repeat count is taken from
      // Forefox code,
      // https://searchfox.org/mozilla-central/source/dom/media/platforms/wmf/WMFAudioMFTManager.cpp#227
      ++stream_change_count;
      if (stream_change_count > 100) {
        av_log(avctx, AV_LOG_ERROR, "probable ProcessOutput infinite loop\n");
        status = AVERROR_UNKNOWN;
        goto cleanup;
      }
      // Update the output media type and try to process the output again.
      status = ffwmf_set_output_media_type(avctx);
      FFWMF_CHECK_STATUS();
      continue;
    }
    // Report any other non-successful HRESULT.
    FFWMF_CHECK_HRESULT(hr, "IMFTransform::ProcessOutput()");
    wmf->input_output_delta--;

    status = process_output_sample(avctx, output_data_buffer.pSample, frame,
                                   got_frame_ptr);
    if (status == AVERROR(EAGAIN)) {
      status = 0;
      continue;
    }
    FFWMF_CHECK_STATUS();
    if (*got_frame_ptr) {
      wmf->after_successful_output = true;
    }
    break;
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
  WMFDecodeContext* wmf = avctx->priv_data;

  av_log(avctx, AV_LOG_INFO,
         "Initial sample_rate=%d channels=%d profile=%d level=%d\n",
         avctx->sample_rate, avctx->ch_layout.nb_channels, avctx->profile,
         avctx->level);

  wmf->last_pts = AV_NOPTS_VALUE;
  wmf->sample_rate = avctx->sample_rate;
  wmf->channels = avctx->ch_layout.nb_channels;

  status =
      ffwmf_copy_extra_data(avctx, avctx->extradata, avctx->extradata_size);
  FFWMF_CHECK_STATUS();

cleanup:
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

  av_free(wmf->extradata);
  wmf->extradata = NULL;
  wmf->extradata_size = 0;

  if (wmf->input_sample) {
    wmf->input_sample->lpVtbl->Release(wmf->input_sample);
    wmf->input_sample = NULL;
  }
  ffviv_finish_adts_converter(&wmf->adts_converter);
  av_log(avctx, AV_LOG_INFO, "closed\n");
  return 0;
}

static int ffwmf_decode_frame(AVCodecContext* avctx,
                              void* data,
                              int* got_frame_ptr,
                              AVPacket* avpkt) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  AVFrame* frame = data;
  bool consumed_packet = false;

  wmf->decode_frame_calls++;
  *got_frame_ptr = 0;
  if (avpkt->size) {
    // Clear stalled drain status
    wmf->eof_drain = false;
  } else if (!wmf->decoder) {
    // We got EOF on the first packet before we initialized the decoder, just
    // return.
    goto cleanup;
  }

  if (wmf->after_successful_output) {
    // See comments for `after_successful_output` field definition.
    status = ffwmf_process_output(avctx, frame, got_frame_ptr);
    FFWMF_CHECK_STATUS();
    if (*got_frame_ptr)
      goto cleanup;
  }

  if (!wmf->input_sample) {
    status = ffwmf_prepare_input(avctx, avpkt, &consumed_packet);
    FFWMF_CHECK_STATUS();
  }

  if (wmf->input_sample) {
    HRESULT hr = wmf->decoder->lpVtbl->ProcessInput(
        wmf->decoder, FFWMF_DEFAULT_STREAM_ID, wmf->input_sample,
        /*dwFlags=*/0);
    if (hr == MF_E_NOTACCEPTING) {
      av_log(avctx, AV_LOG_DEBUG, "[N=%u] Full decoder, switching to output\n",
             wmf->decode_frame_calls);
    } else {
      FFWMF_CHECK_HRESULT(hr, "IMFSample::ProcessInput()");

      wmf->input_output_delta++;

      // Release the sample as we no longer need it.
      wmf->input_sample->lpVtbl->Release(wmf->input_sample);
      wmf->input_sample = NULL;
      av_log(avctx, AV_LOG_DEBUG, "[N=%u] Consumed input sample, io_delta=%d\n",
             wmf->decode_frame_calls, wmf->input_output_delta);
    }
  }

  status = ffwmf_process_output(avctx, frame, got_frame_ptr);
  FFWMF_CHECK_STATUS();

cleanup:
  if (status >= 0 && consumed_packet) {
    // Tell FFmpeg we consumed all packet bytes.
    status = avpkt->size;
  }
  FFWMF_RETURN_STATUS();
}

static void ffwmf_flush(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  if (!wmf)
    return;

  if (wmf->input_sample) {
    wmf->input_sample->lpVtbl->Release(wmf->input_sample);
    wmf->input_sample = NULL;
  }
  // `decoder` is null when the flush is called before the decoder is
  // lazy-initialized.
  if (wmf->decoder) {
    HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
        wmf->decoder, MFT_MESSAGE_COMMAND_FLUSH, 0);
    if (FAILED(hr)) {
      FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
    }
  }
  wmf->last_pts = AV_NOPTS_VALUE;
  wmf->eof_drain = false;
  wmf->after_successful_output = false;

  av_log(avctx, AV_LOG_INFO, "flushed\n");
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
    .cb.decode = ffwmf_decode_frame,
    .p.sample_fmts =
        (const enum AVSampleFormat[]){AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S16,
                                      AV_SAMPLE_FMT_NONE},
    .p.wrapper_name = "wmf",
    .p.capabilities =
        AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY,
    .caps_internal = FF_CODEC_CAP_INIT_CLEANUP,
    .flush = ffwmf_flush,
    .p.priv_class = &ffwmf_aac_decoder_class,
};
