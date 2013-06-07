#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "sync.h"
#include "http.h"
#include "util.h"
#include "weather_layer.h"
#include "time_layer.h"
#include "info_layer.h"
#include "link_monitor.h"
#include "config.h"

// #define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
#define MY_UUID {0xB8, 0x93, 0xF0, 0xA2, 0xE7, 0x22, 0x42, 0xEE, 0xA1, 0xFD, 0x05, 0x03, 0x4D, 0x63, 0x79, 0xBD}



PBL_APP_INFO(MY_UUID,
             "Pebblers Futura", "Bartosz Grabowski",
             1, 61, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);


// POST variables
#define WEATHER_KEY_LATITUDE 1
#define WEATHER_KEY_LONGITUDE 2
#define WEATHER_KEY_UNIT_SYSTEM 3
	
// Received variables
#define WEATHER_KEY_ICON 1
#define WEATHER_KEY_TEMPERATURE 2
	
#define WEATHER_HTTP_COOKIE 1949327671
#define TIME_HTTP_COOKIE 1131038282	

#define TIME_FRAME      (GRect(1, 0, 144, 168-6))
#define DATE_FRAME      (GRect(1, 52, 144, 168-62))
#define INFO_FRAME		(GRect(1, 70, 141, 24))

Window window;          	/* main window */
TextLayer date_layer;   	/* layer for the date */
TimeLayer time_layer;   	/* layer for the time */
WeatherLayer weather_layer;	/* weather ingo */
InfoLayer info_layer;   	/* layer for phone info */

GFont font_date;        /* font for date (normal) */
GFont font_hour;        /* font for hour (bold) */
GFont font_minute;      /* font for minute (thin) */
GFont font_info;		/* font for phone info */

int request_weather() {
	int result=0;
	if(!located) {
		result = http_location_request();
		return result;
	}
	// Build the HTTP request
	DictionaryIterator *body;
	HTTPResult res = http_out_get("http://www.zone-mr.net/api/weather.php", WEATHER_HTTP_COOKIE, &body);
	if(result != HTTP_OK) {
		weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
		return (int)res;
	}
	dict_write_int32(body, WEATHER_KEY_LATITUDE, our_latitude);
	dict_write_int32(body, WEATHER_KEY_LONGITUDE, our_longitude);
	dict_write_cstring(body, WEATHER_KEY_UNIT_SYSTEM, UNIT_SYSTEM);
	// Send it.
	result = http_out_send(); 
	if(result != HTTP_OK) {
		weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
		return result;
	}
	return result;
}

void failed(int32_t cookie, int http_status, void* context) {
	if(cookie == 0 || cookie == WEATHER_HTTP_COOKIE) {
		// just leave last data on the screen
		//weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
		//text_layer_set_text(&weather_layer.temp_layer, "___°   ");
	}
	//Re-request the location and subsequently weather on next minute tick
	located = false;
	// link may be fine, but phone can have no connection, so handle it in app_failed
	// link_monitor_handle_failure(http_status, &data);	
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
	if(cookie != WEATHER_HTTP_COOKIE) return;
	Tuple* icon_tuple = dict_find(received, WEATHER_KEY_ICON);
	if(icon_tuple) {
		int icon = icon_tuple->value->int8;
		if(icon >= 0 && icon < 10) {
			weather_layer_set_icon(&weather_layer, icon);
		} else {
			weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
		}
	}
	Tuple* temperature_tuple = dict_find(received, WEATHER_KEY_TEMPERATURE);
	if(temperature_tuple) {
		weather_layer_set_temperature(&weather_layer, temperature_tuple->value->int16);
	}
	
	link_monitor_handle_success(&data);
	display_counters(&info_layer, data);
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	// Fix the floats
	our_latitude = latitude * 10000;
	our_longitude = longitude * 10000;
	located = true;
	request_weather();
}

void reconnect(void* context) {
	located = false;
	request_weather();
}


bool read_state_data(DictionaryIterator* received, struct Data* d, InfoLayer layer){
	(void)d;
	bool has_data = false;
	Tuple* tuple = dict_read_first(received);
	if(!tuple) return false;
	do {
		switch(tuple->key) {
	  		case TUPLE_MISSED_CALLS:
				d->missed = tuple->value->uint8;
				has_data = true;
				break;
			case TUPLE_UNREAD_SMS:
				d->unread = tuple->value->uint8;
				has_data = true;
				break;
		}
	}
	while((tuple = dict_read_next(received)));
	return has_data;
}

void app_received_msg(DictionaryIterator* received, void* context) {	
	link_monitor_handle_success(&data);
	if(read_state_data(received, &data, info_layer)) 
	{
		display_counters(&info_layer, data);
		if(!located) request_weather();
	}
}
static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context) {
	link_monitor_handle_failure(reason, &data);
	display_counters(&info_layer, data);	
}

bool register_callbacks() {
	if (callbacks_registered) {
		if (app_message_deregister_callbacks(&app_callbacks) == APP_MSG_OK)
			callbacks_registered = false;
	}
	if (!callbacks_registered) {
		app_callbacks = (AppMessageCallbacksNode){
			.callbacks = { .in_received = app_received_msg, .out_failed = app_send_failed} };
		if (app_message_register_callbacks(&app_callbacks) == APP_MSG_OK) {
      callbacks_registered = true;
      }
	}
	return callbacks_registered;
}

