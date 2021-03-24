#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "sp_math.h"
#include "sp_gui.h"
#include "sp_wm.h"
#include "sp_box.h"
#include "sp_limits.h"

typedef struct spooky_box_data {
  const char * name;
  size_t name_len;
} spooky_box_data;

static bool spooky_box_handle_event(const spooky_base * self, SDL_Event * event);
static void spooky_box_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
static void spooky_box_render(const spooky_base * self, SDL_Renderer * renderer);

const spooky_base * spooky_box_as_base(const spooky_box * self) {
  return (const spooky_base *)self;
}

const spooky_box * spooky_box_alloc() {
  spooky_box * self = calloc(1, sizeof * self);
  return self;
}

const spooky_box * spooky_box_init(spooky_box * self) {
  assert(self);
  if(!self) { abort(); }

  self = (spooky_box *)(uintptr_t)spooky_base_init((spooky_base *)(uintptr_t)self);

  self->as_base = &spooky_box_as_base;
  self->ctor = &spooky_box_ctor;
  self->dtor = &spooky_box_dtor;
  self->free = &spooky_box_free;
  self->release = &spooky_box_release;

  self->super.handle_event = &spooky_box_handle_event;
  self->super.handle_delta = &spooky_box_handle_delta;
  self->super.render = &spooky_box_render;

  return self;
}

const spooky_box * spooky_box_acquire() {
  return spooky_box_init((spooky_box *)(uintptr_t)spooky_box_alloc());
}

const spooky_box * spooky_box_ctor(const spooky_box * self) {
  assert(self);
  if(!self) { abort(); }

  SDL_Rect origin = { .x = 0, .y = 0, .w = 0, .h = 0 };
  self = (spooky_box *)(uintptr_t)spooky_base_ctor((spooky_base *)(uintptr_t)self, origin);

  spooky_box_data * data = calloc(1, sizeof * data);

  ((spooky_box *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_box * spooky_box_dtor(const spooky_box * self) {
  if(!self) { return self; }

  free(((spooky_box *)(uintptr_t)self)->data), ((spooky_box *)(uintptr_t)self)->data = NULL;

  return self;
}

void spooky_box_free(const spooky_box * self) {
  free((void *)(uintptr_t)self), self = NULL;
}

void spooky_box_release(const spooky_box * self) {
  self->free(self->dtor(self));
}

static bool spooky_box_handle_event(const spooky_base * self, SDL_Event * event) {
  if(self->get_children_count(self) > 0) {
    const spooky_iter * it = self->get_iterator(self);
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object && object->handle_event) {
        bool handled = object->handle_event(object, event);
        if(handled) { return handled; }
      }
    }
  }

  return false;
}

static void spooky_box_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation) {
  if(self->get_children_count(self) > 0) {
    const spooky_iter * it = self->get_iterator(self);
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object && object->handle_delta) {
        object->handle_delta(object, last_update_time,interpolation);
      }
    }
  }
}

