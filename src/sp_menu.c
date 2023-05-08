#define _POSIX_C_SOURCE 200809
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "../config.h"
#include "../include/sp_context.h"
#include "../include/sp_base.h"
#include "../include/sp_menu.h"
#include "../include/sp_math.h"
#include "../include/sp_io.h"

static const spooky_menu * spooky_menu_read_definitions(const spooky_context * context, SDL_Rect rect, const char * path);

static const int SPOOKY_MENU_MARGIN_TOP_BOTTOM = 5;

static const spooky_menu ** menu_stack = NULL;
static size_t menu_stack_len = 0;
static size_t menu_stack_capacity = 8;

typedef struct spooky_menu_data {
	const spooky_context * context;
	const spooky_font * font;
	const char * name;

	const spooky_menu * active_menu;

	spooky_menu * menu_items;

	size_t menu_items_capacity;
	size_t menu_items_count;
	size_t menu_item_offset;

	int name_width;
	int name_height;

	SDL_Rect rect;
	SDL_Color bg_color;
	SDL_Color fg_color;

	int last_mouse_x;
	int last_mouse_y;

	spooky_menu_types menu_type;

	bool is_active;
	bool is_visible;
	bool is_hover;
	bool is_intersected;

	bool is_expanded;

	char padding[7];

	const spooky_menu * parent;

	int max_width;
	int max_height;

	/* DEBUG: SDL_Rect hover_rect; */
} spooky_menu_data;

/* Menu configuration (main menu, menu items, etc. */
typedef void (*event_handler)(void);

typedef struct menu_selection {
	const char * name;
	event_handler on_click;

	const spooky_menu * menu_action;
} menu_selection;

typedef struct menu_items {
	const char * name;
	const spooky_menu * menu_item;
	size_t selections_count;
	menu_selection * selections;
} menu_items;

static const spooky_menu * spooky_menu_get_active_menu(const spooky_menu * self);
static void spooky_menu_set_active_menu(const spooky_menu * self, const spooky_menu * active_menu);
static const spooky_menu * spooky_menu_attach_item(const spooky_menu * self, const spooky_menu * item, const spooky_menu ** out_copy);

static bool spooky_menu_handle_event(const spooky_base * self, SDL_Event * event);
// static void spooky_menu_handle_delta(const spooky_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void spooky_menu_render(const spooky_base * self, SDL_Renderer * renderer);

static void spooky_menu_render_main_menu(const spooky_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect);
static void spooky_menu_render_main_menu_item(const spooky_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect);
static void spooky_menu_render_menu_action(const spooky_menu * self, SDL_Renderer * renderer, const spooky_menu * parent, const SDL_Rect * rect);

static spooky_menu_types spooky_menu_get_menu_type(const spooky_menu * self);

static int spooky_menu_get_x(const spooky_menu * self);
static void spooky_menu_set_x(const spooky_menu * self, int x);
static int spooky_menu_get_y(const spooky_menu * self);
static void spooky_menu_set_y(const spooky_menu * self, int y);
static int spooky_menu_get_w(const spooky_menu * self);
static void spooky_menu_set_w(const spooky_menu * self, int w);
static int spooky_menu_get_h(const spooky_menu * self);
static void spooky_menu_set_h(const spooky_menu * self, int h);

static const char  * spooky_menu_get_name(const spooky_menu * self);

static void spooky_menu_set_parent(const spooky_menu * self, const spooky_menu * parent);
static bool spooky_menu_get_is_active(const spooky_menu * self);

static int spooky_menu_get_name_width(const spooky_menu * self);
static int spooky_menu_get_name_height(const spooky_menu * self);

static void spooky_menu_get_menu_action_hover_rect(const spooky_menu * self, SDL_Rect r, SDL_Rect * out_rect);

static void spooky_menu_set_is_expanded(const spooky_menu * self, bool value);

static void spooky_menu_copy(const spooky_menu * source, spooky_menu * dest);

static const spooky_base * spooky_menu_as_base(const spooky_menu * self) {
	return (const spooky_base *)self;
}

const spooky_menu * spooky_menu_alloc(void) {
	spooky_menu * self = calloc(1, sizeof * self);
	if(!self) abort();

	return self;
}

