#include <pebble.h>
#include "layout.h"
#include "livetv.h"

#define KEY_ERROR     0
#define KEY_DATA_REQUEST  1
#define KEY_DATA_REQUEST_ID  100
#define KEY_NB_MENU_ITEMS    9999
#define KEY_FIRST_ITEM    10000
#define ITEM_SIZE    10
#define KEY_PLAY_CHANNEL    1002
#define KEY_PLAY_RECORDING    1003

#define MENUITEM_BUFFER_SIZE    64



static Tuple *s_error;



static ActionMenu *s_action_menu;
static ActionMenuLevel *s_root_level;

AppMessageInboxReceived s_main_msg_callback;

static uint32_t s_channelgroupids[32];
static uint32_t s_channelids[50];

static char *s_channelgroups_titles;
static char *s_channels_titles;
static char *s_channels_subtitles;


static char s_appglance[256];

static void set_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
    if (limit < 1) return;
    APP_LOG(APP_LOG_LEVEL_INFO, "Setting app glance to: %s", s_appglance);

    const AppGlanceSlice entry = (AppGlanceSlice) {
        .layout = {
            .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
            .subtitle_template_string = s_appglance
        },
            .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION
    };

    const AppGlanceResult result = app_glance_add_slice(session, entry);
    if (result != APP_GLANCE_RESULT_SUCCESS) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlance Error: %d", result);
    }
}


/* LIVE TV CHANNELS LIST */

static Window *s_channels_window;
static TextLayer *s_channels_loading_layer;
static TextLayer *s_no_channels_layer;
static SimpleMenuItem *s_channels_menu_items = NULL;
static SimpleMenuSection s_channels_menu_section;
static SimpleMenuLayer *s_channels_menu_layer;
char s_channels_sortcriteria[32];
uint32_t s_channelgroupid;

static void play_channel(int index, void *context) {
    uint32_t channelid = s_channelids[index];
    
    snprintf(s_appglance, sizeof(s_appglance), "%s", s_channels_menu_items[index].title);
    
    app_glance_reload(set_app_glance, NULL);
    
    
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint32(iter, KEY_PLAY_CHANNEL, channelid);
	dict_write_end(iter);
	app_message_outbox_send();
    
    // then pop all & exit
    // ...or not - keep the screen on for quick switching
    //exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
    //window_stack_pop_all(true);
}


static void channels_list_received(DictionaryIterator *iter, void *context) {
    s_error = dict_find(iter, KEY_ERROR);
    
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "channels app message callback");
  
 	Tuple *nb_items_tuple = dict_find(iter, KEY_NB_MENU_ITEMS);
    if (!nb_items_tuple || nb_items_tuple->value->int32 < 1) {
        text_layer_set_text(s_channels_loading_layer, "");
        text_layer_set_text(s_no_channels_layer, "No channels");
        return;
    }

    int nb_items = nb_items_tuple->value->int32;
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "size=%d", nb_items);

    s_channels_menu_items = calloc(nb_items, sizeof(SimpleMenuItem));
    s_channels_titles = calloc(nb_items, MENUITEM_BUFFER_SIZE);
    s_channels_subtitles = calloc(nb_items, MENUITEM_BUFFER_SIZE);

    for (int i = 0; i < nb_items; i++) {
        Tuple *title_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 1);
        Tuple *subtitle_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 2);
        Tuple *id_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 0);
        s_channelids[i] = id_tuple->value->uint32;
        snprintf(s_channels_titles + i*MENUITEM_BUFFER_SIZE, MENUITEM_BUFFER_SIZE, "%s", title_tuple->value->cstring);
        s_channels_menu_items[i].title = s_channels_titles + i*MENUITEM_BUFFER_SIZE;
        if (subtitle_tuple) {
            snprintf(s_channels_subtitles + i*MENUITEM_BUFFER_SIZE, MENUITEM_BUFFER_SIZE, "%s", subtitle_tuple->value->cstring);
            s_channels_menu_items[i].subtitle = s_channels_subtitles + i*MENUITEM_BUFFER_SIZE;
        } else {
            s_channels_menu_items[i].subtitle = NULL;
        }
        s_channels_menu_items[i].icon = NULL;
        s_channels_menu_items[i].callback = play_channel;
    }

    s_channels_menu_section = (SimpleMenuSection) {
        "Channels", s_channels_menu_items, nb_items
    };

    Layer *window_layer = window_get_root_layer(s_channels_window);
	GRect bounds = layer_get_bounds(window_layer);

    //APP_LOG(APP_LOG_LEVEL_DEBUG, "creating channels menu");

    s_channels_menu_layer = simple_menu_layer_create(bounds, s_channels_window, &s_channels_menu_section, 1, NULL);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(simple_menu_layer_get_menu_layer(s_channels_menu_layer), GColorChromeYellow, GColorWhite);
#endif
    layer_add_child(window_layer, (Layer *)s_channels_menu_layer);
}