static void spooky_box_render(const spooky_base * self, SDL_Renderer * renderer) {
  if(self->get_children_count(self) > 0) {
    const spooky_iter * it = self->get_iterator(self);
    it->reset(it);
    while(it->next(it)) {
      const spooky_base * object = it->current(it);
      if(object && object->render) {
        object->render(object, renderer);
      }
    }
  }

  uint8_t r, g, b, a;
  SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
  SDL_SetRenderDrawColor(renderer, 255, 0, 0, 0);
  SDL_RenderDrawRect(renderer, self->get_rect(self));
  SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

#if 1 == 2
static int spooky_gui_y_padding = 10;
static int spooky_gui_x_padding = 10;

#define WIDTH(w) ((w) * (int)((get_ui_scale_factor() + (0.1f) - 1) * 10) / 2)
#define HEIGHT(h) ((h) * (int)((get_ui_scale_factor() + (0.1f) - 1) * 10) / 2)

static const int max_texture_rect_width = 64;
static const int max_texture_rect_height = 64;

static const bool spooky_box_enable_diagnostics = false;

/* TODO: Fix non-const initializer */
const int scroll_bar_width = /* spooky_gui_x_padding */ 5 * 2;
const int scroll_bar_height = /* spooky_gui_y_padding*/ 5 * 2;
const int scroll_bar_height_scale = 2;

static const SDL_Color white = { .r = 255, .g = 255, .b = 255, .a = 255 };
static const SDL_Color black = { .r = 0, .g = 0, .b = 0, .a = 255 };

static const int MENU_HEIGHT = 12;

typedef struct spooky_box_data {
  const spooky_context * context;
  const spooky_wm * wm;
  const spooky_font * font;

  const char * name;
  int name_height;
  int name_width;

  SDL_Window * window;
  SDL_Renderer * renderer;
  SDL_Texture * dialog_texture;
  SDL_Texture * sprite_sheet_texture;

  const spooky_box * parent;
  const spooky_box ** boxes;

  int boxes_capacity;
  int boxes_count;

  SDL_Rect rect;

  SDL_Color bg_color;

  int offset_x;
  int offset_y;
  int z_order;

  spooky_box_types box_type;

  bool is_draggable;
  bool is_dragging;
  bool is_visible;

  bool is_active;
  bool is_top;
  bool is_hit;

  bool is_shade; /* needs improvement */
  bool is_minimized; /* not implemented */
  bool is_maximized; /* not implemented */

  bool is_image_collection;
  char padding[2];
} spooky_box_data;

typedef enum spooky_box_scroll_box_button_types {
  SBSBB_NULL,
  SBSBB_UP,
  SBSBB_DOWN,
  SBSBB_VERTICAL_SCROLL,
  SBSBB_LEFT,
  SBSBB_RIGHT,
  SBSBB_HORIZONTAL_SCROLL,
  SBSBB_EOE
} spooky_box_scroll_box_button_types;

typedef struct spooky_box_scroll_box_button {
  spooky_box_scroll_box_button_types button;
  bool is_hover;
  bool is_active;
  char padding0[2];
  SDL_Rect * rect;
  char padding1[8];
} spooky_box_scroll_box_button;

typedef enum spooky_box_scroll_box_item_types {
  SBSBIT_NULL = 0,
  SBSBIT_TEXT,
  SBSBIT_IMAGE,
  SBSBIT_EOE
} spooky_box_scroll_box_item_types;

typedef struct spooky_box_scroll_box_item spooky_box_scroll_box_item;

typedef struct spooky_box_scroll_box_data {
  spooky_box_data _super;

  spooky_box_scroll_box_item * scroll_bar_items;

  SDL_Texture * box_texture;
  SDL_Texture * diamond_texture;
  SDL_Texture * scroll_window_texture;

  int * selection_offsets; /* offsets (x * y) of selected items; used for hover detection */

  int scroll_bar_items_count;
  int scroll_bar_items_capacity;

  int scroll_region_height; /* scroll bar height based on items count */
  int scroll_region_width;

  SDL_Rect scroll_box_vertical_rect; /* full vertical scroll box region */
  SDL_Rect scroll_box_horizontal_rect; /* full horizontal scroll box region */

  SDL_Rect up_rect; /* up button region */
  SDL_Rect scroll_vertical_rect; /* scroll bar vertical region (relative to _super->rect) */
  SDL_Rect down_rect; /* down button region */

  SDL_Rect left_rect; /* left button region */
  SDL_Rect scroll_horizontal_rect; /* scroll bar horizontal region (relative to _super->rect */
  SDL_Rect right_rect; /* right button region */

  SDL_Rect visible_scroll_window_rect;

  int last_mouse_x;
  int last_mouse_y;

  int scroll_vertical_offset_x;
  int scroll_vertical_offset_y;

  int scroll_horizontal_offset_x;
  int scroll_horizontal_offset_y;

  /* shortcut to the texture width/height */
  int scroll_window_texture_width;
  int scroll_window_texture_height;

  int hover_item_ordinal;
  int selected_item_ordinal;

  float percent_vertically_scrolled;
  float percent_horizontally_scrolled;

  spooky_box_scroll_box_direction direction;
  char padding0[4];
  spooky_box_scroll_box_button scroll_button;

  bool is_vertical_scroll_bar_visible;
  bool is_horizontal_scroll_bar_visible;

  bool is_scroll_window_texture_invalidated;
  bool is_vertical_scroll_bar_dragging;
  bool is_horizontal_scroll_bar_dragging;

  char padding1[3];
} spooky_box_scroll_box_data;

typedef struct spooky_box_scroll_box_item {
  const spooky_box * parent;
  const char * text;
  const char * description;

  const char * path;
  SDL_Texture * texture;

  SDL_Rect rect;
  int width;
  int height;

  spooky_box_scroll_box_item_types type;

  bool is_selected;
  bool is_hover;
  bool is_enabled;
  bool is_text_alloc;
} spooky_box_scroll_box_item;

const spooky_box * spooky_box_attach_box(const spooky_box * self, const spooky_box * box);
static const spooky_box_scroll_box_item * spooky_box_attach_scroll_box_item_text(const spooky_box * self, const char * text, const char * description);
static const spooky_box_scroll_box_item * spooky_box_attach_scroll_box_item_image(const spooky_box * self, const char * text, const char * description, const char * path);

bool spooky_box_handle_event(const spooky_base * self, SDL_Event * event);
void spooky_box_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
void spooky_box_render(const spooky_base * self, SDL_Renderer * renderer);

bool spooky_box_scroll_box_handle_event(const spooky_base * self, SDL_Event * event);
void spooky_box_scroll_box_handle_delta(const spooky_base * self, int64_t last_update_time, double interpolation);
void spooky_box_scroll_box_render(const spooky_base * self, SDL_Renderer * renderer);


static void spooky_box_set_z_order(const spooky_box * self, int z_order);
static int spooky_box_get_z_order(const spooky_box * self);

const SDL_Rect * spooky_box_get_rect(const spooky_box * self);

static int spooky_box_get_x(const spooky_box * self);
static void spooky_box_set_x(const spooky_box * self, int x);

static int spooky_box_get_y(const spooky_box * self);
static void spooky_box_set_y(const spooky_box * self, int y);

static int spooky_box_get_w(const spooky_box * self);
static void spooky_box_set_w(const spooky_box * self, int w);

static int spooky_box_get_h(const spooky_box * self);
static void spooky_box_set_h(const spooky_box * self, int h);

static const char * spooky_box_get_name(const spooky_box * self);

static spooky_box_types spooky_box_get_box_type(const spooky_box * self);

static void spooky_box_draw_window(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_window_textured(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_button(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_main_menu(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_main_menu_item(const spooky_box * self, const SDL_Rect * rect);

static void spooky_box_draw_image(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_text(const spooky_box * self, const SDL_Rect * rect);

static void spooky_box_draw_3d_rectangle(const spooky_box * self, const SDL_Rect * rect);
static void spooky_box_draw_3d_rectangle_v2(const spooky_box * self, const SDL_Rect * rect, const SDL_Color * top_line_color, const SDL_Color * bottom_line_color);

static void spooky_box_render_scroll_button(const spooky_box * self, const SDL_Rect * button_dest, spooky_box_scroll_box_button_types button_type);

static void spooky_box_scroll_box_recalculate_scroll_bars(const spooky_box * self);

static int spooky_box_get_max_visible_items(const spooky_box * self);

static const spooky_box * spooky_box_render_scroll_box_texture(const spooky_box * self);
static void spooky_box_set_direction(const spooky_box * self, spooky_box_scroll_box_direction direction);
static void spooky_box_set_scrolled_percentage(float * percent, float new_value);
static void spooky_box_scroll_box_mouse_move_delta(int len, int delta, int offset, int * value, float * percentage);

static void spooky_box_calculate_max_text_properties(const spooky_box * self, int * max_text_width, int * max_text_height);

const spooky_box_interface spooky_box_class = {
  .ctor = &spooky_box_ctor,
  .dtor = &spooky_box_dtor,
  .free = &spooky_box_free,
  .release = &spooky_box_release,

  .super.handle_event = &spooky_box_handle_event,
  .super.handle_delta = &spooky_box_handle_delta,
  .super.render = &spooky_box_render,

  .set_z_order = &spooky_box_set_z_order,
  .get_z_order = &spooky_box_get_z_order,

  .get_name = &spooky_box_get_name,
  .get_rect = &spooky_box_get_rect,
  .attach_box = &spooky_box_attach_box,
  .get_box_type = &spooky_box_get_box_type,

  .get_x = &spooky_box_get_x,
  .set_x = &spooky_box_set_x,

  .get_y = &spooky_box_get_y,
  .set_y = &spooky_box_set_y,

  .get_w = &spooky_box_get_w,
  .set_w = &spooky_box_set_w,

  .get_h = &spooky_box_get_h,
  .set_h = &spooky_box_set_h,

  .attach_scroll_box_item_text = &spooky_box_attach_scroll_box_item_text,
  .attach_scroll_box_item_image = &spooky_box_attach_scroll_box_item_image
};

const spooky_box_interface spooky_box_scroll_box_class = {
  .ctor = &spooky_box_scroll_box_ctor,
  .dtor = &spooky_box_scroll_box_dtor,
  .free = &spooky_box_free,
  .release = &spooky_box_release,

  .super.handle_event = &spooky_box_scroll_box_handle_event,
  .super.handle_delta = &spooky_box_scroll_box_handle_delta,
  .super.render = &spooky_box_scroll_box_render,

  .set_z_order = &spooky_box_set_z_order,
  .get_z_order = &spooky_box_get_z_order,

  .get_name = &spooky_box_get_name,
  .get_rect = &spooky_box_get_rect,
  .attach_box = &spooky_box_attach_box,
  .get_box_type = &spooky_box_get_box_type,

  .get_x = &spooky_box_get_x,
  .set_x = &spooky_box_set_x,

  .get_y = &spooky_box_get_y,
  .set_y = &spooky_box_set_y,

  .get_w = &spooky_box_get_w,
  .set_w = &spooky_box_set_w,

  .get_h = &spooky_box_get_h,
  .set_h = &spooky_box_set_h,

  .attach_scroll_box_item_text = &spooky_box_attach_scroll_box_item_text,
  .attach_scroll_box_item_image = &spooky_box_attach_scroll_box_item_image,

  .set_direction = &spooky_box_set_direction
};

const spooky_base * spooky_box_as_base(const spooky_box * self) {
  return (const spooky_base *)self;
}

const spooky_box * spooky_box_alloc() {
  spooky_box * self = malloc(sizeof * self);
  if(!self) abort();
  memset(self, 0, sizeof * self);

  spooky_box_data * data = malloc(sizeof * data);
  if(!data) abort();
  memset(data, 0, sizeof * data);

  self->data = data;

  return self;
}

const spooky_box * spooky_box_scroll_box_alloc() {
  spooky_box * self = malloc(sizeof * self);
  if(!self) abort();
  memset(self, 0, sizeof * self);

  spooky_box_scroll_box_data * data = malloc(sizeof * data);
  if(!data) abort();
  memset(data, 0, sizeof * data);

  self->data = (spooky_box_data *)data;

  return self;
}

const spooky_box * spooky_box_init(spooky_box * self) {
  if(!self) abort();
  /* No-op functions for now */
  return self;
}

const spooky_box * spooky_box_scroll_box_init(spooky_box * self) {
  if(!self) abort();
  /* No-op functions for now */
  return self;
}

const spooky_box * spooky_box_acquire() {
  return spooky_box_init((spooky_box *)(uintptr_t)spooky_box_alloc());
}

const spooky_box * spooky_box_scroll_box_acquire() {
  return spooky_box_scroll_box_init((spooky_box *)(uintptr_t)spooky_box_scroll_box_alloc());
}

const spooky_box * spooky_box_ctor(const spooky_box * self, const spooky_context * context, const spooky_wm * wm, const spooky_box * parent,  SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, spooky_box_types box_type) {
  spooky_box_data * data = self->data;

  data->context = context;
  data->wm = wm;
  data->window = window;
  data->renderer = renderer;

  data->name = name;

  data->font = data->context->get_font(data->context);
  data->font->measure_text(data->font, data->name, strnlen(data->name, 256), &(data->name_width), &(data->name_height));

  data->is_hit = false;
  data->is_top = false;
  data->is_active = false;

  data->is_draggable = box_type == SBT_WINDOW || box_type == SBT_WINDOW_TEXTURED;
  data->is_dragging = false;

  data->is_visible = is_visible;
  data->box_type = box_type;

  data->bg_color = (SDL_Color){ .r = 170, .g = 170, .b = 170, .a = 255 };

  data->rect = rect;

  data->offset_x = 0;
  data->offset_y = 0;
  data->z_order = 0;

  SDL_Surface * dialog = NULL;
  spooky_load_image("./res/dialog_texture.png", strlen("./res/dialog_texture.png"), &dialog);
  data->dialog_texture = SDL_CreateTextureFromSurface(renderer, dialog);
  SDL_FreeSurface(dialog), dialog = NULL;

  SDL_Surface * sprite_sheet = NULL;
  spooky_load_image("./res/sprites.png", strlen("./res/sprites.png"), &sprite_sheet);
  data->sprite_sheet_texture = SDL_CreateTextureFromSurface(renderer, sprite_sheet);
  SDL_FreeSurface(sprite_sheet), sprite_sheet = NULL;

  data->parent = parent;
  data->boxes = NULL;
  data->boxes_capacity = 16;
  data->boxes_count = 0;

  data->is_shade = false;
  data->is_minimized = false;
  data->is_maximized = false;
  data->is_image_collection = false;

  return self;
}

const spooky_box * spooky_box_scroll_box_ctor(const spooky_box * self, const spooky_context * context, const spooky_wm * wm, const spooky_box * parent,  SDL_Window * window, SDL_Renderer * renderer, const char * name, SDL_Rect rect, bool is_visible, spooky_box_types box_type) {
  self = spooky_box_ctor(self, context, wm, parent, window, renderer, name, rect, is_visible, box_type);

  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(self->data);

  data->scroll_bar_items = NULL;
  data->scroll_bar_items_capacity = 16;
  data->scroll_bar_items_count = 0;

  SDL_Surface * diamond_texture = NULL;
  spooky_load_image("./res/diamond.png", strlen("./res/diamond.png"), &diamond_texture);
  data->diamond_texture = SDL_CreateTextureFromSurface(renderer, diamond_texture);
  SDL_FreeSurface(diamond_texture), diamond_texture = NULL;

  SDL_Rect r = rect;

  data->box_texture = data->scroll_window_texture = NULL;
  data->visible_scroll_window_rect = (SDL_Rect){ .x = r.x, .y = r.y, .w = r.w, .h = r.h };

  /* set initial scroll state */
  data->up_rect = (SDL_Rect){ .x = r.w - scroll_bar_width + 1, .y = 1, .w = scroll_bar_width - 2, .h = spooky_gui_y_padding * scroll_bar_height_scale - 3 };
  data->scroll_box_vertical_rect = (SDL_Rect){ .x = r.w - scroll_bar_width + 1, .y = 1, .w = scroll_bar_width, .h = r.h - scroll_bar_height };
  data->down_rect = (SDL_Rect){ .x = r.w - scroll_bar_width, .y = r.h - ((spooky_gui_y_padding * scroll_bar_height_scale) - 2) - 1, .w = scroll_bar_width - 2, .h = (spooky_gui_y_padding * scroll_bar_height_scale) - 3};

  data->left_rect = (SDL_Rect){ .x = 1, .y = r.h - scroll_bar_height + 1, .w = scroll_bar_width - 2, .h = spooky_gui_y_padding * scroll_bar_height_scale - 3};
  data->scroll_box_horizontal_rect = (SDL_Rect){ .x = 1, .y = r.h - scroll_bar_height + 1, .w = r.w - scroll_bar_width, .h = scroll_bar_height - 2};
  data->right_rect = (SDL_Rect){ .x = r.w - scroll_bar_width - scroll_bar_width + 2, .y = r.h - ((spooky_gui_y_padding * scroll_bar_height_scale) - 2) - 1, .w = scroll_bar_width - 2, .h = (spooky_gui_y_padding * scroll_bar_height_scale) - 3};

  /* calculate height of scroll bar button; sets scroll_region_height */
  /* Scroll bar button */
  spooky_box_scroll_box_recalculate_scroll_bars(self);

  data->scroll_region_height = 20;
  data->scroll_vertical_rect = (SDL_Rect) { .x = data->up_rect.x, .y = r.y + data->up_rect.h, .w = scroll_bar_width - 2, .h = scroll_bar_height - 3 };
  data->scroll_vertical_rect.h = data->scroll_region_height + 1;

  data->scroll_region_width = 20;
  data->scroll_horizontal_rect = (SDL_Rect) { .x = r.x + data->left_rect.w, .y = data->left_rect.y, .w = scroll_bar_width - 2, .h = scroll_bar_height - 3};
  data->scroll_horizontal_rect.w = data->scroll_region_width;

  data->last_mouse_x = data->last_mouse_y = 0;
  data->scroll_vertical_offset_x = data->scroll_vertical_offset_y = 0;
  data->scroll_horizontal_offset_x = data->scroll_horizontal_offset_y = 0;

  data->box_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);

  data->scroll_window_texture_height = rect.h;
  data->scroll_window_texture_width = rect.w;
  data->scroll_window_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);

  data->is_scroll_window_texture_invalidated = true;
  data->is_vertical_scroll_bar_dragging = false;
  data->is_horizontal_scroll_bar_dragging = false;

  data->percent_vertically_scrolled = data->percent_horizontally_scrolled = 0.0f;

  data->hover_item_ordinal = data->selected_item_ordinal = -1;
  data->selection_offsets = NULL;
  data->_super.is_image_collection = false;

  return self;
}

const spooky_box * spooky_box_dtor(const spooky_box * self) {
  if(self) {
    spooky_box_data * data = self->data;
    if(data->dialog_texture) {
      SDL_DestroyTexture(data->dialog_texture), data->dialog_texture = NULL;
    }
    if(data->sprite_sheet_texture) {
      SDL_DestroyTexture(data->sprite_sheet_texture), data->sprite_sheet_texture = NULL;
    }
    if(data->boxes_count > 0) {
      free(data->boxes), data->boxes = NULL;
    }
  }

  return self;
}

const spooky_box * spooky_box_scroll_box_dtor(const spooky_box * self) {
  self = spooky_box_dtor(self);
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)self->data;
  if(data) {
    if(data->box_texture) SDL_DestroyTexture(data->box_texture);
    if(data->scroll_window_texture) SDL_DestroyTexture(data->scroll_window_texture);
    if(data->diamond_texture) {
      SDL_DestroyTexture(data->diamond_texture), data->diamond_texture = NULL;
    }

    if(data->scroll_bar_items) {
      for(int i = 0; i < data->scroll_bar_items_count; i++) { 
        const spooky_box_scroll_box_item * item = &(data->scroll_bar_items[i]);
        if(item) {
          if(item->is_text_alloc) {
            /* had to dup/trim item->text during item attach; need to free, now */
            free((void *)(uintptr_t)item->text);
          }
          if(item->texture) SDL_DestroyTexture(item->texture);
        }
      }
      if(data->selection_offsets) {
        free(data->selection_offsets), data->selection_offsets = NULL;
      }

      free((void*)(uintptr_t)data->scroll_bar_items), data->scroll_bar_items = NULL;
    }
  }

  return self;
}

void spooky_box_free(const spooky_box * self) {
  if(self) {
    free((void *)(uintptr_t)self->data);
    free((void *)(uintptr_t)self);
  }
}

void spooky_box_release(const spooky_box * self) {
  self->free(self->dtor(self));
}

static spooky_box_scroll_box_item * spooky_box_get_next_available_scroll_box_item(const spooky_box * self) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;

  if(!data->scroll_bar_items) {
    assert(data->scroll_bar_items_capacity > 0);
    data->scroll_bar_items = calloc(sizeof data->scroll_bar_items[0],  (size_t)data->scroll_bar_items_capacity);
  }
  if(data->scroll_bar_items_count + 1 > data->scroll_bar_items_capacity) {
    assert(data->scroll_bar_items_capacity > 0);
    data->scroll_bar_items_capacity += 8;
    data->scroll_bar_items = realloc((void*)(uintptr_t)data->scroll_bar_items, sizeof data->scroll_bar_items[0] * (size_t)data->scroll_bar_items_capacity);
  }

  spooky_box_scroll_box_item * temp = &(data->scroll_bar_items[data->scroll_bar_items_count]);

  data->scroll_bar_items_count++;
  memset(temp, 0, sizeof * temp);

  return temp;
}

static const spooky_box_scroll_box_item * spooky_box_attach_scroll_box_item_text(const spooky_box * self, const char * text, const char * description) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;

  spooky_box_scroll_box_item * temp = spooky_box_get_next_available_scroll_box_item(self);
  temp->parent = self;
  temp->text = text;
  temp->path = NULL;
  temp->texture = NULL;
  temp->is_text_alloc = false;
  const spooky_font * font = data->_super.font;

  int text_width, text_height;
  font->measure_text(font, text, strnlen(text, SPOOKY_MAX_STRING_LEN), &text_width, &text_height);

  temp->width = text_width;
  temp->height = text_height;

  temp->rect.x = 0;
  temp->rect.y = 0;
  temp->rect.w = text_width;
  /* Use the height of M; prevents inconsisent height measurement between glyphs */
  data->_super.font->measure_text(data->_super.font, "M", strlen("M"), NULL, &(temp->rect.h));

  temp->description = description;
  temp->type = SBSBIT_TEXT;
  temp->is_selected = false;
  temp->is_hover = false;
  temp->is_enabled = true;

  /* calculate the maximum length (in pixels) and height (in pixels) for the texture rect */
  int max_text_height, max_text_width;
  spooky_box_calculate_max_text_properties(self, &max_text_width, &max_text_height);

  int max_rect_width = spooky_int_max(max_text_width, data->scroll_window_texture_width);

  if(data->scroll_window_texture) {
    SDL_DestroyTexture(data->scroll_window_texture), data->scroll_window_texture = NULL;
  }

  data->scroll_window_texture = SDL_CreateTexture(data->_super.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, max_rect_width, data->scroll_bar_items_count * (text_height + 2));

  int w, h;
  SDL_QueryTexture(data->scroll_window_texture, NULL, NULL, &w, &h);
  data->scroll_window_texture_height = h;
  data->scroll_window_texture_width = w;

  data->is_scroll_window_texture_invalidated = true;
  spooky_box_scroll_box_recalculate_scroll_bars(self);
  spooky_box_render_scroll_box_texture(self);

  data->is_vertical_scroll_bar_visible = data->scroll_window_texture_height > data->_super.rect.h;
  data->is_horizontal_scroll_bar_visible = data->scroll_window_texture_width > data->_super.rect.w;

  if(!data->is_vertical_scroll_bar_visible) {
    data->scroll_box_horizontal_rect.w = data->_super.rect.w;
  }
  if(!data->is_horizontal_scroll_bar_visible) {
    data->scroll_box_vertical_rect.h = data->_super.rect.h - 2;
  }

  return temp;
}

static const spooky_box_scroll_box_item * spooky_box_attach_scroll_box_item_image(const spooky_box * self, const char * text, const char * description, const char * path) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;
  data->_super.is_image_collection = true;

  spooky_box_scroll_box_item * temp = spooky_box_get_next_available_scroll_box_item(self);
  temp->parent = self;
  temp->text = NULL;

  SDL_Surface * surface = NULL;
  spooky_load_image(path, strnlen(path, SPOOKY_MAX_STRING_LEN), &surface);
  temp->texture = SDL_CreateTextureFromSurface(data->_super.renderer, surface);
  SDL_FreeSurface(surface), surface = NULL;

  int item_width, item_height;
  SDL_QueryTexture(temp->texture, NULL, NULL, &item_width, &item_height);

  temp->rect.x = 0;
  temp->rect.y = 0;
  temp->rect.w = temp->width = item_width;
  temp->rect.h = temp->height = item_height;
  temp->path = path;
  temp->description = description;
  temp->type = SBSBIT_IMAGE;
  temp->is_selected = false;
  temp->is_hover = false;
  temp->is_enabled = true;

  if(data->scroll_window_texture) {
    SDL_DestroyTexture(data->scroll_window_texture), data->scroll_window_texture = NULL;
  }

  const spooky_font * font = data->_super.font;
  int em_dash_width, em_dash_height;
  font->measure_text(font, "M", strlen("M"), &em_dash_width, &em_dash_height);

  temp->rect.h += em_dash_height;
  temp->height += em_dash_height;

  int vertical_scroll_bar = data->is_vertical_scroll_bar_visible ? data->scroll_box_vertical_rect.w : 0;
  // int horizontal_scroll_bar = data->is_horizontal_scroll_bar_visible ? data->scroll_box_horizontal_rect.h : 0;

  int max_items_per_row = (int)floor((float)(data->_super.rect.w - vertical_scroll_bar) / (float)max_texture_rect_width);

  int texture_width = spooky_int_max((int)max_texture_rect_width, (int)data->scroll_window_texture_width);
  int texture_height = ((data->scroll_bar_items_count / max_items_per_row) * max_texture_rect_height) + max_texture_rect_height;

  SDL_Renderer * renderer = data->_super.renderer;
  data->scroll_window_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, texture_width, texture_height);

  int w, h;
  SDL_QueryTexture(data->scroll_window_texture, NULL, NULL, &w, &h);

  int max_text_height, max_text_width;
  spooky_box_calculate_max_text_properties(self, &max_text_width, &max_text_height);

  /* For images, the text length in pixels must not exceed the max_texture_width in pixels to prevent clipping */
  int text_width, text_height;
  font->measure_text(font, text, strnlen(text, SPOOKY_MAX_STRING_LEN), &text_width, &text_height);

  if(text_width > max_texture_rect_width) {
    /* trim the text which will alloc a new string */
    int m_dash = font->get_m_dash(font);
    int max_temp_text_width = max_texture_rect_width / m_dash;

    char * temp_text = malloc(sizeof(char) * ((size_t)max_temp_text_width + 1));
    memcpy(temp_text, text, sizeof(char) * ((size_t)max_temp_text_width));
    temp_text[max_temp_text_width] = '\0';
    temp->text = temp_text;
    temp->is_text_alloc = true;
  } else {
    temp->text = text;
    temp->is_text_alloc = false;
  }

  data->scroll_window_texture_width = texture_width;
  data->scroll_window_texture_height = texture_height;

  data->is_scroll_window_texture_invalidated = true;
  spooky_box_scroll_box_recalculate_scroll_bars(self);
  spooky_box_render_scroll_box_texture(self);

  data->is_vertical_scroll_bar_visible = data->scroll_window_texture_height > data->_super.rect.h;
  data->is_horizontal_scroll_bar_visible = data->scroll_window_texture_width > data->_super.rect.w;

  if(data->selection_offsets) {
    free(data->selection_offsets), data->selection_offsets = NULL;
  }

  int X_LEN = data->scroll_bar_items_count, Y_LEN = max_items_per_row;
  data->selection_offsets = calloc(sizeof * data->selection_offsets, (size_t)(X_LEN * Y_LEN));
  int * base = &data->selection_offsets[0];
  for(int i = 0; i < X_LEN * Y_LEN; i++) {
    base[i] = i;
  }

  if(!data->is_vertical_scroll_bar_visible) {
    data->scroll_box_horizontal_rect.w = data->_super.rect.w;
  }
  if(!data->is_horizontal_scroll_bar_visible) {
    data->scroll_box_vertical_rect.h = data->_super.rect.h - 2;
  }

  return temp;
}

