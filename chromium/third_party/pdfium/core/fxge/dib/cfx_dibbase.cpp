// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/dib/cfx_dibbase.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_2d_size.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_memcpy_wrappers.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/notreached.h"
#include "core/fxcrt/span.h"
#include "core/fxcrt/span_util.h"
#include "core/fxcrt/stl_util.h"
#include "core/fxcrt/zip.h"
#include "core/fxge/calculate_pitch.h"
#include "core/fxge/cfx_cliprgn.h"
#include "core/fxge/dib/cfx_bitmapstorer.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/cfx_imagestretcher.h"
#include "core/fxge/dib/cfx_imagetransformer.h"

namespace {

void ColorDecode(uint32_t pal_v, uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = static_cast<uint8_t>((pal_v & 0xf00) >> 4);
  *g = static_cast<uint8_t>(pal_v & 0x0f0);
  *b = static_cast<uint8_t>((pal_v & 0x00f) << 4);
}

class CFX_Palette {
 public:
  // (Amount, Color) pairs
  using LutsData = std::pair<uint32_t, uint32_t>;

  explicit CFX_Palette(const RetainPtr<const CFX_DIBBase>& pBitmap);
  ~CFX_Palette();

  pdfium::span<const uint32_t> GetPalette() { return m_Palette; }
  pdfium::span<const LutsData> GetValidLuts() {
    return pdfium::make_span(m_Luts).first(m_lut);
  }

 private:
  void ObtainPalette();

  std::vector<uint32_t> m_Palette;
  std::vector<LutsData> m_Luts;
  uint32_t m_lut = 0;
};

CFX_Palette::CFX_Palette(const RetainPtr<const CFX_DIBBase>& pBitmap)
    : m_Palette(256), m_Luts(4096) {
  int bpp = pBitmap->GetBPP() / 8;
  int width = pBitmap->GetWidth();
  int height = pBitmap->GetHeight();
  for (int row = 0; row < height; ++row) {
    pdfium::span<const uint8_t> scan_line = pBitmap->GetScanline(row);
    for (int col = 0; col < width; ++col) {
      UNSAFE_TODO({
        const uint8_t* src_port =
            scan_line.subspan(Fx2DSizeOrDie(col, bpp)).data();
        uint32_t b = src_port[0] & 0xf0;
        uint32_t g = src_port[1] & 0xf0;
        uint32_t r = src_port[2] & 0xf0;
        uint32_t index = (r << 4) + g + (b >> 4);
        ++m_Luts[index].first;
      });
    }
  }
  // Move non-zeros to the front and count them
  for (uint32_t row = 0; row < m_Luts.size(); ++row) {
    if (m_Luts[row].first != 0) {
      m_Luts[m_lut].first = m_Luts[row].first;
      m_Luts[m_lut].second = row;
      ++m_lut;
    }
  }
  pdfium::span<LutsData> lut_span = pdfium::make_span(m_Luts).first(m_lut);
  std::sort(lut_span.begin(), lut_span.end(),
            [](const LutsData& arg1, const LutsData& arg2) {
              return arg1.first < arg2.first;
            });
  ObtainPalette();
}

CFX_Palette::~CFX_Palette() = default;

void CFX_Palette::ObtainPalette() {
  for (uint32_t row = 0; row < m_Palette.size(); ++row) {
    const uint32_t lut_offset = (m_lut - row - 1) % m_Palette.size();
    const uint32_t color = m_Luts[lut_offset].second;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    ColorDecode(color, &r, &g, &b);
    m_Palette[row] = (static_cast<uint32_t>(r) << 16) |
                     (static_cast<uint32_t>(g) << 8) | b | 0xff000000;
    m_Luts[lut_offset].first = row;
  }
  if (m_lut > 256) {
    int err;
    int min_err;
    const uint32_t lut_256 = m_lut - 256;
    for (uint32_t row = 0; row < lut_256; ++row) {
      min_err = 1000000;
      uint8_t r;
      uint8_t g;
      uint8_t b;
      ColorDecode(m_Luts[row].second, &r, &g, &b);
      uint32_t clrindex = 0;
      for (int col = 0; col < 256; ++col) {
        uint32_t p_color = m_Palette[col];
        int d_r = r - static_cast<uint8_t>(p_color >> 16);
        int d_g = g - static_cast<uint8_t>(p_color >> 8);
        int d_b = b - static_cast<uint8_t>(p_color);
        err = d_r * d_r + d_g * d_g + d_b * d_b;
        if (err < min_err) {
          min_err = err;
          clrindex = col;
        }
      }
      m_Luts[row].first = clrindex;
    }
  }
}

void ConvertBuffer_1bppMask2Gray(pdfium::span<uint8_t> dest_buf,
                                 int dest_pitch,
                                 int width,
                                 int height,
                                 const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                                 int src_left,
                                 int src_top) {
  static constexpr uint8_t kSetGray = 0xff;
  static constexpr uint8_t kResetGray = 0x00;
  for (int row = 0; row < height; ++row) {
    pdfium::span<uint8_t> dest_span =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch));
    pdfium::span<const uint8_t> src_span =
        pSrcBitmap->GetScanline(src_top + row);
    fxcrt::Fill(dest_span.first(width), kResetGray);
    uint8_t* dest_scan = dest_span.data();
    const uint8_t* src_scan = src_span.data();
    UNSAFE_TODO({
      for (int col = src_left; col < src_left + width; ++col) {
        if (src_scan[col / 8] & (1 << (7 - col % 8))) {
          *dest_scan = kSetGray;
        }
        ++dest_scan;
      }
    });
  }
}

