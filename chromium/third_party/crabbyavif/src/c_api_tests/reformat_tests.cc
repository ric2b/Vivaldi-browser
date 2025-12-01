// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>

#include "avif/avif.h"
#include "gtest/gtest.h"
#include "testutil.h"

using testing::Bool;
using testing::Combine;
using testing::Values;

namespace avif {
namespace {

constexpr uint8_t kWidth = 4;
constexpr uint8_t kHeight = 4;
constexpr uint8_t kPlaneSize = 16;
constexpr uint8_t kUOffset = 16;
constexpr uint8_t kVOffset = 32;
constexpr uint8_t kYuv[][kPlaneSize * 4] = {
    // White
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
     0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
     0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
    // Red
    {0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c,
     0x4c, 0x4c, 0x4c, 0x4c, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
     0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    // Mixed
    {0x88, 0x88, 0x88, 0x88, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c,
     0x88, 0x88, 0x88, 0x88, 0xa4, 0xa4, 0xa4, 0xa4, 0x72, 0x72, 0x72, 0x72,
     0x72, 0x72, 0x72, 0x72, 0xa4, 0xa4, 0xa4, 0xa4, 0x7a, 0x7a, 0x7a, 0x7a,
     0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0x7a, 0x7a, 0x7a, 0x7a}};

constexpr uint8_t kRgb[][kWidth * kHeight * 4] = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
     0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00,
     0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
     0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
     0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
     0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff},
    {0x80, 0x80, 0xc8, 0xff, 0x80, 0x80, 0xc8, 0xff, 0x80, 0x80, 0xc8,
     0xff, 0x80, 0x80, 0xc8, 0xff, 0xe5, 0x4b, 0x63, 0xff, 0xe5, 0x4b,
     0x63, 0xff, 0xe5, 0x4b, 0x63, 0xff, 0xe5, 0x4b, 0x63, 0xff, 0xe5,
     0x4b, 0x63, 0xff, 0xe5, 0x4b, 0x63, 0xff, 0xe5, 0x4b, 0x63, 0xff,
     0xe5, 0x4b, 0x63, 0xff, 0x80, 0x80, 0xc8, 0xff, 0x80, 0x80, 0xc8,
     0xff, 0x80, 0x80, 0xc8, 0xff, 0x80, 0x80, 0xc8, 0xff}};

TEST(ReformatTest, YUVToRGBConversion) {
  for (int p = 0; p < 3; ++p) {
    ImagePtr image(
        avifImageCreate(kWidth, kHeight, 8, AVIF_PIXEL_FORMAT_YUV444));
    ASSERT_NE(image, nullptr);
    ASSERT_EQ(avifImageAllocatePlanes(image.get(), AVIF_PLANES_YUV),
              AVIF_RESULT_OK);
    memcpy(image->yuvPlanes[0], kYuv[p], kPlaneSize);
    memcpy(image->yuvPlanes[1], kYuv[p] + kUOffset, kPlaneSize);
    memcpy(image->yuvPlanes[2], kYuv[p] + kVOffset, kPlaneSize);
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image.get());
    std::vector<uint8_t> rgb_pixels(kWidth * kHeight * 4);
    rgb.pixels = rgb_pixels.data();
    rgb.rowBytes = kWidth * 4;
    ASSERT_EQ(avifImageYUVToRGB(image.get(), &rgb), AVIF_RESULT_OK);
    for (int i = 0; i < rgb_pixels.size(); ++i) {
      EXPECT_EQ(rgb.pixels[i], kRgb[p][i]);
    }
    avifImageFreePlanes(image.get(), AVIF_PLANES_YUV);
  }
}

constexpr uint8_t kRedNoise[testutil::kModifierSize] = {
    7,  14, 11, 5,   // Random permutation of 16 values.
    4,  6,  8,  15,  //
    2,  9,  13, 3,   //
    12, 1,  10, 0};
constexpr uint8_t kGreenNoise[testutil::kModifierSize] = {
    3,  2,  12, 15,  // Random permutation of 16 values
    14, 10, 7,  13,  // that is somewhat close to kRedNoise.
    5,  1,  9,  0,   //
    8,  4,  11, 6};
constexpr uint8_t kBlueNoise[testutil::kModifierSize] = {
    0,  8,  14, 9,   // Random permutation of 16 values
    13, 12, 2,  7,   // that is somewhat close to kGreenNoise.
    3,  1,  11, 10,  //
    6,  15, 5,  4};

