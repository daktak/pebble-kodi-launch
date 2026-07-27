#include <pebble.h>
#include "layout.h"
#include "playback.h"
#include "volume.h"

typedef enum { MODE_PLAYER, MODE_SKIPPER, MODE_NAV } PlaybackMode;

static Window *s_main_window = NULL;
static TextLayer *s_title_layer;
static TextLayer *s_mainline_layer = NULL;
static TextLayer *s_subline_layer = NULL;
static TextLayer *s_elapsed_layer = NULL;
static ActionBarLayer *s_action_bar_layer;

static PlaybackMode s_mode = MODE_PLAYER;
static char s_play_status[16] = "Stopped";

static GBitmap *s_play_bitmap;
static GBitmap *s_pause_bitmap;
static GBitmap *s_skip_rev_bitmap;
static GBitmap *s_skip_fwd_bitmap;
static GBitmap *s_vol_up_bitmap;
static GBitmap *s_vol_down_bitmap;
static GBitmap *s_stop_bitmap;
static GBitmap *s_up_bitmap_nav;
static GBitmap *s_down_bitmap_nav;
static GBitmap *s_check_bitmap;

static void click_config_provider(void *context);

static void update_action_bar() {
  switch (s_mode) {
    case MODE_PLAYER:
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, s_vol_up_bitmap);
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, s_vol_down_bitmap);
      if (!strcmp(s_play_status, "Playing")) {
        action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, s_pause_bitmap);
      } else {
        action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, s_play_bitmap);
      }
      break;
    case MODE_SKIPPER:
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, s_skip_rev_bitmap);
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, s_skip_fwd_bitmap);
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, s_stop_bitmap);
      break;
    case MODE_NAV:
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, s_up_bitmap_nav);
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, s_down_bitmap_nav);
      action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, s_check_bitmap);
      break;
  }
}

static void send_outbox(uint16_t key, uint8_t value) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, key, &value, 1, true);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void send_nav(uint8_t key) {
  send_outbox(KEY_NAV_KEYPRESS, key);
}

static void skip_rev() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Skip Rev");
  send_outbox(KEY_SKIP_REV, 1);
}

static void skip_fwd() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Skip Fwd");
  send_outbox(KEY_SKIP_FWD, 1);
}

static void goto_prev() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Goto prev");
  send_outbox(KEY_GOTO_PREV, 1);
}

static void goto_next() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Goto next");
  send_outbox(KEY_GOTO_NEXT, 1);
}

static void play_pause() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Play/Pause");
  send_outbox(KEY_PAUSE, 1);
  if (!strcmp(s_play_status, "Playing")) {
    snprintf(s_play_status, sizeof(s_play_status), "Paused");
  } else {
    snprintf(s_play_status, sizeof(s_play_status), "Playing");
  }
  update_action_bar();
}

static void stop() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Stop");
  send_outbox(KEY_STOP, 1);
}

static void volume_up() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Volume up");
  send_outbox(KEY_VOLUME_UP, 1);
}

static void volume_down() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Volume down");
  send_outbox(KEY_VOLUME_DOWN, 1);
}

static void nav_up() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Nav Up");
  send_nav(3);
}

static void nav_down() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Nav Down");
  send_nav(4);
}

static void nav_select() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Nav Select");
  send_nav(5);
}

static void nav_left() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Nav Left");
  send_nav(1);
}

static void nav_right() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Nav Right");
  send_nav(2);
}

static void cycle_mode() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Cycle mode: %d -> ", s_mode);
  switch (s_mode) {
    case MODE_PLAYER:
      s_mode = MODE_SKIPPER;
      break;
    case MODE_SKIPPER:
      s_mode = MODE_NAV;
      break;
    case MODE_NAV:
      s_mode = MODE_PLAYER;
      break;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "%d", s_mode);
  update_action_bar();
  action_bar_layer_set_click_config_provider(s_action_bar_layer, (ClickConfigProvider)click_config_provider);
}

static void click_config_provider(void *context) {
  switch (s_mode) {
    case MODE_PLAYER:
      window_single_click_subscribe(BUTTON_ID_UP, volume_up);
      window_single_click_subscribe(BUTTON_ID_SELECT, play_pause);
      window_single_click_subscribe(BUTTON_ID_DOWN, volume_down);
      window_long_click_subscribe(BUTTON_ID_UP, 0, goto_prev, NULL);
      window_long_click_subscribe(BUTTON_ID_DOWN, 0, goto_next, NULL);
      window_long_click_subscribe(BUTTON_ID_SELECT, 0, cycle_mode, NULL);
      break;
    case MODE_SKIPPER:
      window_single_click_subscribe(BUTTON_ID_UP, skip_rev);
      window_single_click_subscribe(BUTTON_ID_SELECT, stop);
      window_single_click_subscribe(BUTTON_ID_DOWN, skip_fwd);
      window_long_click_subscribe(BUTTON_ID_UP, 0, goto_prev, NULL);
      window_long_click_subscribe(BUTTON_ID_DOWN, 0, goto_next, NULL);
      window_long_click_subscribe(BUTTON_ID_SELECT, 0, cycle_mode, NULL);
      break;
    case MODE_NAV:
      window_single_click_subscribe(BUTTON_ID_UP, nav_up);
      window_single_click_subscribe(BUTTON_ID_SELECT, nav_select);
      window_single_click_subscribe(BUTTON_ID_DOWN, nav_down);
      window_long_click_subscribe(BUTTON_ID_UP, 0, nav_left, NULL);
      window_long_click_subscribe(BUTTON_ID_DOWN, 0, nav_right, NULL);
      window_long_click_subscribe(BUTTON_ID_SELECT, 0, cycle_mode, NULL);
      break;
  }
}

