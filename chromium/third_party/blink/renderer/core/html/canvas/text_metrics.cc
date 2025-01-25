// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_baselines.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(const TextBaseline& text_baseline,
                                   const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case kTopTextBaseline:
      return font_data.NormalizedTypoAscent().ToFloat();
    case kHangingTextBaseline:
      if (font_metrics.HangingBaseline().has_value()) {
        return font_metrics.HangingBaseline().value();
      }
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent(kAlphabeticBaseline,
                                      FontMetrics::ApplyBaselineTable(true)) *
             kHangingAsPercentOfAscent / 100.0;
    case kIdeographicTextBaseline:
      if (font_metrics.IdeographicBaseline().has_value()) {
        return font_metrics.IdeographicBaseline().value();
      }
      return -font_metrics.FloatDescent(kAlphabeticBaseline,
                                        FontMetrics::ApplyBaselineTable(true));
    case kBottomTextBaseline:
      return -font_data.NormalizedTypoDescent().ToFloat();
    case kMiddleTextBaseline: {
      const FontHeight metrics = font_data.NormalizedTypoAscentAndDescent();
      return (metrics.ascent.ToFloat() - metrics.descent.ToFloat()) / 2.0f;
    }
    case kAlphabeticTextBaseline:
      if (font_metrics.AlphabeticBaseline().has_value()) {
        return font_metrics.AlphabeticBaseline().value();
      }
      return 0;
    default:
      // Do nothing.
      return 0;
  }
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
  visitor->Trace(font_);
  visitor->Trace(runs_with_offset_);
  ScriptWrappable::Trace(visitor);
}

TextMetrics::TextMetrics() : baselines_(Baselines::Create()) {}

TextMetrics::TextMetrics(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text)
    : TextMetrics() {
  Update(font, direction, baseline, align, text);
}

void TextMetrics::Update(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text) {
  const SimpleFontData* font_data = font.PrimaryFont();
  if (!font_data)
    return;

  font_ = font;
  text_length_ = text.length();
  direction_ = direction;
  runs_with_offset_.clear();
  shaping_needed_ = true;

  // x direction
  // Run bidi algorithm on the given text. Step 5 of:
  // https://html.spec.whatwg.org/multipage/canvas.html#text-preparation-algorithm
  gfx::RectF glyph_bounds;
  String text16 = text;
  text16.Ensure16Bit();
  BidiParagraph bidi;
  bidi.SetParagraph(text16, direction);
  BidiParagraph::Runs runs;
  bidi.GetVisualRuns(text16, &runs);
  float xpos = 0;
  runs_with_offset_.reserve(runs.size());
  for (const auto& run : runs) {
    // Measure each run.
    TextRun text_run(StringView(text, run.start, run.Length()), run.Direction(),
                     /* directional_override */ false);
    text_run.SetNormalizeSpace(true);
    gfx::RectF run_glyph_bounds;
    float run_width = font.Width(text_run, &run_glyph_bounds);

    // Save the run for computing selection boxes. It will be shaped the first
    // time it is used.
    RunWithOffset run_with_offset = {
        .shape_result_ = nullptr,
        .text_ = text_run.ToStringView().ToString(),
        .direction_ = run.Direction(),
        .character_offset_ = run.start,
        .num_characters_ = run.Length(),
        .x_position_ = xpos};
    runs_with_offset_.push_back(run_with_offset);

    // Accumulate the position and the glyph bounding box.
    run_glyph_bounds.Offset(xpos, 0);
    glyph_bounds.Union(run_glyph_bounds);
    xpos += run_width;
  }
  double real_width = xpos;
  width_ = real_width;

  text_align_dx_ = 0.0f;
  if (align == kCenterTextAlign) {
    text_align_dx_ = real_width / 2.0f;
  } else if (align == kRightTextAlign ||
             (align == kStartTextAlign && direction == TextDirection::kRtl) ||
             (align == kEndTextAlign && direction != TextDirection::kRtl)) {
    text_align_dx_ = real_width;
  }
  actual_bounding_box_left_ = -glyph_bounds.x() + text_align_dx_;
  actual_bounding_box_right_ = glyph_bounds.right() - text_align_dx_;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  const float descent = font_metrics.FloatDescent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -glyph_bounds.y() - baseline_y;
  actual_bounding_box_descent_ = glyph_bounds.bottom() + baseline_y;
  // TODO(kojii): We use normalized sTypoAscent/Descent here, but this should be
  // revisited when the spec evolves.
  const FontHeight normalized_typo_metrics =
      font_data->NormalizedTypoAscentAndDescent();
  em_height_ascent_ = normalized_typo_metrics.ascent - baseline_y;
  em_height_descent_ = normalized_typo_metrics.descent + baseline_y;

  // Setting baselines:
  if (font_metrics.AlphabeticBaseline().has_value()) {
    baselines_->setAlphabetic(font_metrics.AlphabeticBaseline().value() -
                              baseline_y);
  } else {
    baselines_->setAlphabetic(-baseline_y);
  }

  if (font_metrics.HangingBaseline().has_value()) {
    baselines_->setHanging(font_metrics.HangingBaseline().value() - baseline_y);
  } else {
    baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                           baseline_y);
  }

  if (font_metrics.IdeographicBaseline().has_value()) {
    baselines_->setIdeographic(font_metrics.IdeographicBaseline().value() -
                               baseline_y);
  } else {
    baselines_->setIdeographic(-descent - baseline_y);
  }
}

