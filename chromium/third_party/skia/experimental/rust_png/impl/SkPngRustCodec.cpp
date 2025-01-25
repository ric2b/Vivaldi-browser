/*
 * Copyright 2024 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/rust_png/impl/SkPngRustCodec.h"

#include <limits>
#include <memory>
#include <utility>

#include "experimental/rust_png/ffi/FFI.rs.h"
#include "include/core/SkStream.h"
#include "include/private/SkEncodedInfo.h"
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkTemplates.h"
#include "modules/skcms/skcms.h"
#include "src/base/SkAutoMalloc.h"
#include "src/codec/SkFrameHolder.h"
#include "src/codec/SkSwizzler.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace {

SkEncodedInfo::Color ToColor(rust_png::ColorType colorType) {
    // TODO(https://crbug.com/359279096): Take `sBIT` chunk into account to
    // sometimes return `kXAlpha_Color` or `k565_Color`.  This may require
    // a small PR to expose `sBIT` chunk from the `png` crate.

    switch (colorType) {
        case rust_png::ColorType::Grayscale:
            return SkEncodedInfo::kGray_Color;
        case rust_png::ColorType::Rgb:
            return SkEncodedInfo::kRGB_Color;
        case rust_png::ColorType::GrayscaleAlpha:
            return SkEncodedInfo::kGrayAlpha_Color;
        case rust_png::ColorType::Rgba:
            return SkEncodedInfo::kRGBA_Color;
        // `Indexed` is impossible, because of `png::Transformations::EXPAND`.
        case rust_png::ColorType::Indexed:
            break;
    }
    SK_ABORT("Unexpected `rust_png::ColorType`: %d", static_cast<int>(colorType));
}

SkEncodedInfo::Alpha ToAlpha(rust_png::ColorType colorType) {
    switch (colorType) {
        case rust_png::ColorType::Grayscale:
        case rust_png::ColorType::Rgb:
            return SkEncodedInfo::kOpaque_Alpha;
        case rust_png::ColorType::GrayscaleAlpha:
        case rust_png::ColorType::Rgba:
            return SkEncodedInfo::kUnpremul_Alpha;
        // `Indexed` is impossible, because of `png::Transformations::EXPAND`.
        case rust_png::ColorType::Indexed:
            break;
    }
    SK_ABORT("Unexpected `rust_png::ColorType`: %d", static_cast<int>(colorType));
}

std::unique_ptr<SkEncodedInfo::ICCProfile> CreateColorProfile(const rust_png::Reader& reader) {
    // NOTE: This method is based on `read_color_profile` in
    // `src/codec/SkPngCodec.cpp` but has been refactored to use Rust inputs
    // instead of `libpng`.

    rust::Slice<const uint8_t> iccp;
    if (reader.try_get_iccp(iccp)) {
        skcms_ICCProfile profile;
        skcms_Init(&profile);
        if (skcms_Parse(iccp.data(), iccp.size(), &profile)) {
            return SkEncodedInfo::ICCProfile::Make(profile);
        }
    }

    if (reader.is_srgb()) {
        // TODO(https://crbug.com/362304558): Consider the intent field from the
        // `sRGB` chunk.
        return nullptr;
    }

    // Default to SRGB gamut.
    skcms_Matrix3x3 toXYZD50 = skcms_sRGB_profile()->toXYZD50;

    // Next, check for chromaticities.
    float rx = 0.0;
    float ry = 0.0;
    float gx = 0.0;
    float gy = 0.0;
    float bx = 0.0;
    float by = 0.0;
    float wx = 0.0;
    float wy = 0.0;
    if (reader.try_get_chrm(wx, wy, rx, ry, gx, gy, bx, by)) {
        skcms_Matrix3x3 tmp;
        if (skcms_PrimariesToXYZD50(rx, ry, gx, gy, bx, by, wx, wy, &tmp)) {
            toXYZD50 = tmp;
        } else {
            // Note that Blink simply returns nullptr in this case. We'll fall
            // back to srgb.
            //
            // TODO(https://crbug.com/362306048): If this implementation ends up
            // replacing the one from Blink, then we should 1) double-check that
            // we are comfortable with the difference and 2) remove this comment
            // (since the Blink code that it refers to will get removed).
        }
    }

    skcms_TransferFunction fn;
    float gamma;
    if (reader.try_get_gama(gamma)) {
        fn.a = 1.0f;
        fn.b = fn.c = fn.d = fn.e = fn.f = 0.0f;
        fn.g = 1.0f / gamma;
    } else {
        // Default to sRGB gamma if the image has color space information,
        // but does not specify gamma.
        // Note that Blink would again return nullptr in this case.
        fn = *skcms_sRGB_TransferFunction();
    }

    skcms_ICCProfile profile;
    skcms_Init(&profile);
    skcms_SetTransferFunction(&profile, &fn);
    skcms_SetXYZD50(&profile, &toXYZD50);
    return SkEncodedInfo::ICCProfile::Make(profile);
}

SkEncodedInfo CreateEncodedInfo(const rust_png::Reader& reader) {
    rust_png::ColorType rust_color = reader.output_color_type();
    SkEncodedInfo::Color sk_color = ToColor(rust_color);

    std::unique_ptr<SkEncodedInfo::ICCProfile> profile = CreateColorProfile(reader);
    if (!SkPngCodecBase::isCompatibleColorProfileAndType(profile.get(), sk_color)) {
        profile = nullptr;
    }

    return SkEncodedInfo::Make(reader.width(),
                               reader.height(),
                               sk_color,
                               ToAlpha(rust_color),
                               reader.output_bits_per_component(),
                               std::move(profile));
}

SkCodec::Result ToSkCodecResult(rust_png::DecodingResult rustResult) {
    switch (rustResult) {
        case rust_png::DecodingResult::Success:
            return SkCodec::kSuccess;
        case rust_png::DecodingResult::FormatError:
            return SkCodec::kErrorInInput;
        case rust_png::DecodingResult::ParameterError:
            return SkCodec::kInvalidParameters;
        case rust_png::DecodingResult::LimitsExceededError:
            return SkCodec::kInternalError;
    }
    SK_ABORT("Unexpected `rust_png::DecodingResult`: %d", static_cast<int>(rustResult));
}

// This helper class adapts `SkStream` to expose the API required by Rust FFI
// (i.e. the `ReadTrait` API).
class ReadTraitAdapterForSkStream final : public rust_png::ReadTrait {
public:
    // SAFETY: The caller needs to guarantee that `stream` will be alive for
    // as long as `ReadTraitAdapterForSkStream`.
    explicit ReadTraitAdapterForSkStream(SkStream* stream) : fStream(stream) {}

    ~ReadTraitAdapterForSkStream() override = default;

    // Non-copyable and non-movable (we want a stable `this` pointer, because we
    // will be passing a `ReadTrait*` pointer over the FFI boundary and
    // retaining it inside `png::Reader`).
    ReadTraitAdapterForSkStream(const ReadTraitAdapterForSkStream&) = delete;
    ReadTraitAdapterForSkStream& operator=(const ReadTraitAdapterForSkStream&) = delete;
    ReadTraitAdapterForSkStream(ReadTraitAdapterForSkStream&&) = delete;
    ReadTraitAdapterForSkStream& operator=(ReadTraitAdapterForSkStream&&) = delete;

    // Implementation of the `std::io::Read::read` method.  See `RustTrait`'s
    // doc comments and
    // https://doc.rust-lang.org/nightly/std/io/trait.Read.html#tymethod.read
    // for guidance on the desired implementation and behavior of this method.
    size_t read(rust::Slice<uint8_t> buffer) override {
        // Avoiding operating on `buffer.data()` if the slice is empty helps to avoid
        // UB risk described at https://davidben.net/2024/01/15/empty-slices.html.
        if (buffer.empty()) {
            return 0;
        }

        return fStream->read(buffer.data(), buffer.size());
    }

private:
    SkStream* fStream = nullptr;  // Non-owning pointer.
};

}  // namespace

class SkPngRustCodec::PngFrame final : public SkFrame {
public:
    PngFrame(int id, SkEncodedInfo::Alpha alpha) : SkFrame(id), fReportedAlpha(alpha) {}

private:
    SkEncodedInfo::Alpha onReportedAlpha() const override { return fReportedAlpha; };

    const SkEncodedInfo::Alpha fReportedAlpha;
};

// static
std::unique_ptr<SkPngRustCodec> SkPngRustCodec::MakeFromStream(std::unique_ptr<SkStream> stream,
                                                               Result* result) {
    SkASSERT(stream);
    SkASSERT(result);

    auto readTraitAdapter = std::make_unique<ReadTraitAdapterForSkStream>(stream.get());
    rust::Box<rust_png::ResultOfReader> resultOfReader =
            rust_png::new_reader(std::move(readTraitAdapter));
    *result = ToSkCodecResult(resultOfReader->err());
    if (*result != kSuccess) {
        return nullptr;
    }
    rust::Box<rust_png::Reader> reader = resultOfReader->unwrap();

    return std::make_unique<SkPngRustCodec>(
            CreateEncodedInfo(*reader), std::move(stream), std::move(reader));
}

SkPngRustCodec::SkPngRustCodec(SkEncodedInfo&& encodedInfo,
                               std::unique_ptr<SkStream> stream,
                               rust::Box<rust_png::Reader> reader)
        : SkPngCodecBase(std::move(encodedInfo), std::move(stream)), fReader(std::move(reader)) {
    // Initialize propoerties of the first (maybe the only) animation frame.
    constexpr int kIdOfFirstFrame = 0;
    fFrames.push_back(PngFrame(kIdOfFirstFrame, this->getEncodedInfo().alpha()));
    SkFrame& first_frame = fFrames.back();
    first_frame.setXYWH(0, 0, this->getEncodedInfo().width(), this->getEncodedInfo().height());
    first_frame.setHasAlpha(this->getEncodedInfo().alpha() == SkEncodedInfo::kUnpremul_Alpha);
    first_frame.setRequiredFrame(kNoFrame);
    // No need to call `setDuration` or `setBlend` - the default values are ok.
    //
    // TODO(https://crbug.com/356922876): Call setDisposalMethod`, based on
    // `png::FrameControl`.
}

SkPngRustCodec::~SkPngRustCodec() = default;

SkCodec::Result SkPngRustCodec::startDecoding(const SkImageInfo& dstInfo,
                                              void* pixels,
                                              size_t rowBytes,
                                              const Options& options,
                                              DecodingState* decodingState) {
    decodingState->dst = SkSpan(static_cast<uint8_t*>(pixels), rowBytes * dstInfo.height());
    decodingState->dstRowSize = rowBytes;
    decodingState->bytesPerPixel = dstInfo.bytesPerPixel();

    // TODO(https://crbug.com/356922876): Expose `png` crate's ability to decode
    // multiple frames.
    if (options.fFrameIndex != 0) {
        return kUnimplemented;
    }

    // TODO(https://crbug.com/362830091): Consider handling `fSubset` (if not
    // for `onGetPixels` then at least for `onStartIncrementalDecode`).
    if (options.fSubset) {
        return kUnimplemented;
    }

    return this->initializeXforms(dstInfo, options);
}

SkCodec::Result SkPngRustCodec::incrementalDecode(DecodingState& decodingState,
                                                  int* rowsDecodedPtr) {
    this->initializeXformParams();

    int rowsDecoded = 0;
    bool interlaced = fReader->interlaced();
    std::vector<uint8_t> decodedInterlacedFullWidthRow;
    std::vector<uint8_t> xformedInterlacedRow;
    while (true) {
        // TODO(https://crbug.com/357876243): Avoid an unconditional buffer hop
        // through buffer owned by `fReader` (e.g. when we can decode directly
        // into `dst`, because the pixel format received from `fReader` is
        // similar enough to `dstInfo`).
        rust::Slice<const uint8_t> decodedRow;

        Result result = ToSkCodecResult(fReader->next_interlaced_row(decodedRow));
        if (result != kSuccess) {
            if (result == kIncompleteInput && rowsDecodedPtr) {
                // TODO(https://crbug.com/356923435): Handle `kIncompleteInput` (right
                // now the FFI layer will never return `kIncompleteInput` but we will
                // need to handle it for incremental, row-by-row decoding).
                SkUNREACHABLE;

                *rowsDecodedPtr = rowsDecoded;
            }
            return result;
        }

        if (decodedRow.empty()) {  // This is how FFI layer says "no more rows".
            fIncrementalDecodingState.reset();
            fNumOfFullyReceivedFrames++;
            return kSuccess;
        }

        if (interlaced) {
            // Copy (potentially shorter for initial Adam7 passes) `decodedRow`
            // into a full-width `decodedInterlacedFullWidthRow`.  This is
            // needed becxause `applyXformRow` requires full-width rows as
            // input (can't change `SkSwizzler::fSrcWidth` after
            // `initializeXforms`).
            //
            // TODO(https://crbug.com/357876243): Having `Reader.read_row` API (see
            // https://github.com/image-rs/image-png/pull/493) would help avoid
            // an extra copy here.
            decodedInterlacedFullWidthRow.resize(this->getEncodedInfoRowSize(), 0x00);
            SkASSERT(decodedInterlacedFullWidthRow.size() >= decodedRow.size());
            memcpy(decodedInterlacedFullWidthRow.data(), decodedRow.data(), decodedRow.size());

            xformedInterlacedRow.resize(decodingState.dstRowSize, 0x00);
            this->applyXformRow(xformedInterlacedRow, decodedInterlacedFullWidthRow);

            fReader->expand_last_interlaced_row(rust::Slice<uint8_t>(decodingState.dst),
                                                decodingState.dstRowSize,
                                                rust::Slice<const uint8_t>(xformedInterlacedRow),
                                                decodingState.bytesPerPixel * 8);
            // `rowsDecoded` is not incremented, because full, contiguous rows
            // are not decoded until pass 6 (or 7 depending on how you look) of
            // Adam7 interlacing scheme.
        } else {
            this->applyXformRow(decodingState.dst, decodedRow);
            decodingState.dst = decodingState.dst.subspan(decodingState.dstRowSize);
            rowsDecoded++;
        }
    }
}

SkCodec::Result SkPngRustCodec::onGetPixels(const SkImageInfo& dstInfo,
                                            void* pixels,
                                            size_t rowBytes,
                                            const Options& options,
                                            int* rowsDecoded) {
    DecodingState decodingState;
    Result result = this->startDecoding(dstInfo, pixels, rowBytes, options, &decodingState);
    if (result != kSuccess) {
        return result;
    }

    return this->incrementalDecode(decodingState, rowsDecoded);
}

SkCodec::Result SkPngRustCodec::onStartIncrementalDecode(const SkImageInfo& dstInfo,
                                                         void* pixels,
                                                         size_t rowBytes,
                                                         const Options& options) {
    DecodingState decodingState;
    Result result = this->startDecoding(dstInfo, pixels, rowBytes, options, &decodingState);
    if (result != kSuccess) {
        return result;
    }

    SkASSERT(!fIncrementalDecodingState.has_value());
    fIncrementalDecodingState = decodingState;
    return kSuccess;
}

SkCodec::Result SkPngRustCodec::onIncrementalDecode(int* rowsDecoded) {
    SkASSERT(fIncrementalDecodingState.has_value());
    return this->incrementalDecode(*fIncrementalDecodingState, rowsDecoded);
}

bool SkPngRustCodec::onGetFrameInfo(int index, FrameInfo* info) const {
    if ((0 <= index) && (index < fFrames.size())) {
        if (info) {
            fFrames[index].fillIn(info, fNumOfFullyReceivedFrames > index);
        }
        return true;
    }

    return false;
}

int SkPngRustCodec::onGetRepetitionCount() {
    if (!fReader->has_actl_chunk()) {
        return 0;
    }

    uint32_t num_frames = fReader->get_actl_num_frames();
    if (num_frames <= 1) {
        return 0;
    }

    // APNG spec says that "`num_plays` indicates the number of times that this
    // animation should play; if it is 0, the animation should play
    // indefinitely."
    uint32_t num_plays = fReader->get_actl_num_plays();
    constexpr unsigned int kMaxInt = static_cast<unsigned int>(std::numeric_limits<int>::max());
    if ((num_plays == 0) || (num_plays > kMaxInt)) {
        return kRepetitionCountInfinite;
    }

    // Subtracting 1, because `SkCodec::onGetRepetitionCount` doc comment says
    // that "This number does not include the first play through of each frame.
    // For example, a repetition count of 4 means that each frame is played 5
    // times and then the animation stops."
    return num_plays - 1;
}

std::optional<SkSpan<const SkPngCodecBase::PaletteColorEntry>> SkPngRustCodec::onTryGetPlteChunk() {
    if (fReader->output_color_type() != rust_png::ColorType::Indexed) {
        return std::nullopt;
    }

    // We shouldn't get here because we always use
    // `png::Transformations::EXPAND`.
    //
    // TODO(https://crbug.com/356882657): Handle pLTE and tRNS inside
    // `SkPngRustCodec` rather than via `png::Transformations::EXPAND`.
    SkUNREACHABLE;
}

std::optional<SkSpan<const uint8_t>> SkPngRustCodec::onTryGetTrnsChunk() {
    if (fReader->output_color_type() != rust_png::ColorType::Indexed) {
        return std::nullopt;
    }

    // We shouldn't get here because we always use
    // `png::Transformations::EXPAND`.
    //
    // TODO(https://crbug.com/356882657): Handle pLTE and tRNS inside
    // `SkPngRustCodec` rather than via `png::Transformations::EXPAND`.
    SkUNREACHABLE;
}