void playback_window_refresh(char *source, char *status, char *mainline, char *subline, char *elapsed) {
  if (!s_main_window || !s_mainline_layer || !s_subline_layer || !s_elapsed_layer) {
    return;
  }
  text_layer_set_text(s_title_layer, source);
  text_layer_set_text(s_mainline_layer, mainline);
  text_layer_set_text(s_subline_layer, subline);
  text_layer_set_text(s_elapsed_layer, elapsed);
  snprintf(s_play_status, sizeof(s_play_status), "%s", status);
  update_action_bar();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  const GEdgeInsets title_insets = {.top = SCALE_H(bounds, 3), .right = ACTION_BAR_WIDTH, .bottom = SCALE_H(bounds, 21), .left = ACTION_BAR_WIDTH / 4};
  s_title_layer = text_layer_create(grect_inset(bounds, title_insets));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_font(s_title_layer, fonts_get_system_font(font_for_height(bounds, FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_18_BOLD)));
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));

  const GEdgeInsets elapsed_insets = {.top = SCALE_H(bounds, 18), .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 4};
  s_elapsed_layer = text_layer_create(grect_inset(bounds, elapsed_insets));
  text_layer_set_background_color(s_elapsed_layer, GColorClear);
  text_layer_set_text_alignment(s_elapsed_layer, GTextAlignmentCenter);
  text_layer_set_font(s_elapsed_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(window_layer, text_layer_get_layer(s_elapsed_layer));

  const GEdgeInsets mainline_insets = {.top = SCALE_H(bounds, 34), .right = ACTION_BAR_WIDTH, .bottom = SCALE_H(bounds, 56), .left = ACTION_BAR_WIDTH / 4};
  s_mainline_layer = text_layer_create(grect_inset(bounds, mainline_insets));
  text_layer_set_background_color(s_mainline_layer, GColorClear);
  text_layer_set_text_alignment(s_mainline_layer, GTextAlignmentCenter);
  text_layer_set_font(s_mainline_layer, fonts_get_system_font(font_for_height(bounds, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_28_BOLD)));
  layer_add_child(window_layer, text_layer_get_layer(s_mainline_layer));

  const GEdgeInsets subline_insets = {.top = SCALE_H(bounds, 115), .right = ACTION_BAR_WIDTH, .left = ACTION_BAR_WIDTH / 4};
  s_subline_layer = text_layer_create(grect_inset(bounds, subline_insets));
  text_layer_set_background_color(s_subline_layer, GColorClear);
  text_layer_set_text_alignment(s_subline_layer, GTextAlignmentCenter);
  text_layer_set_font(s_subline_layer, fonts_get_system_font(font_for_height(bounds, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD)));
  layer_add_child(window_layer, text_layer_get_layer(s_subline_layer));

  s_play_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_PLAY);
  s_pause_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_PAUSE);
  s_skip_rev_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_SKIP_REV);
  s_skip_fwd_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_SKIP_FWD);
  s_vol_up_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_VOLUME_UP);
  s_vol_down_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_VOLUME_DOWN);
  s_stop_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_STOP);
  s_up_bitmap_nav = gbitmap_create_with_resource(RESOURCE_ID_ICON_UP);
  s_down_bitmap_nav = gbitmap_create_with_resource(RESOURCE_ID_ICON_DOWN);
  s_check_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);

  s_action_bar_layer = action_bar_layer_create();
  update_action_bar();
  action_bar_layer_add_to_window(s_action_bar_layer, window);
  action_bar_layer_set_click_config_provider(s_action_bar_layer, (ClickConfigProvider)click_config_provider);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_mainline_layer);
  text_layer_destroy(s_subline_layer);
  text_layer_destroy(s_elapsed_layer);
  action_bar_layer_destroy(s_action_bar_layer);

  gbitmap_destroy(s_play_bitmap);
  gbitmap_destroy(s_pause_bitmap);
  gbitmap_destroy(s_skip_rev_bitmap);
  gbitmap_destroy(s_skip_fwd_bitmap);
  gbitmap_destroy(s_vol_up_bitmap);
  gbitmap_destroy(s_vol_down_bitmap);
  gbitmap_destroy(s_stop_bitmap);
  gbitmap_destroy(s_up_bitmap_nav);
  gbitmap_destroy(s_down_bitmap_nav);
  gbitmap_destroy(s_check_bitmap);

  window_destroy(window);
  s_main_window = NULL;
}

void playback_window_push(char *source, char *status, char *mainline, char *subline, char *elapsed) {
  s_mode = MODE_PLAYER;
  if (!s_main_window) {
    s_main_window = window_create();
    window_set_background_color(s_main_window, PBL_IF_COLOR_ELSE(GColorMalachite, GColorWhite));
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
  }
  window_stack_push(s_main_window, true);
  playback_window_refresh(source, status, mainline, subline, elapsed);
}
