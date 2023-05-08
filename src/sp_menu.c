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

static const sp_menu * sp_menu_read_definitions(const sp_context * context, SDL_Rect rect, const char * path);

static const int sp_MENU_MARGIN_TOP_BOTTOM = 5;

static const sp_menu ** menu_stack = NULL;
static size_t menu_stack_len = 0;
static size_t menu_stack_capacity = 8;

typedef struct sp_menu_data {
	const sp_context * context;
	const sp_font * font;
	const char * name;

	const sp_menu * active_menu;

	sp_menu * menu_items;

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

	sp_menu_types menu_type;

	bool is_active;
	bool is_visible;
	bool is_hover;
	bool is_intersected;

	bool is_expanded;

	char padding[7];

	const sp_menu * parent;

	int max_width;
	int max_height;

	/* DEBUG: SDL_Rect hover_rect; */
} sp_menu_data;

/* Menu configuration (main menu, menu items, etc. */
typedef void (*event_handler)(void);

typedef struct menu_selection {
	const char * name;
	event_handler on_click;

	const sp_menu * menu_action;
} menu_selection;

typedef struct menu_items {
	const char * name;
	const sp_menu * menu_item;
	size_t selections_count;
	menu_selection * selections;
} menu_items;

static const sp_menu * sp_menu_get_active_menu(const sp_menu * self);
static void sp_menu_set_active_menu(const sp_menu * self, const sp_menu * active_menu);
static const sp_menu * sp_menu_attach_item(const sp_menu * self, const sp_menu * item, const sp_menu ** out_copy);

static bool sp_menu_handle_event(const sp_base * self, SDL_Event * event);
// static void sp_menu_handle_delta(const sp_base * self, const SDL_Event * event, uint64_t last_update_time, double interpolation);
static void sp_menu_render(const sp_base * self, SDL_Renderer * renderer);

static void sp_menu_render_main_menu(const sp_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect);
static void sp_menu_render_main_menu_item(const sp_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect);
static void sp_menu_render_menu_action(const sp_menu * self, SDL_Renderer * renderer, const sp_menu * parent, const SDL_Rect * rect);

static sp_menu_types sp_menu_get_menu_type(const sp_menu * self);

static int sp_menu_get_x(const sp_menu * self);
static void sp_menu_set_x(const sp_menu * self, int x);
static int sp_menu_get_y(const sp_menu * self);
static void sp_menu_set_y(const sp_menu * self, int y);
static int sp_menu_get_w(const sp_menu * self);
static void sp_menu_set_w(const sp_menu * self, int w);
static int sp_menu_get_h(const sp_menu * self);
static void sp_menu_set_h(const sp_menu * self, int h);

static const char  * sp_menu_get_name(const sp_menu * self);

static void sp_menu_set_parent(const sp_menu * self, const sp_menu * parent);
static bool sp_menu_get_is_active(const sp_menu * self);

static int sp_menu_get_name_width(const sp_menu * self);
static int sp_menu_get_name_height(const sp_menu * self);

static void sp_menu_get_menu_action_hover_rect(const sp_menu * self, SDL_Rect r, SDL_Rect * out_rect);

static void sp_menu_set_is_expanded(const sp_menu * self, bool value);

static void sp_menu_copy(const sp_menu * source, sp_menu * dest);

static const sp_base * sp_menu_as_base(const sp_menu * self) {
	return (const sp_base *)self;
}

const sp_menu * sp_menu_alloc(void) {
	sp_menu * self = calloc(1, sizeof * self);
	if(!self) abort();

	return self;
}

static void sp_menu_copy(const sp_menu * source, sp_menu * dest) {
	const sp_menu * new = sp_menu_init(dest);

	sp_menu_data * data = calloc(1, sizeof * data);
	if(!data) { abort(); }

	((sp_menu *)(uintptr_t)new)->data = data;

	memmove((void *)(uintptr_t)new->data, source->data, sizeof * source->data);
}