static void channels_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	s_channels_loading_layer = text_layer_create(GRect(0, bounds.size.h - 16, bounds.size.w, 16));
	text_layer_set_text(s_channels_loading_layer, "Loading...");
	text_layer_set_font(s_channels_loading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_channels_loading_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_channels_loading_layer, GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_channels_loading_layer));
	s_no_channels_layer = text_layer_create(GRect(0, SCALE_H(bounds, 50), bounds.size.w, SCALE_H(bounds, 80)));
	text_layer_set_text(s_no_channels_layer, "");
	text_layer_set_font(s_no_channels_layer, fonts_get_system_font(font_for_height(bounds, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_28_BOLD)));
	text_layer_set_text_alignment(s_no_channels_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_no_channels_layer));
    
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_cstring(iter, KEY_DATA_REQUEST, "channels");
	dict_write_uint32(iter, KEY_DATA_REQUEST_ID, s_channelgroupid);
	dict_write_end(iter);
	app_message_outbox_send();
}

static void channels_window_unload(Window *window) {
    text_layer_destroy(s_channels_loading_layer);
    text_layer_destroy(s_no_channels_layer);
    simple_menu_layer_destroy(s_channels_menu_layer);
    free(s_channels_titles);
    free(s_channels_subtitles);
    free(s_channels_menu_items);
    window_destroy(s_channels_window);
    s_channels_window = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "restoring main appmessage callback");
    app_message_register_inbox_received(s_main_msg_callback);
    
	DictionaryIterator *iter;
	if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
		dict_write_cstring(iter, KEY_DATA_REQUEST, "getbasicinfo");
		dict_write_end(iter);
		app_message_outbox_send();
	}
}

static void show_channels(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    app_message_register_inbox_received(channels_list_received);
    uint32_t *action_data = (uint32_t *)action_menu_item_get_action_data(action);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sort criteria=%s", (char *)context);
    s_channelgroupid = *action_data;

    if(!s_channels_window) {
        s_channels_window = window_create();
        //window_set_background_color(s_first_window, GColorBlack);
        window_set_window_handlers(s_channels_window, (WindowHandlers) {
            .load = channels_window_load,
            .unload = channels_window_unload,
        });
    }
    window_stack_push(s_channels_window, true);
}


/*
RECENT RECORDINGS LIST
*/

static Window *s_recordings_window;
static TextLayer *s_recordings_loading_layer;
static TextLayer *s_no_recordings_layer;
static SimpleMenuItem *s_recordings_menu_items = NULL;
static SimpleMenuSection s_recordings_menu_section;
static SimpleMenuLayer *s_recordings_menu_layer;

static uint32_t s_recordingids[50];
static char *s_recordings_titles;
static char *s_recordings_subtitles;

static void defer_pop_all(void *data) {
    window_stack_pop_all(true);
}

static void play_recording(int index, void *context) {
    uint32_t recordingid = s_recordingids[index];

    snprintf(s_appglance, sizeof(s_appglance), "%s", s_recordings_menu_items[index].title);

    app_glance_reload(set_app_glance, NULL);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint32(iter, KEY_PLAY_RECORDING, recordingid);
	dict_write_end(iter);
	app_message_outbox_send();

    exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
    app_timer_register(1, defer_pop_all, NULL);
}

static void recordings_list_received(DictionaryIterator *iter, void *context) {
    s_error = dict_find(iter, KEY_ERROR);

 	Tuple *nb_items_tuple = dict_find(iter, KEY_NB_MENU_ITEMS);
    if (!nb_items_tuple || nb_items_tuple->value->int32 < 1) {
        text_layer_set_text(s_recordings_loading_layer, "");
        text_layer_set_text(s_no_recordings_layer, "No recordings");
        return;
    }

    int nb_items = nb_items_tuple->value->int32;

    s_recordings_menu_items = calloc(nb_items, sizeof(SimpleMenuItem));
    s_recordings_titles = calloc(nb_items, MENUITEM_BUFFER_SIZE);
    s_recordings_subtitles = calloc(nb_items, MENUITEM_BUFFER_SIZE);

    for (int i = 0; i < nb_items; i++) {
        Tuple *title_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 1);
        Tuple *subtitle_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 2);
        Tuple *id_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 0);
        s_recordingids[i] = id_tuple->value->uint32;
        snprintf(s_recordings_titles + i*MENUITEM_BUFFER_SIZE, MENUITEM_BUFFER_SIZE, "%s", title_tuple->value->cstring);
        snprintf(s_recordings_subtitles + i*MENUITEM_BUFFER_SIZE, MENUITEM_BUFFER_SIZE, "%s", subtitle_tuple->value->cstring);
        s_recordings_menu_items[i].title = s_recordings_titles + i*MENUITEM_BUFFER_SIZE;
        s_recordings_menu_items[i].subtitle = s_recordings_subtitles + i*MENUITEM_BUFFER_SIZE;
        s_recordings_menu_items[i].icon = NULL;
        s_recordings_menu_items[i].callback = play_recording;
    }

    s_recordings_menu_section = (SimpleMenuSection) {
        "Recordings", s_recordings_menu_items, nb_items
    };

    Layer *window_layer = window_get_root_layer(s_recordings_window);
	GRect bounds = layer_get_bounds(window_layer);

    s_recordings_menu_layer = simple_menu_layer_create(bounds, s_recordings_window, &s_recordings_menu_section, 1, NULL);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(simple_menu_layer_get_menu_layer(s_recordings_menu_layer), GColorChromeYellow, GColorWhite);