static void spooky_menu_copy(const spooky_menu * source, spooky_menu * dest) {
	const spooky_menu * new = spooky_menu_init(dest);

	spooky_menu_data * data = calloc(1, sizeof * data);
	if(!data) { abort(); }

	((spooky_menu *)(uintptr_t)new)->data = data;

	memmove((void *)(uintptr_t)new->data, source->data, sizeof * source->data);
}

const spooky_menu * spooky_menu_init(spooky_menu * self) {
	if(!self) { abort(); }

	self->ctor = &spooky_menu_ctor;
	self->dtor = &spooky_menu_dtor;
	self->free = &spooky_menu_free;
	self->release = &spooky_menu_release;
	self->as_base = &spooky_menu_as_base;

	self->attach_item = &spooky_menu_attach_item;

	self->get_name = &spooky_menu_get_name;

	self->super.handle_event = &spooky_menu_handle_event;
	self->super.render = &spooky_menu_render;
	self->get_menu_type = &spooky_menu_get_menu_type;

	self->get_x = &spooky_menu_get_x;
	self->set_x = &spooky_menu_set_x;

	self->get_y = &spooky_menu_get_y;
	self->set_y = &spooky_menu_set_y;

	self->get_w = &spooky_menu_get_w;
	self->set_w = &spooky_menu_set_w;

	self->get_h = &spooky_menu_get_h;
	self->set_h = &spooky_menu_set_h;

	self->get_active_menu = &spooky_menu_get_active_menu;
	self->set_active_menu = &spooky_menu_set_active_menu;

	self->set_parent = &spooky_menu_set_parent;
	self->get_is_active = &spooky_menu_get_is_active;

	self->get_name_width = &spooky_menu_get_name_width;
	self->get_name_height = &spooky_menu_get_name_height;
	self->set_is_expanded = &spooky_menu_set_is_expanded;

	return self;
}

const spooky_menu * spooky_menu_acquire(void) {
	return spooky_menu_init((spooky_menu *)(uintptr_t)spooky_menu_alloc());
}

const spooky_menu * spooky_menu_ctor(const spooky_menu * self, const spooky_font * font, const spooky_context * context, const char * name, SDL_Rect rect, spooky_menu_types menu_type) {
	spooky_menu_data * data = calloc(1, sizeof * data);
	if(!data) { abort(); }

	data->context = context;
	data->font = font;
	data->name = strdup(name);
	data->menu_items = NULL;
	data->menu_items_capacity = 8;
	data->menu_items_count = 0;
	data->menu_item_offset = 0;

	data->menu_type = menu_type;
	data->bg_color = (SDL_Color){ .r = 255, .g = 255, .b = 255, .a = 255 };
	data->fg_color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = 255 };

	((spooky_menu *)(uintptr_t)self)->data = data;

	data->font->measure_text(data->font, data->name, strnlen(data->name, 1024), &(data->name_width), &(data->name_height));

	data->rect = (SDL_Rect){ .x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h };

	data->is_active = false;
	data->is_visible = true;

	data->last_mouse_x = 0;
	data->last_mouse_y = 0;
	data->is_hover = false;
	data->is_intersected = false;
	data->is_expanded = false;

	data->parent = NULL;

	data->max_width = 0;
	data->max_height = 0;

	return self;
}

const spooky_menu * spooky_menu_dtor(const spooky_menu * self) {
	if(self) {
		spooky_menu_data * data = self->data;

		free((void *)(uintptr_t)data->name), data->name = NULL;
		if(data && data->menu_items_count > 0) {
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const spooky_menu * menu = &(data->menu_items[i]);
				menu->free(menu->dtor(menu));
			}
			free((void *)(uintptr_t)data->menu_items), data->menu_items = NULL;
		}

		free(self->data);
	}

	return self;
}

void spooky_menu_free(const spooky_menu * self) {
	if(self) {
		free((void *)(uintptr_t)self), self = NULL;
	}
}

void spooky_menu_release(const spooky_menu * self) {
	self->free(self->dtor(self));
}

const spooky_menu * spooky_menu_load_from_file(const spooky_context * context, SDL_Rect rect, const char * path) {
	return spooky_menu_read_definitions(context, rect, path);
}