void ConvertBuffer_8bppMask2Gray(pdfium::span<uint8_t> dest_buf,
                                 int dest_pitch,
                                 int width,
                                 int height,
                                 const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                                 int src_left,
                                 int src_top) {
  for (int row = 0; row < height; ++row) {
    fxcrt::Copy(pSrcBitmap->GetScanline(src_top + row).subspan(src_left, width),
                dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)));
  }
}

void ConvertBuffer_1bppPlt2Gray(pdfium::span<uint8_t> dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                                int src_left,
                                int src_top) {
  pdfium::span<const uint32_t> src_palette = pSrcBitmap->GetPaletteSpan();
  const uint8_t reset_r = FXARGB_R(src_palette[0]);
  const uint8_t reset_g = FXARGB_G(src_palette[0]);
  const uint8_t reset_b = FXARGB_B(src_palette[0]);
  const uint8_t set_r = FXARGB_R(src_palette[1]);
  const uint8_t set_g = FXARGB_G(src_palette[1]);
  const uint8_t set_b = FXARGB_B(src_palette[1]);
  const uint8_t gray0 = FXRGB2GRAY(reset_r, reset_g, reset_b);
  const uint8_t gray1 = FXRGB2GRAY(set_r, set_g, set_b);

  for (int row = 0; row < height; ++row) {
    pdfium::span<uint8_t> dest_span =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch));
    fxcrt::Fill(dest_span.first(width), gray0);
    uint8_t* dest_scan = dest_span.data();
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row).data();
    UNSAFE_TODO({
      for (int col = src_left; col < src_left + width; ++col) {
        if (src_scan[col / 8] & (1 << (7 - col % 8))) {
          *dest_scan = gray1;
        }
        ++dest_scan;
      }
    });
  }
}

void ConvertBuffer_8bppPlt2Gray(pdfium::span<uint8_t> dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const RetainPtr<const CFX_DIBBase>& src,
                                int src_left,
                                int src_top) {
  pdfium::span<const uint32_t> src_palette = src->GetPaletteSpan();
  CHECK_EQ(256u, src_palette.size());
  std::array<uint8_t, 256> gray;
  for (auto [input, output] : fxcrt::Zip(src_palette, gray)) {
    output = FXRGB2GRAY(FXARGB_R(input), FXARGB_G(input), FXARGB_B(input));
  }
  for (int row = 0; row < height; ++row) {
    auto dest_scan = dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch));
    auto src_scan = src->GetScanline(src_top + row).subspan(src_left, width);
    for (auto [input, output] : fxcrt::Zip(src_scan, dest_scan)) {
      output = gray[input];
    }
  }
}

void ConvertBuffer_Rgb2Gray(pdfium::span<uint8_t> dest_buf,
                            int dest_pitch,
                            int width,
                            int height,
                            const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                            int src_left,
                            int src_top) {
  const int Bpp = pSrcBitmap->GetBPP() / 8;
  const size_t x_offset = Fx2DSizeOrDie(src_left, Bpp);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row).subspan(x_offset).data();
    UNSAFE_TODO({
      for (int col = 0; col < width; ++col) {
        *dest_scan++ = FXRGB2GRAY(src_scan[2], src_scan[1], src_scan[0]);
        src_scan += Bpp;
      }
    });
  }
}

void ConvertBuffer_IndexCopy(pdfium::span<uint8_t> dest_buf,
                             int dest_pitch,
                             int width,
                             int height,
                             const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                             int src_left,
                             int src_top) {
  if (pSrcBitmap->GetBPP() == 1) {
    for (int row = 0; row < height; ++row) {
      pdfium::span<uint8_t> dest_span =
          dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch));
      // Set all destination pixels to be white initially.
      fxcrt::Fill(dest_span.first(width), 255);
      uint8_t* dest_scan = dest_span.data();
      const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row).data();
      UNSAFE_TODO({
        for (int col = src_left; col < src_left + width; ++col) {
          // If the source bit is set, then set the destination pixel to be
          // black.
          if (src_scan[col / 8] & (1 << (7 - col % 8))) {
            *dest_scan = 0;
          }

          ++dest_scan;
        }
      });
    }
  } else {
    for (int row = 0; row < height; ++row) {
      fxcrt::Copy(
          pSrcBitmap->GetScanline(src_top + row).subspan(src_left, width),
          dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)));
    }
  }
}

// Returns a palette of a fixed size.
DataVector<uint32_t> ConvertBuffer_Plt2PltRgb8(
    pdfium::span<uint8_t> dest_buf,
    int dest_pitch,
    int width,
    int height,
    const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
    int src_left,
    int src_top) {
  ConvertBuffer_IndexCopy(dest_buf, dest_pitch, width, height, pSrcBitmap,
                          src_left, src_top);
  const size_t plt_size = pSrcBitmap->GetRequiredPaletteSize();
  pdfium::span<const uint32_t> src_span = pSrcBitmap->GetPaletteSpan();
  CHECK_LE(plt_size, src_span.size());

  pdfium::span<const uint32_t> src_palette_span = src_span.first(plt_size);
  return DataVector<uint32_t>(src_palette_span.begin(), src_palette_span.end());
}