const sp_menu * sp_menu_init(sp_menu * self) {
	if(!self) { abort(); }

	self->ctor = &sp_menu_ctor;
	self->dtor = &sp_menu_dtor;
	self->free = &sp_menu_free;
	self->release = &sp_menu_release;
	self->as_base = &sp_menu_as_base;

	self->attach_item = &sp_menu_attach_item;

	self->get_name = &sp_menu_get_name;

	self->super.handle_event = &sp_menu_handle_event;
	self->super.render = &sp_menu_render;
	self->get_menu_type = &sp_menu_get_menu_type;

	self->get_x = &sp_menu_get_x;
	self->set_x = &sp_menu_set_x;

	self->get_y = &sp_menu_get_y;
	self->set_y = &sp_menu_set_y;

	self->get_w = &sp_menu_get_w;
	self->set_w = &sp_menu_set_w;

	self->get_h = &sp_menu_get_h;
	self->set_h = &sp_menu_set_h;

	self->get_active_menu = &sp_menu_get_active_menu;
	self->set_active_menu = &sp_menu_set_active_menu;

	self->set_parent = &sp_menu_set_parent;
	self->get_is_active = &sp_menu_get_is_active;

	self->get_name_width = &sp_menu_get_name_width;
	self->get_name_height = &sp_menu_get_name_height;
	self->set_is_expanded = &sp_menu_set_is_expanded;

	return self;
}

const sp_menu * sp_menu_acquire(void) {
	return sp_menu_init((sp_menu *)(uintptr_t)sp_menu_alloc());
}

const sp_menu * sp_menu_ctor(const sp_menu * self, const sp_font * font, const sp_context * context, const char * name, SDL_Rect rect, sp_menu_types menu_type) {
	sp_menu_data * data = calloc(1, sizeof * data);
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

	((sp_menu *)(uintptr_t)self)->data = data;

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

const sp_menu * sp_menu_dtor(const sp_menu * self) {
	if(self) {
		sp_menu_data * data = self->data;

		free((void *)(uintptr_t)data->name), data->name = NULL;
		if(data && data->menu_items_count > 0) {
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const sp_menu * menu = &(data->menu_items[i]);
				menu->free(menu->dtor(menu));
			}
			free((void *)(uintptr_t)data->menu_items), data->menu_items = NULL;
		}

		free(self->data);
	}

	return self;
}

void sp_menu_free(const sp_menu * self) {
	if(self) {
		free((void *)(uintptr_t)self), self = NULL;
	}
}

void sp_menu_release(const sp_menu * self) {
	self->free(self->dtor(self));
}

const sp_menu * sp_menu_load_from_file(const sp_context * context, SDL_Rect rect, const char * path) {
	return sp_menu_read_definitions(context, rect, path);
}

static const sp_menu * sp_menu_attach_item(const sp_menu * self, const sp_menu * item, const sp_menu ** out_copy) {
	sp_menu_data * data = self->data;
	if(!data->menu_items) {
		data->menu_items = calloc(data->menu_items_capacity, sizeof * data->menu_items);
		if(!data->menu_items) { abort(); }
	}

	if(data->menu_items_count + 1 > data->menu_items_capacity) {
		data->menu_items_capacity += 8;
		sp_menu * temp_menu_items = realloc(data->menu_items, sizeof data->menu_items[0] * data->menu_items_capacity);
		if(!temp_menu_items) { abort(); }
		data->menu_items = temp_menu_items;
	}

	sp_menu * copy = &(data->menu_items[data->menu_items_count]);
	sp_menu_copy(item, copy);

	if(out_copy) {
		*out_copy = copy;
	}

	sp_menu_data * copy_data = copy->data;

	copy->set_parent(copy, self);

	data->menu_items_count++;
	copy_data->menu_item_offset = data->menu_items_count;

	int width, height;
	data->font->measure_text(data->font, copy_data->name, strnlen(copy_data->name, 1024), &width, &height);
	data->max_width = sp_int_max(data->max_width, width);
	data->max_height = data->font->get_line_skip(data->font) * ((int)data->menu_items_count + 1);

	return self;
}