/* Called by the OS once per minute. Update the time and date.
*/
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t)
{
    /* Need to be static because pointers to them are stored in the text
    * layers.
    */
    static char date_text[] = "XXX 00";
    static char hour_text[] = "00";
    static char minute_text[] = ":00";

    (void)ctx;  /* prevent "unused parameter" warning */

    if (t->units_changed & DAY_UNIT)
    {
        string_format_time(date_text,
                           sizeof(date_text),
                           "%a %d",
                           t->tick_time);
        text_layer_set_text(&date_layer, date_text);
    }

    if (clock_is_24h_style())
    {
        string_format_time(hour_text, sizeof(hour_text), "%H", t->tick_time);
		if (hour_text[0] == '0')
        {
            /* This is a hack to get rid of the leading zero. */
            memmove(&hour_text[0], &hour_text[1], sizeof(hour_text) - 1);
        }
    }
    else
    {
        string_format_time(hour_text, sizeof(hour_text), "%I", t->tick_time);
        if (hour_text[0] == '0')
        {
            /* This is a hack to get rid of the leading zero. */
            memmove(&hour_text[0], &hour_text[1], sizeof(hour_text) - 1);
        }
    }

    string_format_time(minute_text, sizeof(minute_text), ":%M", t->tick_time);
    time_layer_set_text(&time_layer, hour_text, minute_text);
	
	if(!(t->tick_time->tm_min % 1) || data.link_status == LinkStatusUnknown) link_monitor_ping();
	else if(!(t->tick_time->tm_min % 15)) request_weather();
}


/* Initialize the application.
*/
void handle_init(AppContextRef ctx)
{
    PblTm tm;
    PebbleTickEvent t;
    ResHandle res_d;
    ResHandle res_h;
    ResHandle res_m;

    window_init(&window, "Futura");
    window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, GColorBlack);

    resource_init_current_app(&APP_RESOURCES);

    res_d = resource_get_handle(RESOURCE_ID_FUTURA_18);
    res_h = resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53);

	// load fonts
    font_date = fonts_load_custom_font(res_d);
    font_hour = fonts_load_custom_font(res_h);
    font_minute = fonts_load_custom_font(res_h);
	font_info = fonts_load_custom_font(res_d);
	// time layer
    time_layer_init(&time_layer, window.layer.frame);
    time_layer_set_text_color(&time_layer, GColorWhite);
    time_layer_set_background_color(&time_layer, GColorClear);
    time_layer_set_fonts(&time_layer, font_hour, font_minute);
    layer_set_frame(&time_layer.layer, TIME_FRAME);
    layer_add_child(&window.layer, &time_layer.layer);

    text_layer_init(&date_layer, window.layer.frame);
    text_layer_set_text_color(&date_layer, GColorWhite);
    text_layer_set_background_color(&date_layer, GColorClear);
    text_layer_set_font(&date_layer, font_date);
    text_layer_set_text_alignment(&date_layer, GTextAlignmentCenter);
    layer_set_frame(&date_layer.layer, DATE_FRAME);
    layer_add_child(&window.layer, &date_layer.layer);

	// weather layer
	weather_layer_init(&weather_layer, GPoint(0, 89)); 
	layer_add_child(&window.layer, &weather_layer.layer);
	
	// info layer 
	info_layer_init(&info_layer, window.layer.frame);
	info_layer_set_text_color(&info_layer, GColorWhite);
	info_layer_set_background_color(&info_layer, GColorBlack);
	info_layer_set_font(&info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_set_frame(&info_layer.layer, INFO_FRAME);
	layer_add_child(&window.layer, &info_layer.layer);
	info_layer_set_text(&info_layer, "...");

	data.link_status = LinkStatusUnknown;
	link_monitor_ping();

	// request data refresh on window appear (for example after notification)
	WindowHandlers handlers = { .appear = &link_monitor_ping};
	window_set_window_handlers(&window, handlers);
	
	http_register_callbacks((HTTPCallbacks){.failure=failed,.success=success,.reconnect=reconnect,.location=location}, (void*)ctx);
	register_callbacks();

	// Refresh time
	get_time(&tm);
    t.tick_time = &tm;
    t.units_changed = SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT;
	
	handle_minute_tick(ctx, &t);
}

/* Shut down the application
*/

void handle_deinit(AppContextRef ctx)
{
    fonts_unload_custom_font(font_date);
    fonts_unload_custom_font(font_hour);
    fonts_unload_custom_font(font_minute);
	
	weather_layer_deinit(&weather_layer);
}
/********************* Main Program *******************/
void pbl_main(void *params)
{
    PebbleAppHandlers handlers =
    {
        .init_handler = &handle_init,
        .deinit_handler = &handle_deinit,
        .tick_info =
        {
            .tick_handler = &handle_minute_tick,
            .tick_units = MINUTE_UNIT
        },
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 256,
			}
		}
    };
    app_event_loop(params, &handlers);
}