static void spooky_box_calculate_max_text_properties(const spooky_box * self, int * max_text_width, int * max_text_height) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;
  const spooky_font * font = data->_super.font;

  if(max_text_width) *max_text_width = 0;
  if(max_text_height) *max_text_height = 0;

  for(int i = 0; i < data->scroll_bar_items_count; i++) {
    int temp_text_width, temp_text_height;
    const char * text = data->scroll_bar_items[i].text;
    font->measure_text(font, text, strnlen(text, SPOOKY_MAX_STRING_LEN), &temp_text_width, &temp_text_height);

    if(max_text_width) *max_text_width = spooky_int_max(*max_text_width, temp_text_width);
    if(max_text_height) *max_text_height = spooky_int_max(*max_text_height, temp_text_height);
  }
}

static const spooky_box * spooky_box_render_scroll_box_texture(const spooky_box * self) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;
  const spooky_font * font = data->_super.font;

  static const int top_padding = 1;
  static const int bottom_padding = 1;

  if(data->is_scroll_window_texture_invalidated) {
    //SDL_Rect r = data->_super.rect;
    SDL_Renderer * renderer = data->_super.renderer;
    SDL_SetRenderTarget(renderer, data->scroll_window_texture);

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 100);
    SDL_RenderClear(renderer);

    int max_text_height, max_text_width;
    spooky_box_calculate_max_text_properties(self, &max_text_width, &max_text_height);

    font->set_is_drop_shadow(font, false);
    int w = data->_super.rect.w;
    if(data->is_vertical_scroll_bar_visible) {
      /* don't include the scroll bar in width calc */
      w = w - scroll_bar_width - 1;
    }

    if(data->scroll_bar_items) {
      const spooky_box_scroll_box_item * items = data->scroll_bar_items;
      const spooky_box_scroll_box_item * end = data->scroll_bar_items + data->scroll_bar_items_count;
      const spooky_box_scroll_box_item * item = items;

      int vertical_scroll_bar =  data->is_vertical_scroll_bar_visible ? data->scroll_box_vertical_rect.w : 0;
      int max_items_per_row = (int)floor((float)(data->_super.rect.w - vertical_scroll_bar) / (float)max_texture_rect_width);

      int i = 0, row = 0;
      while(item != end) {
/*
 * -----------------
 * |--|  |--|  |--|  (0, 0), (0, 1), (0, 2)
 * -----------------
 * |--|  |--|  |--|  (1, 0), (1, 1), (1, 2)
 * -----------------
 */
        /* Render each line of the scroll box */
        switch(item->type) {
          case SBSBIT_TEXT:
            {
              int y = i * (item->rect.h + top_padding + bottom_padding);
              if(item->text) {
                SDL_Point p = { .x = 1, .y = y };
                font->write(font, &p, &white, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
              }
            }
            break;
          case SBSBIT_IMAGE:
            {
              int em_dash_height;
              data->_super.font->measure_text(data->_super.font, "M", strlen("M"), NULL, &em_dash_height);

              int col = i % max_items_per_row;
              SDL_Rect s = { .x = 0, .y = 0, .w = max_texture_rect_width, .h = max_texture_rect_height };
              SDL_Rect d = { .x = col * max_texture_rect_width, .y = row * max_texture_rect_height, .w = max_texture_rect_width, .h = max_texture_rect_height };
              spooky_box_scroll_box_item * mutable_item = (spooky_box_scroll_box_item *)(uintptr_t)item;
              mutable_item->rect.x = d.x;
              mutable_item->rect.y = d.y + em_dash_height;
              SDL_RenderCopy(renderer, item->texture, &s, &d);
              if(item->text) {
                /* item text appears below the image */
                int t_width, t_height;
                font->measure_text(font, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), &t_width, &t_height);

                int text_center = d.x + ((d.w / 2) - (t_width / 2));
                SDL_Point p = { .x = text_center, .y = d.y + d.h - max_text_height + 1};
                font->write(font, &p, &white, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
              }

              if(col >= max_items_per_row - 1) row++;
            }
            break;
          case SBSBIT_EOE:
          case SBSBIT_NULL:
          default:
            break;
        }
        ++i, ++item;
      }
    }
    font->set_is_drop_shadow(font, false);
    SDL_SetRenderTarget(renderer, NULL);
    data->is_scroll_window_texture_invalidated = false;
  }

  return self;
}

const SDL_Rect * spooky_box_get_rect(const spooky_box * self) {
  spooky_box_data * data = self->data;

  return &(data->rect);
}

const spooky_box * spooky_box_attach_box(const spooky_box * self, const spooky_box * box) {
  spooky_box_data * data = self->data;
  if(!data->boxes) {
    assert(data->boxes_capacity > 0);
    data->boxes = malloc(sizeof data->boxes[0] * (size_t)data->boxes_capacity);
  }
  if(data->boxes_count + 1 > data->boxes_capacity) {
    assert(data->boxes_capacity > 0);
    data->boxes_capacity += 8;
    data->boxes = realloc(data->boxes, sizeof data->boxes[0] * (size_t)data->boxes_capacity);
  }

  data->boxes[data->boxes_count] = box;
  data->boxes_count++;

  return self;
}

