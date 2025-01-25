// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';

import {Time, TimeSpan, time} from '../base/time';
import {colorForCpu} from '../core/colorizer';
import {timestampFormat, TimestampFormat} from '../core/timestamp_format';

import {
  OVERVIEW_TIMELINE_NON_VISIBLE_COLOR,
  TRACK_SHELL_WIDTH,
} from './css_constants';
import {BorderDragStrategy} from './drag/border_drag_strategy';
import {DragStrategy} from './drag/drag_strategy';
import {InnerDragStrategy} from './drag/inner_drag_strategy';
import {OuterDragStrategy} from './drag/outer_drag_strategy';
import {DragGestureHandler} from './drag_gesture_handler';
import {globals} from './globals';
import {
  getMaxMajorTicks,
  MIN_PX_PER_STEP,
  generateTicks,
  TickType,
} from './gridline_helper';
import {Size} from '../base/geom';
import {Panel} from './panel_container';
import {PxSpan, TimeScale} from './time_scale';
import {HighPrecisionTimeSpan} from '../common/high_precision_time_span';

export class OverviewTimelinePanel implements Panel {
  private static HANDLE_SIZE_PX = 5;
  readonly kind = 'panel';
  readonly selectable = false;

  private width = 0;
  private gesture?: DragGestureHandler;
  private timeScale?: TimeScale;
  private dragStrategy?: DragStrategy;
  private readonly boundOnMouseMove = this.onMouseMove.bind(this);

  // Must explicitly type now; arguments types are no longer auto-inferred.
  // https://github.com/Microsoft/TypeScript/issues/1373
  onupdate({dom}: m.CVnodeDOM) {
    this.width = dom.getBoundingClientRect().width;
    const traceTime = globals.traceContext;
    if (this.width > TRACK_SHELL_WIDTH) {
      const pxSpan = new PxSpan(TRACK_SHELL_WIDTH, this.width);
      const hpTraceTime = HighPrecisionTimeSpan.fromTime(
        traceTime.start,
        traceTime.end,
      );
      this.timeScale = new TimeScale(hpTraceTime, pxSpan);
      if (this.gesture === undefined) {
        this.gesture = new DragGestureHandler(
          dom as HTMLElement,
          this.onDrag.bind(this),
          this.onDragStart.bind(this),
          this.onDragEnd.bind(this),
        );
      }
    } else {
      this.timeScale = undefined;
    }
  }

  oncreate(vnode: m.CVnodeDOM) {
    this.onupdate(vnode);
    (vnode.dom as HTMLElement).addEventListener(
      'mousemove',
      this.boundOnMouseMove,
    );
  }

  onremove({dom}: m.CVnodeDOM) {
    if (this.gesture) {
      this.gesture[Symbol.dispose]();
      this.gesture = undefined;
    }
    (dom as HTMLElement).removeEventListener(
      'mousemove',
      this.boundOnMouseMove,
    );
  }

  render(): m.Children {
    return m('.overview-timeline', {
      oncreate: (vnode) => this.oncreate(vnode),
      onupdate: (vnode) => this.onupdate(vnode),
      onremove: (vnode) => this.onremove(vnode),
    });
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: Size) {
    if (this.width === undefined) return;
    if (this.timeScale === undefined) return;
    const headerHeight = 20;
    const tracksHeight = size.height - headerHeight;
    const traceContext = new TimeSpan(
      globals.traceContext.start,
      globals.traceContext.end,
    );

    if (size.width > TRACK_SHELL_WIDTH && traceContext.duration > 0n) {
      const maxMajorTicks = getMaxMajorTicks(this.width - TRACK_SHELL_WIDTH);
      const offset = globals.timestampOffset();
      const tickGen = generateTicks(traceContext, maxMajorTicks, offset);

      // Draw time labels
      ctx.font = '10px Roboto Condensed';
      ctx.fillStyle = '#999';
      for (const {type, time} of tickGen) {
        const xPos = Math.floor(this.timeScale.timeToPx(time));
        if (xPos <= 0) continue;
        if (xPos > this.width) break;
        if (type === TickType.MAJOR) {
          ctx.fillRect(xPos - 1, 0, 1, headerHeight - 5);
          const domainTime = globals.toDomainTime(time);
          renderTimestamp(ctx, domainTime, xPos + 5, 18, MIN_PX_PER_STEP);
        } else if (type == TickType.MEDIUM) {
          ctx.fillRect(xPos - 1, 0, 1, 8);
        } else if (type == TickType.MINOR) {
          ctx.fillRect(xPos - 1, 0, 1, 5);
        }
      }
    }

    // Draw mini-tracks with quanitzed density for each process.
    if (globals.overviewStore.size > 0) {
      const numTracks = globals.overviewStore.size;
      let y = 0;
      const trackHeight = (tracksHeight - 1) / numTracks;
      for (const key of globals.overviewStore.keys()) {
        const loads = globals.overviewStore.get(key)!;
        for (let i = 0; i < loads.length; i++) {
          const xStart = Math.floor(this.timeScale.timeToPx(loads[i].start));
          const xEnd = Math.ceil(this.timeScale.timeToPx(loads[i].end));
          const yOff = Math.floor(headerHeight + y * trackHeight);
          const lightness = Math.ceil((1 - loads[i].load * 0.7) * 100);
          const color = colorForCpu(y).setHSL({s: 50, l: lightness});
          ctx.fillStyle = color.cssString;
          ctx.fillRect(xStart, yOff, xEnd - xStart, Math.ceil(trackHeight));
        }
        y++;
      }
    }

    // Draw bottom border.
    ctx.fillStyle = '#dadada';
    ctx.fillRect(0, size.height - 1, this.width, 1);

    // Draw semi-opaque rects that occlude the non-visible time range.
    const [vizStartPx, vizEndPx] = OverviewTimelinePanel.extractBounds(
      this.timeScale,
    );

    ctx.fillStyle = OVERVIEW_TIMELINE_NON_VISIBLE_COLOR;
    ctx.fillRect(
      TRACK_SHELL_WIDTH - 1,
      headerHeight,
      vizStartPx - TRACK_SHELL_WIDTH,
      tracksHeight,
    );
    ctx.fillRect(vizEndPx, headerHeight, this.width - vizEndPx, tracksHeight);

    // Draw brushes.
    ctx.fillStyle = '#999';
    ctx.fillRect(vizStartPx - 1, headerHeight, 1, tracksHeight);
    ctx.fillRect(vizEndPx, headerHeight, 1, tracksHeight);

    const hbarWidth = OverviewTimelinePanel.HANDLE_SIZE_PX;
    const hbarHeight = tracksHeight * 0.4;
    // Draw handlebar
    ctx.fillRect(
      vizStartPx - Math.floor(hbarWidth / 2) - 1,
      headerHeight,
      hbarWidth,
      hbarHeight,
    );
    ctx.fillRect(
      vizEndPx - Math.floor(hbarWidth / 2),
      headerHeight,
      hbarWidth,
      hbarHeight,
    );
  }

