// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
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

import {currentTargetOffset} from '../base/dom_utils';
import {Icons} from '../base/semantic_icons';
import {Time} from '../base/time';
import {Actions} from '../common/actions';
import {randomColor} from '../core/colorizer';
import {SpanNote, Note, Selection} from '../common/state';
import {raf} from '../core/raf_scheduler';
import {Button, ButtonBar} from '../widgets/button';

import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {getMaxMajorTicks, generateTicks, TickType} from './gridline_helper';
import {Size} from '../base/geom';
import {Panel} from './panel_container';
import {isTraceLoaded} from './sidebar';
import {Timestamp} from './widgets/timestamp';
import {uuidv4} from '../base/uuid';
import {assertUnreachable} from '../base/logging';
import {DetailsPanel} from '../public';
import {PxSpan, TimeScale} from './time_scale';
import {canvasClip} from '../common/canvas_utils';

const FLAG_WIDTH = 16;
const AREA_TRIANGLE_WIDTH = 10;
const FLAG = `\uE153`;

function toSummary(s: string) {
  const newlineIndex = s.indexOf('\n') > 0 ? s.indexOf('\n') : s.length;
  return s.slice(0, Math.min(newlineIndex, s.length, 16));
}

function getStartTimestamp(note: Note | SpanNote) {
  const noteType = note.noteType;
  switch (noteType) {
    case 'SPAN':
      return note.start;
    case 'DEFAULT':
      return note.timestamp;
    default:
      assertUnreachable(noteType);
  }
}

export class NotesPanel implements Panel {
  readonly kind = 'panel';
  readonly selectable = false;
  private timescale?: TimeScale; // The timescale from the last render()
  private hoveredX: null | number = null;

  render(): m.Children {
    const allCollapsed = Object.values(globals.state.trackGroups).every(
      (group) => group.collapsed,
    );

    return m(
      '.notes-panel',
      {
        onclick: (e: MouseEvent) => {
          const {x, y} = currentTargetOffset(e);
          this.onClick(x - TRACK_SHELL_WIDTH, y);
          e.stopPropagation();
        },
        onmousemove: (e: MouseEvent) => {
          this.hoveredX = currentTargetOffset(e).x - TRACK_SHELL_WIDTH;
          raf.scheduleRedraw();
        },
        onmouseenter: (e: MouseEvent) => {
          this.hoveredX = currentTargetOffset(e).x - TRACK_SHELL_WIDTH;
          raf.scheduleRedraw();
        },
        onmouseout: () => {
          this.hoveredX = null;
          globals.dispatch(Actions.setHoveredNoteTimestamp({ts: Time.INVALID}));
        },
      },
      isTraceLoaded() &&
        m(
          ButtonBar,
          {className: 'pf-toolbar'},
          m(Button, {
            onclick: (e: Event) => {
              e.preventDefault();
              if (allCollapsed) {
                globals.commandManager.runCommand(
                  'dev.perfetto.CoreCommands#ExpandAllGroups',
                );
              } else {
                globals.commandManager.runCommand(
                  'dev.perfetto.CoreCommands#CollapseAllGroups',
                );
              }
            },
            title: allCollapsed ? 'Expand all' : 'Collapse all',
            icon: allCollapsed ? 'unfold_more' : 'unfold_less',
            compact: true,
          }),
          m(Button, {
            onclick: (e: Event) => {
              e.preventDefault();
              globals.dispatch(Actions.clearAllPinnedTracks({}));
            },
            title: 'Clear all pinned tracks',
            icon: 'clear_all',
            compact: true,
          }),
        ),
    );
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: Size) {
    ctx.fillStyle = '#999';
    ctx.fillRect(TRACK_SHELL_WIDTH - 2, 0, 2, size.height);

    const trackSize = {...size, width: size.width - TRACK_SHELL_WIDTH};

    ctx.save();
    ctx.translate(TRACK_SHELL_WIDTH, 0);
    canvasClip(ctx, 0, 0, trackSize.width, trackSize.height);
    this.renderPanel(ctx, trackSize);
    ctx.restore();
  }

