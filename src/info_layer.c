#include "util.h"
#include "info_layer.h"

/* Called by the graphics layers when the time layer needs to be updated.
*/
	
void info_layer_update_proc(InfoLayer *il, GContext* ctx)
{
    if (il->background_color != GColorClear)
    {
        graphics_context_set_fill_color(ctx, il->background_color);
        graphics_fill_rect(ctx, il->layer.bounds, 0, GCornerNone);
    }
    graphics_context_set_text_color(ctx, il->text_color);
   	
	if (data.info)
    {
        graphics_text_draw(ctx,
                           data.info,
                           il->info_font,
                           il->layer.bounds,
                           il->overflow_mode,
                           GTextAlignmentCenter,
                           il->layout_cache); 
    } 
}


void info_layer_set_text(InfoLayer *il, char *info)
{
    data.info = info;
    layer_mark_dirty(&(il->layer));
}

void info_layer_set_font(InfoLayer *il, GFont info_font)
{
    il->info_font = info_font;
    if (data.info) layer_mark_dirty(&(il->layer));
}

/* Set the text color of the time layer.
*/
void info_layer_set_text_color(InfoLayer *il, GColor color)
{
    il->text_color = color;
    if (data.info) layer_mark_dirty(&(il->layer));
}

/* Set the background color of the time layer.
*/
void info_layer_set_background_color(InfoLayer *il, GColor color)
{
    il->background_color = color;
    if (data.info) layer_mark_dirty(&(il->layer));
}

/* Initialize the time layer with default colors and fonts.
*/
void info_layer_init(InfoLayer *il, GRect frame)
{
    layer_init(&il->layer, frame);
    il->layer.update_proc = (LayerUpdateProc)info_layer_update_proc;
    il->text_color = GColorWhite;
    il->background_color = GColorBlack;
    il->overflow_mode = GTextOverflowModeWordWrap;
    il->info_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
	
	info_layer_set_text(il, "?");	
}

void update_info(InfoLayer* i, struct Data d) {

	char* state = "XXX missed, XXX sms. #00";
	
	if(d.link_status != LinkStatusOK){
		strcpy(state, "?");
		strcat(state, "  (");	
		strcat(state, itoa(d.link_status));
		strcat(state, " )");	
	}
	else {
		strcpy(state, " ");
		if(d.missed) {
			strcat(state, itoa(d.missed));
			strcat(state, " call");
			if(d.missed>1) strcat(state, "s");
		}
		if(d.missed&&d.unread)  {
			strcat(state, "      ");
		}
		if(d.unread)
		{
			strcat(state, itoa(d.unread));
			strcat(state, " sms");
		}
		/*if(d.missed||data.unread)  {
			strcat(state, ".");
		}*/
	}

//	strcat(state, "  #");	
//	strcat(state, itoa(d.link_status));
	
	info_layer_set_text(i, state);
}