static const spooky_menu * spooky_menu_attach_item(const spooky_menu * self, const spooky_menu * item, const spooky_menu ** out_copy) {
	spooky_menu_data * data = self->data;
	if(!data->menu_items) {
		data->menu_items = calloc(data->menu_items_capacity, sizeof * data->menu_items);
		if(!data->menu_items) { abort(); }
	}

	if(data->menu_items_count + 1 > data->menu_items_capacity) {
		data->menu_items_capacity += 8;
		spooky_menu * temp_menu_items = realloc(data->menu_items, sizeof data->menu_items[0] * data->menu_items_capacity);
		if(!temp_menu_items) { abort(); }
		data->menu_items = temp_menu_items;
	}

	spooky_menu * copy = &(data->menu_items[data->menu_items_count]);
	spooky_menu_copy(item, copy);

	if(out_copy) {
		*out_copy = copy;
	}

	spooky_menu_data * copy_data = copy->data;

	copy->set_parent(copy, self);

	data->menu_items_count++;
	copy_data->menu_item_offset = data->menu_items_count;

	int width, height;
	data->font->measure_text(data->font, copy_data->name, strnlen(copy_data->name, 1024), &width, &height);
	data->max_width = spooky_int_max(data->max_width, width);
	data->max_height = data->font->get_line_skip(data->font) * ((int)data->menu_items_count + 1);

	return self;
}

static bool spooky_menu_handle_event(const spooky_base * self, SDL_Event * event) {
	spooky_menu_data * data = ((const spooky_menu *)self)->data;

	SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };

	int mouse_x = event->button.x;
	if(mouse_x == 0) { mouse_x = data->last_mouse_x; }

	int mouse_y = event->button.y;
	if(mouse_y == 0) { mouse_y = data->last_mouse_y; }

	data->last_mouse_x = mouse_x;
	data->last_mouse_y = mouse_y;

	// data->is_intersected = SDL_EnclosePoints(&mouse_coords, 1, &r, NULL);
	data->is_intersected = (mouse_x >= r.x) && (mouse_x <= r.x + r.w) && (mouse_y >= r.y) && (mouse_y <= r.y + r.h);

	const spooky_menu * parent = data->parent;

	bool handled = false;
	if(data->menu_type == SMT_MAIN_MENU) {
		spooky_menu_set_is_expanded((const spooky_menu *)self, data->is_intersected || data->is_active);

		if(event->type == SDL_MOUSEBUTTONDOWN && data->is_intersected) {
			// main menu has been clicked
			data->is_active = true;
		}
		else if((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) && !data->is_intersected) {
			// click outside of main menu
			data->active_menu = NULL;
			data->is_active = false;
		}

		if(data->menu_items_count > 0) {
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const spooky_menu * menu = &(data->menu_items[i]);
				spooky_menu_handle_event((const spooky_base *)menu, event);
			}
		}
		handled = false;
	}
	else if(data->menu_type == SMT_MAIN_MENU_ITEM) {
		if(!data->is_expanded) return false;

		data->is_hover = data->is_intersected;

		data->is_active = (data->is_intersected && parent->get_active_menu(parent)) || (data->is_intersected && parent->get_active_menu(parent) == NULL && event->type == SDL_MOUSEBUTTONDOWN);
		if(data->is_active) {
			parent->set_active_menu(parent, (const spooky_menu *)self);
		}
		uint8_t menu_alpha = 243;
		if(data->is_hover || data->is_active || parent->get_active_menu(parent) == (const spooky_menu *)self) {
			data->fg_color = (SDL_Color){ .r = 255, .g = 255, .b = 255, .a = menu_alpha};
		} else {
			data->fg_color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = menu_alpha};
		}

		/* Handle actions */
		if(data->menu_items_count > 0) {
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const spooky_menu * action = &(data->menu_items[i]);
				action->super.handle_event((const spooky_base *)action, event);
			}
		}
	}
	else if(data->menu_type == SMT_MAIN_MENU_ACTION) {
		if(!data->is_expanded) return false;

		SDL_Rect hover_rect;
		spooky_menu_get_menu_action_hover_rect((const spooky_menu *)self, r, &hover_rect);

		hover_rect.y += SPOOKY_MENU_MARGIN_TOP_BOTTOM;
		hover_rect.x = parent->get_x(parent);

		/* DEBUG: data->hover_rect = (SDL_Rect){ .x = hover_rect.x, .y = hover_rect.y, .w = hover_rect.w, .h = hover_rect.h }; */
		data->is_intersected = (mouse_x >= hover_rect.x) && (mouse_x <= hover_rect.x + hover_rect.w) && (mouse_y >= hover_rect.y) && (mouse_y <= hover_rect.y + hover_rect.h);
	}

	return handled || data->is_active;
}

