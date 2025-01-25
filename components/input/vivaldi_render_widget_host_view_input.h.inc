// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

public:
  virtual void VivaldiForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) {
    RenderWidgetHostViewInput::ForwardTouchpadZoomEventIfNecessary(
        event, ack_result);
  }