DataVector<uint32_t> ConvertBuffer_Rgb2PltRgb8(
    pdfium::span<uint8_t> dest_buf,
    int dest_pitch,
    int width,
    int height,
    const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
    int src_left,
    int src_top) {
  int bpp = pSrcBitmap->GetBPP() / 8;
  CFX_Palette src_palette(pSrcBitmap);
  pdfium::span<const CFX_Palette::LutsData> luts = src_palette.GetValidLuts();
  for (int row = 0; row < height; ++row) {
    pdfium::span<const uint8_t> src_span =
        pSrcBitmap->GetScanline(src_top + row).subspan(src_left);
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    UNSAFE_TODO({
      for (int col = 0; col < width; ++col) {
        const uint8_t* src_port =
            src_span.subspan(Fx2DSizeOrDie(col, bpp)).data();
        int r = src_port[2] & 0xf0;
        int g = src_port[1] & 0xf0;
        int b = src_port[0] & 0xf0;
        uint32_t clrindex = (r << 4) + g + (b >> 4);
        for (size_t i = luts.size(); i > 0; --i) {
          if (clrindex == luts[i - 1].second) {
            *(dest_scan + col) = static_cast<uint8_t>(luts[i - 1].first);
            break;
          }
        }
      }
    });
  }

  pdfium::span<const uint32_t> src_palette_span = src_palette.GetPalette();
  return DataVector<uint32_t>(src_palette_span.begin(), src_palette_span.end());
}

void ConvertBuffer_1bppMask2Rgb(FXDIB_Format dest_format,
                                pdfium::span<uint8_t> dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                                int src_left,
                                int src_top) {
  int comps = GetCompsFromFormat(dest_format);
  static constexpr uint8_t kSetGray = 0xff;
  static constexpr uint8_t kResetGray = 0x00;
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row).data();
    UNSAFE_TODO({
      for (int col = src_left; col < src_left + width; ++col) {
        uint8_t value =
            (src_scan[col / 8] & (1 << (7 - col % 8))) ? kSetGray : kResetGray;
        FXSYS_memset(dest_scan, value, 3);
        dest_scan += comps;
      }
    });
  }
}

void ConvertBuffer_8bppMask2Rgb(FXDIB_Format dest_format,
                                pdfium::span<uint8_t> dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                                int src_left,
                                int src_top) {
  int comps = GetCompsFromFormat(dest_format);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row).subspan(src_left).data();
    UNSAFE_TODO({
      for (int col = 0; col < width; ++col) {
        FXSYS_memset(dest_scan, *src_scan, 3);
        dest_scan += comps;
        ++src_scan;
      }
    });
  }
}

void ConvertBuffer_1bppPlt2Rgb(FXDIB_Format dest_format,
                               pdfium::span<uint8_t> dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                               int src_left,
                               int src_top) {
  pdfium::span<const uint32_t> src_palette = pSrcBitmap->GetPaletteSpan();
  const uint8_t dst_palette[6] = {
      FXARGB_B(src_palette[0]), FXARGB_G(src_palette[0]),
      FXARGB_R(src_palette[0]), FXARGB_B(src_palette[1]),
      FXARGB_G(src_palette[1]), FXARGB_R(src_palette[1])};
  int comps = GetCompsFromFormat(dest_format);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row).data();
    UNSAFE_TODO({
      for (int col = src_left; col < src_left + width; ++col) {
        size_t offset = (src_scan[col / 8] & (1 << (7 - col % 8))) ? 3 : 0;
        FXSYS_memcpy(dest_scan, dst_palette + offset, 3);
        dest_scan += comps;
      }
    });
  }
}

void ConvertBuffer_8bppPlt2Rgb(FXDIB_Format dest_format,
                               pdfium::span<uint8_t> dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                               int src_left,
                               int src_top) {
  pdfium::span<const uint32_t> src_palette = pSrcBitmap->GetPaletteSpan();
  CHECK_EQ(256u, src_palette.size());
  uint8_t dst_palette[768];
  UNSAFE_TODO({
    for (int i = 0; i < 256; ++i) {
      dst_palette[3 * i] = FXARGB_B(src_palette[i]);
      dst_palette[3 * i + 1] = FXARGB_G(src_palette[i]);
      dst_palette[3 * i + 2] = FXARGB_R(src_palette[i]);
    }
  });
  const int comps = GetCompsFromFormat(dest_format);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row).subspan(src_left).data();
    for (int col = 0; col < width; ++col) {
      UNSAFE_TODO({
        uint8_t* src_pixel = dst_palette + 3 * (*src_scan++);
        FXSYS_memcpy(dest_scan, src_pixel, 3);
        dest_scan += comps;
      });
    }
  }
}

void ConvertBuffer_24bppRgb2Rgb24(
    pdfium::span<uint8_t> dest_buf,
    int dest_pitch,
    int width,
    int height,
    const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
    int src_left,
    int src_top) {
  const size_t x_offset = Fx2DSizeOrDie(src_left, 3);
  const size_t byte_count = Fx2DSizeOrDie(width, 3);
  for (int row = 0; row < height; ++row) {
    fxcrt::Copy(
        pSrcBitmap->GetScanline(src_top + row).subspan(x_offset, byte_count),
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)));
  }
}