const ShapeResult* ShapeWord(const TextRun& word_run, const Font& font) {
  ShapeResultSpacing<TextRun> spacing(word_run);
  spacing.SetSpacingAndExpansion(font.GetFontDescription());
  HarfBuzzShaper shaper(word_run.ToStringView().ToString());
  ShapeResult* shape_result = shaper.Shape(&font, word_run.Direction());
  if (!spacing.HasSpacing()) {
    return shape_result;
  }
  return shape_result->ApplySpacingToCopy(spacing, word_run);
}

void TextMetrics::ShapeTextIfNeeded() {
  if (!shaping_needed_) {
    return;
  }
  for (auto& run : runs_with_offset_) {
    TextRun word_run(run.text_, run.direction_, false);
    run.shape_result_ = ShapeWord(word_run, font_);
  }
  shaping_needed_ = false;
}

const HeapVector<Member<DOMRectReadOnly>> TextMetrics::getSelectionRects(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  HeapVector<Member<DOMRectReadOnly>> selection_rects;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start > text_length_ || end > text_length_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start > text_length_ ? "start" : "end"));
    return selection_rects;
  }

  ShapeTextIfNeeded();
  const double height = font_bounding_box_ascent_ + font_bounding_box_descent_;
  const double y = -font_bounding_box_ascent_;

  for (const auto& run_with_offset : runs_with_offset_) {
    const unsigned int run_start_index = run_with_offset.character_offset_;
    const unsigned int run_end_index =
        run_start_index + run_with_offset.num_characters_;

    // Handle start >= end case the same way the DOM does, returning a
    // zero-width rect after the advance of the character right before the end
    // position. If the position is mid-cluster, the whole cluster is added as a
    // rect.
    if (start >= end) {
      if (run_start_index <= end && end <= run_end_index) {
        const unsigned int index =
            base::CheckSub(end, run_start_index).ValueOrDie();
        float from_x =
            run_with_offset.shape_result_->CaretPositionForOffset(
                index, run_with_offset.text_, AdjustMidCluster::kToStart) +
            run_with_offset.x_position_;
        float to_x =
            run_with_offset.shape_result_->CaretPositionForOffset(
                index, run_with_offset.text_, AdjustMidCluster::kToEnd) +
            run_with_offset.x_position_;
        if (from_x < to_x) {
          selection_rects.push_back(DOMRectReadOnly::Create(
              from_x - text_align_dx_, y, to_x - from_x, height));
        } else {
          selection_rects.push_back(DOMRectReadOnly::Create(
              to_x - text_align_dx_, y, from_x - to_x, height));
        }
      }
      continue;
    }

    // Outside the required interval.
    if (run_end_index <= start || run_start_index >= end) {
      continue;
    }

    // Calculate the required indexes for this specific run.
    const unsigned int starting_index =
        start > run_start_index ? start - run_start_index : 0;
    const unsigned int ending_index = end < run_end_index
                                          ? end - run_start_index
                                          : run_with_offset.num_characters_;

    // Use caret positions to determine the start and end of the selection rect.
    float from_x =
        run_with_offset.shape_result_->CaretPositionForOffset(
            starting_index, run_with_offset.text_, AdjustMidCluster::kToStart) +
        run_with_offset.x_position_;
    float to_x =
        run_with_offset.shape_result_->CaretPositionForOffset(
            ending_index, run_with_offset.text_, AdjustMidCluster::kToEnd) +
        run_with_offset.x_position_;
    if (from_x < to_x) {
      selection_rects.push_back(DOMRectReadOnly::Create(
          from_x - text_align_dx_, y, to_x - from_x, height));
    } else {
      selection_rects.push_back(DOMRectReadOnly::Create(
          to_x - text_align_dx_, y, from_x - to_x, height));
    }
  }

  return selection_rects;
}