static int spooky_box_get_max_visible_items(const spooky_box * self) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;

  static const int padding_top = 1, padding_bottom = 1;

  int horizontal_scroll_bar = data->is_horizontal_scroll_bar_visible ? data->scroll_box_horizontal_rect.h : 0;
  int max_visible_items = 0;

  int text_height;
  if(data->scroll_bar_items_count > 0) {
    text_height = data->scroll_bar_items[0].rect.h;
  } else {
    data->_super.font->measure_text(data->_super.font, "M", strlen("M"), NULL, &text_height);
  }

  if(!data->_super.is_image_collection) {
    max_visible_items = (int)round((float)((float)data->scroll_box_vertical_rect.h - (float)horizontal_scroll_bar) / (float)(text_height + padding_top + padding_bottom));
  } else {
    static int t_width = 0, t_height = 0;
    if(t_width == 0 || t_height == 0) {
      const spooky_font * font = data->_super.font;
      font->measure_text(font, "M", strlen("M"), &t_width, &t_height);
    }

    /* Include partially visible items on image collections */
    max_visible_items = (int)ceil((float)((float)data->scroll_box_vertical_rect.h - (float)horizontal_scroll_bar) / (float)(max_texture_rect_height + t_height + padding_top + padding_bottom));
  }

  return max_visible_items;
}

bool spooky_box_handle_scroll_box(const spooky_box * self, SDL_Event * event) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;

  SDL_Rect r = { .x = data->_super.rect.x, .y = data->_super.rect.y, .w = data->_super.rect.w, .h = data->_super.rect.h };
  int mouse_x = event->motion.x;
  int mouse_y = event->motion.y;
  bool intersected = (mouse_x >= r.x) && (mouse_x <= r.x + r.w) && (mouse_y >= r.y) && (mouse_y <= r.y + r.h);

  bool handled = false;

  int max_visible_items = spooky_box_get_max_visible_items(self);

  int text_height, em_dash_width;
  if(data->scroll_bar_items_count > 0) {
    text_height = data->scroll_bar_items[0].rect.h;
    data->_super.font->measure_text(data->_super.font, "M", strlen("M"), &em_dash_width, NULL);
  } else {
    data->_super.font->measure_text(data->_super.font, "M", strlen("M"), &em_dash_width, &text_height);
  }

  float single_item_vertical_percentage;
  if(!data->_super.is_image_collection) {
    single_item_vertical_percentage = ((float)text_height + 2) / (float)(data->scroll_window_texture_height - r.h);
  } else {
    single_item_vertical_percentage = ((float)max_texture_rect_height + (float)text_height) / (float)(data->scroll_window_texture_height - r.h);
  }

  float single_item_horizontal_percentage;
  if(!data->_super.is_image_collection) {
    single_item_horizontal_percentage = ((float)em_dash_width) / (float)(data->scroll_window_texture_width);
  } else {
    single_item_horizontal_percentage = ((float)(max_texture_rect_width + em_dash_width)) / (float)(data->scroll_window_texture_width);
  }

  SDL_Rect relative_vertical_scroll_bar = { .x = r.x + data->scroll_vertical_rect.x, .y = r.y + data->scroll_vertical_rect.y, .w = data->scroll_vertical_rect.w, .h = data->scroll_vertical_rect.h };
  SDL_Rect relative_horizontal_scroll_bar = { .x = r.x + data->scroll_horizontal_rect.x, .y = r.y + data->scroll_horizontal_rect.y, .w = data->scroll_horizontal_rect.w, .h = data->scroll_horizontal_rect.h };

  bool vertical_scroll_bar_intersected = spooky_box_intersect(&relative_vertical_scroll_bar, mouse_x, mouse_y);
  bool horizontal_scroll_bar_intersected = spooky_box_intersect(&relative_horizontal_scroll_bar, mouse_x, mouse_y);

  /* Save the last mouse position; SDL2 doesn't preserve the x/y coords for
   * MOUSEHEEL and GetMouseState returns goofball results on OS X */
  if(SDL_MOUSEMOTION == event->type) {
    data->last_mouse_x = mouse_x;
    data->last_mouse_y = mouse_y;
  }

  bool is_mouse_move = event->type == SDL_MOUSEMOTION;
  bool is_mouse_up = event->type == SDL_MOUSEBUTTONUP;
  bool is_mouse_down = event->type == SDL_MOUSEBUTTONDOWN;

  if(SDL_KEYUP == event->type) {
    if((intersected = spooky_box_intersect(&r, data->last_mouse_x, data->last_mouse_y))) {
      int top_ordinal = (int)round(data->percent_vertically_scrolled * (float)(data->scroll_window_texture_height - r.h) / (float)(text_height + 2));

      SDL_Keycode sym = event->key.keysym.sym;
      if(SDLK_j == sym || SDLK_DOWN == sym) {
        data->selected_item_ordinal++;
        if(data->selected_item_ordinal - top_ordinal > max_visible_items - 1) {
          float new_percent = data->percent_vertically_scrolled + single_item_vertical_percentage;
          spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), new_percent);
        }
        if(data->selected_item_ordinal < top_ordinal || data->selected_item_ordinal > top_ordinal + max_visible_items) {
          float new_percent = (float)(((float)data->selected_item_ordinal) * (float)(text_height + 2) / (float)(data->scroll_window_texture_height - r.h)) - single_item_vertical_percentage;
          spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), new_percent);
        }
      }
      if(SDLK_k == sym || SDLK_UP == sym) {
        data->selected_item_ordinal--;
        if(data->selected_item_ordinal - top_ordinal < 1) {
          float new_percent = data->percent_vertically_scrolled - single_item_vertical_percentage;
          spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), new_percent);
        }
        /* correct % scrolled */
        if(data->selected_item_ordinal < top_ordinal || data->selected_item_ordinal > top_ordinal + max_visible_items) {
          float new_percent = (float)(((float)data->selected_item_ordinal - (float)max_visible_items) * (float)(text_height + 2) / (float)(data->scroll_window_texture_height - r.h)) + single_item_vertical_percentage;
          spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), new_percent);
        }
      }

      if(SDLK_h == sym || SDLK_LEFT == sym) {
        spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled - single_item_horizontal_percentage);
      }
      if(SDLK_l == sym || SDLK_RIGHT == sym) {
        spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled + single_item_horizontal_percentage);
      }
    }
  }
  else if(is_mouse_down && ((data->is_vertical_scroll_bar_dragging || vertical_scroll_bar_intersected) || (data->is_horizontal_scroll_bar_dragging || horizontal_scroll_bar_intersected))) {
    /* begin/continue drag */
    if(intersected && data->scroll_bar_items_count > max_visible_items && vertical_scroll_bar_intersected) {
      data->scroll_vertical_offset_y = (event->button.y - data->scroll_vertical_rect.y) + data->up_rect.h;
      data->is_vertical_scroll_bar_dragging = true;
    } else if (intersected && data->scroll_window_texture_width > r.w && horizontal_scroll_bar_intersected) {
      data->scroll_horizontal_offset_x = (event->button.x - data->scroll_horizontal_rect.x) + data->left_rect.w;
      data->is_horizontal_scroll_bar_dragging = true;
    }
  }
  else if(is_mouse_up) {
    /* End drag */
    data->scroll_vertical_offset_x = data->scroll_vertical_offset_y = data->scroll_horizontal_offset_x = data->scroll_horizontal_offset_y = 0;
    data->is_vertical_scroll_bar_dragging = data->is_horizontal_scroll_bar_dragging = false;
  }
  else if(is_mouse_move && (data->is_vertical_scroll_bar_dragging || data->is_horizontal_scroll_bar_dragging)) {
    if(data->is_vertical_scroll_bar_dragging) {
      /* during vertical drag */
      int length = data->scroll_box_vertical_rect.h - data->down_rect.h - data->up_rect.h - data->scroll_vertical_rect.h;
      int delta = event->motion.y - data->scroll_vertical_offset_y;
      spooky_box_scroll_box_mouse_move_delta(length, delta, data->up_rect.h, &(data->scroll_vertical_rect.y), &(data->percent_vertically_scrolled));
    } else {
      /* during horizontal drag */
      int length = data->scroll_box_horizontal_rect.w - data->right_rect.w - data->left_rect.w - data->scroll_horizontal_rect.w; 
      int delta = event->motion.x - data->scroll_horizontal_offset_x;
      spooky_box_scroll_box_mouse_move_delta(length, delta, data->left_rect.w, &(data->scroll_horizontal_rect.x), &(data->percent_horizontally_scrolled));
    }
  } else if(SDL_MOUSEWHEEL == event->type) {
    /* oh crap, mouse wheel */
    /* adjust offsets based on scroll wheel up/down */
    if((intersected = spooky_box_intersect(&r, data->last_mouse_x, data->last_mouse_y))) {
      int height = data->scroll_box_vertical_rect.h - data->down_rect.h - data->up_rect.h - data->scroll_vertical_rect.h - 1; 
      int width =  data->scroll_box_horizontal_rect.w - data->right_rect.w - data->left_rect.w - data->scroll_horizontal_rect.w - 1; 

      /* calculate wheel vertical offset */
      float y_delta = ((float)event->wheel.y / (float)height);
      spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), data->percent_vertically_scrolled - y_delta);

      /* calculate wheel horizontal offset */
      float x_delta = ((float)event->wheel.x / (float)width);
      spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled - x_delta);
    }
  }

  if(data->hover_item_ordinal <= -1) data->hover_item_ordinal = 0;
  if(data->hover_item_ordinal > data->scroll_bar_items_count - 1) data->hover_item_ordinal = data->scroll_bar_items_count - 1;
  if(data->selected_item_ordinal <= -1) data->selected_item_ordinal = 0;
  if(data->selected_item_ordinal > data->scroll_bar_items_count - 1) data->selected_item_ordinal = data->scroll_bar_items_count - 1;

  if(data->scroll_bar_items_count > 0) {
    /* handle the presence of the scroll bar */
    if(is_mouse_move) {
      mouse_x = data->last_mouse_x;
      mouse_y = data->last_mouse_y;
    }
    int vertical_scroll_bar_padding = 0;
    if(data->is_vertical_scroll_bar_visible) {
      /* if visible items overflow the scroll box, don't include the scroll bar on hover intersection */
      SDL_Rect scroll_vertical_rect = { .x = r.x, .y = r.y, .w = r.w - scroll_bar_width - 1, .h = r.h };
      vertical_scroll_bar_padding = scroll_bar_width - 1;
      intersected = spooky_box_intersect(&scroll_vertical_rect, mouse_x, mouse_y);
    }
    if(data->is_horizontal_scroll_bar_visible) {
      /* if visible items overflow the scroll box, don't include the scroll bar on hover intersection */
      SDL_Rect scroll_horizontal_rect = { .x = r.x, .y = r.y, .w = r.w - vertical_scroll_bar_padding , .h = r.h - scroll_bar_width - 1 };
      intersected = spooky_box_intersect(&scroll_horizontal_rect, mouse_x, mouse_y);
    }

    const int button_height = ((spooky_gui_y_padding * scroll_bar_height_scale) - 3);

    int horizontal_padding = !data->is_vertical_scroll_bar_visible ? scroll_bar_width - 3 : -1;
    int vertical_padding = !data->is_horizontal_scroll_bar_visible ? scroll_bar_height - 3 : -1;

    /* Scroll up button */
    data->up_rect = (SDL_Rect){ .x = r.w - scroll_bar_width + 1, .y = 1, .w = scroll_bar_width - 1, .h = button_height };
    SDL_Rect relative_up_button = { .x = r.x + data->up_rect.x, .y = r.y + data->up_rect.y, .w = data->up_rect.w, .h = data->up_rect.h  };
    bool up_button = spooky_box_intersect(&relative_up_button, mouse_x, mouse_y);

    /* Scroll down button */
    data->down_rect = (SDL_Rect){ .x = r.w - ((spooky_gui_x_padding * scroll_bar_height_scale) - 2) - 1, .y = r.h - scroll_bar_height - scroll_bar_height + 4 + vertical_padding, .w = scroll_bar_width - 1, .h = button_height};
    SDL_Rect relative_down_button = { .x = r.x + data->down_rect.x, .y = r.y + data->down_rect.y, .w = scroll_bar_width - 1, .h = button_height };
    bool down_button = spooky_box_intersect(&relative_down_button, mouse_x, mouse_y);

    /* Scroll left button */
    data->left_rect = (SDL_Rect){ .x = 1, .y = r.h - scroll_bar_height + 1, .w = scroll_bar_width - 1, .h = button_height };
    SDL_Rect relative_left_button = { .x = r.x + data->left_rect.x, .y = r.y + data->left_rect.y, .w = scroll_bar_width - 1, .h = button_height };
    bool left_button = spooky_box_intersect(&relative_left_button, mouse_x, mouse_y);

    /* Scroll right button */
    data->right_rect = (SDL_Rect){ .x = r.w - scroll_bar_width - scroll_bar_width + 4 + horizontal_padding, .y = r.h - ((spooky_gui_y_padding * scroll_bar_height_scale) - 2) - 1, .w = scroll_bar_width - 1, .h = button_height };
    SDL_Rect relative_right_button = { .x = r.x + data->right_rect.x, .y = r.y + data->right_rect.y, .w = scroll_bar_width - 1, .h = button_height };
    bool right_button = spooky_box_intersect(&relative_right_button, mouse_x, mouse_y);;

    /* Scroll vertical button */
    SDL_Rect relative_scroll_vertical_rect = { .x = r.x + data->scroll_vertical_rect.x, .y = r.y + data->scroll_vertical_rect.y, .w = data->scroll_vertical_rect.w, .h = data->scroll_vertical_rect.h };
    bool vertical_scroll_button = spooky_box_intersect(&relative_scroll_vertical_rect, mouse_x, mouse_y);

    /* SCroll horizontal button */
    SDL_Rect horizontal_scroll_bar_region_rect = { .x = r.x + data->scroll_horizontal_rect.x, .y = r.y + data->scroll_horizontal_rect.y, .w = data->scroll_horizontal_rect.w, .h = data->scroll_horizontal_rect.h };
    bool horizontal_scroll_button = spooky_box_intersect(&horizontal_scroll_bar_region_rect, mouse_x, mouse_y);

    /* Set fields according to mouse hover and click activity */
    spooky_box_scroll_box_button * temp = &(data->scroll_button);

    temp->is_active = (up_button || down_button || vertical_scroll_button || right_button || left_button || horizontal_scroll_button) && is_mouse_down;
    temp->is_hover = up_button || down_button || vertical_scroll_button || right_button || left_button || horizontal_scroll_button;

    if(temp->is_active && up_button) {
      /* Click Up button */
      spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), data->percent_vertically_scrolled - single_item_vertical_percentage);
    } else if(temp->is_active && down_button) {
      /* Click Down button */
      spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), data->percent_vertically_scrolled + single_item_vertical_percentage);
    } else if(temp->is_active && left_button) {
      /* Click Left button */
      spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled - single_item_horizontal_percentage);
    } else if(temp->is_active && right_button) {
      /* Click Right button */
      spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled + single_item_horizontal_percentage);
    } else {
      SDL_Rect vertical_page_up_region = { .x = r.x + data->scroll_box_vertical_rect.x, .y = r.y + data->scroll_box_vertical_rect.y + data->up_rect.h + 1, .w = data->scroll_box_vertical_rect.w, .h = data->scroll_vertical_rect.y - (data->up_rect.y  + data->up_rect.h) };
      SDL_Rect vertical_page_left_region = { .x = r.x + data->scroll_box_horizontal_rect.x - data->left_rect.w + 1, .y = r.y + data->scroll_box_horizontal_rect.y, .w = data->scroll_box_horizontal_rect.x - (data->left_rect.x  + data->left_rect.w), .h = data->scroll_box_vertical_rect.h };

      SDL_Rect vertical_page_down_region = { .x = r.x + data->scroll_box_vertical_rect.x, .y = r.y + data->scroll_vertical_rect.y + data->scroll_vertical_rect.h + 1, .w = data->scroll_box_vertical_rect.w, .h = data->scroll_box_vertical_rect.h - vertical_page_up_region.h - data->up_rect.h - data->down_rect.h  - data->scroll_vertical_rect.h - 2 };

      SDL_Rect horizontal_page_left_region = { .x = r.x + data->scroll_box_horizontal_rect.x + data->left_rect.w, .y = r.y + data->scroll_box_horizontal_rect.y, .w = data->scroll_horizontal_rect.x - (data->left_rect.x + data->left_rect.w), .h = data->scroll_box_horizontal_rect.h };

      SDL_Rect horizontal_page_right_region = { .x = r.x + data->scroll_horizontal_rect.x +  data->scroll_horizontal_rect.w, .y = r.y + data->scroll_box_horizontal_rect.y, .w = data->scroll_box_horizontal_rect.w - vertical_page_left_region.w - data->left_rect.w - data->right_rect.w - data->scroll_horizontal_rect.w, .h = data->scroll_box_horizontal_rect.h };

      if(is_mouse_down && spooky_box_intersect(&vertical_page_up_region, mouse_x, mouse_y)) {
        /* Click 'page-up' region */
        spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), data->percent_vertically_scrolled - (single_item_vertical_percentage * (float)max_visible_items));
      } else if(is_mouse_down && spooky_box_intersect(&vertical_page_down_region, mouse_x, mouse_y)) {
        /* Click 'page-down' region */
        spooky_box_set_scrolled_percentage(&(data->percent_vertically_scrolled), data->percent_vertically_scrolled + (single_item_vertical_percentage * (float)max_visible_items));
      } else if(is_mouse_down && spooky_box_intersect(&horizontal_page_left_region, mouse_x, mouse_y)) {
        /* Click 'page-left' region */
        spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled - (single_item_horizontal_percentage * (((float)data->_super.rect.w / (float)em_dash_width))));
      } else if(is_mouse_down && spooky_box_intersect(&horizontal_page_right_region, mouse_x, mouse_y)) {
        /* Click 'page-right' region */
        spooky_box_set_scrolled_percentage(&(data->percent_horizontally_scrolled), data->percent_horizontally_scrolled + (single_item_horizontal_percentage * (((float)data->_super.rect.w / (float)em_dash_width))));
      }
    }

    /* recalculate the scroll button height based on our update information */

    /* calculate the hovered item based on the mouse position, the number of items in our list, the current item ordinal, etc */
    if(intersected) {
      int xx = mouse_x - r.x;
      int yy = mouse_y - r.y;

      int vertical_scroll_bar = data->is_vertical_scroll_bar_visible ? data->scroll_box_vertical_rect.w : 0;
      /* y_offset is the top of the visible rect */
      int horizontal_scroll_bar = data->is_horizontal_scroll_bar_visible ? data->scroll_box_horizontal_rect.h : 0;

      int x_offset = (int)(data->percent_horizontally_scrolled * (float)(data->scroll_window_texture_width - r.w + vertical_scroll_bar));
      int y_offset = (int)(data->percent_vertically_scrolled * (float)(data->scroll_window_texture_height - r.h + horizontal_scroll_bar));

      int new_hover_item_ordinal = 0;

      int max_items_per_row = (int)floor((float)(data->_super.rect.w - vertical_scroll_bar) / (float)max_texture_rect_width);
      if(!data->_super.is_image_collection) {
        new_hover_item_ordinal = (y_offset + yy) / (text_height + 2);
      } else if(xx < (max_texture_rect_width * max_items_per_row)) {
        int * base = &data->selection_offsets[0];

        int row = (y_offset + yy) / max_texture_rect_height;
        int col = (x_offset + xx) / max_texture_rect_width;

        const int dims = 2;
        const int indices[] = { row, col };

        register int offset = 0;
        for(register int i = 0; i < dims; i++) {
          offset += (int)pow(max_items_per_row, i) * indices[dims - (i + 1)];
        }

        new_hover_item_ordinal = *(base + offset);
      } else { new_hover_item_ordinal = data->hover_item_ordinal; }

      if(new_hover_item_ordinal >= 0)  {
        data->hover_item_ordinal = new_hover_item_ordinal;
      }
      if(is_mouse_down) {
        data->selected_item_ordinal = data->hover_item_ordinal;
      }
    }

    /* Recalculate y position of vertical scroll bar after all events */
    int height = (data->down_rect.y - data->scroll_vertical_rect.h + 1) - (data->scroll_box_vertical_rect.y + data->up_rect.h - 1);
    height = spooky_int_max(height, spooky_ratcliff_factor);

    data->scroll_vertical_rect.y = (int)round((float)height * data->percent_vertically_scrolled);
    if(data->scroll_vertical_rect.y < data->scroll_vertical_rect.y + data->up_rect.h + 1) data->scroll_vertical_rect.y = data->scroll_vertical_rect.y + data->up_rect.h;
    if(data->scroll_vertical_rect.y > data->down_rect.y - data->scroll_vertical_rect.h - 1) data->scroll_vertical_rect.y = data->down_rect.y - data->scroll_vertical_rect.h;

    /* recalculate x position of horizontal scroll bar after all events */
    int width = (data->right_rect.x - data->scroll_horizontal_rect.w + 1) - (data->scroll_box_horizontal_rect.x + data->left_rect.w - 1);
    width = spooky_int_max(width, spooky_ratcliff_factor);

    data->scroll_horizontal_rect.x = (int)round((float)width * data->percent_horizontally_scrolled);
    if(data->scroll_horizontal_rect.x < data->scroll_horizontal_rect.x + data->left_rect.w + 1) data->scroll_horizontal_rect.x = data->scroll_horizontal_rect.x + data->left_rect.w;
    if(data->scroll_horizontal_rect.x > data->right_rect.x - data->scroll_horizontal_rect.w - 1) data->scroll_horizontal_rect.x = data->right_rect.x - data->scroll_horizontal_rect.w;

    if(up_button) {
      temp->rect = &(data->up_rect);
      temp->button = SBSBB_UP;
    } else if (down_button) {
      temp->rect = &(data->down_rect);
      temp->button = SBSBB_DOWN;
    } else if (vertical_scroll_button) {
      temp->rect = &(data->scroll_vertical_rect);
      temp->button = SBSBB_VERTICAL_SCROLL;
    } else if (left_button) {
      temp->rect = &(data->left_rect);
      temp->button = SBSBB_LEFT;
    } else if (right_button) {
      temp->rect = &(data->right_rect);;
      temp->button = SBSBB_RIGHT;
    } else if (horizontal_scroll_button) {
      temp->rect = &(data->scroll_horizontal_rect);
      temp->button = SBSBB_HORIZONTAL_SCROLL;
    } else {
      temp->button = SBSBB_NULL;
    }

    handled = true;
  }

  return handled;
}