static void spooky_menu_render(const spooky_base * self, SDL_Renderer * renderer) {
	static const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 };

	spooky_menu_data * data = ((const spooky_menu *)self)->data;

	if(!data->is_visible) return;

	SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };

	uint8_t red, green, blue, alpha;
	SDL_BlendMode old_blend;

	/* Preserve original draw color and blend mode */
	SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
	SDL_GetRenderDrawBlendMode(renderer, &old_blend);

	switch(data->menu_type) {
		case SMT_MAIN_MENU:
			spooky_menu_render_main_menu((const spooky_menu *)self, renderer, &r);
			break;
		case SMT_MAIN_MENU_ITEM:
		case SMT_MAIN_MENU_ACTION:
			if(data->is_expanded) {
				spooky_menu_render_main_menu_item((const spooky_menu *)self, renderer, &r);
			}
			break;
		case SMT_NULL:
		case SMT_EOE:
		default:
			break;
	}

	if(!data->is_expanded) {
		static char score[] = "Score: -2147483648 of -2147483648";
		static int score_max_len = -1;

		if(score_max_len < 0) {
			score_max_len = strlen("Score: -2147483648 of -2147483648");
		}

		/* Draw the score on the right-side of the screen */
		int sz = snprintf(NULL, 0, "Score: %i of %i", data->context->get_current_score(data->context), data->context->get_max_score(data->context));
		data->font->set_is_drop_shadow(data->font, false);
		if(sz > 0 && sz <= score_max_len) {
			int max_score = data->context->get_max_score(data->context);
			int current_score = data->context->get_current_score(data->context);

			if(current_score > 999) { current_score = 999; }
			if(max_score > 999) { max_score = 999; }

			snprintf(score, (size_t)sz + 1, "Score: %i of %i", current_score, max_score);

			SDL_Point score_point = { .x = data->font->get_m_dash(data->font), .y = 3 };
			data->font->write_to_renderer(data->font, renderer, &score_point, &black, score, strnlen(score, 1024), NULL, NULL);
		}

		static const char * title = PACKAGE_NAME;

		int width;

		data->font->measure_text(data->font, title, strnlen(title, 1024), &width, NULL);
		/* TODO: Get the proper screen width */
		SDL_Point title_point = { .x = (spooky_gui_window_default_logical_width - width) - data->font->get_m_dash(data->font), .y = 3 };
		data->font->write_to_renderer(data->font, renderer, &title_point, &black, title, strnlen(title, 1024), NULL, NULL);

		data->font->set_is_drop_shadow(data->font, true);
	}
	/* restore original blend and draw color */
	SDL_SetRenderDrawBlendMode(renderer, old_blend);
	SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

static void spooky_menu_render_main_menu(const spooky_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect) {
	spooky_menu_data * data = self->data;

	SDL_Rect r = *rect;
	SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
	SDL_SetRenderDrawBlendMode(renderer, blend_mode);

	//r.h -= 1;
	SDL_SetRenderDrawColor(renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, 255);
	SDL_RenderFillRect(renderer, &r);

	if(data->is_expanded && data->menu_items_count > 0) {
		/* render children */
		int width_accum = 0;
		for(size_t i = 0; i < data->menu_items_count; i++) {
			const spooky_menu * menu = &(data->menu_items[i]);
			if(menu->get_menu_type(menu) == SMT_MAIN_MENU_ITEM) {
				int w, h;
				data->font->measure_text(data->font, menu->get_name(menu), strnlen(menu->get_name(menu), 1024), &w, &h);
				int m_dash = data->font->get_m_dash(data->font);

				/* Spacing between menu items */
				int spacing = i == 0 ? 0 : (m_dash * 2) - 3;

				menu->set_x(menu, spacing + r.x + width_accum);
				menu->set_y(menu, r.y);

				menu->set_w(menu, w + m_dash);
				menu->set_h(menu, data->rect.h);
				menu->super.render(menu->as_base(menu), renderer);

				if(i > 0) {
					/* draw menu separators */
					SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
					SDL_RenderDrawLine(renderer, r.x + (spacing + width_accum) - 1, r.y + 2, r.x + (spacing + width_accum) - 1, r.y + r.h - 4);
				}

				width_accum += w + spacing;
			}
		}
	}
}