// Accumulates stats about the differences between the images a and b.
template <typename PixelType>
void GetDiffSumAndSqDiffSum(const avifRGBImage& a, const avifRGBImage& b,
                            int64_t* abs_diff_sum, int64_t* sq_diff_sum,
                            int64_t* max_abs_diff) {
  const uint32_t channel_count = avifRGBFormatChannelCount(a.format);
  for (uint32_t y = 0; y < a.height; ++y) {
    const PixelType* row_a =
        reinterpret_cast<PixelType*>(a.pixels + a.rowBytes * y);
    const PixelType* row_b =
        reinterpret_cast<PixelType*>(b.pixels + b.rowBytes * y);
    for (uint32_t x = 0; x < a.width * channel_count; ++x) {
      const int64_t diff =
          static_cast<int64_t>(row_b[x]) - static_cast<int64_t>(row_a[x]);
      *abs_diff_sum += std::abs(diff);
      *sq_diff_sum += diff * diff;
      *max_abs_diff = std::max(*max_abs_diff, std::abs(diff));
    }
  }
}

void GetDiffSumAndSqDiffSum(const avifRGBImage& a, const avifRGBImage& b,
                            int64_t* abs_diff_sum, int64_t* sq_diff_sum,
                            int64_t* max_abs_diff) {
  (a.depth <= 8) ? GetDiffSumAndSqDiffSum<uint8_t>(a, b, abs_diff_sum,
                                                   sq_diff_sum, max_abs_diff)
                 : GetDiffSumAndSqDiffSum<uint16_t>(a, b, abs_diff_sum,
                                                    sq_diff_sum, max_abs_diff);
}

// Returns the Peak Signal-to-Noise Ratio from accumulated stats.
double GetPsnr(double sq_diff_sum, double num_diffs, double max_abs_diff) {
  if (sq_diff_sum == 0.) {
    return 99.;  // Lossless.
  }
  const double distortion =
      sq_diff_sum / (num_diffs * max_abs_diff * max_abs_diff);
  return (distortion > 0.) ? std::min(-10 * std::log10(distortion), 98.9)
                           : 98.9;  // Not lossless.
}

// Contains the sample position of each channel for a given avifRGBFormat.
// The alpha sample position is set to 0 for layouts having no alpha channel.
struct RgbChannelOffsets {
  uint8_t r, g, b, a;
};

RgbChannelOffsets GetRgbChannelOffsets(avifRGBFormat format) {
  switch (format) {
    case AVIF_RGB_FORMAT_RGB:
      return {/*r=*/0, /*g=*/1, /*b=*/2, /*a=*/0};
    case AVIF_RGB_FORMAT_RGBA:
      return {/*r=*/0, /*g=*/1, /*b=*/2, /*a=*/3};
    case AVIF_RGB_FORMAT_ARGB:
      return {/*r=*/1, /*g=*/2, /*b=*/3, /*a=*/0};
    case AVIF_RGB_FORMAT_BGR:
      return {/*r=*/2, /*g=*/1, /*b=*/0, /*a=*/0};
    case AVIF_RGB_FORMAT_BGRA:
      return {/*r=*/2, /*g=*/1, /*b=*/0, /*a=*/3};
    case AVIF_RGB_FORMAT_ABGR:
      return {/*r=*/3, /*g=*/2, /*b=*/1, /*a=*/0};
    default:
      return {/*r=*/0, /*g=*/0, /*b=*/0, /*a=*/0};
  }
}