static void spooky_box_scroll_box_mouse_move_delta(int len, int delta, int offset, int * value, float * percentage) {
  int dividend = delta;
  if(delta < 2) {
    dividend = 0;
    delta = 2;
  }
  if(delta > len) {
    delta = dividend = len;
  }
  *value = delta + offset;
  spooky_box_set_scrolled_percentage(percentage, ((float)dividend / (float)len));
}

static void spooky_box_set_scrolled_percentage(float * percent, float new_value) {
  if(new_value > 1.0f) new_value = 1.0f;
  if(new_value < 0.0f) new_value = 0.0f;

  *percent = new_value;
}

static void spooky_box_scroll_box_recalculate_scroll_bars(const spooky_box * self) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)(uintptr_t)self->data;

  /* Vertical */
  float scroll_bar_region_height = (float)(data->scroll_box_vertical_rect.h - data->up_rect.h - data->down_rect.h);
  float vertical_height = (float)data->scroll_window_texture_height;
  data->scroll_region_height = (int)round((float)(scroll_bar_region_height / vertical_height) * (float)(data->scroll_box_vertical_rect.h - data->up_rect.h - data->down_rect.h));
  data->scroll_region_height = spooky_int_max(spooky_ratcliff_factor, data->scroll_region_height);
  data->scroll_vertical_rect.h = data->scroll_region_height;

  /* Horizontal */
  float scroll_bar_region_width = (float)(data->scroll_box_horizontal_rect.w - data->left_rect.w - data->right_rect.w);
  float horizontal_width = (float)data->scroll_window_texture_width;
  data->scroll_region_width = (int)round((float)(scroll_bar_region_width / horizontal_width) * (float)(data->scroll_box_horizontal_rect.w - data->left_rect.w - data->right_rect.w));
  data->scroll_region_width = spooky_int_max(spooky_ratcliff_factor, data->scroll_region_width);
  data->scroll_horizontal_rect.w = data->scroll_region_width;
}