static void spooky_menu_render_main_menu_item(const spooky_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect) {
	spooky_menu_data * data = self->data;

	SDL_Rect r = (SDL_Rect) { .x = rect->x, .y = rect->y, .w = rect->w + 3, .h = rect->h } ;

	SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
	SDL_SetRenderDrawBlendMode(renderer, blend_mode);

	const spooky_menu * parent = data->parent;

	if(parent) {
		if(data->is_hover || data->is_active || parent->get_active_menu(parent) == self) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderFillRect(renderer, &r);
		}
		if(data->is_active || parent->get_active_menu(parent) == self) {
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

			/* Draw the drop-down rectangle */
			SDL_Rect drop_down_rect = {
				.x = r.x,
				.y = data->font->get_line_skip(data->font) + SPOOKY_MENU_MARGIN_TOP_BOTTOM,
				.w = data->max_width + 8,
				.h = data->max_height + (2 * (int)data->menu_items_count) // (((int)data->menu_items_count + 1) * data->font->get_line_skip(data->font)) + data->font->get_line_skip(data->font) + 5
			};

			SDL_RenderFillRect(renderer, &drop_down_rect);

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderDrawRect(renderer, &drop_down_rect);

			/* Draw each action */
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const spooky_menu * action = &(data->menu_items[i]);
				if(action->get_menu_type(action) == SMT_MAIN_MENU_ACTION) {
					spooky_menu_render_menu_action(action, renderer, self, &r);
				}
			}
		}

		SDL_Point text_p = { .x = r.x + (r.w / 2) - (data->name_width / 2) + 1, .y = r.y + (r.h / 2) - (data->name_height / 2)};

		data->font->set_is_drop_shadow(data->font, false);
		data->font->write_to_renderer(data->font, renderer, &text_p, &data->fg_color, data->name, strnlen(data->name, 1024), NULL, NULL);
		data->font->set_is_drop_shadow(data->font, true);
	}
}

static void spooky_menu_get_menu_action_hover_rect(const spooky_menu * self, SDL_Rect r, SDL_Rect * hover_rect) {
	spooky_menu_data * data = self->data;
	const spooky_menu * parent = data->parent;

	spooky_menu_data * parent_data = parent->data;

	SDL_Point action_p = { .x = r.x + (r.w / 2) - (parent->get_name_width(parent) / 2) + 1, .y = r.y + (r.h / 2) - (parent->get_name_height(parent) / 2)};
	action_p.y += ((int)(data->menu_item_offset) * data->font->get_line_skip(data->font)) + 5 + ((int)(data->menu_item_offset * 2));

	/* Parent max_width because the list of actions items is contained in the
	 * parent */
	*hover_rect = (SDL_Rect){ .x = r.x, .y = action_p.y, .w = parent_data->max_width + 8, .h = data->font->get_line_skip(data->font)};
}

static void spooky_menu_render_menu_action(const spooky_menu * self, SDL_Renderer * renderer, const spooky_menu * parent, const SDL_Rect * rect) {
	static const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 };
	static const SDL_Color white = { .r = 255, .g = 255, .b = 255, .a = 255 };

	(void)parent;
	spooky_menu_data * data = self->data;

	SDL_Rect r = (SDL_Rect) { .x = rect->x, .y = rect->y, .w = rect->w, .h = rect->h } ;

	data->font->set_is_drop_shadow(data->font, false);
	const char * name = spooky_menu_get_name(self);

	SDL_Rect hover_rect;

	spooky_menu_get_menu_action_hover_rect(self, r, &hover_rect);
	SDL_Point action_p = { .x = hover_rect.x + 4, .y = hover_rect.y };

	if(name[0] != '-') {
		if(data->is_intersected) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			hover_rect.y -= 1;
			hover_rect.h += 2;
			SDL_RenderFillRect(renderer, &hover_rect);

			/* DEBUG: SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); */
			/* DEBUG: SDL_RenderDrawRect(renderer, &hover_rect); */

			data->font->write_to_renderer(data->font, renderer, &action_p, &white, name, strnlen(name, 1024), NULL, NULL);
		} else {
			data->font->write_to_renderer(data->font, renderer, &action_p, &black, name, strnlen(name, 1024), NULL, NULL);
		}
	}
	data->font->set_is_drop_shadow(data->font, true);
}

static spooky_menu_types spooky_menu_get_menu_type(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->menu_type;
}

