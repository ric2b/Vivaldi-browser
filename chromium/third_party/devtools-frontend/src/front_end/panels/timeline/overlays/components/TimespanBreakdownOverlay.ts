// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import * as i18n from '../../../../core/i18n/i18n.js';
import * as TraceEngine from '../../../../models/trace/trace.js';
import * as ComponentHelpers from '../../../../ui/components/helpers/helpers.js';
import * as LitHtml from '../../../../ui/lit-html/lit-html.js';

import styles from './timespanBreakdownOverlay.css.js';

/**
 * An EntryBreakdown, or section, that makes up a TimespanBreakdown.
 */
export type EntryBreakdown = {
  bounds: TraceEngine.Types.Timing.TraceWindowMicroSeconds,
  label: string|LitHtml.LitTemplate,
  showDuration: boolean,
};

export class TimespanBreakdownOverlay extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-timespan-breakdown-overlay`;
  /**
   * Size to stagger sections of a TimespanBreakdownOverlay.
   */
  static readonly TIMESPAN_BREAKDOWN_OVERLAY_STAGGER_PX = 5;

  readonly #shadow = this.attachShadow({mode: 'open'});
  readonly #boundRender = this.#render.bind(this);
  #canvasRect: DOMRect|null = null;
  #sections: Array<EntryBreakdown>|null = null;

  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [styles];
    this.#render();
  }

  set canvasRect(rect: DOMRect|null) {
    if (this.#canvasRect && rect && this.#canvasRect.width === rect.width && this.#canvasRect.height === rect.height) {
      return;
    }
    this.#canvasRect = rect;
    void ComponentHelpers.ScheduledRender.scheduleRender(this, this.#boundRender);
  }

  set sections(sections: Array<EntryBreakdown>|null) {
    if (sections === this.#sections) {
      return;
    }
    this.#sections = sections;
    void ComponentHelpers.ScheduledRender.scheduleRender(this, this.#boundRender);
  }

  /**
   * We use this method after the overlay has been positioned in order to move
   * the section label as required to keep it on screen.
   * If the label is off to the left or right, we fix it to that corner and
   * align the text so the label is visible as long as possible.
   */
  checkSectionLabelPositioning(): void {
    const sections = this.#shadow.querySelectorAll<HTMLElement>('.timespan-breakdown-overlay-section');
    if (!sections) {
      return;
    }

    if (!this.#canvasRect) {
      return;
    }

    // On the RHS of the panel a scrollbar can be shown which means the canvas
    // has a 9px gap on the right hand edge. We use this value when calculating
    // values and label positioning from the left hand side in order to be
    // consistent on both edges of the UI.
    const paddingForScrollbar = 9;

    // Fetch the rects for each section and label now, rather than in the loop,
    // to avoid causing a bunch of recalcStyles
    const sectionLayoutData = new Map<HTMLElement, {sectionRect: DOMRect, labelRect: DOMRect, label: HTMLElement}>();
    for (const section of sections) {
      const label = section.querySelector<HTMLElement>('.timespan-breakdown-overlay-label');
      if (!label) {
        continue;
      }
      const sectionRect = section.getBoundingClientRect();
      const labelRect = label.getBoundingClientRect();
      sectionLayoutData.set(section, {sectionRect, labelRect, label});
    }

    const minSectionWidthToShowAnyLabel = 30;

    // Align the labels for all the breakdown sections.
    for (const section of sections) {
      const layoutData = sectionLayoutData.get(section);
      if (!layoutData) {
        break;
      }
      const {labelRect, sectionRect, label} = layoutData;

      const labelHidden = sectionRect.width < minSectionWidthToShowAnyLabel;
      // Subtract 5 from the section width to allow a tiny bit of padding.
      const labelTruncated = sectionRect.width - 5 <= labelRect.width;
      // We differentiate between hidden + truncated; if it is truncated we
      // will show the text with ellipsis for overflow, but if the section is
      // really small we just hide the label entirely.
      label.classList.toggle('labelHidden', labelHidden);
      label.classList.toggle('labelTruncated', labelTruncated);

      if (labelHidden || labelTruncated) {
        // Label is hidden or doesn't fully fit, so we don't need to do the
        // logic to left/right align if it needs it.
        continue;
      }

      // Check if label is off the LHS of the screen.
      const labelLeftMarginToCenter = (sectionRect.width - labelRect.width) / 2;
      const newLabelX = sectionRect.x + labelLeftMarginToCenter;

      const labelOffLeftOfScreen = newLabelX < this.#canvasRect.x;
      label.classList.toggle('offScreenLeft', labelOffLeftOfScreen);

      // Check if label is off the RHS of the screen
      const rightBound = this.#canvasRect.x + this.#canvasRect.width;
      // The label's right hand edge is the gap from the left of the range to the
      // label, and then the width of the label.
      const labelRightEdge = sectionRect.x + labelLeftMarginToCenter + labelRect.width;
      const labelOffRightOfScreen = labelRightEdge > rightBound;
      label.classList.toggle('offScreenRight', labelOffRightOfScreen);

      if (labelOffLeftOfScreen) {
        // If the label is off the left of the screen, we adjust by the
        // difference between the X that represents the start of the cavnas, and
        // the X that represents the start of the overlay.
        // We then take the absolute value of this - because if the canvas starts
        // at 0, and the overlay is -200px, we have to adjust the label by +200.
        // Add on 9 pixels to pad from the left; this is the width of the sidebar
        // on the RHS so we match it so the label is equally padded on either
        // side.
        label.style.marginLeft = `${Math.abs(this.#canvasRect.x - sectionRect.x) + paddingForScrollbar}px`;

      } else if (labelOffRightOfScreen) {
        // To calculate how far left to push the label, we take the right hand
        // bound (the canvas width and subtract the label's width).
        // Finally, we subtract the X position of the overlay (if the overlay is
        // 200px within the view, we don't need to push the label that 200px too
        // otherwise it will be off-screen)
        const leftMargin = rightBound - labelRect.width - sectionRect.x;

        label.style.marginLeft = `${leftMargin}px`;

      } else {
        // Keep the label central.
        label.style.marginLeft = `${labelLeftMarginToCenter}px`;
      }
    }
  }

  renderSection(section: EntryBreakdown): LitHtml.TemplateResult {
    const sectionRange = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(section.bounds.range);
    // clang-format off
    return LitHtml.html`
      <div class="timespan-breakdown-overlay-section">
        <div class="timespan-breakdown-overlay-label">
        ${section.showDuration ?
          LitHtml.html`
            <span class="duration-text">${i18n.TimeUtilities.preciseMillisToString(sectionRange, 2)}</span>
          ` : LitHtml.nothing}
          ${section.label}
        </div>
      </div>`;
    // clang-format on
  }

  #render(): void {
    LitHtml.render(LitHtml.html`${this.#sections?.map(this.renderSection)}`, this.#shadow, {host: this});
    this.checkSectionLabelPositioning();
  }
}

customElements.define('devtools-timespan-breakdown-overlay', TimespanBreakdownOverlay);

declare global {
  interface HTMLElementTagNameMap {
    'devtools-timespan-breakdown-overlay': TimespanBreakdownOverlay;
  }
}
