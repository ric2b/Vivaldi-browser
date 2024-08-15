// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libweston/libweston.h>
#include <linux/input-event-codes.h>
#include <ui-controls-unstable-v1-server-protocol.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

// Needed for `notify_*()`.
#include "backend.h"
// Needed for `weston_touch_create_touch_device()`.
#include "libweston-internal.h"

static const uint32_t kUiControlsVersion = 2;

struct ui_controls_state {
  struct weston_compositor* compositor;
  struct wl_listener destroy_listener;

  struct weston_seat seat;
  struct weston_touch_device* touch_device;
  bool is_seat_initialized;
};

static int ui_controls_seat_init(struct ui_controls_state* state) {
  assert(!state->is_seat_initialized);

  weston_seat_init(&state->seat, state->compositor, "ui-controls-seat");
  state->is_seat_initialized = true;

  weston_seat_init_pointer(&state->seat);
  if (weston_seat_init_keyboard(&state->seat, NULL) < 0) {
    return -1;
  }
  weston_seat_init_touch(&state->seat);
  state->touch_device = weston_touch_create_touch_device(
      state->seat.touch_state, "ui-controls-touch-device", NULL, NULL);

  return 0;
}

static void ui_controls_seat_release(struct ui_controls_state* state) {
  assert(state->is_seat_initialized);
  state->is_seat_initialized = false;

  weston_touch_device_destroy(state->touch_device);
  state->touch_device = NULL;

  weston_seat_release(&state->seat);
  memset(&state->seat, 0, sizeof(state->seat));
}

static void maybe_translate_coordinates_from_surface_local(
    struct wl_resource* surface_resource,
    int32_t* x,
    int32_t* y) {
  assert(x && y);
  if (surface_resource) {
    struct weston_desktop_surface* surface =
        wl_resource_get_user_data(surface_resource);
    struct weston_geometry geometry;
    weston_desktop_surface_get_root_geometry(surface, &geometry);
    *x += geometry.x;
    *y += geometry.y;
  }
}

static void handle_modifiers(struct ui_controls_state* state,
                             uint32_t pressed_modifiers,
                             enum wl_keyboard_key_state key_state) {
  struct timespec time;

  if (pressed_modifiers & ZCR_UI_CONTROLS_V1_MODIFIER_SHIFT) {
    weston_compositor_get_time(&time);
    notify_key(&state->seat, &time, KEY_LEFTSHIFT, key_state,
               STATE_UPDATE_AUTOMATIC);
  }

  if (pressed_modifiers & ZCR_UI_CONTROLS_V1_MODIFIER_ALT) {
    weston_compositor_get_time(&time);
    notify_key(&state->seat, &time, KEY_LEFTALT, key_state,
               STATE_UPDATE_AUTOMATIC);
  }

  if (pressed_modifiers & ZCR_UI_CONTROLS_V1_MODIFIER_CONTROL) {
    weston_compositor_get_time(&time);
    notify_key(&state->seat, &time, KEY_LEFTCTRL, key_state,
               STATE_UPDATE_AUTOMATIC);
  }
}

static void send_key_events(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t key,
                            uint32_t key_state,
                            uint32_t pressed_modifiers,
                            uint32_t id) {
  struct ui_controls_state* state = wl_resource_get_user_data(resource);
  struct timespec time;

  handle_modifiers(state, pressed_modifiers, WL_KEYBOARD_KEY_STATE_PRESSED);

  if (key_state & ZCR_UI_CONTROLS_V1_KEY_STATE_PRESS) {
    weston_compositor_get_time(&time);
    notify_key(&state->seat, &time, key, WL_KEYBOARD_KEY_STATE_PRESSED,
               STATE_UPDATE_AUTOMATIC);
  }

  if (key_state & ZCR_UI_CONTROLS_V1_KEY_STATE_RELEASE) {
    weston_compositor_get_time(&time);
    notify_key(&state->seat, &time, key, WL_KEYBOARD_KEY_STATE_RELEASED,
               STATE_UPDATE_AUTOMATIC);
  }

  handle_modifiers(state, pressed_modifiers, WL_KEYBOARD_KEY_STATE_RELEASED);

  zcr_ui_controls_v1_send_request_processed(resource, id);
}

static void send_mouse_move(struct wl_client* client,
                            struct wl_resource* resource,
                            int32_t x,
                            int32_t y,
                            struct wl_resource* surface_resource,
                            uint32_t id) {
  struct ui_controls_state* state = wl_resource_get_user_data(resource);
  struct weston_pointer_motion_event event = {0};
  struct timespec time;
  struct weston_pointer* pointer = weston_seat_get_pointer(&state->seat);

  maybe_translate_coordinates_from_surface_local(surface_resource, &x, &y);

  event.mask = WESTON_POINTER_MOTION_ABS;
  event.abs.c = weston_coord(x, y);

  weston_compositor_get_time(&event.time);
  notify_motion(&state->seat, &time, &event);

  // Sending wl_pointer.frame happens automatically.

  zcr_ui_controls_v1_send_request_processed(resource, id);
}