// Converts from RGB to YUV and back to RGB for all RGB combinations, separated
// by a color step for reasonable timing. If add_noise is true, also applies
// some noise to the input samples to exercise chroma subsampling.
void ConvertWholeRange(int rgb_depth, int yuv_depth, avifRGBFormat rgb_format,
                       avifPixelFormat yuv_format, avifRange yuv_range,
                       avifMatrixCoefficients matrix_coefficients,
                       avifChromaDownsampling chroma_downsampling,
                       bool add_noise, uint32_t rgb_step,
                       double max_average_abs_diff, double min_psnr, bool log) {
  // Deduced constants.
  const bool is_monochrome =
      (yuv_format == AVIF_PIXEL_FORMAT_YUV400);  // If so, only test grey input.
  const uint32_t rgb_max = (1 << rgb_depth) - 1;

  // The YUV upsampling treats the first and last rows and columns differently
  // than the remaining pairs of rows and columns. An image of 16 pixels is used
  // to test all these possibilities.
  constexpr int kWidth = 4;
  constexpr int kHeight = 4;
  ImagePtr yuv(avifImageCreate(kWidth, kHeight, yuv_depth, yuv_format));
  ASSERT_NE(yuv, nullptr);
  yuv->matrixCoefficients = matrix_coefficients;
  yuv->yuvRange = yuv_range;
  AvifRgbImage src_rgb(yuv.get(), rgb_depth, rgb_format);
  src_rgb.chromaDownsampling = chroma_downsampling;
  AvifRgbImage dst_rgb(yuv.get(), rgb_depth, rgb_format);
  const RgbChannelOffsets offsets = GetRgbChannelOffsets(rgb_format);

  // Alpha values are not tested here. Keep it opaque.
  if (avifRGBFormatHasAlpha(src_rgb.format)) {
    testutil::FillImageChannel(&src_rgb, offsets.a, rgb_max);
  }

  // Estimate the loss from converting RGB values to YUV and back.
  int64_t abs_diff_sum = 0, sq_diff_sum = 0, max_abs_diff = 0;
  int64_t num_diffs = 0;
  const uint32_t max_value = rgb_max - (add_noise ? 15 : 0);
  for (uint32_t r = 0; r < max_value + rgb_step; r += rgb_step) {
    r = std::min(r, max_value);  // Test the maximum sample value even if it is
                                 // not a multiple of rgb_step.
    testutil::FillImageChannel(&src_rgb, offsets.r, r);
    if (add_noise) {
      testutil::ModifyImageChannel(&src_rgb, offsets.r, kRedNoise);
    }

    if (is_monochrome) {
      // Test only greyish input when converting to a single channel.
      testutil::FillImageChannel(&src_rgb, offsets.g, r);
      testutil::FillImageChannel(&src_rgb, offsets.b, r);
      if (add_noise) {
        testutil::ModifyImageChannel(&src_rgb, offsets.g, kGreenNoise);
        testutil::ModifyImageChannel(&src_rgb, offsets.b, kBlueNoise);
      }

      ASSERT_EQ(avifImageRGBToYUV(yuv.get(), &src_rgb), AVIF_RESULT_OK);
      ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
      GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &abs_diff_sum, &sq_diff_sum,
                             &max_abs_diff);
      num_diffs += src_rgb.width * src_rgb.height * 3;  // Alpha is lossless.
    } else {
      for (uint32_t g = 0; g < max_value + rgb_step; g += rgb_step) {
        g = std::min(g, max_value);
        testutil::FillImageChannel(&src_rgb, offsets.g, g);
        if (add_noise) {
          testutil::ModifyImageChannel(&src_rgb, offsets.g, kGreenNoise);
        }
        for (uint32_t b = 0; b < max_value + rgb_step; b += rgb_step) {
          b = std::min(b, max_value);
          testutil::FillImageChannel(&src_rgb, offsets.b, b);
          if (add_noise) {
            testutil::ModifyImageChannel(&src_rgb, offsets.b, kBlueNoise);
          }

          ASSERT_EQ(avifImageRGBToYUV(yuv.get(), &src_rgb), AVIF_RESULT_OK);
          ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
          GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &abs_diff_sum, &sq_diff_sum,
                                 &max_abs_diff);
          num_diffs +=
              src_rgb.width * src_rgb.height * 3;  // Alpha is lossless.
        }
      }
    }
  }

  // Stats and thresholds.
  // Note: The thresholds defined in this test are calibrated for libyuv fast
  //       paths. See reformat_libyuv.c. Slower non-libyuv conversions in
  //       libavif have a higher precision (using floating point operations).
  const double average_abs_diff =
      static_cast<double>(abs_diff_sum) / static_cast<double>(num_diffs);
  const double psnr = GetPsnr(static_cast<double>(sq_diff_sum),
                              static_cast<double>(num_diffs), rgb_max);
  EXPECT_LE(average_abs_diff, max_average_abs_diff);
  EXPECT_GE(psnr, min_psnr);
}