const DOMRectReadOnly* TextMetrics::getActualBoundingBox(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  gfx::RectF bounding_box;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start >= text_length_ || end > text_length_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start >= text_length_ ? "start" : "end"));
    return DOMRectReadOnly::FromRectF(bounding_box);
  }

  ShapeTextIfNeeded();

  for (const auto& run_with_offset : runs_with_offset_) {
    const unsigned int run_start_index = run_with_offset.character_offset_;
    const unsigned int run_end_index =
        run_start_index + run_with_offset.num_characters_;

    // Outside the required interval.
    if (run_end_index <= start || run_start_index >= end) {
      continue;
    }

    // Position of the left border for this run.
    const double left_border = run_with_offset.x_position_;

    // Calculate the required indexes for this specific run.
    const unsigned int starting_index =
        start > run_start_index ? start - run_start_index : 0;
    const unsigned int ending_index = end < run_end_index
                                          ? end - run_start_index
                                          : run_with_offset.num_characters_;

    const ShapeResultView* view = ShapeResultView::Create(
        run_with_offset.shape_result_, 0, run_with_offset.num_characters_);
    view->ForEachGlyph(
        left_border, starting_index, ending_index, 0,
        [](void* context, unsigned character_index, Glyph glyph,
           gfx::Vector2dF glyph_offset, float total_advance, bool is_horizontal,
           CanvasRotationInVertical rotation, const SimpleFontData* font_data) {
          auto* bounding_box = static_cast<gfx::RectF*>(context);
          gfx::RectF glyph_bounds = font_data->BoundsForGlyph(glyph);
          glyph_bounds.Offset(total_advance, 0.0);
          glyph_bounds.Offset(glyph_offset);
          bounding_box->Union(glyph_bounds);
        },
        static_cast<void*>(&bounding_box));
  }
  bounding_box.Offset(-text_align_dx_, baseline_y);
  return DOMRectReadOnly::FromRectF(bounding_box);
}

unsigned TextMetrics::caretPositionFromPoint(double x) {
  if (runs_with_offset_.empty()) {
    return 0;
  }

  // x is visual direction from the alignment point, regardless of the text
  // direction. Note x can be negative, to enable positions to the left of the
  // alignment point.
  float target_x = text_align_dx_ + x;

  // If to the left (or right), return the leftmost (or rightmost) index
  if (target_x <= 0) {
    const auto& run_with_offset = runs_with_offset_.front();
    if (IsLtr(run_with_offset.direction_)) {
      // The 0 offset within the run is leftmost
      return run_with_offset.character_offset_;
    } else {
      // The highest offset is leftmost.
      return run_with_offset.num_characters_ +
             run_with_offset.character_offset_;
    }
  }
  if (target_x >= width_) {
    const auto& run_with_offset = runs_with_offset_.back();
    if (IsLtr(run_with_offset.direction_)) {
      // The max offset within the run is rightmost
      return run_with_offset.num_characters_ +
             run_with_offset.character_offset_;
    } else {
      // The 0 offset is rightmost.
      return run_with_offset.character_offset_;
    }
  }

  ShapeTextIfNeeded();

  for (HeapVector<RunWithOffset>::reverse_iterator riter =
           runs_with_offset_.rbegin();
       riter != runs_with_offset_.rend(); riter++) {
    if (riter->x_position_ <= target_x) {
      float run_x = target_x - riter->x_position_;
      unsigned run_offset = riter->shape_result_->CaretOffsetForHitTest(
          run_x, StringView(riter->text_), BreakGlyphsOption(true));
      return run_offset + riter->character_offset_;
    }
  }
  return 0;
}

}  // namespace blink