  private renderPanel(ctx: CanvasRenderingContext2D, size: Size): void {
    let aNoteIsHovered = false;

    const visibleWindow = globals.timeline.visibleWindow;
    const timescale = new TimeScale(visibleWindow, new PxSpan(0, size.width));
    const timespan = visibleWindow.toTimeSpan();

    this.timescale = timescale;

    if (size.width > 0 && timespan.duration > 0n) {
      const maxMajorTicks = getMaxMajorTicks(size.width);
      const offset = globals.timestampOffset();
      const tickGen = generateTicks(timespan, maxMajorTicks, offset);
      for (const {type, time} of tickGen) {
        const px = Math.floor(timescale.timeToPx(time));
        if (type === TickType.MAJOR) {
          ctx.fillRect(px, 0, 1, size.height);
        }
      }
    }

    ctx.textBaseline = 'bottom';
    ctx.font = '10px Helvetica';

    for (const note of Object.values(globals.state.notes)) {
      const timestamp = getStartTimestamp(note);
      // TODO(hjd): We should still render area selection marks in viewport is
      // *within* the area (e.g. both lhs and rhs are out of bounds).
      if (
        (note.noteType === 'DEFAULT' &&
          !visibleWindow.contains(note.timestamp)) ||
        (note.noteType === 'SPAN' &&
          !visibleWindow.overlaps(note.start, note.end))
      ) {
        continue;
      }
      const currentIsHovered =
        this.hoveredX !== null && this.hitTestNote(this.hoveredX, note);
      if (currentIsHovered) aNoteIsHovered = true;

      const selection = globals.state.selection;
      const isSelected = selection.kind === 'note' && selection.id === note.id;
      const x = timescale.timeToPx(timestamp);
      const left = Math.floor(x);

      // Draw flag or marker.
      if (note.noteType === 'SPAN') {
        this.drawAreaMarker(
          ctx,
          left,
          Math.floor(timescale.timeToPx(note.end)),
          note.color,
          isSelected,
        );
      } else {
        this.drawFlag(ctx, left, size.height, note.color, isSelected);
      }

      if (note.text) {
        const summary = toSummary(note.text);
        const measured = ctx.measureText(summary);
        // Add a white semi-transparent background for the text.
        ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.fillRect(
          left + FLAG_WIDTH + 2,
          size.height + 2,
          measured.width + 2,
          -12,
        );
        ctx.fillStyle = '#3c4b5d';
        ctx.fillText(summary, left + FLAG_WIDTH + 3, size.height + 1);
      }
    }

    // A real note is hovered so we don't need to see the preview line.
    // TODO(hjd): Change cursor to pointer here.
    if (aNoteIsHovered) {
      globals.dispatch(Actions.setHoveredNoteTimestamp({ts: Time.INVALID}));
    }

    // View preview note flag when hovering on notes panel.
    if (!aNoteIsHovered && this.hoveredX !== null) {
      const timestamp = timescale.pxToHpTime(this.hoveredX).toTime();
      if (visibleWindow.contains(timestamp)) {
        globals.dispatch(Actions.setHoveredNoteTimestamp({ts: timestamp}));
        const x = timescale.timeToPx(timestamp);
        const left = Math.floor(x);
        this.drawFlag(ctx, left, size.height, '#aaa', /* fill */ true);
      }
    }

    ctx.restore();
  }

  private drawAreaMarker(
    ctx: CanvasRenderingContext2D,
    x: number,
    xEnd: number,
    color: string,
    fill: boolean,
  ) {
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    const topOffset = 10;
    // Don't draw in the track shell section.
    if (x >= 0) {
      // Draw left triangle.
      ctx.beginPath();
      ctx.moveTo(x, topOffset);
      ctx.lineTo(x, topOffset + AREA_TRIANGLE_WIDTH);
      ctx.lineTo(x + AREA_TRIANGLE_WIDTH, topOffset);
      ctx.lineTo(x, topOffset);
      if (fill) ctx.fill();
      ctx.stroke();
    }
    // Draw right triangle.
    ctx.beginPath();
    ctx.moveTo(xEnd, topOffset);
    ctx.lineTo(xEnd, topOffset + AREA_TRIANGLE_WIDTH);
    ctx.lineTo(xEnd - AREA_TRIANGLE_WIDTH, topOffset);
    ctx.lineTo(xEnd, topOffset);
    if (fill) ctx.fill();
    ctx.stroke();

    // Start line after track shell section, join triangles.
    const startDraw = Math.max(x, 0);
    ctx.beginPath();
    ctx.moveTo(startDraw, topOffset);
    ctx.lineTo(xEnd, topOffset);
    ctx.stroke();
  }