int spooky_menu_get_x(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->rect.x;
}

void spooky_menu_set_x(const spooky_menu * self, int x) {
	spooky_menu_data * data = self->data;
	data->rect.x = x;
}

int spooky_menu_get_y(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->rect.y;
}

void spooky_menu_set_y(const spooky_menu * self, int y) {
	spooky_menu_data * data = self->data;
	data->rect.y = y;
}

int spooky_menu_get_w(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->rect.w;
}

void spooky_menu_set_w(const spooky_menu * self, int w) {
	spooky_menu_data * data = self->data;
	data->rect.w = w;
}

int spooky_menu_get_h(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->rect.h;
}

void spooky_menu_set_h(const spooky_menu * self, int h) {
	spooky_menu_data * data = self->data;
	data->rect.h = h;
}

const char  * spooky_menu_get_name(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->name;
}

static void spooky_menu_set_active_menu(const spooky_menu * self, const spooky_menu * active_menu) {
	spooky_menu_data * data = self->data;
	data->active_menu = active_menu;
}

static const spooky_menu * spooky_menu_get_active_menu(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->active_menu;
}

static void spooky_menu_set_parent(const spooky_menu * self, const spooky_menu * parent) {
	spooky_menu_data * data = self->data;
	data->parent = parent;
}

static bool spooky_menu_get_is_active(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->is_active;
}

static int spooky_menu_get_name_width(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->name_width;
}

static int spooky_menu_get_name_height(const spooky_menu * self) {
	spooky_menu_data * data = self->data;
	return data->name_height;
}

static void spooky_menu_set_is_expanded(const spooky_menu * self, bool value) {
	spooky_menu_data * data = self->data;
	data->is_expanded = value;
	for(size_t i = 0; i < data->menu_items_count; i++) {
		const spooky_menu * menu = &(data->menu_items[i]);
		menu->set_is_expanded(menu, value);
	}
}





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKEN_LENGTH 1024
#define MAX_CHILDREN 256

typedef enum {
	SMTT_TOKEN_EOF = 0,
	SMTT_TOKEN_LEFT_BRACKET,
	SMTT_TOKEN_RIGHT_BRACKET,
	SMTT_TOKEN_COMMA,
	SMTT_TOKEN_IDENTIFIER,
	SMTT_TOKEN_STRING,
	SMTT_TOKEN_EOE
} spooky_menu_token_type;

typedef struct {
	spooky_menu_token_type type;
	char padding[4];
	char * value;
} spooky_menu_token;