void ConvertBuffer_32bppRgb2Rgb24(
    pdfium::span<uint8_t> dest_buf,
    int dest_pitch,
    int width,
    int height,
    const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
    int src_left,
    int src_top) {
  const size_t x_offset = Fx2DSizeOrDie(src_left, 4);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row).subspan(x_offset).data();
    UNSAFE_TODO({
      for (int col = 0; col < width; ++col) {
        FXSYS_memcpy(dest_scan, src_scan, 3);
        dest_scan += 3;
        src_scan += 4;
      }
    });
  }
}

void ConvertBuffer_Rgb2Rgb32(pdfium::span<uint8_t> dest_buf,
                             int dest_pitch,
                             int width,
                             int height,
                             const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                             int src_left,
                             int src_top) {
  const int comps = pSrcBitmap->GetBPP() / 8;
  const size_t x_offset = Fx2DSizeOrDie(src_left, comps);
  for (int row = 0; row < height; ++row) {
    uint8_t* dest_scan =
        dest_buf.subspan(Fx2DSizeOrDie(row, dest_pitch)).data();
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row).subspan(x_offset).data();
    UNSAFE_TODO({
      for (int col = 0; col < width; ++col) {
        FXSYS_memcpy(dest_scan, src_scan, 3);
        dest_scan += 4;
        src_scan += comps;
      }
    });
  }
}