// Converts from RGB to YUV and back to RGB for multiple buffer dimensions to
// exercise stride computation and subsampling edge cases.
void ConvertWholeBuffer(int rgb_depth, int yuv_depth, avifRGBFormat rgb_format,
                        avifPixelFormat yuv_format, avifRange yuv_range,
                        avifMatrixCoefficients matrix_coefficients,
                        avifChromaDownsampling chroma_downsampling,
                        bool add_noise, double min_psnr) {
  // Deduced constants.
  const bool is_monochrome =
      (yuv_format == AVIF_PIXEL_FORMAT_YUV400);  // If so, only test grey input.
  const uint32_t rgb_max = (1 << rgb_depth) - 1;

  // Estimate the loss from converting RGB values to YUV and back.
  int64_t abs_diff_sum = 0, sq_diff_sum = 0, max_abs_diff = 0;
  int64_t num_diffs = 0;
  for (int width : {1, 2, 127}) {
    for (int height : {1, 2, 251}) {
      ImagePtr yuv(avifImageCreate(width, height, yuv_depth, yuv_format));
      ASSERT_NE(yuv, nullptr);
      yuv->matrixCoefficients = matrix_coefficients;
      yuv->yuvRange = yuv_range;
      AvifRgbImage src_rgb(yuv.get(), rgb_depth, rgb_format);
      src_rgb.chromaDownsampling = chroma_downsampling;
      AvifRgbImage dst_rgb(yuv.get(), rgb_depth, rgb_format);
      const RgbChannelOffsets offsets = GetRgbChannelOffsets(rgb_format);

      // Fill the input buffer with whatever content.
      testutil::FillImageChannel(&src_rgb, offsets.r, /*value=*/0);
      testutil::FillImageChannel(&src_rgb, offsets.g, /*value=*/0);
      testutil::FillImageChannel(&src_rgb, offsets.b, /*value=*/0);
      if (add_noise) {
        testutil::ModifyImageChannel(&src_rgb, offsets.r, kRedNoise);
        testutil::ModifyImageChannel(&src_rgb, offsets.g,
                                     is_monochrome ? kRedNoise : kGreenNoise);
        testutil::ModifyImageChannel(&src_rgb, offsets.b,
                                     is_monochrome ? kRedNoise : kBlueNoise);
      }
      // Alpha values are not tested here. Keep it opaque.
      if (avifRGBFormatHasAlpha(src_rgb.format)) {
        testutil::FillImageChannel(&src_rgb, offsets.a, rgb_max);
      }

      ASSERT_EQ(avifImageRGBToYUV(yuv.get(), &src_rgb), AVIF_RESULT_OK);
      ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
      GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &abs_diff_sum, &sq_diff_sum,
                             &max_abs_diff);
      num_diffs += src_rgb.width * src_rgb.height * 3;
    }
  }
  EXPECT_GE(GetPsnr(static_cast<double>(sq_diff_sum),
                    static_cast<double>(num_diffs), rgb_max),
            min_psnr);
}

TEST(RGBToYUVTest, ExhaustiveSettings) {
  // Coverage of all configurations with all min/max input combinations.
  for (int rgb_depth : {8, 10, 12, 16}) {
    for (int yuv_depth : {8, 10, 12, 16}) {
      for (avifRGBFormat rgb_format :
           {AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_RGBA, AVIF_RGB_FORMAT_ARGB,
            AVIF_RGB_FORMAT_BGR, AVIF_RGB_FORMAT_BGRA, AVIF_RGB_FORMAT_ABGR}) {
        for (avifPixelFormat yuv_format :
             {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
              AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
          for (avifRange yuv_range : {AVIF_RANGE_LIMITED, AVIF_RANGE_FULL}) {
            for (decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY)
                     matrix_coefficients : {AVIF_MATRIX_COEFFICIENTS_IDENTITY,
                                            AVIF_MATRIX_COEFFICIENTS_BT601}) {
              if (matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY &&
                  yuv_format != AVIF_PIXEL_FORMAT_YUV444) {
                // See avifPrepareReformatState().
                continue;
              }
              for (avifChromaDownsampling chroma_downsampling :
                   {AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                    AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                    AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
                    AVIF_CHROMA_DOWNSAMPLING_AVERAGE,
                    AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV}) {
                if (chroma_downsampling == AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV &&
                    (yuv_depth > 12 ||
                     yuv_format != AVIF_PIXEL_FORMAT_YUV420)) {
                  // sharpyuv does not support these combinations.
                  continue;
                }
                ConvertWholeRange(
                    rgb_depth, yuv_depth, rgb_format, yuv_format, yuv_range,
                    static_cast<avifMatrixCoefficients>(matrix_coefficients),
                    chroma_downsampling,
                    /*add_noise=*/true,
                    // Just try min and max values.
                    /*rgb_step=*/(1u << rgb_depth) - 1u,
                    // Barely check the results, this is mostly for coverage.
                    /*max_average_abs_diff=*/(1u << rgb_depth) - 1u,
                    /*min_psnr=*/5.0,
                    // Avoid spam.
                    /*log=*/false);
              }
            }
          }
        }
      }
    }
  }
}

