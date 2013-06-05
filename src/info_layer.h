#ifndef INFO_LAYER_H
#define INFO_LAYER_H

#define MAX_INFO_LENGTH 10

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "sync.h"
	
typedef struct _InfoLayer {
    Layer layer;
    GFont info_font;
    GTextLayoutCacheRef layout_cache;
    GColor text_color : 2;
    GColor background_color : 2;
    GTextOverflowMode overflow_mode : 2;
} InfoLayer;

void info_layer_update_proc(InfoLayer *il, GContext* ctx);
void info_layer_set_text(InfoLayer *il, char *info_text);
void info_layer_set_font(InfoLayer *il, GFont info_font);
void info_layer_set_text_color(InfoLayer *il, GColor color);
void info_layer_set_background_color(InfoLayer *il, GColor color);
void info_layer_init(InfoLayer *il, GRect frame);
void update_info(InfoLayer* i, struct Data data);

#endif