bool spooky_box_handle_event(const spooky_base * self, SDL_Event * event) {
  spooky_box_data * data = ((const spooky_box *)self)->data;

/* TODO:
  if(event->action == SP_GA_TEXT_INPUT) {
    data->is_visible = true;
    return false;
  }
  if(event->action == SP_GA_ESC) {
    data->is_visible = false;
    return false;
  }
*/

  SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };
  int mouse_x = event->motion.x;
  int mouse_y = event->motion.y;

  bool intersected = (mouse_x >= r.x) && (mouse_x <= r.x + r.w) && (mouse_y >= r.y) && (mouse_y <= r.y + r.h);
  bool is_top = data->z_order == data->wm->get_max_z_order(data->wm);
  bool handle_intersected = intersected;

  const spooky_font * font = data->font;
  int header_height = font->get_height(font) + 3; /* top/bottom lines + padding */
/* TODO:
  if(event->action == SP_GA_SCALE_UP || event->action == SP_GA_SCALE_DOWN) {
    // TODO: Scale UI
    return false;
  }
*/

  if(data->box_type == SBT_WINDOW) {
    /* intersected for drag/drop with title bar */
    handle_intersected = (mouse_x >= r.x) && (mouse_x <= r.x + r.w) && (mouse_y >= r.y) && (mouse_y <= r.y + header_height);
  }

  bool is_mouse_down = event->type == SDL_MOUSEBUTTONDOWN;
  bool is_mouse_up = event->type == SDL_MOUSEBUTTONUP;
  bool is_mouse_move = event->type == SDL_MOUSEMOTION;

  bool handled = false;
  if(is_mouse_down && (intersected || data->is_dragging)) {
    if(handle_intersected) {
      data->wm->set_active_object(data->wm, self);
      data->offset_x = event->button.x - data->rect.x;
      data->offset_y = event->button.y - data->rect.y;
      data->is_dragging = true;
    }
    if(!is_top) {
      data->wm->activate_window(data->wm, self);
      data->is_hit = true;
    }
  }

  if(is_mouse_up) {
    /* End drag */
    data->wm->set_active_object(data->wm, NULL);
    data->offset_x = data->offset_y = 0;
    handled = true;
    data->is_hit = false;
    data->is_dragging = false;
  }

  if(is_mouse_move && (intersected || data->is_dragging)) {
    if(data->wm->get_active_object(data->wm) == self && data->is_draggable) {
      /* Perform drag */

      /* old_x/old_y used to adjust child boxes */
      int old_x = data->rect.x;
      int old_y = data->rect.y;

      int new_x = event->motion.x - data->offset_x;
      int new_y = event->motion.y - data->offset_y;

      int window_width = data->context->get_window_width(data->context);
      int window_height = data->context->get_window_height(data->context);
      if(data->box_type != SBT_WINDOW) {
        if(new_x >= 0 && new_x <= window_width - data->rect.w) { data->rect.x = new_x; }
        if(new_y >= MENU_HEIGHT && new_y <= window_height - data->rect.h) { data->rect.y = new_y; }
      } else {
        if(new_x >= 0 - (window_width + data->rect.x + (data->name_width - 2)) && new_x <= window_width - data->name_width - 2) { data->rect.x = new_x; }
        if(new_y >= MENU_HEIGHT && new_y <= window_height - (data->name_height + 4)) { data->rect.y = new_y; }
      }

      if((new_x != old_x || new_y != old_y) && data->boxes_count > 0) {
        for(int i = 0; i < data->boxes_count; i++) {
          const spooky_box * box = data->boxes[i];
          int box_x = box->as_base(box)->get_x(box->as_base(box));
          int box_y = box->as_base(box)->get_y(box->as_base(box));

          int delta_x = data->rect.x - old_x;
          int delta_y = data->rect.y - old_y;

          box_x += delta_x;
          box_y += delta_y;

          spooky_box_data * box_data = box->data;

          box_data->rect.x = box_x;
          box_data->rect.y = box_y;
        }
      }
    }
    handled = true;
  }

  data->is_top = data->z_order == data->wm->get_max_z_order(data->wm);
  data->is_active = data->wm->get_active_object(data->wm) == self;
  if(data->box_type == SBT_WINDOW && handle_intersected && (event->type == SDL_MOUSEBUTTONDOWN && event->button.clicks == 2)) {
    data->is_shade = !data->is_shade;
  }

  uint8_t box_alpha = data->is_top ? 243 : 90;
  if(data->is_active) {
    data->bg_color = (SDL_Color){ .r = 180, .g = 180, .b = 180, .a = box_alpha};
  } else {
    data->bg_color = (SDL_Color){ .r = 170, .g = 170, .b = 170, .a = box_alpha};
  }

  if(data->boxes_count > 0) {
    for(int i = 0; i < data->boxes_count; i++) {
      // if(handled) break;
      const spooky_box * box = data->boxes[i];
      /* TODO: Handle and return? Just handle? */
      handled = box->super.handle_event(box->as_base(box), event);
    }
  }

  return handled;
}

void spooky_box_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_box_data * data = ((const spooky_box *)self)->data;

  if(!data->is_visible) return;

  SDL_Rect r = { .x = data->rect.x, .y = data->rect.y, .w = data->rect.w, .h = data->rect.h };

  uint8_t red, green, blue, alpha;
  SDL_BlendMode old_blend;

  /* Preserve original draw color and blend mode */
  SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
  SDL_GetRenderDrawBlendMode(renderer, &old_blend);

  switch(data->box_type) {
    case SBT_WINDOW_TEXTURED:
      spooky_box_draw_window_textured((const spooky_box *)self, &r);
      break;
    case SBT_WINDOW:
      spooky_box_draw_window((const spooky_box *)self, &r);
      break;
    case SBT_BUTTON:
      spooky_box_draw_button((const spooky_box *)self, &r);
      break;
    case SBT_MAIN_MENU:
      spooky_box_draw_main_menu((const spooky_box *)self, &r);
      break;
    case SBT_MAIN_MENU_ITEM:
      spooky_box_draw_main_menu_item((const spooky_box *)self, &r);
      break;
    case SBT_SCROLL_BOX:
      spooky_box_render(self, renderer);
      //SP_DISPATCH(self, draw, self);
      break;
    case SBT_TEXT:
      spooky_box_draw_text((const spooky_box *)self, &r);
      break;
    case SBT_IMAGE:
      spooky_box_draw_image((const spooky_box *)self, &r);
      break;
    case SBT_NULL:
    case SBT_EOE:
    default:
      break;
  }

  /* restore original blend and draw color */
  SDL_SetRenderDrawBlendMode(renderer, old_blend);
  SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);

  if((data->box_type == SBT_WINDOW || data->box_type == SBT_WINDOW_TEXTURED) && data->boxes_count > 0 && !data->is_shade) {
    /* render children */
    for(int i = 0; i < data->boxes_count; i++) {
      const spooky_box * box = data->boxes[i];
      spooky_box_render(box->as_base(box), renderer);
    }
  }
}

/* Draws a textured, square window, i.e. for dialog */
static void spooky_box_draw_window_textured(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;

  SDL_Rect r = *rect;
  /* Corners */
  static SDL_Rect c_nw = { .x = 0, .y = 0, .w = 8, .h = 8 };
  static SDL_Rect c_ne = { .x = 56, .y = 0, .w = 8, .h = 8 };
  static SDL_Rect c_sw = { .x = 0, .y = 56, .w = 8, .h = 8 };
  static SDL_Rect c_se = { .x = 56, .y = 56, .w = 8, .h = 8 };

  /* Top/Bottom */
  static SDL_Rect top = { .x = 8, .y = 0, .w = 8, .h = 8 };
  static SDL_Rect bottom = { .x = 8, .y = 56, .w = 8, .h = 8 };

  /* Sides */
  static SDL_Rect left = { .x = 0, .y = 8, .w = 8, .h = 8 };
  static SDL_Rect right = { .x = 56, .y = 8, .w = 8, .h = 8 };

  /* Texture Corner Destinations */
  SDL_Rect c_nw_dest = { .x = r.x, .y = r.y, .w = WIDTH(c_nw.w), .h = HEIGHT(c_nw.h) };
  SDL_Rect c_ne_dest = { .x = (r.x + r.w) - WIDTH(c_ne.w), .y = r.y, .w = WIDTH(c_ne.w), .h = HEIGHT(c_ne.h) };
  SDL_Rect c_sw_dest = { .x = r.x, .y = (r.y + r.h) - HEIGHT(c_sw.h), .w = WIDTH(c_sw.w), .h = HEIGHT(c_sw.h) };
  SDL_Rect c_se_dest = { .x = (r.x + r.w) - WIDTH(c_se.w), .y = (r.y + r.h) - HEIGHT(c_se.h), .w = WIDTH(c_se.w), .h = HEIGHT(c_se.h) };

  /* Texture Top/Bottom Destinations */
  SDL_Rect top_dest	= { .x = r.x + WIDTH(c_nw.w), .y = r.y, .w = r.w - (WIDTH(c_nw.w) * 2), .h = HEIGHT(top.w) };
  SDL_Rect bottom_dest = { .x = r.x + WIDTH(c_sw.w), .y = (r.y + r.h) - HEIGHT(c_sw.h), .w = r.w - (WIDTH(c_nw.w) * 2), .h = HEIGHT(top.w) };

  /* Texture Left/Right Side Destinations */
  SDL_Rect left_dest	= { .x = r.x, .y = r.y + HEIGHT(left.h), .w = WIDTH(left.w), .h = r.h - (HEIGHT(left.h) * 2) };
  SDL_Rect right_dest = { .x = r.x + r.w - WIDTH(right.w), .y = r.y + HEIGHT(left.h), .w = WIDTH(right.w), .h = r.h - (HEIGHT(left.h) * 2) };

  SDL_Renderer * renderer = data->renderer;

  /* set transparent blend mode */
  SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
  SDL_SetRenderDrawBlendMode(renderer, blend_mode);

  /* fill interior rectangle with background color */
  SDL_SetRenderDrawColor(renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, data->bg_color.a);
  SDL_RenderFillRect(renderer, &r);

  /* draw textured corners and sides */
  SDL_Texture * texture = data->dialog_texture;

  SDL_RenderCopy(renderer, texture, &c_nw, &c_nw_dest);
  SDL_RenderCopy(renderer, texture, &c_ne, &c_ne_dest);
  SDL_RenderCopy(renderer, texture, &c_sw, &c_sw_dest);
  SDL_RenderCopy(renderer, texture, &c_se, &c_se_dest);

  SDL_RenderCopy(renderer, texture, &top, &top_dest);
  SDL_RenderCopy(renderer, texture, &bottom, &bottom_dest);

  SDL_RenderCopy(renderer, texture, &left, &left_dest);
  SDL_RenderCopy(renderer, texture, &right, &right_dest);
}

/* Draws a window with a title bar */
static void spooky_box_draw_window(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;

  SDL_Rect r = *rect;
  uint8_t box_alpha = data->is_top ? 243 : 90;

  SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
  SDL_SetRenderDrawBlendMode(data->renderer, blend_mode);

  const spooky_font * font = data->font;
  int header_height = font->get_height(font) + 3; /* top/bottom lines + padding */

  SDL_Rect header_rect = { .x = r.x, .y = r.y, .w = r.w, .h = header_height  };

  /* draw box fill rect */
  SDL_SetRenderDrawColor(data->renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, box_alpha);
  if(data->is_shade) {
    /* window is rolled up so don't draw the full box */
    header_rect.h += 1;
    SDL_RenderFillRect(data->renderer, &header_rect);
  } else {
    SDL_RenderFillRect(data->renderer, &r);
  }

  /* draw header */
  /* - header box */
  SDL_SetRenderDrawColor(data->renderer, 0, 130, 255, box_alpha);
  SDL_RenderFillRect(data->renderer, &header_rect);

  SDL_Color w = { .r = 255, .g = 255, .b = 255, .a = box_alpha };
  SDL_Point p = { .x = r.x + 2, .y = r.y + 2 };
  font->write(font, &p, &w, data->name, strnlen(data->name, SPOOKY_MAX_STRING_LEN), NULL, NULL);

  /* Minimize Button */
  /*
  SDL_SetRenderDrawColor(data->renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, box_alpha);
  SDL_Rect min_rect = {.x = (r.x + r.w) - (spooky_gui_x_padding * 3), .y = r.y + 2, .w = spooky_gui_x_padding * 2, .h = (spooky_gui_y_padding * 2) - 3};
  SDL_RenderFillRect(data->renderer, &min_rect);
 spooky_box_draw_3d_rectangle_v2(self, &min_rect, &white, &black);
  */
  SDL_SetRenderDrawColor(data->renderer, 255, 255, 255, box_alpha);

  /* draw untextured box */
  if(data->is_shade) {
    SDL_RenderDrawRect(data->renderer, &header_rect);
  } else {
    /* draw header line */
    SDL_RenderDrawLine(data->renderer, r.x + 1, r.y + header_height, r.x + r.w - 2, r.y + header_height);

    SDL_RenderDrawRect(data->renderer, &r);

    /* Draw drop shadow */
    SDL_SetRenderDrawColor(data->renderer, 0, 0, 0, box_alpha);
    SDL_RenderDrawLine(data->renderer, r.x + r.w, r.y + 1, r.x + r.w, r.y + r.h - 1);
    SDL_RenderDrawLine(data->renderer, r.x + 1, r.y + r.h, r.x + r.w - 1, r.y + r.h );
  }
}