TEST(RGBToYUVTest, AllMatrixCoefficients) {
  // Coverage of all configurations with all min/max input combinations.
  for (int rgb_depth : {8, 10, 12, 16}) {
    for (int yuv_depth : {8, 10, 12, 16}) {
      for (avifPixelFormat yuv_format :
           {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
            AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
        for (avifRange yuv_range : {AVIF_RANGE_LIMITED, AVIF_RANGE_FULL}) {
          for (decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY) matrix_coefficients :
               {
                   AVIF_MATRIX_COEFFICIENTS_BT709,
                   AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED,
                   AVIF_MATRIX_COEFFICIENTS_FCC,
                   AVIF_MATRIX_COEFFICIENTS_BT470BG,
                   AVIF_MATRIX_COEFFICIENTS_BT601,
                   AVIF_MATRIX_COEFFICIENTS_SMPTE240,
                   AVIF_MATRIX_COEFFICIENTS_YCGCO,
                   AVIF_MATRIX_COEFFICIENTS_BT2020_NCL,
                   AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL,
                   AVIF_MATRIX_COEFFICIENTS_YCGCO_RE,
                   AVIF_MATRIX_COEFFICIENTS_YCGCO_RO,
                   // These are unsupported. See avifPrepareReformatState().
                   // AVIF_MATRIX_COEFFICIENTS_BT2020_CL
                   // AVIF_MATRIX_COEFFICIENTS_SMPTE2085
                   // AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL
                   // AVIF_MATRIX_COEFFICIENTS_ICTCP
               }) {
            if (matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO &&
                yuv_range == AVIF_RANGE_LIMITED) {
              // See avifPrepareReformatState().
              continue;
            }
            if ((matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE &&
                 yuv_depth - 2 != rgb_depth) ||
                (matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO &&
                 yuv_depth - 1 != rgb_depth)) {
              // See avifPrepareReformatState().
              continue;
            }
            if ((matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE ||
                 matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO) &&
                yuv_range != AVIF_RANGE_FULL) {
              // YCgCo-R is for lossless.
              continue;
            }
            for (avifChromaDownsampling chroma_downsampling :
                 {AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                  AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY}) {
              ConvertWholeRange(
                  rgb_depth, yuv_depth, AVIF_RGB_FORMAT_RGBA, yuv_format,
                  yuv_range,
                  static_cast<avifMatrixCoefficients>(matrix_coefficients),
                  chroma_downsampling,
                  /*add_noise=*/true,
                  // Just try min and max values.
                  /*rgb_step=*/(1u << rgb_depth) - 1u,
                  // Barely check the results, this is mostly for coverage.
                  /*max_average_abs_diff=*/(1u << rgb_depth) - 1u,
                  /*min_psnr=*/5.0,
                  // Avoid spam.
                  /*log=*/false);
            }
          }
        }
      }
    }
  }
}

class RGBToYUVTest
    : public testing::TestWithParam<std::tuple<
          /*rgb_depth=*/int, /*yuv_depth=*/int, avifRGBFormat, avifPixelFormat,
          avifRange, avifMatrixCoefficients, avifChromaDownsampling,
          /*add_noise=*/bool, /*rgb_step=*/uint32_t,
          /*max_average_abs_diff=*/double, /*min_psnr=*/double>> {};

