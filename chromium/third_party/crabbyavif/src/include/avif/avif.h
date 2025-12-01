#ifndef CRABBYAVIF_AVIF_H
#define CRABBYAVIF_AVIF_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

template <typename T>
using Box = T*;
namespace crabbyavif {
struct avifImage;
struct avifIO;

// Used to initialize avifROData/avifRWData on the stack.
#define AVIF_DATA_EMPTY { NULL, 0 }
}


namespace crabbyavif {

constexpr static const size_t CRABBY_AVIF_MAX_AV1_LAYER_COUNT = 4;

constexpr static const uint32_t CRABBY_AVIF_DEFAULT_IMAGE_SIZE_LIMIT = (16384 * 16384);

constexpr static const uint32_t CRABBY_AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT = 32768;

constexpr static const uint32_t CRABBY_AVIF_DEFAULT_IMAGE_COUNT_LIMIT = ((12 * 3600) * 60);

constexpr static const int CRABBY_AVIF_TRUE = 1;

constexpr static const int CRABBY_AVIF_FALSE = 0;

constexpr static const uint32_t AVIF_STRICT_DISABLED = 0;

constexpr static const uint32_t AVIF_STRICT_PIXI_REQUIRED = (1 << 0);

constexpr static const uint32_t AVIF_STRICT_CLAP_VALID = (1 << 1);

constexpr static const uint32_t AVIF_STRICT_ALPHA_ISPE_REQUIRED = (1 << 2);

constexpr static const uint32_t AVIF_STRICT_ENABLED = ((AVIF_STRICT_PIXI_REQUIRED | AVIF_STRICT_CLAP_VALID) | AVIF_STRICT_ALPHA_ISPE_REQUIRED);

constexpr static const uint32_t AVIF_IMAGE_CONTENT_NONE = 0;

constexpr static const uint32_t AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA = ((1 << 0) | (1 << 1));

constexpr static const uint32_t AVIF_IMAGE_CONTENT_GAIN_MAP = (1 << 2);

constexpr static const uint32_t AVIF_IMAGE_CONTENT_ALL = (AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA | AVIF_IMAGE_CONTENT_GAIN_MAP);

constexpr static const size_t CRABBY_AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE = 256;

constexpr static const size_t CRABBY_AVIF_PLANE_COUNT_YUV = 3;

constexpr static const int32_t CRABBY_AVIF_REPETITION_COUNT_INFINITE = -1;

constexpr static const int32_t CRABBY_AVIF_REPETITION_COUNT_UNKNOWN = -2;

constexpr static const uint32_t AVIF_TRANSFORM_NONE = 0;

constexpr static const uint32_t AVIF_TRANSFORM_PASP = (1 << 0);

constexpr static const uint32_t AVIF_TRANSFORM_CLAP = (1 << 1);

constexpr static const uint32_t AVIF_TRANSFORM_IROT = (1 << 2);

constexpr static const uint32_t AVIF_TRANSFORM_IMIR = (1 << 3);

constexpr static const uint16_t AVIF_COLOR_PRIMARIES_BT709 = 1;

constexpr static const uint16_t AVIF_COLOR_PRIMARIES_IEC61966_2_4 = 1;

constexpr static const uint16_t AVIF_COLOR_PRIMARIES_BT2100 = 9;

constexpr static const uint16_t AVIF_COLOR_PRIMARIES_DCI_P3 = 12;

constexpr static const uint16_t AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084 = 16;

constexpr static const uint32_t AVIF_ADD_IMAGE_FLAG_NONE = 0;

constexpr static const uint32_t AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME = (1 << 0);

constexpr static const uint32_t AVIF_ADD_IMAGE_FLAG_SINGLE = (1 << 1);

constexpr static const uint32_t AVIF_QUALITY_WORST = 0;

constexpr static const uint32_t AVIF_QUALITY_BEST = 100;

constexpr static const uint32_t AVIF_QUALITY_LOSSLESS = 100;

constexpr static const int32_t AVIF_QUALITY_DEFAULT = -1;

constexpr static const uint32_t AVIF_QUANTIZER_WORST_QUALITY = 63;

constexpr static const uint32_t AVIF_QUANTIZER_BEST_QUALITY = 0;

constexpr static const uint32_t AVIF_QUANTIZER_LOSSLESS = 0;

constexpr static const uint32_t AVIF_SPEED_SLOWEST = 0;

constexpr static const uint32_t AVIF_SPEED_FASTEST = 10;

constexpr static const uint32_t AVIF_SPEED_DEFAULT = 6;

enum AndroidMediaCodecOutputColorFormat : int32_t {
    ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_YUV420_FLEXIBLE = 2135033992,
    ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_P010 = 54,
};

enum avifChromaDownsampling {
    AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
    AVIF_CHROMA_DOWNSAMPLING_FASTEST,
    AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
    AVIF_CHROMA_DOWNSAMPLING_AVERAGE,
    AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV,
};

enum avifChromaSamplePosition {
    AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN = 0,
    AVIF_CHROMA_SAMPLE_POSITION_VERTICAL = 1,
    AVIF_CHROMA_SAMPLE_POSITION_COLOCATED = 2,
    AVIF_CHROMA_SAMPLE_POSITION_RESERVED = 3,
};

enum avifChromaUpsampling {
    AVIF_CHROMA_UPSAMPLING_AUTOMATIC,
    AVIF_CHROMA_UPSAMPLING_FASTEST,
    AVIF_CHROMA_UPSAMPLING_BEST_QUALITY,
    AVIF_CHROMA_UPSAMPLING_NEAREST,
    AVIF_CHROMA_UPSAMPLING_BILINEAR,
};

enum avifColorPrimaries : uint16_t {
    AVIF_COLOR_PRIMARIES_UNKNOWN = 0,
    AVIF_COLOR_PRIMARIES_SRGB = 1,
    AVIF_COLOR_PRIMARIES_UNSPECIFIED = 2,
    AVIF_COLOR_PRIMARIES_BT470M = 4,
    AVIF_COLOR_PRIMARIES_BT470BG = 5,
    AVIF_COLOR_PRIMARIES_BT601 = 6,
    AVIF_COLOR_PRIMARIES_SMPTE240 = 7,
    AVIF_COLOR_PRIMARIES_GENERIC_FILM = 8,
    AVIF_COLOR_PRIMARIES_BT2020 = 9,
    AVIF_COLOR_PRIMARIES_XYZ = 10,
    AVIF_COLOR_PRIMARIES_SMPTE431 = 11,
    AVIF_COLOR_PRIMARIES_SMPTE432 = 12,
    AVIF_COLOR_PRIMARIES_EBU3213 = 22,
};

enum CompressionFormat {
    COMPRESSION_FORMAT_AVIF = 0,
    COMPRESSION_FORMAT_HEIC = 1,
};

enum avifRGBFormat {
    AVIF_RGB_FORMAT_RGB,
    AVIF_RGB_FORMAT_RGBA,
    AVIF_RGB_FORMAT_ARGB,
    AVIF_RGB_FORMAT_BGR,
    AVIF_RGB_FORMAT_BGRA,
    AVIF_RGB_FORMAT_ABGR,
    AVIF_RGB_FORMAT_RGB565,
    AVIF_RGB_FORMAT_RGBA1010102,
};

enum avifMatrixCoefficients : uint16_t {
    AVIF_MATRIX_COEFFICIENTS_IDENTITY = 0,
    AVIF_MATRIX_COEFFICIENTS_BT709 = 1,
    AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED = 2,
    AVIF_MATRIX_COEFFICIENTS_RESERVED = 3,
    AVIF_MATRIX_COEFFICIENTS_FCC = 4,
    AVIF_MATRIX_COEFFICIENTS_BT470BG = 5,
    AVIF_MATRIX_COEFFICIENTS_BT601 = 6,
    AVIF_MATRIX_COEFFICIENTS_SMPTE240 = 7,
    AVIF_MATRIX_COEFFICIENTS_YCGCO = 8,
    AVIF_MATRIX_COEFFICIENTS_BT2020_NCL = 9,
    AVIF_MATRIX_COEFFICIENTS_BT2020_CL = 10,
    AVIF_MATRIX_COEFFICIENTS_SMPTE2085 = 11,
    AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL = 12,
    AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL = 13,
    AVIF_MATRIX_COEFFICIENTS_ICTCP = 14,
    AVIF_MATRIX_COEFFICIENTS_YCGCO_RE = 16,
    AVIF_MATRIX_COEFFICIENTS_YCGCO_RO = 17,
};

enum avifPixelFormat {
    AVIF_PIXEL_FORMAT_NONE = 0,
    AVIF_PIXEL_FORMAT_YUV444 = 1,
    AVIF_PIXEL_FORMAT_YUV422 = 2,
    AVIF_PIXEL_FORMAT_YUV420 = 3,
    AVIF_PIXEL_FORMAT_YUV400 = 4,
    AVIF_PIXEL_FORMAT_ANDROID_P010 = 5,
    AVIF_PIXEL_FORMAT_ANDROID_NV12 = 6,
    AVIF_PIXEL_FORMAT_ANDROID_NV21 = 7,
    AVIF_PIXEL_FORMAT_COUNT,
};

enum avifProgressiveState {
    AVIF_PROGRESSIVE_STATE_UNAVAILABLE = 0,
    AVIF_PROGRESSIVE_STATE_AVAILABLE = 1,
    AVIF_PROGRESSIVE_STATE_ACTIVE = 2,
};

enum avifDecoderSource {
    AVIF_DECODER_SOURCE_AUTO = 0,
    AVIF_DECODER_SOURCE_PRIMARY_ITEM = 1,
    AVIF_DECODER_SOURCE_TRACKS = 2,
};

enum avifTransferCharacteristics : uint16_t {
    AVIF_TRANSFER_CHARACTERISTICS_UNKNOWN = 0,
    AVIF_TRANSFER_CHARACTERISTICS_BT709 = 1,
    AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED = 2,
    AVIF_TRANSFER_CHARACTERISTICS_RESERVED = 3,
    AVIF_TRANSFER_CHARACTERISTICS_BT470M = 4,
    AVIF_TRANSFER_CHARACTERISTICS_BT470BG = 5,
    AVIF_TRANSFER_CHARACTERISTICS_BT601 = 6,
    AVIF_TRANSFER_CHARACTERISTICS_SMPTE240 = 7,
    AVIF_TRANSFER_CHARACTERISTICS_LINEAR = 8,
    AVIF_TRANSFER_CHARACTERISTICS_LOG100 = 9,
    AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10 = 10,
    AVIF_TRANSFER_CHARACTERISTICS_IEC61966 = 11,
    AVIF_TRANSFER_CHARACTERISTICS_BT1361 = 12,
    AVIF_TRANSFER_CHARACTERISTICS_SRGB = 13,
    AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT = 14,
    AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT = 15,
    AVIF_TRANSFER_CHARACTERISTICS_PQ = 16,
    AVIF_TRANSFER_CHARACTERISTICS_SMPTE428 = 17,
    AVIF_TRANSFER_CHARACTERISTICS_HLG = 18,
};

enum avifRange {
    AVIF_RANGE_LIMITED = 0,
    AVIF_RANGE_FULL = 1,
};

enum avifChannelIndex {
    AVIF_CHAN_Y = 0,
    AVIF_CHAN_U = 1,
    AVIF_CHAN_V = 2,
    AVIF_CHAN_A = 3,
};

enum avifCodecChoice {
    AVIF_CODEC_CHOICE_AUTO = 0,
    AVIF_CODEC_CHOICE_AOM = 1,
    AVIF_CODEC_CHOICE_DAV1D = 2,
    AVIF_CODEC_CHOICE_LIBGAV1 = 3,
    AVIF_CODEC_CHOICE_RAV1E = 4,
    AVIF_CODEC_CHOICE_SVT = 5,
    AVIF_CODEC_CHOICE_AVM = 6,
};

enum avifCodecFlag {
    AVIF_CODEC_FLAG_CAN_DECODE = (1 << 0),
    AVIF_CODEC_FLAG_CAN_ENCODE = (1 << 1),
};

enum avifHeaderFormat {
    AVIF_HEADER_FULL,
    AVIF_HEADER_REDUCED,
};

enum avifPlanesFlag {
    AVIF_PLANES_YUV = (1 << 0),
    AVIF_PLANES_A = (1 << 1),
    AVIF_PLANES_ALL = 255,
};

enum avifResult {
    AVIF_RESULT_OK = 0,
    AVIF_RESULT_UNKNOWN_ERROR = 1,
    AVIF_RESULT_INVALID_FTYP = 2,
    AVIF_RESULT_NO_CONTENT = 3,
    AVIF_RESULT_NO_YUV_FORMAT_SELECTED = 4,
    AVIF_RESULT_REFORMAT_FAILED = 5,
    AVIF_RESULT_UNSUPPORTED_DEPTH = 6,
    AVIF_RESULT_ENCODE_COLOR_FAILED = 7,
    AVIF_RESULT_ENCODE_ALPHA_FAILED = 8,
    AVIF_RESULT_BMFF_PARSE_FAILED = 9,
    AVIF_RESULT_MISSING_IMAGE_ITEM = 10,
    AVIF_RESULT_DECODE_COLOR_FAILED = 11,
    AVIF_RESULT_DECODE_ALPHA_FAILED = 12,
    AVIF_RESULT_COLOR_ALPHA_SIZE_MISMATCH = 13,
    AVIF_RESULT_ISPE_SIZE_MISMATCH = 14,
    AVIF_RESULT_NO_CODEC_AVAILABLE = 15,
    AVIF_RESULT_NO_IMAGES_REMAINING = 16,
    AVIF_RESULT_INVALID_EXIF_PAYLOAD = 17,
    AVIF_RESULT_INVALID_IMAGE_GRID = 18,
    AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION = 19,
    AVIF_RESULT_TRUNCATED_DATA = 20,
    AVIF_RESULT_IO_NOT_SET = 21,
    AVIF_RESULT_IO_ERROR = 22,
    AVIF_RESULT_WAITING_ON_IO = 23,
    AVIF_RESULT_INVALID_ARGUMENT = 24,
    AVIF_RESULT_NOT_IMPLEMENTED = 25,
    AVIF_RESULT_OUT_OF_MEMORY = 26,
    AVIF_RESULT_CANNOT_CHANGE_SETTING = 27,
    AVIF_RESULT_INCOMPATIBLE_IMAGE = 28,
    AVIF_RESULT_ENCODE_GAIN_MAP_FAILED = 29,
    AVIF_RESULT_DECODE_GAIN_MAP_FAILED = 30,
    AVIF_RESULT_INVALID_TONE_MAPPED_IMAGE = 31,
};

struct CodecSpecificOptions;

struct Decoder;

struct Encoder;

using avifBool = int;

using avifStrictFlags = uint32_t;

struct avifRWData {
    uint8_t *data;
    size_t size;
};

struct ContentLightLevelInformation {
    uint16_t maxCLL;
    uint16_t maxPALL;
};

using avifContentLightLevelInformationBox = ContentLightLevelInformation;

using avifTransformFlags = uint32_t;

struct PixelAspectRatio {
    uint32_t hSpacing;
    uint32_t vSpacing;
};

using avifPixelAspectRatioBox = PixelAspectRatio;

struct avifCleanApertureBox {
    uint32_t widthN;
    uint32_t widthD;
    uint32_t heightN;
    uint32_t heightD;
    uint32_t horizOffN;
    uint32_t horizOffD;
    uint32_t vertOffN;
    uint32_t vertOffD;
};

struct avifImageRotation {
    uint8_t angle;
};

struct avifImageMirror {
    uint8_t axis;
};

struct Fraction {
    int32_t n;
    uint32_t d;
};

using avifSignedFraction = Fraction;

struct UFraction {
    uint32_t n;
    uint32_t d;
};

using avifUnsignedFraction = UFraction;

struct avifGainMap {
    avifImage *image;
    avifSignedFraction gainMapMin[3];
    avifSignedFraction gainMapMax[3];
    avifUnsignedFraction gainMapGamma[3];
    avifSignedFraction baseOffset[3];
    avifSignedFraction alternateOffset[3];
    avifUnsignedFraction baseHdrHeadroom;
    avifUnsignedFraction alternateHdrHeadroom;
    avifBool useBaseColorSpace;
    avifRWData altICC;
    avifColorPrimaries altColorPrimaries;
    avifTransferCharacteristics altTransferCharacteristics;
    avifMatrixCoefficients altMatrixCoefficients;
    avifRange altYUVRange;
    uint32_t altDepth;
    uint32_t altPlaneCount;
    avifContentLightLevelInformationBox altCLLI;
};

struct avifImage {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    avifPixelFormat yuvFormat;
    avifRange yuvRange;
    avifChromaSamplePosition yuvChromaSamplePosition;
    uint8_t *yuvPlanes[CRABBY_AVIF_PLANE_COUNT_YUV];
    uint32_t yuvRowBytes[CRABBY_AVIF_PLANE_COUNT_YUV];
    avifBool imageOwnsYUVPlanes;
    uint8_t *alphaPlane;
    uint32_t alphaRowBytes;
    avifBool imageOwnsAlphaPlane;
    avifBool alphaPremultiplied;
    avifRWData icc;
    avifColorPrimaries colorPrimaries;
    avifTransferCharacteristics transferCharacteristics;
    avifMatrixCoefficients matrixCoefficients;
    avifContentLightLevelInformationBox clli;
    avifTransformFlags transformFlags;
    avifPixelAspectRatioBox pasp;
    avifCleanApertureBox clap;
    avifImageRotation irot;
    avifImageMirror imir;
    avifRWData exif;
    avifRWData xmp;
    avifGainMap *gainMap;
};

struct avifImageTiming {
    uint64_t timescale;
    double pts;
    uint64_t ptsInTimescales;
    double duration;
    uint64_t durationInTimescales;
};

struct avifIOStats {
    size_t colorOBUSize;
    size_t alphaOBUSize;
};

struct avifDiagnostics {
    char error[CRABBY_AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE];
};

struct avifDecoderData {

};

using avifImageContentTypeFlags = uint32_t;

struct avifDecoder {
    avifCodecChoice codecChoice;
    int32_t maxThreads;
    avifDecoderSource requestedSource;
    avifBool allowProgressive;
    avifBool allowIncremental;
    avifBool ignoreExif;
    avifBool ignoreXMP;
    uint32_t imageSizeLimit;
    uint32_t imageDimensionLimit;
    uint32_t imageCountLimit;
    avifStrictFlags strictFlags;
    avifImage *image;
    int32_t imageIndex;
    int32_t imageCount;
    avifProgressiveState progressiveState;
    avifImageTiming imageTiming;
    uint64_t timescale;
    double duration;
    uint64_t durationInTimescales;
    int32_t repetitionCount;
    avifBool alphaPresent;
    avifIOStats ioStats;
    avifDiagnostics diag;
    avifDecoderData *data;
    avifImageContentTypeFlags imageContentToDecode;
    avifBool imageSequenceTrackPresent;
    AndroidMediaCodecOutputColorFormat androidMediaCodecOutputColorFormat;
    CompressionFormat compressionFormat;
    Box<Decoder> rust_decoder;
    avifImage image_object;
    avifGainMap gainmap_object;
    avifImage gainmap_image_object;
};

using avifIODestroyFunc = void(*)(avifIO *io);

struct avifROData {
    const uint8_t *data;
    size_t size;
};

using avifIOReadFunc = avifResult(*)(avifIO *io,
                                     uint32_t readFlags,
                                     uint64_t offset,
                                     size_t size,
                                     avifROData *out);

using avifIOWriteFunc = avifResult(*)(avifIO *io,
                                      uint32_t writeFlags,
                                      uint64_t offset,
                                      const uint8_t *data,
                                      size_t size);

struct avifIO {
    avifIODestroyFunc destroy;
    avifIOReadFunc read;
    avifIOWriteFunc write;
    uint64_t sizeHint;
    avifBool persistent;
    void *data;
};

struct Extent {
    uint64_t offset;
    size_t size;
};

using avifExtent = Extent;

struct IFraction {
    int32_t n;
    int32_t d;
};

struct avifScalingMode {
    IFraction horizontal;
    IFraction vertical;
};

struct avifEncoder {
    avifCodecChoice codecChoice;
    int32_t maxThreads;
    int32_t speed;
    int32_t keyframeInterval;
    uint64_t timescale;
    int32_t repetitionCount;
    uint32_t extraLayerCount;
    int32_t quality;
    int32_t qualityAlpha;
    int32_t minQuantizer;
    int32_t maxQuantizer;
    int32_t minQuantizerAlpha;
    int32_t maxQuantizerAlpha;
    int32_t tileRowsLog2;
    int32_t tileColsLog2;
    avifBool autoTiling;
    avifScalingMode scalingMode;
    avifIOStats ioStats;
    avifDiagnostics diag;
    int32_t qualityGainMap;
    Box<Encoder> rust_encoder;
    bool rust_encoder_initialized;
    Box<CodecSpecificOptions> codec_specific_options;
};

using avifAddImageFlags = uint32_t;

using avifPlanesFlags = uint32_t;

struct CropRect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

using avifCropRect = CropRect;

struct avifRGBImage {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    avifRGBFormat format;
    avifChromaUpsampling chromaUpsampling;
    avifChromaDownsampling chromaDownsampling;
    bool ignoreAlpha;
    bool alphaPremultiplied;
    bool isFloat;
    int32_t maxThreads;
    uint8_t *pixels;
    uint32_t rowBytes;
};

using avifCodecFlags = uint32_t;

struct avifPixelFormatInfo {
    avifBool monochrome;
    int chromaShiftX;
    int chromaShiftY;
};











extern "C" {

/// # Safety
/// Used by the C API to create an avifDecoder object with default values.
avifDecoder *crabby_avifDecoderCreate();

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if io is not null, it has to point to a valid avifIO object.
void crabby_avifDecoderSetIO(avifDecoder *decoder, avifIO *io);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if filename is not null, it has to point to a valid C-style string.
avifResult crabby_avifDecoderSetIOFile(avifDecoder *decoder, const char *filename);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if data is not null, it has to be a valid buffer of size bytes.
avifResult crabby_avifDecoderSetIOMemory(avifDecoder *decoder, const uint8_t *data, size_t size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifResult crabby_avifDecoderSetSource(avifDecoder *decoder, avifDecoderSource source);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifResult crabby_avifDecoderParse(avifDecoder *decoder);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifResult crabby_avifDecoderNextImage(avifDecoder *decoder);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifResult crabby_avifDecoderNthImage(avifDecoder *decoder, uint32_t frameIndex);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if outTiming is not null, it has to point to a valid ImageTiming object.
avifResult crabby_avifDecoderNthImageTiming(const avifDecoder *decoder,
                                            uint32_t frameIndex,
                                            avifImageTiming *outTiming);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
void crabby_avifDecoderDestroy(avifDecoder *decoder);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if image is not null, it has to point to a valid avifImage object.
avifResult crabby_avifDecoderRead(avifDecoder *decoder, avifImage *image);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if image is not null, it has to point to a valid avifImage object.
/// - if data is not null, it has to be a valid buffer of size bytes.
avifResult crabby_avifDecoderReadMemory(avifDecoder *decoder,
                                        avifImage *image,
                                        const uint8_t *data,
                                        size_t size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if image is not null, it has to point to a valid avifImage object.
/// - if filename is not null, it has to point to a valid C-style string.
avifResult crabby_avifDecoderReadFile(avifDecoder *decoder, avifImage *image, const char *filename);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifBool crabby_avifDecoderIsKeyframe(const avifDecoder *decoder, uint32_t frameIndex);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
uint32_t crabby_avifDecoderNearestKeyframe(const avifDecoder *decoder, uint32_t frameIndex);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
uint32_t crabby_avifDecoderDecodedRowCount(const avifDecoder *decoder);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
/// - if outExtent is not null, it has to point to a valid avifExtent object.
avifResult crabby_avifDecoderNthImageMaxExtent(const avifDecoder *decoder,
                                               uint32_t frameIndex,
                                               avifExtent *outExtent);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if input is not null, it has to point to a valid avifROData object.
avifBool crabby_avifPeekCompatibleFileType(const avifROData *input);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if decoder is not null, it has to point to a valid avifDecoder object.
avifResult crabby_avifDecoderReset(avifDecoder *decoder);

/// # Safety
/// Used by the C API to create an avifEncoder object with default values.
avifEncoder *crabby_avifEncoderCreate();

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
void crabby_avifEncoderDestroy(avifEncoder *encoder);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if image is not null, it has to point to a valid avifImage object.
/// - if output is not null, it has to point to a valid avifRWData object.
avifResult crabby_avifEncoderWrite(avifEncoder *encoder,
                                   const avifImage *image,
                                   avifRWData *output);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if image is not null, it has to point to a valid avifImage object.
avifResult crabby_avifEncoderAddImage(avifEncoder *encoder,
                                      const avifImage *image,
                                      uint64_t durationInTimescales,
                                      avifAddImageFlags addImageFlags);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if cellImages is not null, it has to point to valid array of avifImage objects of size
///   |gridCols * gridRows|.
avifResult crabby_avifEncoderAddImageGrid(avifEncoder *encoder,
                                          uint32_t gridCols,
                                          uint32_t gridRows,
                                          const avifImage *const *cellImages,
                                          avifAddImageFlags addImageFlags);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if output is not null, it has to point to a valid avifRWData object.
avifResult crabby_avifEncoderFinish(avifEncoder *encoder, avifRWData *output);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if encoder is not null, it has to point to a valid avifEncoder object.
/// - if key is not null, it has to point to a valid C-style string.
/// - if value is not null, it has to point to a valid C-style string.
avifResult crabby_avifEncoderSetCodecSpecificOption(avifEncoder *encoder,
                                                    const char *key,
                                                    const char *value);

/// # Safety
/// C API function that does not perform any unsafe operation.
avifGainMap *crabby_avifGainMapCreate();

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if gainmap is not null, it has to point to a buffer allocated by crabby_avifGainMapCreate.
void crabby_avifGainMapDestroy(avifGainMap *gainmap);

/// # Safety
/// Used by the C API to create an avifImage object with default values.
avifImage *crabby_avifImageCreateEmpty();

/// # Safety
/// Used by the C API to create an avifImage object with the given parameters.
avifImage *crabby_avifImageCreate(uint32_t width,
                                  uint32_t height,
                                  uint32_t depth,
                                  avifPixelFormat yuvFormat);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if dstImage is not null, it has to point to a valid avifImage object.
/// - if srcImage is not null, it has to point to a valid avifImage object.
avifResult crabby_avifImageCopy(avifImage *dstImage,
                                const avifImage *srcImage,
                                avifPlanesFlags planes);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
avifResult crabby_avifImageAllocatePlanes(avifImage *image, avifPlanesFlags planes);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
void crabby_avifImageFreePlanes(avifImage *image, avifPlanesFlags planes);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
void crabby_avifImageDestroy(avifImage *image);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
avifBool crabby_avifImageUsesU16(const avifImage *image);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
avifBool crabby_avifImageIsOpaque(const avifImage *image);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
uint8_t *crabby_avifImagePlane(const avifImage *image, int channel);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
uint32_t crabby_avifImagePlaneRowBytes(const avifImage *image, int channel);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
uint32_t crabby_avifImagePlaneWidth(const avifImage *image, int channel);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
uint32_t crabby_avifImagePlaneHeight(const avifImage *image, int channel);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if dstImage is not null, it has to point to a valid avifImage object.
/// - if srcImage is not null, it has to point to a valid avifImage object.
/// - if rect is not null, it has to point to a valid avifCropRect object.
avifResult crabby_avifImageSetViewRect(avifImage *dstImage,
                                       const avifImage *srcImage,
                                       const avifCropRect *rect);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if exif is not null, it has to point to a valid buffer of size exifSize bytes.
avifResult crabby_avifImageSetMetadataExif(avifImage *image, const uint8_t *exif, size_t exifSize);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if xmp is not null, it has to point to a valid buffer of size xmpSize bytes.
avifResult crabby_avifImageSetMetadataXMP(avifImage *image, const uint8_t *xmp, size_t xmpSize);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if icc is not null, it has to point to a valid buffer of size iccSize bytes.
avifResult crabby_avifImageSetProfileICC(avifImage *image, const uint8_t *icc, size_t iccSize);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
avifResult crabby_avifRWDataRealloc(avifRWData *raw, size_t newSize);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
/// - if data is not null, it has to point to a valid buffer of size bytes.
avifResult crabby_avifRWDataSet(avifRWData *raw, const uint8_t *data, size_t size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if raw is not null, it has to point to a valid avifRWData object.
void crabby_avifRWDataFree(avifRWData *raw);

/// # Safety
/// Unused C API function.
void cioDestroy(avifIO *_io);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if io is not null, it has to point to a valid avifIO object.
/// - if out is not null, it has to point to a valid avifROData object.
avifResult cioRead(avifIO *io, uint32_t _readFlags, uint64_t offset, size_t size, avifROData *out);

/// # Safety
/// Unused C API function.
avifResult cioWrite(avifIO *_io,
                    uint32_t _writeFlags,
                    uint64_t _offset,
                    const uint8_t *_data,
                    size_t _size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if data is not null, it has to be a valid buffer of size bytes.
avifIO *crabby_avifIOCreateMemoryReader(const uint8_t *data, size_t size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if filename is not null, it has to be a valid C-style string.
avifIO *crabby_avifIOCreateFileReader(const char *filename);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if io is not null, it has to point to a valid avifIO object.
void crabby_avifIODestroy(avifIO *io);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
/// - if image is not null, it has to point to a valid avifImage object.
void crabby_avifRGBImageSetDefaults(avifRGBImage *rgb, const avifImage *image);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
avifResult crabby_avifImageYUVToRGB(const avifImage *image, avifRGBImage *rgb);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
avifResult crabby_avifImageRGBToYUV(avifImage *image, const avifRGBImage *rgb);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if image is not null, it has to point to a valid avifImage object.
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
avifResult crabby_avifImageScale(avifImage *image,
                                 uint32_t dstWidth,
                                 uint32_t dstHeight,
                                 avifDiagnostics *_diag);

/// # Safety
/// C API function that does not do any unsafe operations.
uint32_t crabby_avifRGBFormatChannelCount(avifRGBFormat format);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
uint32_t crabby_avifRGBImagePixelSize(avifRGBImage *rgb);

/// # Safety
/// C API function that does not do any unsafe operations.
avifBool crabby_avifRGBFormatHasAlpha(avifRGBFormat format);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
avifResult crabby_avifRGBImageAllocatePixels(avifRGBImage *rgb);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if rgb is not null, it has to point to a valid avifRGBImage object.
void crabby_avifRGBImageFreePixels(avifRGBImage *rgb);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if name is not null, it has to be a valid C-style string.
avifCodecChoice crabby_avifCodecChoiceFromName(const char *name);

/// # Safety
/// C API function that does not do any unsafe operations.
const char *crabby_avifCodecName(avifCodecChoice _choice, avifCodecFlags requiredFlags);

/// # Safety
/// C API function that does not do any unsafe operations.
const char *crabby_avifPixelFormatToString(avifPixelFormat format);

/// # Safety
/// C API function.
const char *crabby_avifResultToString(avifResult res);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if cropRect is not null, it has to point to a valid avifCropRect object.
/// - if clap is not null, it has to point to a valid avifCleanApertureBox object.
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
avifBool crabby_avifCropRectConvertCleanApertureBox(avifCropRect *cropRect,
                                                    const avifCleanApertureBox *clap,
                                                    uint32_t imageW,
                                                    uint32_t imageH,
                                                    avifPixelFormat yuvFormat,
                                                    avifDiagnostics *_diag);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if cropRect is not null, it has to point to a valid avifCropRect object.
/// - if clap is not null, it has to point to a valid avifCleanApertureBox object.
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
avifBool crabby_avifCleanApertureBoxConvertCropRect(avifCleanApertureBox *clap,
                                                    const avifCropRect *cropRect,
                                                    uint32_t imageW,
                                                    uint32_t imageH,
                                                    avifPixelFormat yuvFormat,
                                                    avifDiagnostics *_diag);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if info is not null, it has to point to a valid avifPixelFormatInfo object.
void crabby_avifGetPixelFormatInfo(avifPixelFormat format, avifPixelFormatInfo *info);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if diag is not null, it has to point to a valid avifDiagnostics object.
void crabby_avifDiagnosticsClearError(avifDiagnostics *diag);

/// # Safety
/// C API function that does not perform any unsafe operation.
void *crabby_avifAlloc(size_t size);

/// # Safety
/// Used by the C API with the following pre-conditions:
/// - if p is not null, it has to point to a buffer allocated by crabby_avifAlloc.
void crabby_avifFree(void *p);

} // extern "C"

} // namespace crabbyavif

#endif // CRABBYAVIF_AVIF_H

#ifndef CRABBYAVIF_AVIF_CXX_H
#define CRABBYAVIF_AVIF_CXX_H

#include <memory>

namespace crabbyavif {

struct UniquePtrDeleter {
    void operator()(avifEncoder * encoder) const { crabby_avifEncoderDestroy(encoder); }
    void operator()(avifDecoder * decoder) const { crabby_avifDecoderDestroy(decoder); }
    void operator()(avifImage * image) const { crabby_avifImageDestroy(image); }
    void operator()(avifGainMap * gainMap) const { crabby_avifGainMapDestroy(gainMap); }
};

using EncoderPtr = std::unique_ptr<avifEncoder, UniquePtrDeleter>;
using DecoderPtr = std::unique_ptr<avifDecoder, UniquePtrDeleter>;
using ImagePtr = std::unique_ptr<avifImage, UniquePtrDeleter>;
using GainMapPtr = std::unique_ptr<avifGainMap, UniquePtrDeleter>;

} // namespace crabbyavif

#endif // CRABBYAVIF_AVIF_CXX_H