/* Draws a button */
static void spooky_box_draw_button(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;
  const spooky_font * font = data->font;

  SDL_Rect r = *rect;
  SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
  SDL_SetRenderDrawBlendMode(data->renderer, blend_mode);

  SDL_SetRenderDrawColor(data->renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, 255);
  SDL_RenderFillRect(data->renderer, &r);

  SDL_Color w = { .r = 255, .g = 255, .b = 255, .a = 255 };
  SDL_Point text_p = { .x = r.x + (r.w / 2) - (data->name_width / 2) + 1, .y = r.y + (r.h / 2) - (data->name_height / 2) + 1};
  if(data->is_active) {
    w = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
    text_p.x = text_p.x + 1;
    text_p.y = text_p.y + 1;
  }
  font->write(font, &text_p, &w, data->name, strnlen(data->name, SPOOKY_MAX_STRING_LEN), NULL, NULL);

  SDL_Color top_line_color = { .r = 255, .g = 255, .b = 255, .a = 255 };
  SDL_Color bottom_line_color = { .r = 0, .g = 0, .b = 0, .a = 255 };

  if(data->is_active) {
    SDL_Color temp = bottom_line_color;
    bottom_line_color = top_line_color;
    top_line_color = temp;
  }

  spooky_box_draw_3d_rectangle_v2(self, rect, &top_line_color, &bottom_line_color);
}

static void spooky_box_draw_main_menu(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;

  SDL_Rect r = *rect;
  SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
  SDL_SetRenderDrawBlendMode(data->renderer, blend_mode);

  SDL_SetRenderDrawColor(data->renderer, data->bg_color.r, data->bg_color.g, data->bg_color.b, 255);
  SDL_RenderFillRect(data->renderer, &r);

  if(data->boxes_count > 0) {
    /* render children */
    int width_accum = 0;
    for(int i = 0; i < data->boxes_count; i++) {
      const spooky_box * box = data->boxes[i];
      if(box->get_box_type(box) == SBT_MAIN_MENU_ITEM) {
        int w, h;
        data->font->measure_text(data->font, box->data->name, strnlen(box->data->name, SPOOKY_MAX_STRING_LEN),  &w, &h);
        int m_dash = data->font->get_m_dash(data->font);

        /* Spacing between menu items */
        int spacing = i == 0 ? 0 : (m_dash * 2) - 3;

        spooky_box_data * box_data = box->data;

        box_data->rect.x = spacing + r.x + width_accum;
        box_data->rect.y = r.y;
        box_data->rect.w = w + m_dash;
        box_data->rect.h = data->rect.h;
        spooky_box_render(box->as_base(box), data->renderer);

        if(i > 0) {
          /* draw menu separators */
          SDL_SetRenderDrawColor(data->renderer, 100, 100, 100, 255);
          SDL_RenderDrawLine(data->renderer, r.x + (spacing + width_accum) - 1, r.y + 2, r.x + (spacing + width_accum) - 1, r.y + r.h - spooky_gui_y_padding); 
        }

        width_accum += w + spacing;
      }
    }
  }
}

static void spooky_box_draw_main_menu_item(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;
  const spooky_font * font = data->font;

  SDL_Rect r = *rect;

  SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
  SDL_SetRenderDrawBlendMode(data->renderer, blend_mode);

  SDL_Color w = { .r = white.r, .g = white.g, .b = white.b, .a = white.a };
  if(data->is_active) {
    w.r = 100;
    w.g = 100;
  }
  SDL_Point text_p = { .x = r.x + (r.w / 2) - (data->name_width / 2) + 1, .y = r.y + (r.h / 2) - (data->name_height / 2)};
  font->write(font, &text_p, &w, data->name, strnlen(data->name, SPOOKY_MAX_STRING_LEN), NULL, NULL);
}

void spooky_box_scroll_box_render(const spooky_base * self, SDL_Renderer * renderer) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)((const spooky_box *)self)->data;
  if(!data->_super.is_visible) return;

  /* we draw onto a texture at (0, 0); but will copy to the renderer at the appropriate (x, y) coords, below */
  SDL_Rect r = { .x = 0, .y = 0, .w = data->_super.rect.w, .h = data->_super.rect.h };

  const SDL_Rect * rect = &(data->_super.rect);

  /* TODO: Optimize temp texture */
  SDL_Texture * box_texture = data->box_texture;

  SDL_SetRenderTarget(renderer, box_texture);
  SDL_RenderClear(renderer);

  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 100);
  SDL_RenderFillRect(renderer, &r);

  /* decrease height by 1 so the bottom of the 3d rect displays on the temp texture */
  SDL_Rect draw_3d_rect = { .x = r.x, .y = r.y, .w = r.w, .h = r.h - 1 };

  /* increase the y axis for the text renderering */
  r.y += 2;

  SDL_Texture * scroll_window_texture = data->scroll_window_texture;

  int min_width = spooky_int_min(r.w, data->scroll_window_texture_width);
  int min_height = spooky_int_min(r.h, data->scroll_window_texture_height);

  int max_visible_items = spooky_box_get_max_visible_items((const spooky_box *)self);

  int horizontal_scroll_bar = data->is_horizontal_scroll_bar_visible ? data->scroll_box_horizontal_rect.h + 2 : 0;
  int y_offset = (int)(data->percent_vertically_scrolled * (float)(data->scroll_window_texture_height - r.h + horizontal_scroll_bar ));
  if(y_offset > data->scroll_window_texture_height - r.h + horizontal_scroll_bar) y_offset = data->scroll_window_texture_height - r.h + horizontal_scroll_bar;

  int vertical_scroll_bar = data->is_vertical_scroll_bar_visible ? data->scroll_box_vertical_rect.w + 2 : 0;
  int x_offset = (int)(data->percent_horizontally_scrolled * (float)(data->scroll_window_texture_width - r.w + vertical_scroll_bar ));
  if(x_offset > data->scroll_window_texture_width - r.w + vertical_scroll_bar) x_offset = data->scroll_window_texture_width - r.w + vertical_scroll_bar;

  if(max_visible_items > data->scroll_bar_items_count - 1) horizontal_scroll_bar = 0;

  SDL_Rect s = { .x = x_offset, .y = y_offset, .w = spooky_int_min(r.w, min_width) - vertical_scroll_bar, .h = spooky_int_min(r.h, min_height) - horizontal_scroll_bar };
  int image_y_offset_correction = 1;
  if(data->_super.is_image_collection) image_y_offset_correction = -2;

  SDL_Rect d = { .x = 0, .y = image_y_offset_correction, .w = spooky_int_min(r.w, min_width) - vertical_scroll_bar, .h = spooky_int_min(r.h, min_height) - horizontal_scroll_bar };
  SDL_RenderCopy(renderer, scroll_window_texture, &s, &d);

  int text_height;
  if(data->scroll_bar_items_count > 0) {
    text_height = data->scroll_bar_items[0].rect.h;
  } else {
    data->_super.font->measure_text(data->_super.font, "M", strlen("M"), NULL, &text_height);
  }

  { /* Draw highlight and selected hovers */
    data->_super.font->set_is_drop_shadow(data->_super.font, false);
    if(data->selected_item_ordinal >= 0 && data->selected_item_ordinal < data->scroll_bar_items_count) {
      const spooky_box_scroll_box_item * item = &(data->scroll_bar_items[data->selected_item_ordinal]);
      if(item->type == SBSBIT_TEXT) {
        if(item && item->text) {
          SDL_Rect selected_rect = { .x = 1, .y = data->selected_item_ordinal * (text_height + 2) - y_offset, .h = text_height + 2, .w = rect->w - (data->is_vertical_scroll_bar_visible ?  scroll_bar_width + 1: 0)};
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
          SDL_RenderFillRect(renderer, &selected_rect);
          SDL_Point selected_p = { .x = selected_rect.x - x_offset, .y = selected_rect.y + 1 };
          data->_super.font->write(data->_super.font, &selected_p, &white, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
        }
      }  else if(item->type == SBSBIT_IMAGE) {
        SDL_Rect hover_rect = { .x = item->rect.x - x_offset, .y = item->rect.y - y_offset - 10, .h = item->height, .w = item->width};

        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_Rect select_rect = { .x = hover_rect.x, .y = hover_rect.y + 3, .h = hover_rect.h - 6, .w = hover_rect.w };
        SDL_RenderFillRect(renderer, &select_rect);

        SDL_Rect selected_rect_source = { .x = 0, .y = 0, .w = max_texture_rect_width, .h = max_texture_rect_width };
        SDL_Rect selected_rect_dest = { .x = hover_rect.x, .y = hover_rect.y + 1, .w = max_texture_rect_width, .h = max_texture_rect_width };
        SDL_RenderCopy(renderer, item->texture, &selected_rect_source , &selected_rect_dest);

        int max_text_height, max_text_width;
        spooky_box_calculate_max_text_properties((const spooky_box *)self, &max_text_width, &max_text_height);

        int t_width, t_height;
        data->_super.font->measure_text(data->_super.font, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), &t_width, &t_height);

        int text_center = (item->rect.x - x_offset) + ((item->rect.w / 2) - (t_width / 2));
        SDL_Point p = { .x = text_center, .y = hover_rect.y + hover_rect.h - max_text_height - 5 };
        data->_super.font->set_is_drop_shadow(data->_super.font, true);
        data->_super.font->write(data->_super.font, &p, &white, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
        data->_super.font->set_is_drop_shadow(data->_super.font, false);
      }
    }
    if(data->hover_item_ordinal >= 0 && data->hover_item_ordinal < data->scroll_bar_items_count && data->hover_item_ordinal != data->selected_item_ordinal) {
      const spooky_box_scroll_box_item * item = &(data->scroll_bar_items[data->hover_item_ordinal]);
      if(item->type == SBSBIT_TEXT) {
        if(item && item->text) {
          SDL_Rect hover_rect = { .x = 1, .y = data->hover_item_ordinal * (text_height + 2) - y_offset, .h = text_height + 2, .w = rect->w - (data->is_vertical_scroll_bar_visible ?  scroll_bar_width + 1: 0)};
          SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
          SDL_RenderFillRect(renderer, &hover_rect);
          SDL_Point hover_p = { .x = hover_rect.x - x_offset, .y = hover_rect.y + 1 };
          data->_super.font->write(data->_super.font, &hover_p, &black, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
        }
      } else if(item->type == SBSBIT_IMAGE) {
        SDL_Rect hover_rect = { .x = item->rect.x - x_offset, .y = item->rect.y - y_offset - 10, .h = item->height, .w = item->width};

        SDL_SetRenderDrawColor(renderer, 55, 55, 55, 100);
        SDL_Rect select_rect = { .x = hover_rect.x, .y = hover_rect.y + 3, .h = hover_rect.h - 6, .w = hover_rect.w };
        SDL_RenderDrawRect(renderer, &select_rect);

        SDL_Rect selected_rect_source = { .x = 0, .y = 0, .w = max_texture_rect_width, .h = max_texture_rect_width };
        SDL_Rect selected_rect_dest = { .x = hover_rect.x, .y = hover_rect.y + 1, .w = max_texture_rect_width, .h = max_texture_rect_width };
        SDL_RenderCopy(renderer, item->texture, &selected_rect_source , &selected_rect_dest);

        int max_text_height, max_text_width;
        spooky_box_calculate_max_text_properties((const spooky_box *)self, &max_text_width, &max_text_height);

        int t_width, t_height;
        data->_super.font->measure_text(data->_super.font, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), &t_width, &t_height);

        int text_center = (item->rect.x - x_offset) + ((item->rect.w / 2) - (t_width / 2));
        SDL_Point p = { .x = text_center, .y = hover_rect.y + hover_rect.h - max_text_height - 5 };
        data->_super.font->write(data->_super.font, &p, &white, item->text, strnlen(item->text, SPOOKY_MAX_STRING_LEN), NULL, NULL);
      }
    }
    data->_super.font->set_is_drop_shadow(data->_super.font, true);
  }

  /* Vertical Scroll Region */
  if(data->is_vertical_scroll_bar_visible) {
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    /* Scroll background */
    SDL_RenderFillRect(renderer, &(data->scroll_box_vertical_rect));

    /* Scroll Up */
    spooky_box_render_scroll_button((const spooky_box *)self, &(data->up_rect), SBSBB_UP);

    /* Scroll Down */
    spooky_box_render_scroll_button((const spooky_box *)self, &(data->down_rect), SBSBB_DOWN);

    /* Scroll vertical bar */
    static const SDL_Rect src_top = { .x = 0 + 16 + 16, .y = 0, .w = 10, .h = 1 };
    static const SDL_Rect src_bar = { .x = 0 + 16 + 16, .y = 1, .w = 10, .h = 7 };
    static const SDL_Rect src_bottom = { .x = 0 + 16 + 16, .y = 9, .w = 10, .h = 1 };

    SDL_Rect dest = data->scroll_vertical_rect;
    SDL_Rect dest_top = { .x = dest.x, .y = dest.y + 2, .w = dest.w + 1, .h = 1 };
    SDL_Rect dest_bar = { .x = dest.x, .y = dest.y + dest_top.h + 1, .w = dest.w, .h = dest.h - 3 };
    SDL_Rect dest_bottom = { .x = dest.x, .y = dest.y + dest_top.h + dest_bar.h + 1, .w = dest.w, .h = 1 };

    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_top, &dest_top);
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_bar, &dest_bar);
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_bottom, &dest_bottom);
  }

  /* Horizontal Scroll Region */
  if(data->is_horizontal_scroll_bar_visible) {
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    // Scroll background
    SDL_RenderFillRect(renderer, &data->scroll_box_horizontal_rect);

    // Scroll Left
    spooky_box_render_scroll_button((const spooky_box *)self, &(data->left_rect), SBSBB_LEFT);

    // Scroll Right
    spooky_box_render_scroll_button((const spooky_box *)self, &(data->right_rect), SBSBB_RIGHT);

    // Scroll horizontal bar
    static const SDL_Rect src_left = { .x = 0 + 16 + 16, .y = 0, .w = 1, .h = 10 };
    static const SDL_Rect src_bar = { .x = 0 + 16 + 16 + 2, .y = 0, .w = 7, .h = 10 };
    static const SDL_Rect src_right = { .x = 0 + 16 + 16 + 9, .y = 0, .w = 1, .h = 10 };

    SDL_Rect dest = data->scroll_horizontal_rect;
    SDL_Rect dest_left = { .x = dest.x, .y = dest.y, .w = 1, .h = dest.h + 1 };
    SDL_Rect dest_bar = { .x = dest.x + dest_left.w, .y = dest.y, .w = dest.w - 2, .h = dest.h + 1 };
    SDL_Rect dest_right = { .x = dest.x + dest_left.w + dest_bar.w, .y = dest.y, .w = 1, .h = dest.h + 1 };

    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_left, &dest_left);
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_bar, &dest_bar);
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &src_right, &dest_right);
  }

  spooky_box_draw_3d_rectangle((const spooky_box *)self, &draw_3d_rect);

  /* diagnostics */
  if(spooky_box_enable_diagnostics) {
    /*
    SDL_Rect scroll_up_region = { .x = data->scroll_box_vertical_rect.x, .y = data->scroll_box_vertical_rect.y + data->up_rect.h + 1, .w = data->scroll_box_vertical_rect.w, .h = data->scroll_vertical_rect.y - (data->up_rect.y  + data->up_rect.h) };
    SDL_Rect scroll_left_region = { .x = data->scroll_box_horizontal_rect.x - data->left_rect.w + 1, .y = data->scroll_box_horizontal_rect.y, .w = data->scroll_horizontal_rect.x - (data->left_rect.x  + data->left_rect.w), .h = data->scroll_box_vertical_rect.h };

    SDL_Rect scroll_down_region = { .x = data->scroll_box_vertical_rect.x, .y = data->scroll_vertical_rect.y + data->scroll_vertical_rect.h + 1, .w = data->scroll_box_vertical_rect.w, .h = data->scroll_box_vertical_rect.h - scroll_up_region.h - data->up_rect.h - data->down_rect.h - data->scroll_vertical_rect.h - 2 };

    SDL_Rect horizontal_page_left_region = { .x = r.x + data->scroll_box_horizontal_rect.x + data->left_rect.w, .y =  data->scroll_box_horizontal_rect.y, .w = data->scroll_horizontal_rect.x - (data->left_rect.x + data->left_rect.w), .h = data->scroll_box_horizontal_rect.h };
    SDL_SetRenderDrawColor(renderer, 200, 100, 255, 255);
    SDL_RenderDrawRect(renderer, &horizontal_page_left_region);

    SDL_Rect horizontal_page_right_region = { .x = r.x + data->scroll_horizontal_rect.x +  data->scroll_horizontal_rect.w, .y = data->scroll_box_horizontal_rect.y, .w = data->scroll_box_horizontal_rect.w - scroll_left_region.w - data->left_rect.w - data->right_rect.w - data->scroll_horizontal_rect.w, .h = data->scroll_box_horizontal_rect.h };
    SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);
    SDL_RenderDrawRect(renderer, &horizontal_page_right_region);

    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &scroll_up_region);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawRect(renderer, &scroll_down_region);

    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &(data->scroll_box_vertical_rect));

    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderDrawRect(renderer, &(data->up_rect));

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    SDL_RenderDrawRect(renderer, &(data->down_rect));

    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderDrawRect(renderer, data->scroll_button.rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect region = {.x = 0, .y = 0, .w = data->scroll_window_texture_width, .h = data->scroll_window_texture_height};
    SDL_RenderDrawRect(renderer, &region);
    */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &(data->scroll_box_vertical_rect));
  }

  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderCopy(renderer, box_texture, NULL, rect);
}