TEST_P(RGBToYUVTest, ConvertWholeRange) {
  ConvertWholeRange(
      /*rgb_depth=*/std::get<0>(GetParam()),
      /*yuv_depth=*/std::get<1>(GetParam()),
      /*rgb_format=*/std::get<2>(GetParam()),
      /*yuv_format=*/std::get<3>(GetParam()),
      /*yuv_range=*/std::get<4>(GetParam()),
      /*matrix_coefficients=*/std::get<5>(GetParam()),
      /*chroma_downsampling=*/std::get<6>(GetParam()),
      // Whether to add noise to the input RGB samples.
      // Should only impact subsampled chroma (4:2:2 and 4:2:0).
      /*add_noise=*/std::get<7>(GetParam()),
      // Testing each RGB combination would be more accurate but results are
      // similar with faster settings.
      /*rgb_step=*/std::get<8>(GetParam()),
      // Thresholds to pass.
      /*max_average_abs_diff=*/std::get<9>(GetParam()),
      /*min_psnr=*/std::get<10>(GetParam()),
      // Useful to see surrounding results when there is a failure.
      /*log=*/true);
}

TEST_P(RGBToYUVTest, ConvertWholeBuffer) {
  ConvertWholeBuffer(
      /*rgb_depth=*/std::get<0>(GetParam()),
      /*yuv_depth=*/std::get<1>(GetParam()),
      /*rgb_format=*/std::get<2>(GetParam()),
      /*yuv_format=*/std::get<3>(GetParam()),
      /*yuv_range=*/std::get<4>(GetParam()),
      /*matrix_coefficients=*/std::get<5>(GetParam()),
      /*chroma_downsampling=*/std::get<6>(GetParam()),
      // Whether to add noise to the input RGB samples.
      /*add_noise=*/std::get<7>(GetParam()),
      // Threshold to pass.
      /*min_psnr=*/std::get<10>(GetParam()));
}

// avifMatrixCoefficients-typed constants for testing::Values() to work on MSVC.
// typedef or using decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY) does not work
// (GTest template "declared using unnamed type, is used but never defined").
constexpr avifMatrixCoefficients kMatrixCoefficientsBT601 =
    AVIF_MATRIX_COEFFICIENTS_BT601;
constexpr avifMatrixCoefficients kMatrixCoefficientsBT709 =
    AVIF_MATRIX_COEFFICIENTS_BT709;
constexpr avifMatrixCoefficients kMatrixCoefficientsIdentity =
    AVIF_MATRIX_COEFFICIENTS_IDENTITY;
constexpr avifMatrixCoefficients kMatrixCoefficientsYCgCoRe =
    AVIF_MATRIX_COEFFICIENTS_YCGCO_RE;

// This is the default crabbyavif setup when encoding from 8b PNG files to AVIF.
INSTANTIATE_TEST_SUITE_P(
    DefaultFormat, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(8),
            /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Values(true),
            /*rgb_step=*/Values(3),
            /*max_average_abs_diff=*/Values(2.88),
            /*min_psnr=*/Values(36.)  // Subsampling distortion is acceptable.
            ));

// Keeping RGB samples in full range and same or higher bit depth should not
// bring any loss in the roundtrip.
INSTANTIATE_TEST_SUITE_P(Identity8b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(8),
                                 /*yuv_depth=*/Values(8, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(31),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity10b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(10),
                                 /*yuv_depth=*/Values(10, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(101),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity12b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(12),
                                 /*yuv_depth=*/Values(12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(401),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity16b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(16),
                                 /*yuv_depth=*/Values(16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(6421),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));

// 4:4:4 and chroma subsampling have similar distortions on plain color inputs.
INSTANTIATE_TEST_SUITE_P(
    PlainAnySubsampling8b, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_FULL), Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
        /*add_noise=*/Values(false),
        /*rgb_step=*/Values(17),
        /*max_average_abs_diff=*/Values(0.84),
        /*min_psnr=*/Values(45.)  // RGB>YUV>RGB distortion is barely
                                  // noticeable.
        ));

// Converting grey RGB samples to full-range monochrome of same or greater bit
// depth should be lossless.
INSTANTIATE_TEST_SUITE_P(MonochromeLossless8b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(8),
                                 /*yuv_depth=*/Values(8),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless10b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(10),
                                 /*yuv_depth=*/Values(10),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless12b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(12),
                                 /*yuv_depth=*/Values(12),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless16b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(16),
                                 /*yuv_depth=*/Values(16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(401),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));

// Tests YCGCO_RE is lossless.
INSTANTIATE_TEST_SUITE_P(YCgCo_Re8b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(8),
                                 /*yuv_depth=*/Values(10),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsYCgCoRe),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(101),
                                 /*max_average_abs_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));

// More coverage cases.
INSTANTIATE_TEST_SUITE_P(
    All8bTo8b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(8),
            /*yuv_depth=*/Values(8),
            Values(AVIF_RGB_FORMAT_RGBA, AVIF_RGB_FORMAT_BGR),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED), Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(61),  // High or it would be too slow.
            /*max_average_abs_diff=*/Values(2.96),  // Not very accurate because
                                                    // of high rgb_step.
            /*min_psnr=*/Values(36.)));