static void send_mouse_button(struct wl_client* client,
                              struct wl_resource* resource,
                              uint32_t button,
                              uint32_t button_state,
                              uint32_t pressed_modifiers,
                              uint32_t id) {
  struct ui_controls_state* state = wl_resource_get_user_data(resource);
  struct timespec time;

  switch (button) {
    case ZCR_UI_CONTROLS_V1_MOUSE_BUTTON_LEFT:
      button = BTN_LEFT;
      break;
    case ZCR_UI_CONTROLS_V1_MOUSE_BUTTON_MIDDLE:
      button = BTN_MIDDLE;
      break;
    case ZCR_UI_CONTROLS_V1_MOUSE_BUTTON_RIGHT:
      button = BTN_RIGHT;
      break;
  }

  handle_modifiers(state, pressed_modifiers, WL_KEYBOARD_KEY_STATE_PRESSED);

  if (button_state & ZCR_UI_CONTROLS_V1_MOUSE_BUTTON_STATE_DOWN) {
    weston_compositor_get_time(&time);
    notify_button(&state->seat, &time, button, WL_POINTER_BUTTON_STATE_PRESSED);
  }

  if (button_state & ZCR_UI_CONTROLS_V1_MOUSE_BUTTON_STATE_UP) {
    weston_compositor_get_time(&time);
    notify_button(&state->seat, &time, button,
                  WL_POINTER_BUTTON_STATE_RELEASED);
  }

  handle_modifiers(state, pressed_modifiers, WL_KEYBOARD_KEY_STATE_RELEASED);

  // Sending wl_pointer.frame happens automatically.

  zcr_ui_controls_v1_send_request_processed(resource, id);
}

static void send_touch(struct wl_client* client,
                       struct wl_resource* resource,
                       uint32_t action,
                       uint32_t touch_id,
                       int32_t x,
                       int32_t y,
                       struct wl_resource* surface_resource,
                       uint32_t id) {
  struct ui_controls_state* state = wl_resource_get_user_data(resource);
  struct timespec time;

  maybe_translate_coordinates_from_surface_local(surface_resource, &x, &y);

  struct weston_coord_global pos = {weston_coord(x, y)};

  if (action & ZCR_UI_CONTROLS_V1_TOUCH_TYPE_PRESS) {
    weston_compositor_get_time(&time);
    notify_touch(state->touch_device, &time, touch_id, &pos, WL_TOUCH_DOWN);
  }

  if (action & ZCR_UI_CONTROLS_V1_TOUCH_TYPE_MOVE) {
    weston_compositor_get_time(&time);
    notify_touch(state->touch_device, &time, touch_id, &pos, WL_TOUCH_MOTION);
  }

  if (action & ZCR_UI_CONTROLS_V1_TOUCH_TYPE_RELEASE) {
    weston_compositor_get_time(&time);
    notify_touch(state->touch_device, &time, touch_id, NULL, WL_TOUCH_UP);
  }

  notify_touch_frame(state->touch_device);

  zcr_ui_controls_v1_send_request_processed(resource, id);
}

static const struct zcr_ui_controls_v1_interface ui_controls_implementation = {
    send_key_events, send_mouse_move, send_mouse_button, send_touch};

static void reset_inputs(struct wl_resource* resource) {
  struct ui_controls_state* state = wl_resource_get_user_data(resource);
  struct weston_keyboard* keyboard = weston_seat_get_keyboard(&state->seat);
  struct weston_pointer* pointer = weston_seat_get_pointer(&state->seat);
  struct weston_touch* touch = weston_seat_get_touch(&state->seat);
  struct timespec time;

  if (keyboard->grab_key && keyboard->keys.size > 0) {
    struct wl_array keys;
    uint32_t* key;

    wl_array_init(&keys);
    wl_array_copy(&keys, &keyboard->keys);
    wl_array_for_each(key, &keys) {
      weston_compositor_get_time(&time);
      notify_key(&state->seat, &time, *key, WL_KEYBOARD_KEY_STATE_RELEASED,
                 STATE_UPDATE_AUTOMATIC);
    }
    wl_array_release(&keys);
  }

  if (pointer->grab_button && pointer->button_count > 0) {
    while (pointer->button_count > 0) {
      weston_compositor_get_time(&time);
      notify_button(&state->seat, &time, pointer->grab_button,
                    WL_POINTER_BUTTON_STATE_RELEASED);
    }
  }

  while (touch->num_tp > 0) {
    weston_compositor_get_time(&time);
    // `touch_id` doesn't really matter here, because it only gets sent to the
    // client - but the client has already disconnected when this function runs.
    notify_touch(state->touch_device, &time, /*touch_id=*/0, NULL, WL_TOUCH_UP);
  }
  notify_touch_frame(state->touch_device);
}

static void bind_ui_controls(struct wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  struct ui_controls_state* state = data;
  struct wl_resource* resource = wl_resource_create(
      client, &zcr_ui_controls_v1_interface, kUiControlsVersion, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &ui_controls_implementation, state,
                                 &reset_inputs);
}

static void handle_compositor_destroy(struct wl_listener* listener,
                                      void* weston_compositor) {
  struct ui_controls_state* state =
      wl_container_of(listener, state, destroy_listener);

  if (state->is_seat_initialized) {
    ui_controls_seat_release(state);
  }

  free(state);
}

WL_EXPORT int wet_module_init(struct weston_compositor* compositor,
                              int* argc,
                              char* argv[]) {
  struct ui_controls_state* state = zalloc(sizeof(*state));
  if (state == NULL) {
    return -1;
  }

  if (!weston_compositor_add_destroy_listener_once(
          compositor, &state->destroy_listener, handle_compositor_destroy)) {
    free(state);
    return 0;
  }

  state->compositor = compositor;

  if (wl_global_create(compositor->wl_display, &zcr_ui_controls_v1_interface,
                       kUiControlsVersion, state, bind_ui_controls) == NULL) {
    goto out_free;
  }

  if (ui_controls_seat_init(state) == -1) {
    goto out_free;
  }

  return 0;

out_free:
  wl_list_remove(&state->destroy_listener.link);
  free(state);
  return -1;
}
