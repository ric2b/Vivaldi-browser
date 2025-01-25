// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_H_
#define COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_H_

#include <memory>
#include <optional>
#include <string>

#include "components/input/event_with_latency_info.h"
#include "components/input/input_router_impl.h"
#include "components/input/render_input_router.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "base/component_export.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/event.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}  // namespace blink

namespace ui {
class Cursor;
class LatencyInfo;
}  // namespace ui

namespace input {

class CursorManager;
class RenderInputRouter;
class RenderWidgetHostViewInputObserver;

// RenderWidgetHostViewInput is an interface implemented by an object that acts
// as the "View" portion of a RenderWidgetHost. This interface acts as a helper
// for receiving input events from the surrounding environment and forwarding
// them to the RenderWidgetHost, allowing breaking dependency on RWHV and RWHI
// for input handling including targeting and dispatching. This interface would
// assist VizCompositor Thread to implement input handling in the future with
// InputVizard.
// (https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE)
//
// The lifetime of RenderWidgetHostViewInput is tied to the lifetime of the
// renderer process. If the render process dies, the RenderWidgetHostViewInput
// goes away and all references to it must become nullptr.
class COMPONENT_EXPORT(INPUT) RenderWidgetHostViewInput :
    public StylusInterface {
 public:
  virtual base::WeakPtr<RenderWidgetHostViewInput> GetInputWeakPtr() = 0;

  virtual float GetDeviceScaleFactor() const = 0;
  // Returns true if the mouse pointer is currently locked.
  virtual bool IsPointerLocked() = 0;

  virtual RenderInputRouter* GetViewRenderInputRouter() = 0;

  virtual void ProcessTouchEvent(const blink::WebTouchEvent& event,
                                 const ui::LatencyInfo& latency) = 0;
  virtual void ProcessMouseEvent(const blink::WebMouseEvent& event,
                                 const ui::LatencyInfo& latency) = 0;
  virtual void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                                      const ui::LatencyInfo& latency) = 0;
  virtual void ProcessGestureEvent(const blink::WebGestureEvent& event,
                                   const ui::LatencyInfo& latency) = 0;

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessAckedTouchEvent releases
  // it to be consumed (when |ack_result| is NOT_CONSUMED OR NO_CONSUMER_EXISTS)
  // or ignored (when |ack_result| is CONSUMED).
  // |touch|'s coordinates are in the coordinate space of the view to which it
  // was targeted.
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) = 0;

  virtual void DidOverscroll(const ui::DidOverscrollParams& params) = 0;

  virtual void DidStopFlinging() = 0;

  // Returns the root-view associated with this view. For derived views that are
  // not embeddable, this method always returns a RenderWidgetHostViewBase
  // instance.
  virtual RenderWidgetHostViewInput* GetRootView() = 0;

  // Obtains the root window FrameSinkId.
  virtual viz::FrameSinkId GetRootFrameSinkId() = 0;

  // Returns the ID associated with the CompositorFrameSink of this view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;

  // Returns the LocalSurfaceId allocated by the parent client for this view.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() const = 0;

  // Returns the SurfaceId currently in use by the renderer to submit compositor
  // frames.
  virtual viz::SurfaceId GetCurrentSurfaceId() const = 0;

  // Called whenever the browser receives updated hit test data from viz.
  virtual void NotifyHitTestRegionUpdated(
      const viz::AggregatedHitTestRegion& region) = 0;

  // Indicates whether the widget has resized or moved within its embedding
  // page during a feature-parameter-determined time interval.
  virtual bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event) = 0;

  // See kTargetFrameMovedRecentlyForIOv2 in web_input_event.h.
  virtual bool ScreenRectIsUnstableForIOv2For(
      const blink::WebInputEvent& event) = 0;

  virtual void PreProcessTouchEvent(const blink::WebTouchEvent& event) = 0;
  virtual void PreProcessMouseEvent(const blink::WebMouseEvent& event) = 0;

  // Coordinate points received from a renderer process need to be transformed
  // to the top-level frame's coordinate space. For coordinates received from
  // the top-level frame's renderer this is a no-op as they are already
  // properly transformed; however, coordinates received from an out-of-process
  // iframe renderer process require transformation.
  virtual gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) = 0;

  // Converts a point in the root view's coordinate space to the coordinate
  // space of whichever view is used to call this method.
  virtual gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) = 0;

  // Transform a point that is in the coordinate space of a Surface that is
  // embedded within the RenderWidgetHostViewInput's Surface to the
  // coordinate space of an embedding, or embedded, Surface. Typically this
  // means that a point was received from an out-of-process iframe's
  // RenderWidget and needs to be translated to viewport coordinates for the
  // root RWHV, in which case this method is called on the root RWHV with the
  // out-of-process iframe's SurfaceId.
  // Returns false when this attempts to transform a point between coordinate
  // spaces of surfaces where one does not contain the other. To transform
  // between sibling surfaces, the point must be transformed to the root's
  // coordinate space as an intermediate step.
  virtual bool TransformPointToLocalCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point) = 0;

  // Given a RenderWidgetHostViewInput that renders to a Surface that is
  // contained within this class' Surface, find the relative transform between
  // the Surfaces and apply it to a point. Returns false if a Surface has not
  // yet been created or if |target_view| is not a descendant RWHV from our
  // client.
  virtual bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) = 0;

  // On success, returns true and modifies |*transform| to represent the
  // transformation mapping a point in the coordinate space of this view
  // into the coordinate space of the target view.
  // On Failure, returns false, and leaves |*transform| unchanged.
  // This function will fail if viz hit testing is not enabled, or if either
  // this view or the target view has no current FrameSinkId. The latter may
  // happen if either view is not currently visible in the viewport.
  // This function is useful if there are multiple points to transform between
  // the same two views. |target_view| must be non-null.
  virtual bool GetTransformToViewCoordSpace(
      RenderWidgetHostViewInput* target_view,
      gfx::Transform* transform) = 0;

  // Transforms |point| to be in the coordinate space of browser compositor's
  // surface. This is in DIP.
  virtual void TransformPointToRootSurface(gfx::PointF* point) = 0;

  // Retrieves the size of the viewport for the visible region in DIP. May be
  // smaller than the view size if a portion of the view is obstructed (e.g. by
  // a virtual keyboard).
  virtual gfx::Size GetVisibleViewportSize() = 0;

  // Returns the view into which this view is directly embedded. This can
  // return nullptr when this view's associated child frame is not connected
  // to the frame tree or when view is the root view.
  virtual RenderWidgetHostViewInput* GetParentViewInput() = 0;

  // Called prior to forwarding input event messages to the renderer, giving
  // the view a chance to perform in-process event filtering or processing.
  // Return values of |NOT_CONSUMED| or |UNKNOWN| will result in |input_event|
  // being forwarded.
  virtual blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event) = 0;

  virtual void GestureEventAck(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void WheelEventAck(
      const blink::WebMouseWheelEvent& event,
      blink::mojom::InputEventResultState ack_result) = 0;

  virtual void ChildDidAckGestureEvent(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) = 0;

  virtual void SetLastPointerType(ui::EventPointerType last_pointer_type) = 0;

  // Sets the cursor for this view to the one specified.
  virtual void UpdateCursor(const ui::Cursor& cursor) = 0;

  // Changes the cursor that is displayed on screen. This may or may not match
  // the current cursor's view which was set by UpdateCursor.
  virtual void DisplayCursor(const ui::Cursor& cursor) = 0;

  // Views that manage cursors for window return a CursorManager. Other views
  // return nullptr.
  virtual CursorManager* GetCursorManager() = 0;

  // Calls UpdateTooltip if the view is under the cursor.
  virtual void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) = 0;

  // Updates the tooltip text and displays the requested tooltip on the screen.
  // An empty string will clear a visible tooltip.
  virtual void UpdateTooltip(const std::u16string& tooltip_text) = 0;

  // If mouse wheels can only specify the number of ticks of some static
  // multiplier constant, this method returns that constant (in DIPs). If mouse
  // wheels can specify an arbitrary delta this returns 0.
  virtual int GetMouseWheelMinimumGranularity() const = 0;

  // This message is received when the stylus writable element is focused.
  // It receives the focused edit element bounds and the current caret bounds
  // needed for stylus writing service. These bounds would be empty when the
  // stylus writable element could not be focused.
  virtual void OnEditElementFocusedForStylusWriting(
      const gfx::Rect& focused_edit_bounds,
      const gfx::Rect& caret_bounds) = 0;

  virtual void OnAutoscrollStart() = 0;

  // Add and remove observers for lifetime event notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  virtual void AddObserver(
      RenderWidgetHostViewInputObserver* observer) = 0;
  virtual void RemoveObserver(
      RenderWidgetHostViewInputObserver* observer) = 0;

 protected:
  virtual void UpdateFrameSinkIdRegistration() = 0;

  // Stops flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  virtual void StopFlingingIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) = 0;

  // If |event| is a touchpad pinch or double tap event for which we've sent a
  // synthetic wheel event, forward the |event| to the renderer, subject to
  // |ack_result| which is the ACK result of the synthetic wheel.
  virtual void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) = 0;
};
}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_H_