static void spooky_menu_advance_input(const char **str, spooky_menu_token * lookahead) {
	const char * s = *str;
	while(isspace(*s)) {
		s++;
	}
	if(*s == '[' || *s == ']' || *s == ':' || *s == ',') {
		lookahead->value = calloc(2, sizeof * lookahead->value);
	}
	if(*s == '\0') {
		lookahead->type = SMTT_TOKEN_EOF;
		*str = s;
		return;
	}
	if(*s == '[') {
		lookahead->type = SMTT_TOKEN_LEFT_BRACKET;
		lookahead->value[0] = *s;
		lookahead->value[1] = '\0';
		s++;
		*str = s;
		return;
	}
	if(*s == ']') {
		lookahead->type = SMTT_TOKEN_RIGHT_BRACKET;
		lookahead->value[0] = *s;
		lookahead->value[1] = '\0';
		s++;
		*str = s;
		return;
	}
	if(*s == ',') {
		lookahead->type = SMTT_TOKEN_COMMA;
		lookahead->value[0] = *s;
		lookahead->value[1] = '\0';
		s++;
		*str = s;
		return;
	}
	if(*s == '"' || *s == '\'') {
		char buf[1024] = { 0 };
		char quote = *s;
		s++;
		int i = 0;
		while (*s != quote && *s != '\0') {
			buf[i] = *s;
			i++;
			s++;
		}
		buf[i] = '\0';
		size_t len = strnlen(buf, 1024);
		lookahead->value = calloc(len, sizeof * lookahead->value);
		memmove(lookahead->value, buf, len);
		lookahead->type = SMTT_TOKEN_STRING;
		s++;
		*str = s;
		return;
	}
	bool is_alpha = (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_';
	if(is_alpha) {
		int i = 0;
		char buf[1024] = { 0 };
		while (((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_' || (*s >= '0' && *s <= '9')) && i < MAX_TOKEN_LENGTH - 1) {
			buf[i] = *s;
			i++;
			s++;
		}
		buf[i] = '\0';
		size_t len = strnlen(buf, 1024);
		lookahead->value = calloc(len, sizeof * lookahead->value);
		memmove(lookahead->value, buf, len);
		lookahead->type = SMTT_TOKEN_IDENTIFIER;
		*str = s;
		return;
	}

	fprintf(stderr, "Invalid character: %c\n", *s);
	abort();
}

static const spooky_menu * spooky_menu_push_item(const spooky_context * context, SDL_Rect rect, const char * text, spooky_menu_types type) {
	if(menu_stack == NULL) {
		menu_stack = calloc(menu_stack_capacity, sizeof ** menu_stack);
		if(!menu_stack) { abort(); }
	}

	if(menu_stack_len + 1 > menu_stack_capacity) {
		const spooky_menu ** temp = realloc(menu_stack, (menu_stack_capacity + 8) * sizeof ** temp);
		if(!temp) { abort(); }

		menu_stack_capacity += 8;
		menu_stack = temp;
	}

	const spooky_menu * menu = spooky_menu_acquire();
	menu = menu->ctor(menu, context->get_font(context), context, text, rect, type);

	menu_stack[menu_stack_len] = menu;
	menu_stack_len++;

	return menu;
}

static const spooky_menu * spooky_menu_read_definitions(const spooky_context * context, SDL_Rect rect, const char * path) {
	static const spooky_ex _ex = { 0 };
	static const spooky_ex *ex = &_ex;

	char * buffer = NULL;
	errno_t res = spooky_io_read_buffer_from_file(path, &buffer, &ex);
	if(res || !buffer) {
		spooky_ex_print(ex);
		abort();
	}

	const char * s = buffer;

	/* Array to hold the parsed tokens */
	spooky_menu_token * tokens = calloc(256, sizeof * tokens);
	size_t tokens_len = 0;
	size_t tokens_capacity = 256;

	int count = 0;
	while(s && *s != '\0' && *s != EOF) {
		if(tokens_len + 1 > tokens_capacity) {
			spooky_menu_token * temp = realloc(tokens, (tokens_capacity * 2) * sizeof * temp);
			if(!temp) { abort(); }
			tokens_capacity *= 2;
			tokens = temp;
		}

		spooky_menu_token * T = &(tokens[tokens_len++]);
		spooky_menu_advance_input(&s, T);
		count++;
	}

	const spooky_menu * main_menu = spooky_menu_push_item(context, rect, "Main Menu", SMT_MAIN_MENU);
	const spooky_menu * current = main_menu;

	int j = 0, level = 0;
	while(j < count) {
		spooky_menu_token prev = { .type = SMTT_TOKEN_EOE, { 0 } };
		if(j - 1 > 0) {
			prev = tokens[j - 1];
		}
		spooky_menu_token t = tokens[j++];

		switch(t.type) {
			case SMTT_TOKEN_LEFT_BRACKET:
				level++;
				break;
			case SMTT_TOKEN_RIGHT_BRACKET:
				level--;
				if(level <= 0) {
					current = main_menu;
					level = 0;
				}
				break;
			case SMTT_TOKEN_IDENTIFIER:
				break;
			case SMTT_TOKEN_STRING:
				{
					if(prev.type == SMTT_TOKEN_IDENTIFIER) {
						/* TODO: Attach on_click and action handlers */
						break;
					}
					SDL_Rect r = { .x = 0, .y = 0, .w = 0, .h = 0 };
					if(level == 1) {
						/* Top-level menu item */
						const spooky_menu * menu = spooky_menu_push_item(context, r, t.value, SMT_MAIN_MENU_ITEM);
						main_menu->attach_item(main_menu, menu, &current);
					} else {
						/* Action */
						const spooky_menu * menu = spooky_menu_push_item(context, r, t.value, SMT_MAIN_MENU_ACTION);
						current->attach_item(current, menu, NULL);
					}
				}
				break;
			case SMTT_TOKEN_COMMA:
			case SMTT_TOKEN_EOE:
			case SMTT_TOKEN_EOF:
			default:
				break;
		}
	}

	free(buffer), buffer = NULL;
	free(tokens), tokens = NULL;
	free(menu_stack), menu_stack = NULL;

	return main_menu;
}