INSTANTIATE_TEST_SUITE_P(
    All10b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(10),
            /*yuv_depth=*/Values(10), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_FULL), Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(211),  // High or it would be too slow.
            /*max_average_abs_diff=*/Values(2.83),  // Not very accurate because
                                                    // of high rgb_step.
            /*min_psnr=*/Values(47.)));
INSTANTIATE_TEST_SUITE_P(
    All12b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(12),
            /*yuv_depth=*/Values(12), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED), Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(809),  // High or it would be too slow.
            /*max_average_abs_diff=*/Values(2.82),  // Not very accurate because
                                                    // of high rgb_step.
            /*min_psnr=*/Values(52.)));
INSTANTIATE_TEST_SUITE_P(
    All16b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(16),
            /*yuv_depth=*/Values(16), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_FULL), Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(16001),  // High or it would be too slow.
            /*max_average_abs_diff=*/Values(2.82),
            /*min_psnr=*/Values(80.)));

// Coverage for sharpyuv.
INSTANTIATE_TEST_SUITE_P(
    SharpYuv8Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8, 10, 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(17),
        /*max_average_abs_diff=*/Values(2.97),  // Sharp YUV introduces some
                                                // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv8BitRanges, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(17),
        /*max_average_abs_diff=*/Values(2.94),  // Sharp YUV introduces some
                                                // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv8BitMatrixCoefficients, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601, kMatrixCoefficientsBT709),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(17),
        /*max_average_abs_diff=*/Values(2.94),  // Sharp YUV introduces some
                                                // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv10Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(10),
        /*yuv_depth=*/Values(10), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(211),               // High or it would be too slow.
        /*max_average_abs_diff=*/Values(2.94),  // Sharp YUV introduces some
                                                // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv12Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(12),
        /*yuv_depth=*/Values(8, 10, 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(840),               // High or it would be too slow.
        /*max_average_abs_diff=*/Values(6.57),  // Sharp YUV introduces some
                                                // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv16Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(16),
        /*yuv_depth=*/Values(8, /*10,*/ 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(4567),  // High or it would be too slow.
        /*max_average_abs_diff=*/Values(111.7),  // Sharp YUV introduces some
                                                 // color shift.
        /*min_psnr=*/Values(49.)  // SharpYuv distortion is acceptable.
        ));

TEST(ReformatTest, NullCases) {
  ImagePtr image(avifImageCreate(kWidth, kHeight, 8, AVIF_PIXEL_FORMAT_YUV444));
  avifRGBImage rgb;

  avifRGBImageSetDefaults(nullptr, nullptr);
  avifRGBImageSetDefaults(nullptr, image.get());
  avifRGBImageSetDefaults(&rgb, nullptr);

  EXPECT_NE(avifImageYUVToRGB(nullptr, nullptr), AVIF_RESULT_OK);
  EXPECT_NE(avifImageYUVToRGB(image.get(), nullptr), AVIF_RESULT_OK);
  EXPECT_NE(avifImageYUVToRGB(nullptr, &rgb), AVIF_RESULT_OK);

  EXPECT_NE(avifImageRGBToYUV(nullptr, nullptr), AVIF_RESULT_OK);
  EXPECT_NE(avifImageRGBToYUV(image.get(), nullptr), AVIF_RESULT_OK);
  EXPECT_NE(avifImageRGBToYUV(nullptr, &rgb), AVIF_RESULT_OK);

  avifDiagnostics diag;
  EXPECT_NE(avifImageScale(nullptr, 8, 8, nullptr), AVIF_RESULT_OK);
  EXPECT_NE(avifImageScale(nullptr, 8, 8, &diag), AVIF_RESULT_OK);

  EXPECT_EQ(avifRGBImagePixelSize(nullptr), 0);

  EXPECT_NE(avifRGBImageAllocatePixels(nullptr), AVIF_RESULT_OK);

  avifRGBImageFreePixels(nullptr);
}

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