static bool sp_menu_handle_event(const sp_base * self, SDL_Event * event) {
	sp_menu_data * data = ((const sp_menu *)self)->data;

	SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };

	int mouse_x = event->button.x;
	if(mouse_x == 0) { mouse_x = data->last_mouse_x; }

	int mouse_y = event->button.y;
	if(mouse_y == 0) { mouse_y = data->last_mouse_y; }

	data->last_mouse_x = mouse_x;
	data->last_mouse_y = mouse_y;

	// data->is_intersected = SDL_EnclosePoints(&mouse_coords, 1, &r, NULL);
	data->is_intersected = (mouse_x >= r.x) && (mouse_x <= r.x + r.w) && (mouse_y >= r.y) && (mouse_y <= r.y + r.h);

	const sp_menu * parent = data->parent;

	bool handled = false;
	if(data->menu_type == SMT_MAIN_MENU) {
		sp_menu_set_is_expanded((const sp_menu *)self, data->is_intersected || data->is_active);

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
				const sp_menu * menu = &(data->menu_items[i]);
				sp_menu_handle_event((const sp_base *)menu, event);
			}
		}
		handled = false;
	}
	else if(data->menu_type == SMT_MAIN_MENU_ITEM) {
		if(!data->is_expanded) return false;

		data->is_hover = data->is_intersected;

		data->is_active = (data->is_intersected && parent->get_active_menu(parent)) || (data->is_intersected && parent->get_active_menu(parent) == NULL && event->type == SDL_MOUSEBUTTONDOWN);
		if(data->is_active) {
			parent->set_active_menu(parent, (const sp_menu *)self);
		}
		uint8_t menu_alpha = 243;
		if(data->is_hover || data->is_active || parent->get_active_menu(parent) == (const sp_menu *)self) {
			data->fg_color = (SDL_Color){ .r = 255, .g = 255, .b = 255, .a = menu_alpha};
		} else {
			data->fg_color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = menu_alpha};
		}

		/* Handle actions */
		if(data->menu_items_count > 0) {
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const sp_menu * action = &(data->menu_items[i]);
				action->super.handle_event((const sp_base *)action, event);
			}
		}
	}
	else if(data->menu_type == SMT_MAIN_MENU_ACTION) {
		if(!data->is_expanded) return false;

		SDL_Rect hover_rect;
		sp_menu_get_menu_action_hover_rect((const sp_menu *)self, r, &hover_rect);

		hover_rect.y += sp_MENU_MARGIN_TOP_BOTTOM;
		hover_rect.x = parent->get_x(parent);

		/* DEBUG: data->hover_rect = (SDL_Rect){ .x = hover_rect.x, .y = hover_rect.y, .w = hover_rect.w, .h = hover_rect.h }; */
		data->is_intersected = (mouse_x >= hover_rect.x) && (mouse_x <= hover_rect.x + hover_rect.w) && (mouse_y >= hover_rect.y) && (mouse_y <= hover_rect.y + hover_rect.h);
	}

	return handled || data->is_active;
}