void ConvertBuffer_8bppMask(int bpp,
                            pdfium::span<uint8_t> dest_buf,
                            int dest_pitch,
                            int width,
                            int height,
                            const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                            int src_left,
                            int src_top) {
  switch (bpp) {
    case 1:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_1bppPlt2Gray(dest_buf, dest_pitch, width, height,
                                   pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_1bppMask2Gray(dest_buf, dest_pitch, width, height,
                                    pSrcBitmap, src_left, src_top);
      }
      break;
    case 8:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_8bppPlt2Gray(dest_buf, dest_pitch, width, height,
                                   pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_8bppMask2Gray(dest_buf, dest_pitch, width, height,
                                    pSrcBitmap, src_left, src_top);
      }
      break;
    case 24:
    case 32:
      ConvertBuffer_Rgb2Gray(dest_buf, dest_pitch, width, height, pSrcBitmap,
                             src_left, src_top);
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

void ConvertBuffer_Rgb(int bpp,
                       FXDIB_Format dest_format,
                       pdfium::span<uint8_t> dest_buf,
                       int dest_pitch,
                       int width,
                       int height,
                       const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                       int src_left,
                       int src_top) {
  switch (bpp) {
    case 1:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_1bppPlt2Rgb(dest_format, dest_buf, dest_pitch, width,
                                  height, pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_1bppMask2Rgb(dest_format, dest_buf, dest_pitch, width,
                                   height, pSrcBitmap, src_left, src_top);
      }
      break;
    case 8:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_8bppPlt2Rgb(dest_format, dest_buf, dest_pitch, width,
                                  height, pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_8bppMask2Rgb(dest_format, dest_buf, dest_pitch, width,
                                   height, pSrcBitmap, src_left, src_top);
      }
      break;
    case 24:
      ConvertBuffer_24bppRgb2Rgb24(dest_buf, dest_pitch, width, height,
                                   pSrcBitmap, src_left, src_top);
      break;
    case 32:
      ConvertBuffer_32bppRgb2Rgb24(dest_buf, dest_pitch, width, height,
                                   pSrcBitmap, src_left, src_top);
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

void ConvertBuffer_Argb(int bpp,
                        FXDIB_Format dest_format,
                        pdfium::span<uint8_t> dest_buf,
                        int dest_pitch,
                        int width,
                        int height,
                        const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
                        int src_left,
                        int src_top) {
  switch (bpp) {
    case 1:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_1bppPlt2Rgb(dest_format, dest_buf, dest_pitch, width,
                                  height, pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_1bppMask2Rgb(dest_format, dest_buf, dest_pitch, width,
                                   height, pSrcBitmap, src_left, src_top);
      }
      break;
    case 8:
      if (pSrcBitmap->HasPalette()) {
        ConvertBuffer_8bppPlt2Rgb(dest_format, dest_buf, dest_pitch, width,
                                  height, pSrcBitmap, src_left, src_top);
      } else {
        ConvertBuffer_8bppMask2Rgb(dest_format, dest_buf, dest_pitch, width,
                                   height, pSrcBitmap, src_left, src_top);
      }
      break;
    case 24:
    case 32:
      ConvertBuffer_Rgb2Rgb32(dest_buf, dest_pitch, width, height, pSrcBitmap,
                              src_left, src_top);
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

CFX_DIBBase::CFX_DIBBase() = default;

CFX_DIBBase::~CFX_DIBBase() = default;

bool CFX_DIBBase::SkipToScanline(int line, PauseIndicatorIface* pPause) const {
  return false;
}

size_t CFX_DIBBase::GetEstimatedImageMemoryBurden() const {
  return GetRequiredPaletteSize() * sizeof(uint32_t);
}

#if BUILDFLAG(IS_WIN) || defined(PDF_USE_SKIA)
RetainPtr<const CFX_DIBitmap> CFX_DIBBase::RealizeIfNeeded() const {
  return Realize();
}
#endif

RetainPtr<CFX_DIBitmap> CFX_DIBBase::Realize() const {
  return ClipToInternal(nullptr);
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::ClipTo(const FX_RECT& rect) const {
  return ClipToInternal(&rect);
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::ClipToInternal(
    const FX_RECT* pClip) const {
  FX_RECT rect(0, 0, GetWidth(), GetHeight());
  if (pClip) {
    rect.Intersect(*pClip);
    if (rect.IsEmpty())
      return nullptr;
  }
  auto pNewBitmap = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pNewBitmap->Create(rect.Width(), rect.Height(), GetFormat()))
    return nullptr;

  pNewBitmap->SetPalette(GetPaletteSpan());
  if (GetBPP() == 1 && rect.left % 8 != 0) {
    int left_shift = rect.left % 32;
    int right_shift = 32 - left_shift;
    int dword_count = pNewBitmap->GetPitch() / 4;
    for (int row = rect.top; row < rect.bottom; ++row) {
      auto src_span = GetScanlineAs<const uint32_t>(row);
      auto dst_span =
          pNewBitmap->GetWritableScanlineAs<uint32_t>(row - rect.top);
      // Bounds check for free with first/subspan.
      const uint32_t* src_scan =
          src_span.subspan(rect.left / 32, dword_count + 1).data();
      uint32_t* dst_scan = dst_span.first(dword_count).data();
      UNSAFE_TODO({
        for (int i = 0; i < dword_count; ++i) {
          dst_scan[i] =
              (src_scan[i] << left_shift) | (src_scan[i + 1] >> right_shift);
        }
      });
    }
  } else {
    std::optional<uint32_t> copy_len = fxge::CalculatePitch8(
        pNewBitmap->GetBPP(), /*components=*/1, pNewBitmap->GetWidth());
    if (!copy_len.has_value()) {
      return nullptr;
    }

    copy_len = std::min<uint32_t>(GetPitch(), copy_len.value());

    FX_SAFE_UINT32 offset = rect.left;
    offset *= GetBPP();
    offset /= 8;
    if (!offset.IsValid())
      return nullptr;

    for (int row = rect.top; row < rect.bottom; ++row) {
      const uint8_t* src_scan =
          GetScanline(row).subspan(offset.ValueOrDie()).data();
      uint8_t* dest_scan =
          pNewBitmap->GetWritableScanline(row - rect.top).data();
      UNSAFE_TODO(FXSYS_memcpy(dest_scan, src_scan, copy_len.value()));
    }
  }
  return pNewBitmap;
}

void CFX_DIBBase::BuildPalette() {
  if (HasPalette())
    return;

  if (GetBPP() == 1) {
    palette_ = {0xff000000, 0xffffffff};
  } else if (GetBPP() == 8) {
    palette_.resize(256);
    for (int i = 0; i < 256; ++i)
      palette_[i] = ArgbEncode(0xff, i, i, i);
  }
}

size_t CFX_DIBBase::GetRequiredPaletteSize() const {
  if (IsMaskFormat())
    return 0;

  switch (GetBPP()) {
    case 1:
      return 2;
    case 8:
      return 256;
    default:
      return 0;
  }
}

uint32_t CFX_DIBBase::GetPaletteArgb(int index) const {
  DCHECK((GetBPP() == 1 || GetBPP() == 8) && !IsMaskFormat());
  if (HasPalette())
    return GetPaletteSpan()[index];

  if (GetBPP() == 1)
    return index ? 0xffffffff : 0xff000000;

  return ArgbEncode(0xff, index, index, index);
}

void CFX_DIBBase::SetPaletteArgb(int index, uint32_t color) {
  DCHECK((GetBPP() == 1 || GetBPP() == 8) && !IsMaskFormat());
  BuildPalette();
  palette_[index] = color;
}

int CFX_DIBBase::FindPalette(uint32_t color) const {
  DCHECK((GetBPP() == 1 || GetBPP() == 8) && !IsMaskFormat());
  if (HasPalette()) {
    int palsize = (1 << GetBPP());
    pdfium::span<const uint32_t> palette = GetPaletteSpan();
    for (int i = 0; i < palsize; ++i) {
      if (palette[i] == color)
        return i;
    }
    return -1;
  }

  if (GetBPP() == 1)
    return (static_cast<uint8_t>(color) == 0xff) ? 1 : 0;
  return static_cast<uint8_t>(color);
}

bool CFX_DIBBase::GetOverlapRect(int& dest_left,
                                 int& dest_top,
                                 int& width,
                                 int& height,
                                 int src_width,
                                 int src_height,
                                 int& src_left,
                                 int& src_top,
                                 const CFX_ClipRgn* pClipRgn) const {
  if (width == 0 || height == 0)
    return false;

  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  if (dest_left > GetWidth() || dest_top > GetHeight()) {
    return false;
  }

  FX_SAFE_INT32 safe_src_width = src_left;
  safe_src_width += width;
  if (!safe_src_width.IsValid())
    return false;

  FX_SAFE_INT32 safe_src_height = src_top;
  safe_src_height += height;
  if (!safe_src_height.IsValid())
    return false;

  FX_RECT src_rect(src_left, src_top, safe_src_width.ValueOrDie(),
                   safe_src_height.ValueOrDie());
  FX_RECT src_bound(0, 0, src_width, src_height);
  src_rect.Intersect(src_bound);

  FX_SAFE_INT32 safe_x_offset = dest_left;
  safe_x_offset -= src_left;
  if (!safe_x_offset.IsValid())
    return false;

  FX_SAFE_INT32 safe_y_offset = dest_top;
  safe_y_offset -= src_top;
  if (!safe_y_offset.IsValid())
    return false;

  FX_SAFE_INT32 safe_dest_left = safe_x_offset;
  safe_dest_left += src_rect.left;
  if (!safe_dest_left.IsValid())
    return false;

  FX_SAFE_INT32 safe_dest_top = safe_y_offset;
  safe_dest_top += src_rect.top;
  if (!safe_dest_top.IsValid())
    return false;

  FX_SAFE_INT32 safe_dest_right = safe_x_offset;
  safe_dest_right += src_rect.right;
  if (!safe_dest_right.IsValid())
    return false;

  FX_SAFE_INT32 safe_dest_bottom = safe_y_offset;
  safe_dest_bottom += src_rect.bottom;
  if (!safe_dest_bottom.IsValid())
    return false;

  FX_RECT dest_rect(safe_dest_left.ValueOrDie(), safe_dest_top.ValueOrDie(),
                    safe_dest_right.ValueOrDie(),
                    safe_dest_bottom.ValueOrDie());
  FX_RECT dest_bound(0, 0, GetWidth(), GetHeight());
  dest_rect.Intersect(dest_bound);

  if (pClipRgn)
    dest_rect.Intersect(pClipRgn->GetBox());
  dest_left = dest_rect.left;
  dest_top = dest_rect.top;

  FX_SAFE_INT32 safe_new_src_left = dest_left;
  safe_new_src_left -= safe_x_offset;
  if (!safe_new_src_left.IsValid())
    return false;
  src_left = safe_new_src_left.ValueOrDie();

  FX_SAFE_INT32 safe_new_src_top = dest_top;
  safe_new_src_top -= safe_y_offset;
  if (!safe_new_src_top.IsValid())
    return false;
  src_top = safe_new_src_top.ValueOrDie();

  if (dest_rect.IsEmpty())
    return false;

  width = dest_rect.Width();
  height = dest_rect.Height();
  return true;
}

void CFX_DIBBase::SetPalette(pdfium::span<const uint32_t> src_palette) {
  TakePalette(DataVector<uint32_t>(src_palette.begin(), src_palette.end()));
}

void CFX_DIBBase::TakePalette(DataVector<uint32_t> src_palette) {
  if (src_palette.empty() || GetBPP() > 8) {
    palette_.clear();
    return;
  }

  palette_ = std::move(src_palette);
  uint32_t pal_size = 1 << GetBPP();
  CHECK_LE(pal_size, kPaletteSize);
  palette_.resize(pal_size);
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::CloneAlphaMask() const {
  DCHECK_EQ(GetFormat(), FXDIB_Format::kArgb);
  auto pMask = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pMask->Create(GetWidth(), GetHeight(), FXDIB_Format::k8bppMask)) {
    return nullptr;
  }

  for (int row = 0; row < GetHeight(); ++row) {
    const uint8_t* src_scan = GetScanline(row).subspan(3).data();
    uint8_t* dest_scan = pMask->GetWritableScanline(row).data();
    UNSAFE_TODO({
      for (int col = 0; col < GetWidth(); ++col) {
        *dest_scan++ = *src_scan;
        src_scan += 4;
      }
    });
  }
  return pMask;
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::FlipImage(bool bXFlip, bool bYFlip) const {
  auto pFlipped = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pFlipped->Create(GetWidth(), GetHeight(), GetFormat())) {
    return nullptr;
  }

  pFlipped->SetPalette(GetPaletteSpan());
  const int Bpp = GetBPP() / 8;
  if (!bXFlip) {
    for (int row = 0; row < GetHeight(); ++row) {
      UNSAFE_TODO({
        const uint8_t* src_scan = GetScanline(row).data();
        uint8_t* dest_scan =
            pFlipped->GetWritableScanline(bYFlip ? GetHeight() - row - 1 : row)
                .data();
        FXSYS_memcpy(dest_scan, src_scan, GetPitch());
      });
    }
    return pFlipped;
  }

  if (GetBPP() == 1) {
    for (int row = 0; row < GetHeight(); ++row) {
      UNSAFE_TODO({
        const uint8_t* src_scan = GetScanline(row).data();
        uint8_t* dest_scan =
            pFlipped->GetWritableScanline(bYFlip ? GetHeight() - row - 1 : row)
                .data();
        FXSYS_memset(dest_scan, 0, GetPitch());
        for (int col = 0; col < GetWidth(); ++col) {
          if (src_scan[col / 8] & (1 << (7 - col % 8))) {
            int dest_col = GetWidth() - col - 1;
            dest_scan[dest_col / 8] |= (1 << (7 - dest_col % 8));
          }
        }
      });
    }
    return pFlipped;
  }

  if (Bpp == 1) {
    for (int row = 0; row < GetHeight(); ++row) {
      UNSAFE_TODO({
        const uint8_t* src_scan = GetScanline(row).data();
        uint8_t* dest_scan =
            pFlipped->GetWritableScanline(bYFlip ? GetHeight() - row - 1 : row)
                .data();
        dest_scan += (GetWidth() - 1) * Bpp;
        for (int col = 0; col < GetWidth(); ++col) {
          *dest_scan = *src_scan;
          --dest_scan;
          ++src_scan;
        }
      });
    }
    return pFlipped;
  }

  if (Bpp == 3) {
    for (int row = 0; row < GetHeight(); ++row) {
      UNSAFE_TODO({
        const uint8_t* src_scan = GetScanline(row).data();
        uint8_t* dest_scan =
            pFlipped->GetWritableScanline(bYFlip ? GetHeight() - row - 1 : row)
                .data();
        dest_scan += (GetWidth() - 1) * Bpp;
        for (int col = 0; col < GetWidth(); ++col) {
          FXSYS_memcpy(dest_scan, src_scan, 3);
          dest_scan -= 3;
          src_scan += 3;
        }
      });
    }
    return pFlipped;
  }

  CHECK_EQ(Bpp, 4);
  for (int row = 0; row < GetHeight(); ++row) {
    UNSAFE_TODO({
      const uint8_t* src_scan = GetScanline(row).data();
      uint8_t* dest_scan =
          pFlipped->GetWritableScanline(bYFlip ? GetHeight() - row - 1 : row)
              .data();
      dest_scan += (GetWidth() - 1) * Bpp;
      for (int col = 0; col < GetWidth(); ++col) {
        const auto* src_scan32 = reinterpret_cast<const uint32_t*>(src_scan);
        uint32_t* dest_scan32 = reinterpret_cast<uint32_t*>(dest_scan);
        *dest_scan32 = *src_scan32;
        dest_scan -= 4;
        src_scan += 4;
      }
    });
  }
  return pFlipped;
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::ConvertTo(FXDIB_Format dest_format) const {
  if (dest_format == GetFormat())
    return Realize();

  auto pClone = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pClone->Create(GetWidth(), GetHeight(), dest_format)) {
    return nullptr;
  }

  if (dest_format == FXDIB_Format::kArgb) {
    pClone->SetUniformOpaqueAlpha();
  }

  RetainPtr<const CFX_DIBBase> holder(this);
  DataVector<uint32_t> pal_8bpp =
      ConvertBuffer(dest_format, pClone->GetWritableBuffer(),
                    pClone->GetPitch(), GetWidth(), GetHeight(), holder, 0, 0);
  if (!pal_8bpp.empty()) {
    pClone->TakePalette(std::move(pal_8bpp));
  }
  return pClone;
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::SwapXY(bool bXFlip, bool bYFlip) const {
  FX_RECT dest_clip(0, 0, GetHeight(), GetWidth());
  if (dest_clip.IsEmpty())
    return nullptr;

  auto pTransBitmap = pdfium::MakeRetain<CFX_DIBitmap>();
  const int result_height = dest_clip.Height();
  const int result_width = dest_clip.Width();
  if (!pTransBitmap->Create(result_width, result_height, GetFormat()))
    return nullptr;

  pTransBitmap->SetPalette(GetPaletteSpan());
  const int dest_pitch = pTransBitmap->GetPitch();
  pdfium::span<uint8_t> dest_span = pTransBitmap->GetWritableBuffer().first(
      Fx2DSizeOrDie(dest_pitch, result_height));
  const size_t dest_last_row_offset =
      Fx2DSizeOrDie(dest_pitch, result_height - 1);
  const int row_start = bXFlip ? GetHeight() - dest_clip.right : dest_clip.left;
  const int row_end = bXFlip ? GetHeight() - dest_clip.left : dest_clip.right;
  const int col_start = bYFlip ? GetWidth() - dest_clip.bottom : dest_clip.top;
  const int col_end = bYFlip ? GetWidth() - dest_clip.top : dest_clip.bottom;
  if (GetBPP() == 1) {
    fxcrt::Fill(dest_span, 0xff);
    if (bYFlip) {
      dest_span = dest_span.subspan(dest_last_row_offset);
    }
    const int dest_step = bYFlip ? -dest_pitch : dest_pitch;
    for (int row = row_start; row < row_end; ++row) {
      UNSAFE_TODO({
        const uint8_t* src_scan = GetScanline(row).data();
        int dest_col =
            (bXFlip ? dest_clip.right - (row - row_start) - 1 : row) -
            dest_clip.left;
        uint8_t* dest_scan = dest_span.data();
        for (int col = col_start; col < col_end; ++col) {
          if (!(src_scan[col / 8] & (1 << (7 - col % 8)))) {
            dest_scan[dest_col / 8] &= ~(1 << (7 - dest_col % 8));
          }
          dest_scan += dest_step;
        }
      });
    }
    return pTransBitmap;
  }

  const int nBytes = GetBPP() / 8;
  int dest_step = bYFlip ? -dest_pitch : dest_pitch;
  if (nBytes == 3) {
    dest_step -= 2;
  }
  if (bYFlip) {
    dest_span = dest_span.subspan(dest_last_row_offset);
  }

  if (nBytes == 1) {
    for (int row = row_start; row < row_end; ++row) {
      UNSAFE_TODO({
        int dest_col =
            (bXFlip ? dest_clip.right - (row - row_start) - 1 : row) -
            dest_clip.left;
        size_t dest_offset = Fx2DSizeOrDie(dest_col, nBytes);
        uint8_t* dest_scan = dest_span.subspan(dest_offset).data();
        const uint8_t* src_scan =
            GetScanline(row).subspan(col_start * nBytes).data();
        for (int col = col_start; col < col_end; ++col) {
          *dest_scan = *src_scan++;
          dest_scan += dest_step;
        }
      });
    }
    return pTransBitmap;
  }

  if (nBytes == 3) {
    for (int row = row_start; row < row_end; ++row) {
      UNSAFE_TODO({
        int dest_col =
            (bXFlip ? dest_clip.right - (row - row_start) - 1 : row) -
            dest_clip.left;
        size_t dest_offset = Fx2DSizeOrDie(dest_col, nBytes);
        uint8_t* dest_scan = dest_span.subspan(dest_offset).data();
        const uint8_t* src_scan =
            GetScanline(row).subspan(col_start * nBytes).data();
        for (int col = col_start; col < col_end; ++col) {
          FXSYS_memcpy(dest_scan, src_scan, 3);
          dest_scan += 2 + dest_step;
          src_scan += 3;
        }
      });
    }
    return pTransBitmap;
  }

  CHECK_EQ(nBytes, 4);
  for (int row = row_start; row < row_end; ++row) {
    UNSAFE_TODO({
      int dest_col = (bXFlip ? dest_clip.right - (row - row_start) - 1 : row) -
                     dest_clip.left;
      size_t dest_offset = Fx2DSizeOrDie(dest_col, nBytes);
      uint8_t* dest_scan = dest_span.subspan(dest_offset).data();
      const uint32_t* src_scan =
          GetScanlineAs<const uint32_t>(row).subspan(col_start).data();
      for (int col = col_start; col < col_end; ++col) {
        uint32_t* dest_scan32 = reinterpret_cast<uint32_t*>(dest_scan);
        *dest_scan32 = *src_scan++;
        dest_scan += dest_step;
      }
    });
  }
  return pTransBitmap;
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::TransformTo(const CFX_Matrix& mtDest,
                                                 int* result_left,
                                                 int* result_top) const {
  RetainPtr<const CFX_DIBBase> holder(this);
  CFX_ImageTransformer transformer(holder, mtDest, FXDIB_ResampleOptions(),
                                   nullptr);
  transformer.Continue(nullptr);
  *result_left = transformer.result().left;
  *result_top = transformer.result().top;
  return transformer.DetachBitmap();
}

RetainPtr<CFX_DIBitmap> CFX_DIBBase::StretchTo(
    int dest_width,
    int dest_height,
    const FXDIB_ResampleOptions& options,
    const FX_RECT* pClip) const {
  RetainPtr<const CFX_DIBBase> holder(this);
  FX_RECT clip_rect(0, 0, abs(dest_width), abs(dest_height));
  if (pClip)
    clip_rect.Intersect(*pClip);

  if (clip_rect.IsEmpty())
    return nullptr;

  if (dest_width == GetWidth() && dest_height == GetHeight()) {
    return ClipTo(clip_rect);
  }

  CFX_BitmapStorer storer;
  CFX_ImageStretcher stretcher(&storer, holder, dest_width, dest_height,
                               clip_rect, options);
  if (stretcher.Start())
    stretcher.Continue(nullptr);

  return storer.Detach();
}

// static
DataVector<uint32_t> CFX_DIBBase::ConvertBuffer(
    FXDIB_Format dest_format,
    pdfium::span<uint8_t> dest_buf,
    int dest_pitch,
    int width,
    int height,
    const RetainPtr<const CFX_DIBBase>& pSrcBitmap,
    int src_left,
    int src_top) {
  const int src_bpp = pSrcBitmap->GetBPP();
  switch (dest_format) {
    case FXDIB_Format::kInvalid:
    case FXDIB_Format::k1bppRgb:
    case FXDIB_Format::k1bppMask: {
      NOTREACHED_NORETURN();
    }
    case FXDIB_Format::k8bppMask: {
      ConvertBuffer_8bppMask(src_bpp, dest_buf, dest_pitch, width, height,
                             pSrcBitmap, src_left, src_top);
      return {};
    }
    case FXDIB_Format::k8bppRgb: {
      if (src_bpp == 1 || src_bpp == 8) {
        if (pSrcBitmap->HasPalette()) {
          return ConvertBuffer_Plt2PltRgb8(dest_buf, dest_pitch, width, height,
                                           pSrcBitmap, src_left, src_top);
        }
        return ConvertBuffer(FXDIB_Format::k8bppMask, dest_buf, dest_pitch,
                             width, height, pSrcBitmap, src_left, src_top);
      }
      CHECK_GE(src_bpp, 24);
      return ConvertBuffer_Rgb2PltRgb8(dest_buf, dest_pitch, width, height,
                                       pSrcBitmap, src_left, src_top);
    }
    case FXDIB_Format::kRgb: {
      ConvertBuffer_Rgb(src_bpp, dest_format, dest_buf, dest_pitch, width,
                        height, pSrcBitmap, src_left, src_top);
      return {};
    }
    case FXDIB_Format::kArgb:
    case FXDIB_Format::kRgb32: {
      ConvertBuffer_Argb(src_bpp, dest_format, dest_buf, dest_pitch, width,
                         height, pSrcBitmap, src_left, src_top);
      return {};
    }
  }
}