static void spooky_box_render_scroll_button(const spooky_box * self, const SDL_Rect * button_dest, spooky_box_scroll_box_button_types button_type) {
  static const SDL_Rect up = { .x = 0 + 16 + 16, .y = 0, .w = 10, .h = 10 };
  static const SDL_Rect down = { .x = 0 + 16 + 16 + 10, .y = 0, .w = 10, .h = 10 };

  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)self->data;
  SDL_Renderer * renderer = data->_super.renderer;

  unsigned char gray = 200;
  if(data->scroll_button.is_hover && data->scroll_button.button == button_type) {
    gray = 220;
  }

  unsigned char r, g, b, a;
  SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

  SDL_SetRenderDrawColor(renderer, gray, gray, gray, 255);
  //SDL_RenderFillRect(renderer, button_dest);

  if(data->scroll_button.is_active && data->scroll_button.button == button_type) {
    SDL_Rect d = {.x = button_dest->x, .y = button_dest->y, .w = 8, .h = 8 };
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &down, &d);
  } else {
    SDL_Rect d = {.x = button_dest->x, .y = button_dest->y, .w = 8, .h = 8 };
    SDL_RenderCopy(renderer, data->_super.sprite_sheet_texture, &up, &d);
  }

  SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

static void spooky_box_draw_image(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;

  SDL_Rect r = *rect;

  /* Render preview image */
  static SDL_Texture * background = NULL;
  if(!background) {
    SDL_Surface * background_surface = NULL;
    spooky_load_image("./res/backgrounds/preview.png", strlen("./res/backgrounds/preview.png"), &background_surface);
    background = SDL_CreateTextureFromSurface(data->renderer, background_surface);
    SDL_FreeSurface(background_surface), background_surface = NULL;
  }

  SDL_RenderCopy(data->renderer, background, NULL, &r);

  /* Draw the border around the preview image */
  spooky_box_draw_3d_rectangle(self, rect);

  /* Write the text "Preview" centered over the preview image */
  SDL_Point preview_text_p = { .x = r.x + ((r.w / 2) - (data->name_width / 2)), .y = r.y + ((r.h / 2) - (data->name_height / 2)) };

  data->font->write(data->font, &preview_text_p, &white, data->name, strnlen(data->name, SPOOKY_MAX_STRING_LEN), NULL, NULL);
}

static void spooky_box_draw_text(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_data * data = self->data;
  spooky_box_draw_3d_rectangle(self, rect);
  SDL_Point meta_text_p = { .x = rect->x + spooky_gui_x_padding, .y = rect->y + spooky_gui_y_padding };
  data->font->write(data->font, &meta_text_p, &white, "Slot: After Death\nDate: 2019/07/25 11:13A\n\n", strlen("Slot: After Death\nDate: 2019/07/25 11:13A\n\n"), NULL, NULL);
}

static void spooky_box_draw_3d_rectangle(const spooky_box * self, const SDL_Rect * rect) {
  spooky_box_draw_3d_rectangle_v2(self, rect, NULL, NULL);
}

static void spooky_box_draw_3d_rectangle_v2(const spooky_box * self, const SDL_Rect * rect, const SDL_Color * top_line_color, const SDL_Color * bottom_line_color) {
  spooky_box_data * data = self->data;

  static SDL_Color top_line_color_default = { .r = 0, .g = 0, .b = 0, .a = 255 };
  static SDL_Color bottom_line_color_default = { .r = 255, .g = 255, .b = 255, .a = 255 };

  const SDL_Color * top_color = top_line_color;
  const SDL_Color * bottom_color = bottom_line_color;
  if(!top_line_color) {
    top_color = &top_line_color_default;
  }
  if(!bottom_line_color) {
    bottom_color = &bottom_line_color_default;
  }
  unsigned char r, g, b, a;

  SDL_GetRenderDrawColor(data->renderer, &r, &g, &b, &a);
  // top and left
  SDL_SetRenderDrawColor(data->renderer, top_color->r, top_color->g, top_color->b, top_color->a);
  // - left line
  SDL_RenderDrawLine(data->renderer, rect->x, rect->y + 1, rect->x, rect->y + rect->h - 1);

  // - top line
  SDL_RenderDrawLine(data->renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y);

  // bottom and right
  SDL_SetRenderDrawColor(data->renderer, bottom_color->r, bottom_color->g, bottom_color->b, bottom_color->a);
  // - right line
  SDL_RenderDrawLine(data->renderer, rect->x + rect->w - 1, rect->y + 1, rect->x + rect->w - 1, rect->y + rect->h - 1);

  // - bottom line
  SDL_RenderDrawLine(data->renderer, rect->x, rect->y + rect->h, rect->x + rect->w - 1, rect->y + rect->h);
  SDL_SetRenderDrawColor(data->renderer, r, g, b, a);
}

static void spooky_box_set_z_order(const spooky_box * self, int z_order) {
  spooky_box_data * data = self->data;
  data->z_order = z_order;
}

static int spooky_box_get_z_order(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->z_order;
}

static const char  * spooky_box_get_name(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->name;
}

static spooky_box_types spooky_box_get_box_type(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->box_type;
}

static int spooky_box_get_x(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->rect.x;
}

static void spooky_box_set_x(const spooky_box * self, int x) {
  spooky_box_data * data = self->data;
  data->rect.x = x;
}

static int spooky_box_get_y(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->rect.y;
}

static void spooky_box_set_y(const spooky_box * self, int y) {
  spooky_box_data * data = self->data;
  data->rect.y = y;
}

static int spooky_box_get_w(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->rect.w;
}

static void spooky_box_set_w(const spooky_box * self, int w) {
  spooky_box_data * data = self->data;
  data->rect.w = w;
}

static int spooky_box_get_h(const spooky_box * self) {
  spooky_box_data * data = self->data;
  return data->rect.h;
}

static void spooky_box_set_h(const spooky_box * self, int h) {
  spooky_box_data * data = self->data;
  data->rect.h = h;
}

static void spooky_box_set_direction(const spooky_box * self, spooky_box_scroll_box_direction direction) {
  spooky_box_scroll_box_data * data = (spooky_box_scroll_box_data *)self->data;

  data->direction = direction;
}

#undef WIDTH
#undef HEIGHT

#endif /* if 1 == 2 */