static void sp_menu_render(const sp_base * self, SDL_Renderer * renderer) {
	static const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 };

	sp_menu_data * data = ((const sp_menu *)self)->data;

	if(!data->is_visible) return;

	SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };

	uint8_t red, green, blue, alpha;
	SDL_BlendMode old_blend;

	/* Preserve original draw color and blend mode */
	SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
	SDL_GetRenderDrawBlendMode(renderer, &old_blend);

	switch(data->menu_type) {
		case SMT_MAIN_MENU:
			sp_menu_render_main_menu((const sp_menu *)self, renderer, &r);
			break;
		case SMT_MAIN_MENU_ITEM:
		case SMT_MAIN_MENU_ACTION:
			if(data->is_expanded) {
				sp_menu_render_main_menu_item((const sp_menu *)self, renderer, &r);
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
		SDL_Point title_point = { .x = (sp_gui_window_default_logical_width - width) - data->font->get_m_dash(data->font), .y = 3 };
		data->font->write_to_renderer(data->font, renderer, &title_point, &black, title, strnlen(title, 1024), NULL, NULL);

		data->font->set_is_drop_shadow(data->font, true);
	}
	/* restore original blend and draw color */
	SDL_SetRenderDrawBlendMode(renderer, old_blend);
	SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

static void sp_menu_render_main_menu(const sp_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect) {
	sp_menu_data * data = self->data;

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
			const sp_menu * menu = &(data->menu_items[i]);
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

static void sp_menu_render_main_menu_item(const sp_menu * self, SDL_Renderer * renderer, const SDL_Rect * rect) {
	sp_menu_data * data = self->data;

	SDL_Rect r = (SDL_Rect) { .x = rect->x, .y = rect->y, .w = rect->w + 3, .h = rect->h } ;

	SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
	SDL_SetRenderDrawBlendMode(renderer, blend_mode);

	const sp_menu * parent = data->parent;

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
				.y = data->font->get_line_skip(data->font) + sp_MENU_MARGIN_TOP_BOTTOM,
				.w = data->max_width + 8,
				.h = data->max_height + (2 * (int)data->menu_items_count) // (((int)data->menu_items_count + 1) * data->font->get_line_skip(data->font)) + data->font->get_line_skip(data->font) + 5
			};

			SDL_RenderFillRect(renderer, &drop_down_rect);

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderDrawRect(renderer, &drop_down_rect);

			/* Draw each action */
			for(size_t i = 0; i < data->menu_items_count; i++) {
				const sp_menu * action = &(data->menu_items[i]);
				if(action->get_menu_type(action) == SMT_MAIN_MENU_ACTION) {
					sp_menu_render_menu_action(action, renderer, self, &r);
				}
			}
		}

		SDL_Point text_p = { .x = r.x + (r.w / 2) - (data->name_width / 2) + 1, .y = r.y + (r.h / 2) - (data->name_height / 2)};

		data->font->set_is_drop_shadow(data->font, false);
		data->font->write_to_renderer(data->font, renderer, &text_p, &data->fg_color, data->name, strnlen(data->name, 1024), NULL, NULL);
		data->font->set_is_drop_shadow(data->font, true);
	}
}

static void sp_menu_get_menu_action_hover_rect(const sp_menu * self, SDL_Rect r, SDL_Rect * hover_rect) {
	sp_menu_data * data = self->data;
	const sp_menu * parent = data->parent;

	sp_menu_data * parent_data = parent->data;

	SDL_Point action_p = { .x = r.x + (r.w / 2) - (parent->get_name_width(parent) / 2) + 1, .y = r.y + (r.h / 2) - (parent->get_name_height(parent) / 2)};
	action_p.y += ((int)(data->menu_item_offset) * data->font->get_line_skip(data->font)) + 5 + ((int)(data->menu_item_offset * 2));

	/* Parent max_width because the list of actions items is contained in the
	 * parent */
	*hover_rect = (SDL_Rect){ .x = r.x, .y = action_p.y, .w = parent_data->max_width + 8, .h = data->font->get_line_skip(data->font)};
}

static void sp_menu_render_menu_action(const sp_menu * self, SDL_Renderer * renderer, const sp_menu * parent, const SDL_Rect * rect) {
	static const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 };
	static const SDL_Color white = { .r = 255, .g = 255, .b = 255, .a = 255 };

	(void)parent;
	sp_menu_data * data = self->data;

	SDL_Rect r = (SDL_Rect) { .x = rect->x, .y = rect->y, .w = rect->w, .h = rect->h } ;

	data->font->set_is_drop_shadow(data->font, false);
	const char * name = sp_menu_get_name(self);

	SDL_Rect hover_rect;

	sp_menu_get_menu_action_hover_rect(self, r, &hover_rect);
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

static sp_menu_types sp_menu_get_menu_type(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->menu_type;
}

int sp_menu_get_x(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->rect.x;
}

void sp_menu_set_x(const sp_menu * self, int x) {
	sp_menu_data * data = self->data;
	data->rect.x = x;
}

int sp_menu_get_y(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->rect.y;
}

void sp_menu_set_y(const sp_menu * self, int y) {
	sp_menu_data * data = self->data;
	data->rect.y = y;
}

int sp_menu_get_w(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->rect.w;
}

void sp_menu_set_w(const sp_menu * self, int w) {
	sp_menu_data * data = self->data;
	data->rect.w = w;
}

