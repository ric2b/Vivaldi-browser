// ***NOTE**
//
// This is a fork of chromium/third_party/ffmpeg/libavcodec/audiotoolboxdec.c
// This is based on the revision e481fc655a6287e657a88e8c2bcd6f411d254d70 in
// chromium/third_party/ffmpeg submodule.
//
/*
 * Audio Toolbox system codecs
 *
 * copyright (c) 2016 rcombs
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <AudioToolbox/AudioToolbox.h>

#include "config.h"
#include "config_components.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/ac3_parser_internal.h"
#include "libavcodec/bytestream.h"
#include "libavcodec/codec_internal.h"
#include "libavcodec/internal.h"
#include "libavcodec/mpegaudiodecheader.h"
#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"

#include "platform_media/ffmpeg/ffviv_audio_kit.h"
#include "third_party/ffmpeg/libavcodec/decode.h"
#include "third_party/ffmpeg/libavutil/mem.h"

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
#define kAudioFormatEnhancedAC3 'ec-3'
#endif

// Normally with AAC audio we convert ADTS headers into AudioSpecificConfig
// bytes which in turn is converted into what Apple documentation calls magic
// cookie and then passed to AudioFormatGetProperty to get
// AudioStreamBasicDescription. But Apple also provides API that can extract
// AudioStreamBasicDescription from AAC with ADTS header. As using the cookie
// works in all cases when those API work and it also works with
// AudioSpecificConfig supplied from the container format so we do not use the
// API. But for testing it can be useful to see what Apple API thinks about a
// particular audio and set the following to 1.
#define FFAT_USE_FILE_STREAM_PARSE 0

typedef struct ATDecodeContext {
    AVClass *av_class;

    AudioConverterRef converter;
    AudioStreamPacketDescription pkt_desc;
    AudioStreamBasicDescription format;
    AVPacket* in_pkt;
    char *decoded_data;
    int channel_map[64];
    FFVIV_AdtsConverter adts_converter;

    uint8_t *extradata;
    int extradata_size;

    int64_t last_pts;
} ATDecodeContext;

static UInt32 ffat_get_format_id(enum AVCodecID codec, int profile)
{
    switch (codec) {
    case AV_CODEC_ID_AAC:
        return kAudioFormatMPEG4AAC;
    case AV_CODEC_ID_AC3:
        return kAudioFormatAC3;
    case AV_CODEC_ID_ADPCM_IMA_QT:
        return kAudioFormatAppleIMA4;
    case AV_CODEC_ID_ALAC:
        return kAudioFormatAppleLossless;
    case AV_CODEC_ID_AMR_NB:
        return kAudioFormatAMR;
    case AV_CODEC_ID_EAC3:
        return kAudioFormatEnhancedAC3;
    case AV_CODEC_ID_GSM_MS:
        return kAudioFormatMicrosoftGSM;
    case AV_CODEC_ID_ILBC:
        return kAudioFormatiLBC;
    case AV_CODEC_ID_MP1:
        return kAudioFormatMPEGLayer1;
    case AV_CODEC_ID_MP2:
        return kAudioFormatMPEGLayer2;
    case AV_CODEC_ID_MP3:
        return kAudioFormatMPEGLayer3;
    case AV_CODEC_ID_PCM_ALAW:
        return kAudioFormatALaw;
    case AV_CODEC_ID_PCM_MULAW:
        return kAudioFormatULaw;
    case AV_CODEC_ID_QDMC:
        return kAudioFormatQDesign;
    case AV_CODEC_ID_QDM2:
        return kAudioFormatQDesign2;
    default:
        av_assert0(!"Invalid codec ID!");
        return 0;
    }
}

static int ffat_get_channel_id(AudioChannelLabel label)
{
    if (label == 0)
        return -1;
    else if (label <= kAudioChannelLabel_LFEScreen)
        return label - 1;
    else if (label <= kAudioChannelLabel_RightSurround)
        return label + 4;
    else if (label <= kAudioChannelLabel_CenterSurround)
        return label + 1;
    else if (label <= kAudioChannelLabel_RightSurroundDirect)
        return label + 23;
    else if (label <= kAudioChannelLabel_TopBackRight)
        return label - 1;
    else if (label < kAudioChannelLabel_RearSurroundLeft)
        return -1;
    else if (label <= kAudioChannelLabel_RearSurroundRight)
        return label - 29;
    else if (label <= kAudioChannelLabel_RightWide)
        return label - 4;
    else if (label == kAudioChannelLabel_LFE2)
        return ff_ctzll(AV_CH_LOW_FREQUENCY_2);
    else if (label == kAudioChannelLabel_Mono)
        return ff_ctzll(AV_CH_FRONT_CENTER);
    else
        return -1;
}

static int ffat_compare_channel_descriptions(const void* a, const void* b)
{
    const AudioChannelDescription* da = a;
    const AudioChannelDescription* db = b;
    return ffat_get_channel_id(da->mChannelLabel) - ffat_get_channel_id(db->mChannelLabel);
}

static AudioChannelLayout *ffat_convert_layout(AudioChannelLayout *layout, UInt32* size)
{
    AudioChannelLayoutTag tag = layout->mChannelLayoutTag;
    AudioChannelLayout *new_layout;
    if (tag == kAudioChannelLayoutTag_UseChannelDescriptions)
        return layout;
    else if (tag == kAudioChannelLayoutTag_UseChannelBitmap)
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForBitmap,
                                   sizeof(UInt32), &layout->mChannelBitmap, size);
    else
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag,
                                   sizeof(AudioChannelLayoutTag), &tag, size);
    new_layout = av_malloc(*size);
    if (!new_layout) {
        av_free(layout);
        return NULL;
    }
    if (tag == kAudioChannelLayoutTag_UseChannelBitmap)
        AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                               sizeof(UInt32), &layout->mChannelBitmap, size, new_layout);
    else
        AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                               sizeof(AudioChannelLayoutTag), &tag, size, new_layout);
    new_layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    av_free(layout);
    return new_layout;
}

static int ffat_update_ctx(AVCodecContext *avctx)
{
    ATDecodeContext *at = avctx->priv_data;
    UInt32 size = sizeof(at->format);
    if (!AudioConverterGetProperty(at->converter,
                                   kAudioConverterCurrentInputStreamDescription,
                                   &size, &at->format)) {
        if (at->format.mSampleRate)
            avctx->sample_rate = at->format.mSampleRate;
        av_channel_layout_uninit(&avctx->ch_layout);
        av_channel_layout_default(
            &avctx->ch_layout, at->format.mChannelsPerFrame);
        avctx->frame_size = at->format.mFramesPerPacket;
    }

    if (!AudioConverterGetProperty(at->converter,
                                   kAudioConverterCurrentOutputStreamDescription,
                                   &size, &at->format)) {
        at->format.mSampleRate = avctx->sample_rate;
        at->format.mChannelsPerFrame = avctx->ch_layout.nb_channels;
        AudioConverterSetProperty(at->converter,
                                  kAudioConverterCurrentOutputStreamDescription,
                                  size, &at->format);
    }

    if (!AudioConverterGetPropertyInfo(at->converter, kAudioConverterOutputChannelLayout,
                                       &size, NULL) && size) {
        AudioChannelLayout *layout = av_malloc(size);
        uint64_t layout_mask = 0;
        int i;
        if (!layout)
            return AVERROR(ENOMEM);
        AudioConverterGetProperty(at->converter, kAudioConverterOutputChannelLayout,
                                  &size, layout);
        if (!(layout = ffat_convert_layout(layout, &size)))
            return AVERROR(ENOMEM);
        for (i = 0; i < layout->mNumberChannelDescriptions; i++) {
            int id = ffat_get_channel_id(layout->mChannelDescriptions[i].mChannelLabel);
            if (id < 0)
                goto done;
            if (layout_mask & (1 << id))
                goto done;
            layout_mask |= 1 << id;
            layout->mChannelDescriptions[i].mChannelFlags = i; // Abusing flags as index
        }
        av_channel_layout_uninit(&avctx->ch_layout);
        av_channel_layout_from_mask(&avctx->ch_layout, layout_mask);
        qsort(layout->mChannelDescriptions, layout->mNumberChannelDescriptions,
              sizeof(AudioChannelDescription), &ffat_compare_channel_descriptions);
        for (i = 0; i < layout->mNumberChannelDescriptions; i++)
            at->channel_map[i] = layout->mChannelDescriptions[i].mChannelFlags;
done:
        av_free(layout);
    }

    if (!avctx->frame_size)
        avctx->frame_size = 2048;

    return 0;
}

static void put_descr(PutByteContext *pb, int tag, unsigned int size)
{
    int i = 3;
    bytestream2_put_byte(pb, tag);
    for (; i > 0; i--)
        bytestream2_put_byte(pb, (size >> (7 * i)) | 0x80);
    bytestream2_put_byte(pb, size & 0x7F);
}

static uint8_t* ffat_get_magic_cookie(AVCodecContext *avctx, UInt32 *cookie_size)
{
    ATDecodeContext *at = avctx->priv_data;
    if (avctx->codec_id == AV_CODEC_ID_AAC) {
        char *extradata;
        PutByteContext pb;
        *cookie_size = 5 + 3 + 5+13 + 5+at->extradata_size;
        if (!(extradata = av_malloc(*cookie_size)))
            return NULL;

        bytestream2_init_writer(&pb, extradata, *cookie_size);

        // ES descriptor
        put_descr(&pb, 0x03, 3 + 5+13 + 5+at->extradata_size);
        bytestream2_put_be16(&pb, 0);
        bytestream2_put_byte(&pb, 0x00); // flags (= no flags)

        // DecoderConfig descriptor
        put_descr(&pb, 0x04, 13 + 5+at->extradata_size);

        // Object type indication
        bytestream2_put_byte(&pb, 0x40);

        bytestream2_put_byte(&pb, 0x15); // flags (= Audiostream)

        bytestream2_put_be24(&pb, 0); // Buffersize DB

        bytestream2_put_be32(&pb, 0); // maxbitrate
        bytestream2_put_be32(&pb, 0); // avgbitrate

        // DecoderSpecific info descriptor
        put_descr(&pb, 0x05, at->extradata_size);
        bytestream2_put_buffer(&pb, at->extradata, at->extradata_size);
        return extradata;
    } else {
        *cookie_size = at->extradata_size;
        return at->extradata;
    }
}

static av_cold int ffat_usable_extradata(AVCodecContext *avctx)
{
    ATDecodeContext *at = avctx->priv_data;
    switch (avctx->codec_id) {
    case AV_CODEC_ID_ALAC:
    case AV_CODEC_ID_QDM2:
    case AV_CODEC_ID_QDMC:
        return at->extradata_size != 0;
    case AV_CODEC_ID_AAC:
        // The extra data is AudioSpecificConfig, see section 1.6.2.1 in
        // ISO14496-3-2009. It must be at least 2 bytes long.
        return at->extradata_size >= 2;
    default:
        return 0;
    }
}

static void ffat_finish_converter(AVCodecContext *avctx,
                                  AudioConverterRef* converter) {
    if (!*converter)
        return;
    OSStatus status = AudioConverterDispose(*converter);
    *converter = NULL;
    if (status != noErr) {
        av_log(avctx, AV_LOG_WARNING, "OSERROR %i\n", status);
    }
}

static av_cold int ffat_create_decoder(AVCodecContext *avctx,
                                       const AVPacket *pkt)
{
    int ret = 0;
    ATDecodeContext *at = avctx->priv_data;
    OSStatus status;
    int i;
    UInt32 cookie_size = 0;
    uint8_t *cookie = NULL;

    enum AVSampleFormat sample_fmt = (avctx->bits_per_raw_sample == 32) ?
                                     AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;

    AudioStreamBasicDescription in_format = {
        .mFormatID = ffat_get_format_id(avctx->codec_id, avctx->profile),
        .mBytesPerPacket = (avctx->codec_id == AV_CODEC_ID_ILBC) ? avctx->block_align : 0,
    };
    AudioStreamBasicDescription out_format = {
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mFramesPerPacket = 1,
        .mBitsPerChannel = av_get_bytes_per_sample(sample_fmt) * 8,
    };

    avctx->sample_fmt = sample_fmt;

    do {
        if (ffat_usable_extradata(avctx)) {
            UInt32 format_size = sizeof(in_format);
            cookie = ffat_get_magic_cookie(avctx, &cookie_size);
            if (!cookie) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }
            AudioStreamBasicDescription saved = in_format;
            status = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo,
                                            cookie_size, cookie, &format_size, &in_format);
            if (status != 0) {
                av_log(avctx, AV_LOG_ERROR, "OSERROR %i\n", status);
                ret = AVERROR_UNKNOWN;
                goto cleanup;
            }
            if (in_format.mFormatID)
                break;
            // At least on macOS 12 AudioFormatGetProperty() may clear
            // in_format.mFormatID while returning a success if the cookie is
            // valid but the format may not be fully supported. Fallback to the
            // general initialization code if so.
            in_format = saved;
        }

#if CONFIG_MP1_AT_DECODER || CONFIG_MP2_AT_DECODER || CONFIG_MP3_AT_DECODER
        if (pkt && pkt->size >= 4 &&
            (avctx->codec_id == AV_CODEC_ID_MP1 ||
                avctx->codec_id == AV_CODEC_ID_MP2 ||
                    avctx->codec_id == AV_CODEC_ID_MP3)) {
            enum AVCodecID codec_id;
            int bit_rate;
            if (ff_mpa_decode_header(AV_RB32(pkt->data), &avctx->sample_rate,
                                    &in_format.mChannelsPerFrame, &avctx->frame_size,
                                    &bit_rate, &codec_id) < 0) {
                ret = AVERROR_INVALIDDATA;
                goto cleanup;
            }
            avctx->bit_rate = bit_rate;
            in_format.mSampleRate = avctx->sample_rate;
            break;
    }
#endif
#if CONFIG_AC3_AT_DECODER || CONFIG_EAC3_AT_DECODER
        if (pkt && pkt->size >= 7 &&
            (avctx->codec_id == AV_CODEC_ID_AC3 ||
                avctx->codec_id == AV_CODEC_ID_EAC3)) {
            AC3HeaderInfo hdr;
            GetBitContext gbc;
            init_get_bits8(&gbc, pkt->data, pkt->size);
            if (ff_ac3_parse_header(&gbc, &hdr) < 0) {
                ret = AVERROR_INVALIDDATA;
                goto cleanup;
            }
            in_format.mSampleRate = hdr.sample_rate;
            in_format.mChannelsPerFrame = hdr.channels;
            avctx->frame_size = hdr.num_blocks * 256;
            avctx->bit_rate = hdr.bit_rate;
            break;
        }
#endif
#if FFAT_USE_FILE_STREAM_PARSE
        // Use detected format.
        if (at->format.mFormatID) {
            in_format = at->format;
            break;
        }
#endif
        in_format.mSampleRate = avctx->sample_rate ? avctx->sample_rate : 44100;
        in_format.mChannelsPerFrame = avctx->ch_layout.nb_channels ? avctx->ch_layout.nb_channels : 1;
    } while (0);

    avctx->sample_rate = out_format.mSampleRate = in_format.mSampleRate;
    av_channel_layout_uninit(&avctx->ch_layout);
    avctx->ch_layout.order       = AV_CHANNEL_ORDER_UNSPEC;
    avctx->ch_layout.nb_channels = out_format.mChannelsPerFrame = in_format.mChannelsPerFrame;

    if (avctx->codec_id == AV_CODEC_ID_ADPCM_IMA_QT)
        in_format.mFramesPerPacket = 64;

    // At least on macOS 12 AudioConverterNew() requires the following fields
    // in out_format to be filled with the correct values even if they are
    // trivially deducable from the other fields.
    out_format.mBytesPerFrame =
            out_format.mChannelsPerFrame * out_format.mBitsPerChannel / 8;
    out_format.mBytesPerPacket =
        out_format.mBytesPerFrame * out_format.mFramesPerPacket;

    status = AudioConverterNew(&in_format, &out_format, &at->converter);
    if (status != 0) {
        av_log(avctx, AV_LOG_ERROR, "OSERROR %i\n", status);
        ret = AVERROR_UNKNOWN;
        goto cleanup;
    }

    if (cookie_size) {
        status = AudioConverterSetProperty(at->converter,
                                           kAudioConverterDecompressionMagicCookie,
                                           cookie_size, cookie);
        if (status != 0) {
            av_log(avctx, AV_LOG_WARNING, "OSERROR %i\n", status);
            if (!pkt) {
                // Cookie is not yet usable, wait until we get a packet to try
                // again.
                ffat_finish_converter(avctx, &at->converter);
                goto cleanup;
            }
            // Ignore the error and hope for the best.
        }
    }

    for (i = 0; i < (sizeof(at->channel_map) / sizeof(at->channel_map[0])); i++)
        at->channel_map[i] = i;

    ffat_update_ctx(avctx);

    if(!(at->decoded_data = av_malloc(av_get_bytes_per_sample(avctx->sample_fmt)
                                      * avctx->frame_size * avctx->ch_layout.nb_channels))) {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    at->last_pts = AV_NOPTS_VALUE;

cleanup:
    if (cookie && cookie != at->extradata) {
        av_free(cookie);
    }
    return ret;
}

static av_cold int ffat_init_decoder(AVCodecContext *avctx)
{
    ATDecodeContext *at = avctx->priv_data;
    if (avctx->extradata_size) {
        int ret = ffviv_copy_data(avctx->extradata, avctx->extradata_size,
                                  &at->extradata, &at->extradata_size);
        if (ret < 0)
            return ret;
    }

    if ((avctx->ch_layout.nb_channels && avctx->sample_rate) || ffat_usable_extradata(avctx))
        return ffat_create_decoder(avctx, NULL);
    else
        return 0;
}

// AudioConverterFillComplexBuffer() propages non-zero status from the callback
// to the caller after draining internal buffers if any. So use 1 as an
// arbitrary marker to denote that we do not have more data to decode as advised
// by Apple docs.
#define  FFAT_NO_MORE_DATA_STATUS 1

static OSStatus ffat_decode_callback(AudioConverterRef converter, UInt32 *nb_packets,
                                     AudioBufferList *data,
                                     AudioStreamPacketDescription **packets,
                                     void *inctx)
{
    AVCodecContext *avctx = inctx;
    ATDecodeContext *at = avctx->priv_data;

    if (!at->in_pkt) {
        // The packet was already consumed.
        av_log(avctx, AV_LOG_DEBUG, "no more data\n");
        *nb_packets = 0;
        return FFAT_NO_MORE_DATA_STATUS;
    }

    if (packets) {
        at->pkt_desc.mDataByteSize = at->in_pkt->size;
        *packets = &at->pkt_desc;
    }

    if (at->in_pkt->size == 0) {
        // Signal EOF
        *nb_packets = 0;
    } else {
        data->mNumberBuffers              = 1;
        data->mBuffers[0].mNumberChannels = 0;
        data->mBuffers[0].mDataByteSize   = at->in_pkt->size;
        data->mBuffers[0].mData           = at->in_pkt->data;
        *nb_packets = 1;
        at->last_pts = at->in_pkt->pts;
    }
    at->in_pkt = NULL;

    return noErr;
}

#define COPY_SAMPLES(type) \
    type *in_ptr = (type*)at->decoded_data; \
    type *end_ptr = in_ptr + frame->nb_samples * avctx->ch_layout.nb_channels; \
    type *out_ptr = (type*)frame->data[0]; \
    for (; in_ptr < end_ptr; in_ptr += avctx->ch_layout.nb_channels, out_ptr += avctx->ch_layout.nb_channels) { \
        int c; \
        for (c = 0; c < avctx->ch_layout.nb_channels; c++) \
            out_ptr[c] = in_ptr[at->channel_map[c]]; \
    }

static void ffat_copy_samples(AVCodecContext *avctx, AVFrame *frame)
{
    ATDecodeContext *at = avctx->priv_data;
    if (avctx->sample_fmt == AV_SAMPLE_FMT_S32) {
        COPY_SAMPLES(int32_t);
    } else {
        COPY_SAMPLES(int16_t);
    }
}

#if FFAT_USE_FILE_STREAM_PARSE

void ffat_on_audio_property(void* inClientData,
                            AudioFileStreamID streamId,
                            AudioFileStreamPropertyID inPropertyID,
                            UInt32* ioFlags) {
    AVCodecContext *avctx = inClientData;
    ATDecodeContext *at = avctx->priv_data;
    AudioFormatListItem* format_list = NULL;

    if (inPropertyID != kAudioFileStreamProperty_FormatList)
        goto cleanup;

    UInt32 format_list_size = 0;
    OSStatus status = AudioFileStreamGetPropertyInfo(
        streamId, kAudioFileStreamProperty_FormatList, &format_list_size, NULL);
    if (status != noErr) {
        av_log(detection->avctx, AV_LOG_WARNING, "error: %d\n", status);
        goto cleanup;
    }
    if (format_list_size == 0 ||
        format_list_size % sizeof(AudioFormatListItem) != 0) {
        av_log(avctx, AV_LOG_WARNING,
            "unexpected list size: %u\n",format_list_size);
        goto cleanup;
    }
    format_list = av_malloc(format_list_size);
    if (!format_list) {
        goto cleanup;
    }
    UInt32 saved_format_list_size = format_list_size;
    status = AudioFileStreamGetPropertyInfo(
        streamId, kAudioFileStreamProperty_FormatList, &format_list_size,
        format_list);
    if (status != noErr) {
        av_log(avctx, AV_LOG_WARNING, "error: %d\n", status);
        goto cleanup;
    }
    if (format_list_size != saved_format_list_size) {
        av_log(avctx, AV_LOG_WARNING, "unexpected list size: %u\n",
            format_list_size);
        goto cleanup;
    }

    UInt32 format_index = 0;
    UInt32 format_index_size = sizeof(format_index);
    status = AudioFormatGetProperty(
        kAudioFormatProperty_FirstPlayableFormatFromList, format_list_size,
        format_list, &format_index_size, &format_index);
    if (status != noErr) {
        av_log(avctx, AV_LOG_WARNING, "error: %d\n", status);
        goto cleanup;
    }

    at->format = format_list[format_index].mASBD;

cleanup:
    av_free(format_list);
}

void ffat_on_audio_data(void* inClientData,
                           UInt32 inNumberBytes,
                           UInt32 inNumberPackets,
                           const void* inInputData,
                           AudioStreamPacketDescription* inPacketDescriptions) {
    // Do nothing as we parse the data separately.
}

static int ffat_detect_aac_input_format(AVCodecContext *avctx, AVPacket *avpkt) {
    int ret = 0;
    ATDecodeContext *at = avctx->priv_data;

    AudioFileStreamID streamId = NULL;
    OSStatus status = AudioFileStreamOpen(
        avctx, &ffat_on_audio_property, &ffat_on_audio_data,
        kAudioFileAAC_ADTSType, &streamId);
    if (status != noErr) {
        av_log(avctx, AV_LOG_WARNING, "AudioFileStreamOpen() error: %d\n",
            status);
        ret = AVERROR_UNKNOWN;
        goto cleanup;
    }
    status = AudioFileStreamParseBytes(streamId, avpkt->size, avpkt->data, 0);
    if (status != noErr) {
        av_log(avctx, AV_LOG_WARNING, "error: %d\n", status);
        ret = AVERROR_UNKNOWN;
        goto cleanup;
    }
    if (!at->format.mFormatID == 0) {
        av_log(avctx, AV_LOG_DEBUG, "Format was not detected\n");
        goto cleanup;
    }

  av_log(avctx, AV_LOG_DEBUG,
         "Detected format: id=%u sample_rate=%f flags=%u channels=%u bits=%u\n",
         at->format.mFormatID,
         at->format.mSampleRate,
         at->format.mFormatFlags,
         at->format.mChannelsPerFrame,
         at->format.mBitsPerChannel);

cleanup:
    if (streamId) {
      AudioFileStreamClose(streamId);
    }
    return ret;
}

#endif  // FFAT_USE_FILE_STREAM_PARSE

static int ffat_decode(AVCodecContext *avctx, void *data,
                       int *got_frame_ptr, AVPacket *avpkt)
{
    ATDecodeContext *at = avctx->priv_data;
    AVFrame *frame = data;
    int ret;
    AudioBufferList out_buffers;

    if (avctx->codec_id == AV_CODEC_ID_AAC) {
        ret = ffviv_check_adts_header(avctx, &at->adts_converter, avpkt->data,
                                      avpkt->size);
        if (ret < 0)
            return ret;
        if (at->adts_converter.header_size > 0) {
#if FFAT_USE_FILE_STREAM_PARSE
            if (!at->converter) {
                ffat_detect_aac_input_format(avctx, avpkt);
            }
#endif  // FFAT_USE_FILE_STREAM_PARSE
            avpkt->data += at->adts_converter.header_size;
            avpkt->size -= at->adts_converter.header_size;
        }
        if (!at->extradata_size || !at->converter) {
            if (at->adts_converter.header_size > 0) {
                ret = ffviv_convert_adts_to_aac_config(
                    avctx, &at->adts_converter, &at->extradata,
                    &at->extradata_size);
                if (ret < 0)
                    return ret;
            } else {
                uint8_t *side_data;
                size_t side_data_size;

                side_data = av_packet_get_side_data(avpkt,
                                                    AV_PKT_DATA_NEW_EXTRADATA,
                                                    &side_data_size);
                if (side_data_size) {
                    uint8_t* tmp = av_realloc(at->extradata, side_data_size);
                    if (!tmp)
                        return AVERROR(ENOMEM);
                    at->extradata = tmp;
                    at->extradata_size = side_data_size;
                    memcpy(at->extradata, side_data, side_data_size);
                }
            }
        }
    }

    if (!at->converter) {
        if ((ret = ffat_create_decoder(avctx, avpkt)) < 0) {
            return ret;
        }
    }

    out_buffers = (AudioBufferList){
        .mNumberBuffers = 1,
        .mBuffers = {
            {
                .mNumberChannels = avctx->ch_layout.nb_channels,
                .mDataByteSize = av_get_bytes_per_sample(avctx->sample_fmt) * avctx->frame_size
                                 * avctx->ch_layout.nb_channels,
            }
        }
    };

    if (avpkt->size == 0) {
        av_log(avctx, AV_LOG_INFO, "EOF\n");
    }

    frame->sample_rate = avctx->sample_rate;

    frame->nb_samples = avctx->frame_size;

    out_buffers.mBuffers[0].mData = at->decoded_data;

    at->in_pkt = avpkt;
    OSStatus status = AudioConverterFillComplexBuffer(
        at->converter, ffat_decode_callback, avctx, &frame->nb_samples,
        &out_buffers, NULL);
    int consumed_input = !at->in_pkt;
    at->in_pkt = NULL;

    if (status != noErr && status != FFAT_NO_MORE_DATA_STATUS) {
        // Log the error and mark the input as consumed so FFmpeg will not
        // resend the packet.
        av_log(avctx, AV_LOG_WARNING, "Decode OSERROR %d\n", status);
        consumed_input = 1;
    } else if (frame->nb_samples) {
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
            return ret;
        ffat_copy_samples(avctx, frame);
        *got_frame_ptr = 1;
        frame->pts = at->last_pts;
    }
    av_log(avctx, AV_LOG_DEBUG,
        "input_size=%d has_pts=%d consumed=%d decoded_samples=%d\n",
        avpkt->size, avpkt->pts != AV_NOPTS_VALUE, consumed_input,
        frame->nb_samples * avctx->ch_layout.nb_channels);

    return consumed_input ? avpkt->size : 0;
}

static av_cold void ffat_decode_flush(AVCodecContext *avctx)
{
    ATDecodeContext *at = avctx->priv_data;
    if (at->converter) {
        OSStatus status = AudioConverterReset(at->converter);
        if (status != noErr) {
            av_log(avctx, AV_LOG_ERROR, "OSERROR %d\n", status);
        }
    }
    at->last_pts = AV_NOPTS_VALUE;
    av_log(avctx, AV_LOG_INFO, "flushed\n");
}

static av_cold int ffat_close_decoder(AVCodecContext *avctx)
{
    ATDecodeContext *at = avctx->priv_data;

    ffat_finish_converter(avctx, &at->converter);
    av_freep(&at->decoded_data);
    av_freep(&at->extradata);
    ffviv_finish_adts_converter(&at->adts_converter);
    av_log(avctx, AV_LOG_INFO, "closed\n");

    return 0;
}

#define FFAT_DEC_CLASS(NAME) \
    static const AVClass ffat_##NAME##_dec_class = { \
        .class_name = "at_" #NAME "_dec", \
        .version    = LIBAVUTIL_VERSION_INT, \
    };

#define FFAT_DEC(NAME, ID, bsf_name) \
    FFAT_DEC_CLASS(NAME) \
    const FFCodec ff_##NAME##_at_decoder = { \
        .p.name         = #NAME "_at", \
        .p.long_name    = NULL_IF_CONFIG_SMALL(#NAME " (AudioToolbox)"), \
        .p.type         = AVMEDIA_TYPE_AUDIO, \
        .p.id           = ID, \
        .priv_data_size = sizeof(ATDecodeContext), \
        .init           = ffat_init_decoder, \
        .close          = ffat_close_decoder, \
        .cb.decode         = ffat_decode, \
        .flush          = ffat_decode_flush, \
        .p.priv_class   = &ffat_##NAME##_dec_class, \
        .bsfs           = bsf_name, \
        .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_CHANNEL_CONF, \
        .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP, \
        .p.wrapper_name = "at", \
    };

FFAT_DEC(aac,          AV_CODEC_ID_AAC, NULL)
FFAT_DEC(ac3,          AV_CODEC_ID_AC3, NULL)
FFAT_DEC(adpcm_ima_qt, AV_CODEC_ID_ADPCM_IMA_QT, NULL)
FFAT_DEC(alac,         AV_CODEC_ID_ALAC, NULL)
FFAT_DEC(amr_nb,       AV_CODEC_ID_AMR_NB, NULL)
FFAT_DEC(eac3,         AV_CODEC_ID_EAC3, NULL)
FFAT_DEC(gsm_ms,       AV_CODEC_ID_GSM_MS, NULL)
FFAT_DEC(ilbc,         AV_CODEC_ID_ILBC, NULL)
FFAT_DEC(mp1,          AV_CODEC_ID_MP1, NULL)
FFAT_DEC(mp2,          AV_CODEC_ID_MP2, NULL)
FFAT_DEC(mp3,          AV_CODEC_ID_MP3, NULL)
FFAT_DEC(pcm_alaw,     AV_CODEC_ID_PCM_ALAW, NULL)
FFAT_DEC(pcm_mulaw,    AV_CODEC_ID_PCM_MULAW, NULL)
FFAT_DEC(qdmc,         AV_CODEC_ID_QDMC, NULL)
FFAT_DEC(qdm2,         AV_CODEC_ID_QDM2, NULL)