  private onMouseMove(e: MouseEvent) {
    if (this.gesture === undefined || this.gesture.isDragging) {
      return;
    }
    (e.target as HTMLElement).style.cursor = this.chooseCursor(e.offsetX);
  }

  private chooseCursor(x: number) {
    if (this.timeScale === undefined) return 'default';
    const [startBound, endBound] = OverviewTimelinePanel.extractBounds(
      this.timeScale,
    );
    if (
      OverviewTimelinePanel.inBorderRange(x, startBound) ||
      OverviewTimelinePanel.inBorderRange(x, endBound)
    ) {
      return 'ew-resize';
    } else if (x < TRACK_SHELL_WIDTH) {
      return 'default';
    } else if (x < startBound || endBound < x) {
      return 'crosshair';
    } else {
      return 'all-scroll';
    }
  }

  onDrag(x: number) {
    if (this.dragStrategy === undefined) return;
    this.dragStrategy.onDrag(x);
  }

  onDragStart(x: number) {
    if (this.timeScale === undefined) return;
    const pixelBounds = OverviewTimelinePanel.extractBounds(this.timeScale);
    if (
      OverviewTimelinePanel.inBorderRange(x, pixelBounds[0]) ||
      OverviewTimelinePanel.inBorderRange(x, pixelBounds[1])
    ) {
      this.dragStrategy = new BorderDragStrategy(this.timeScale, pixelBounds);
    } else if (x < pixelBounds[0] || pixelBounds[1] < x) {
      this.dragStrategy = new OuterDragStrategy(this.timeScale);
    } else {
      this.dragStrategy = new InnerDragStrategy(this.timeScale, pixelBounds);
    }
    this.dragStrategy.onDragStart(x);
  }

  onDragEnd() {
    this.dragStrategy = undefined;
  }

  private static extractBounds(timeScale: TimeScale): [number, number] {
    const vizTime = globals.timeline.visibleWindow;
    return [
      Math.floor(timeScale.hpTimeToPx(vizTime.start)),
      Math.ceil(timeScale.hpTimeToPx(vizTime.end)),
    ];
  }

  private static inBorderRange(a: number, b: number): boolean {
    return Math.abs(a - b) < this.HANDLE_SIZE_PX / 2;
  }
}

// Print a timestamp in the configured time format
function renderTimestamp(
  ctx: CanvasRenderingContext2D,
  time: time,
  x: number,
  y: number,
  minWidth: number,
): void {
  const fmt = timestampFormat();
  switch (fmt) {
    case TimestampFormat.UTC:
    case TimestampFormat.TraceTz:
    case TimestampFormat.Timecode:
      renderTimecode(ctx, time, x, y, minWidth);
      break;
    case TimestampFormat.Raw:
      ctx.fillText(time.toString(), x, y, minWidth);
      break;
    case TimestampFormat.RawLocale:
      ctx.fillText(time.toLocaleString(), x, y, minWidth);
      break;
    case TimestampFormat.Seconds:
      ctx.fillText(Time.formatSeconds(time), x, y, minWidth);
      break;
    default:
      const z: never = fmt;
      throw new Error(`Invalid timestamp ${z}`);
  }
}

// Print a timecode over 2 lines with this formatting:
// DdHH:MM:SS
// mmm uuu nnn
function renderTimecode(
  ctx: CanvasRenderingContext2D,
  time: time,
  x: number,
  y: number,
  minWidth: number,
): void {
  const timecode = Time.toTimecode(time);
  const {dhhmmss} = timecode;
  ctx.fillText(dhhmmss, x, y, minWidth);
}