int sp_menu_get_h(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->rect.h;
}

void sp_menu_set_h(const sp_menu * self, int h) {
	sp_menu_data * data = self->data;
	data->rect.h = h;
}

const char  * sp_menu_get_name(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->name;
}

static void sp_menu_set_active_menu(const sp_menu * self, const sp_menu * active_menu) {
	sp_menu_data * data = self->data;
	data->active_menu = active_menu;
}

static const sp_menu * sp_menu_get_active_menu(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->active_menu;
}

static void sp_menu_set_parent(const sp_menu * self, const sp_menu * parent) {
	sp_menu_data * data = self->data;
	data->parent = parent;
}

static bool sp_menu_get_is_active(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->is_active;
}

static int sp_menu_get_name_width(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->name_width;
}

static int sp_menu_get_name_height(const sp_menu * self) {
	sp_menu_data * data = self->data;
	return data->name_height;
}

static void sp_menu_set_is_expanded(const sp_menu * self, bool value) {
	sp_menu_data * data = self->data;
	data->is_expanded = value;
	for(size_t i = 0; i < data->menu_items_count; i++) {
		const sp_menu * menu = &(data->menu_items[i]);
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
} sp_menu_token_type;

typedef struct {
	sp_menu_token_type type;
	char padding[4];
	char * value;
} sp_menu_token;

static void sp_menu_advance_input(const char **str, sp_menu_token * lookahead) {
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

static const sp_menu * sp_menu_push_item(const sp_context * context, SDL_Rect rect, const char * text, sp_menu_types type) {
	if(menu_stack == NULL) {
		menu_stack = calloc(menu_stack_capacity, sizeof ** menu_stack);
		if(!menu_stack) { abort(); }
	}

	if(menu_stack_len + 1 > menu_stack_capacity) {
		const sp_menu ** temp = realloc(menu_stack, (menu_stack_capacity + 8) * sizeof ** temp);
		if(!temp) { abort(); }

		menu_stack_capacity += 8;
		menu_stack = temp;
	}

	const sp_menu * menu = sp_menu_acquire();
	menu = menu->ctor(menu, context->get_font(context), context, text, rect, type);

	menu_stack[menu_stack_len] = menu;
	menu_stack_len++;

	return menu;
}

static const sp_menu * sp_menu_read_definitions(const sp_context * context, SDL_Rect rect, const char * path) {
	static const sp_ex _ex = { 0 };
	static const sp_ex *ex = &_ex;

	char * buffer = NULL;
	errno_t res = sp_io_read_buffer_from_file(path, &buffer, &ex);
	if(res || !buffer) {
		sp_ex_print(ex);
		abort();
	}

	const char * s = buffer;

	/* Array to hold the parsed tokens */
	sp_menu_token * tokens = calloc(256, sizeof * tokens);
	size_t tokens_len = 0;
	size_t tokens_capacity = 256;

	int count = 0;
	while(s && *s != '\0' && *s != EOF) {
		if(tokens_len + 1 > tokens_capacity) {
			sp_menu_token * temp = realloc(tokens, (tokens_capacity * 2) * sizeof * temp);
			if(!temp) { abort(); }
			tokens_capacity *= 2;
			tokens = temp;
		}

		sp_menu_token * T = &(tokens[tokens_len++]);
		sp_menu_advance_input(&s, T);
		count++;
	}

	const sp_menu * main_menu = sp_menu_push_item(context, rect, "Main Menu", SMT_MAIN_MENU);
	const sp_menu * current = main_menu;

	int j = 0, level = 0;
	while(j < count) {
		sp_menu_token prev = { .type = SMTT_TOKEN_EOE, { 0 } };
		if(j - 1 > 0) {
			prev = tokens[j - 1];
		}
		sp_menu_token t = tokens[j++];

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
						const sp_menu * menu = sp_menu_push_item(context, r, t.value, SMT_MAIN_MENU_ITEM);
						main_menu->attach_item(main_menu, menu, &current);
					} else {
						/* Action */
						const sp_menu * menu = sp_menu_push_item(context, r, t.value, SMT_MAIN_MENU_ACTION);
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