#endif
    layer_add_child(window_layer, (Layer *)s_recordings_menu_layer);
}

static void recordings_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	s_recordings_loading_layer = text_layer_create(GRect(0, bounds.size.h - 16, bounds.size.w, 16));
	text_layer_set_text(s_recordings_loading_layer, "Loading...");
	text_layer_set_font(s_recordings_loading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_recordings_loading_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_recordings_loading_layer, GColorClear);
	layer_add_child(window_layer, text_layer_get_layer(s_recordings_loading_layer));
	s_no_recordings_layer = text_layer_create(GRect(0, SCALE_H(bounds, 50), bounds.size.w, SCALE_H(bounds, 80)));
	text_layer_set_text(s_no_recordings_layer, "");
	text_layer_set_font(s_no_recordings_layer, fonts_get_system_font(font_for_height(bounds, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_28_BOLD)));
	text_layer_set_text_alignment(s_no_recordings_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_no_recordings_layer));

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_cstring(iter, KEY_DATA_REQUEST, "recordings");
	dict_write_end(iter);
	app_message_outbox_send();
}

static void recordings_window_unload(Window *window) {
    text_layer_destroy(s_recordings_loading_layer);
    text_layer_destroy(s_no_recordings_layer);
    simple_menu_layer_destroy(s_recordings_menu_layer);
    free(s_recordings_titles);
    free(s_recordings_subtitles);
    free(s_recordings_menu_items);
    window_destroy(s_recordings_window);
    s_recordings_window = NULL;
    app_message_register_inbox_received(s_main_msg_callback);

	DictionaryIterator *iter;
	if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
		dict_write_cstring(iter, KEY_DATA_REQUEST, "getbasicinfo");
		dict_write_end(iter);
		app_message_outbox_send();
	}
}

static void show_recordings(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    s_main_msg_callback = app_message_register_inbox_received(recordings_list_received);

    if(!s_recordings_window) {
        s_recordings_window = window_create();
        window_set_window_handlers(s_recordings_window, (WindowHandlers) {
            .load = recordings_window_load,
            .unload = recordings_window_unload,
        });
    }
    window_stack_push(s_recordings_window, true);
}


/*
ACTION MENU
*/

static void channelgroups_list_received(DictionaryIterator *iter, void *context) {
    s_error = dict_find(iter, KEY_ERROR);
    
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "channel groups app message callback");
  
 	Tuple *nb_items_tuple = dict_find(iter, KEY_NB_MENU_ITEMS);
    
    if (!nb_items_tuple || nb_items_tuple->value->int32 < 1) {
    	APP_LOG(APP_LOG_LEVEL_DEBUG, "no channel groups... bailing out");
        return;
    }

    int nb_items = nb_items_tuple->value->int32;
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "size=%d", nb_items);

    s_channelgroups_titles = calloc(nb_items, MENUITEM_BUFFER_SIZE);

    // Create the root level
    s_root_level = action_menu_level_create(nb_items + 1);

    // Add recent recordings first
    action_menu_level_add_action(s_root_level, "Recent recordings", show_recordings, NULL);

    // Create the menu entries
    for (int i = 0; i < nb_items; i++) {
        Tuple *title_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 1);
        Tuple *id_tuple = dict_find(iter, KEY_FIRST_ITEM + (i * ITEM_SIZE) + 0);
        s_channelgroupids[i] = id_tuple->value->uint32;
        snprintf(s_channelgroups_titles + i*MENUITEM_BUFFER_SIZE, MENUITEM_BUFFER_SIZE, "%s", title_tuple->value->cstring);
        action_menu_level_add_action(s_root_level, s_channelgroups_titles + i*MENUITEM_BUFFER_SIZE, show_channels, &id_tuple->value->uint32);
    }

    // Configure the ActionMenu Window about to be shown
    ActionMenuConfig config = (ActionMenuConfig) {
        .root_level = s_root_level,
        .colors = {
            .background = PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite),
            .foreground = GColorBlack,
        },
            .align = ActionMenuAlignCenter
    };

    //APP_LOG(APP_LOG_LEVEL_DEBUG, "restoring main appmessage callback");
    //app_message_register_inbox_received(s_main_msg_callback);
    
    // Show the ActionMenu
    s_action_menu = action_menu_open(&config);  
}    

void init_livetv_action_menu() {
    s_main_msg_callback = app_message_register_inbox_received(channelgroups_list_received);
  
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_cstring(iter, KEY_DATA_REQUEST, "channelgroups");
	dict_write_end(iter);
	app_message_outbox_send();

}