  private drawFlag(
    ctx: CanvasRenderingContext2D,
    x: number,
    height: number,
    color: string,
    fill?: boolean,
  ) {
    const prevFont = ctx.font;
    const prevBaseline = ctx.textBaseline;
    ctx.textBaseline = 'alphabetic';
    // Adjust height for icon font.
    ctx.font = '24px Material Symbols Sharp';
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    // The ligatures have padding included that means the icon is not drawn
    // exactly at the x value. This adjusts for that.
    const iconPadding = 6;
    if (fill) {
      ctx.fillText(FLAG, x - iconPadding, height + 2);
    } else {
      ctx.strokeText(FLAG, x - iconPadding, height + 2.5);
    }
    ctx.font = prevFont;
    ctx.textBaseline = prevBaseline;
  }

  private onClick(x: number, _: number) {
    if (!this.timescale) {
      return;
    }

    // Select the hovered note, or create a new single note & select it
    if (x < 0) return;
    for (const note of Object.values(globals.state.notes)) {
      if (this.hoveredX !== null && this.hitTestNote(this.hoveredX, note)) {
        globals.makeSelection(Actions.selectNote({id: note.id}));
        return;
      }
    }
    const timestamp = this.timescale.pxToHpTime(x).toTime();
    const id = uuidv4();
    const color = randomColor();
    globals.dispatchMultiple([
      Actions.addNote({id, timestamp, color}),
      Actions.selectNote({id}),
    ]);
  }

  private hitTestNote(x: number, note: SpanNote | Note): boolean {
    if (!this.timescale) {
      return false;
    }

    const timescale = this.timescale;
    const noteX = timescale.timeToPx(getStartTimestamp(note));
    if (note.noteType === 'SPAN') {
      return (
        (noteX <= x && x < noteX + AREA_TRIANGLE_WIDTH) ||
        (timescale.timeToPx(note.end) > x &&
          x > timescale.timeToPx(note.end) - AREA_TRIANGLE_WIDTH)
      );
    } else {
      const width = FLAG_WIDTH;
      return noteX <= x && x < noteX + width;
    }
  }
}

export class NotesEditorTab implements DetailsPanel {
  render(selection: Selection) {
    if (selection.kind !== 'note') {
      return undefined;
    }

    const id = selection.id;

    const note = globals.state.notes[id];
    if (note === undefined) {
      return m('.', `No Note with id ${id}`);
    }
    const startTime = getStartTimestamp(note);
    return m(
      '.notes-editor-panel',
      m(
        '.notes-editor-panel-heading-bar',
        m(
          '.notes-editor-panel-heading',
          `Annotation at `,
          m(Timestamp, {ts: startTime}),
        ),
        m('input[type=text]', {
          value: note.text,
          onchange: (e: InputEvent) => {
            const newText = (e.target as HTMLInputElement).value;
            globals.dispatch(
              Actions.changeNoteText({
                id,
                newText,
              }),
            );
          },
        }),
        m(
          'span.color-change',
          `Change color: `,
          m('input[type=color]', {
            value: note.color,
            onchange: (e: Event) => {
              const newColor = (e.target as HTMLInputElement).value;
              globals.dispatch(
                Actions.changeNoteColor({
                  id,
                  newColor,
                }),
              );
            },
          }),
        ),
        m(Button, {
          label: 'Remove',
          icon: Icons.Delete,
          onclick: () => {
            globals.dispatch(Actions.removeNote({id}));
            raf.scheduleFullRedraw();
          },
        }),
      ),
    );
  }
}
