#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sodium.h>

#include "config.h"
#include "sp_types.h"
#include "sp_str.h"
#include "sp_error.h"
#include "sp_math.h"
#include "sp_hash.h"
#include "sp_pak.h"
#include "sp_gui.h"
#include "sp_font.h"
#include "sp_base.h"
#include "sp_console.h"
#include "sp_debug.h"
#include "sp_help.h"
#include "sp_context.h"
#include "sp_wm.h"
#include "sp_log.h"
#include "sp_limits.h"
#include "sp_time.h"
#include "sp_db.h"
#include "sp_box.h"
#include "sp_config.h"

static const size_t MAX_TILES_ROW_LEN = 64;
static const size_t MAX_TILES_COL_LEN = 64;
static const size_t MAX_TILES_DEPTH_LEN = 64;
static const size_t VOXEL_WIDTH = 8;
static const size_t VOXEL_HEIGHT = 8;

typedef struct spooky_vector {
  size_t x;
  size_t y;
  size_t z;
} spooky_vector;

typedef enum spooky_biom {
  SB_EMPTY,
  SB_TUNDRA,
  SB_CONIFEROUS_FOREST,
  SB_TEMPERATE_DECIDUOUS_FOREST,
  SB_GRASSLAND,
  SB_DESERT,
  SB_SHRUBLAND,
  SB_RAINFOREST,
  SB_EOE
} spooky_biom;

typedef enum spooky_tile_type {
  STT_EMPTY,
  /* Impassible */
  STT_BEDROCK,

  /* Rock */
  STT_IGNEOUS,
  STT_SEDIMENTARY,
  STT_METAMORPHIC,

  /* Dirt */
  STT_SAND,
  STT_SILT,
  STT_CLAY,
  STT_LOAM,
  STT_GRAVEL,

  /* Aqua */
  STT_WATER,

  /* Ground */
  STT_TREE,
  STT_EOE
} spooky_tile_type;

typedef struct spooky_tile_meta spooky_tile_meta;
typedef struct spooky_tile_meta {
  spooky_biom biom;
} spooky_tile_meta;

/*
static const spooky_tile_meta meta_definitions[] = {
  { .biom = SB_EMPTY },
  { .biom = SB_TUNDRA },
  { .biom = SB_CONIFEROUS_FOREST },
  { .biom = SB_TEMPERATE_DECIDUOUS_FOREST },
  { .biom = SB_GRASSLAND },
  { .biom = SB_DESERT },
  { .biom = SB_SHRUBLAND },
  { .biom = SB_RAINFOREST }
};
*/
typedef struct spooky_tile spooky_tile;
typedef struct spooky_tile {
  spooky_tile_type type;
} spooky_tile;

static spooky_tile spooky_global_tiles[2] = {
  { .type = STT_EMPTY },
  { .type = STT_BEDROCK }
};

static void spooky_render_landscape(SDL_Renderer * renderer, const spooky_context * context, spooky_tile * tiles, size_t tiles_len, const spooky_vector * cursor);
static errno_t spooky_loop(spooky_context * context, const spooky_ex ** ex);
static errno_t spooky_command_parser(spooky_context * context, const spooky_console * console, const spooky_log * log, const char * command) ;

typedef struct spooky_options {
  bool print_licenses;
  char padding[7];
} spooky_options;

static errno_t spooky_parse_args(int argc, char ** argv, spooky_options * options);
static void spooky_generate_tiles(spooky_tile * tiles_in, size_t tiles_len);
static const char * spooky_tile_info(const spooky_tile * tile, char * buf, size_t buf_len, int * buf_len_out);

inline static size_t SP_OFFSET(size_t x, size_t y, size_t z) {
  return x + (y * MAX_TILES_ROW_LEN) + (z * MAX_TILES_ROW_LEN * MAX_TILES_COL_LEN);
}

int main(int argc, char **argv) {
  spooky_options options = { 0 };

  spooky_parse_args(argc, argv, &options);

  spooky_pack_tests();

  FILE * fp = NULL;

  long pak_offset = 0;
  uint64_t content_offset = 0, content_len = 0, index_entries = 0, index_offset = 0, index_len = 0;

  { /* check if we're a bundled exec + pak file */
    int fd = open(argv[0], O_RDONLY | O_EXCL, S_IRUSR | S_IWUSR);
    if(fd >= 0) {
      fp = fdopen(fd, "rb");
      if(fp) {
        errno_t is_valid = spooky_pack_is_valid_pak_file(fp, &pak_offset, &content_offset, &content_len, &index_entries, &index_offset, &index_len);
        if(is_valid != SP_SUCCESS) {
          fclose(fp);
          fp = NULL;
          /* not a valid bundle */
        }
      } else { fprintf(stderr, "Unable to open file %s\n", argv[0]); }
    } else { fprintf(stderr, "Unable to open file %s\n", argv[0]); }
  }

  if(!fp) {
    /* we're not a bundle; see if default 'pak.spdb' exists; create if not */
    bool create = false;
    const char * pak_file = "pak.spdb";
    int fd = open(pak_file, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    if(fd < 0) {
      /* already exists; open it */
      if(errno == EEXIST) {
        fd = open(pak_file, O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
      }
    } else {
      /* doesn't already exist; empty file... populate it */
      create = true;
    }

    fp = fdopen(fd, "wb+x");

    if(create) {
      /* only create it if it's not already a valid pak file */
      spooky_pack_content_entry content[] = {
        { .path = "res/fonts/PRNumber3.ttf", .name = "pr.number" },
        { .path = "res/fonts/PrintChar21.ttf", .name = "print.char" },
        { .path = "res/fonts/DejaVuSansMono.ttf", .name = "deja.sans" },
        { .path = "res/fonts/SIL Open Font License.txt", .name = "open.font.license" },
        { .path = "res/fonts/deja-license.txt", .name = "deja.license" }
      };

      spooky_pack_create(fp, content, sizeof content / sizeof content[0]);
    }
    fseek(fp, 0, SEEK_SET);
    errno_t is_valid = spooky_pack_is_valid_pak_file(fp, &pak_offset, &content_offset, &content_len, &index_entries, &index_offset, &index_len);
    assert(is_valid == SP_SUCCESS);
  }

  fseek(fp, pak_offset, SEEK_SET);

  spooky_context context = { 0 };
  const spooky_ex * ex = NULL;

  if(spooky_init_context(&context, fp) != SP_SUCCESS) { goto err0; }
  if(spooky_test_resources(&context) != SP_SUCCESS) { goto err0; }
  /* fprintf(stdout, "SPDB Stats: Content Offset:  %lu, Content Len: %lu, Index Entries: %lu, Index Offset:  %lu, Index Len: %lu\n", (size_t)content_offset, (size_t)content_len, (size_t)index_entries, (size_t)index_offset, (size_t)index_len); */

  const spooky_hash_table * hash = context.get_hash(&context);

  fclose(fp);

  {
#ifdef DEBUG
    void * temp = NULL;
    /* Loading a font from the resource pak example */
    spooky_pack_item_file * font = NULL;
    hash->find(hash, "pr.number", strnlen("pr.number", SPOOKY_MAX_STRING_LEN), &temp);
    font = (spooky_pack_item_file *)temp;

    SDL_RWops * src = SDL_RWFromMem(font->data, (int)font->data_len);
    assert(src);
    TTF_Init();
    TTF_Font * ttf = TTF_OpenFontRW(src, 0, 10);

    assert(ttf);
    /* fprintf(stdout, "Font Height: %i\n", TTF_FontHeight(ttf)); */
    fprintf(stdout, "Okay!\n");
#endif
  }

  if(options.print_licenses) {
    fprintf(stdout, "Licenses:\n");
    fprintf(stdout, "********************************************************************************\n");

    spooky_pack_item_file * temp = NULL;
    char * deja_license = NULL, * open_license = NULL;
    if(hash->find(hash, "deja.license", strnlen("deja.license", SPOOKY_MAX_STRING_LEN), ((void *)&temp)) == SP_SUCCESS) {
      deja_license = strndup(temp->data, temp->data_len);
    }

    if(hash->find(hash, "open.font.license", strnlen("open.font.license", SPOOKY_MAX_STRING_LEN), ((void *)&temp)) == SP_SUCCESS) {
      open_license = strndup(temp->data, temp->data_len);
    }

    fprintf(stdout, "%s\n\n%s\n", open_license, deja_license);
    free(deja_license), deja_license = NULL;
    free(open_license), open_license = NULL;

    fprintf(stdout, "********************************************************************************\n");
  }

  {
#ifdef DEBUG
    /* Print out the pak file resources: */
    fseek(fp, 0, SEEK_SET);
    spooky_pack_print_resources(stdout, fp);
#endif
  }

  /* TODO: Saving and Loading Game State:
  spooky_save_game_v1 state = {
    .base.name = "hello, world",
    .base.save_game_version = 1,
    .deaths = 10,
    .seed = 123456,
    .turns = 100,
    .x = 1.53566,
    .y = 2.83883,
    .z = 3.98383
  };
  if(db->save_game(db, ((spooky_save_game *)&state)) != 0) {
    fprintf(stderr, "Unable to save state\n");
    abort();
  }
  spooky_save_game_v1 state_0;
  memset(&state_0, 0, sizeof state_0);
  if(db->load_game(db, "hello, world", ((spooky_save_game *)&state_0)) != 0) {
    fprintf(stderr, "Unable to load state\n");
    abort();
  } else {
    fprintf(stdout, "Loaded state:\n");
    fprintf(stdout, "%s: version %i save id %i\n", state_0.base.name, state_0.base.save_game_version, state_0.base.v1_id);
    fprintf(stdout, "deaths: %i, seed: %i, turns: %i\n", state_0.deaths, state_0.seed, state_0.turns);
    fprintf(stdout, "x: %f, y: %f, z: %f\n", state_0.x, state_0.y, state_0.z);
  }
  */

  if(spooky_loop(&context, &ex) != SP_SUCCESS) { goto err1; }
  if(spooky_quit_context(&context) != SP_SUCCESS) { goto err2; }

  fprintf(stdout, "\nThank you for playing! Happy gaming!\n");
  fflush(stdout);
  return SP_SUCCESS;

err2:
  fprintf(stderr, "A fatal error occurred during shutdown.\n");
  goto err;

err1:
  fprintf(stderr, "A fatal error occurred in the main loop.\n");
  if(ex != NULL) {
    fprintf(stderr, "%s\n", ex->msg);
  }
  spooky_release_context(&context);
  goto err;

err0:
  fprintf(stderr, "A fatal error occurred during initialization.\n");
  goto err;

err:
  return SP_FAILURE;
}

errno_t spooky_loop(spooky_context * context, const spooky_ex ** ex) {
  const double HERTZ = 30.0;
  const int TARGET_FPS = 60;
  const int BILLION = 1000000000;

  const int64_t TIME_BETWEEN_UPDATES = (int64_t) (BILLION / HERTZ);
  const int MAX_UPDATES_BEFORE_RENDER = 5;
  const int TARGET_TIME_BETWEEN_RENDERS = BILLION / TARGET_FPS;

  spooky_tile * tiles = calloc(MAX_TILES_ROW_LEN * MAX_TILES_COL_LEN * MAX_TILES_DEPTH_LEN, sizeof tiles);
  size_t tiles_len = MAX_TILES_ROW_LEN * MAX_TILES_COL_LEN * MAX_TILES_DEPTH_LEN * sizeof tiles;

  fprintf(stdout, "Tiles Len: %lu\n", tiles_len);
  spooky_generate_tiles(tiles, tiles_len);

  int64_t now = 0;
  int64_t last_render_time = sp_get_time_in_us();
  int64_t last_update_time = sp_get_time_in_us();

  int64_t frame_count = 0,
          fps = 0,
          seconds_since_start = 0;

  uint64_t last_second_time = (uint64_t) (last_update_time / BILLION);

  const spooky_db * db = spooky_db_acquire();
  db = db->ctor(db, "spooky.db");
  if(db->create(db) != 0) {
    fprintf(stderr, "Unable to initialize save game storage. Sorry :(\n");
    return EXIT_FAILURE;
  }
  if(db->open(db) != 0) {
    fprintf(stderr, "Unable to open save game storage. Sorry :(\n");
    return EXIT_FAILURE;
  }

  SDL_Window * window = context->get_window(context);
  assert(window);

  SDL_Renderer * renderer = context->get_renderer(context);
  assert(renderer);

#ifdef __APPLE__
  /* On OS X Mojave, screen will appear blank until a call to PumpEvents.
   * This temporarily fixes the blank screen problem */
  SDL_PumpEvents();
#endif

  /* Showing the window here prevented flickering on Linux Mint */
  SDL_ShowWindow(window);

  SDL_Texture * background = NULL;
  SDL_ClearError();
  if(spooky_gui_load_texture(renderer, "./res/bg3.png", 13, &background) != SP_SUCCESS) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_Texture * letterbox_background = NULL;
  SDL_ClearError();
  if(spooky_gui_load_texture(renderer, "./res/bg4.png", 13, &letterbox_background) != SP_SUCCESS) { goto err1; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  assert(background != NULL && letterbox_background != NULL);

  const spooky_wm * wm = spooky_wm_acquire();
  wm = wm->ctor(wm, context);

  const spooky_base * objects[3] = { 0 };
  const spooky_base ** first = objects;
  const spooky_base ** last = objects + ((sizeof objects / sizeof * objects));

  const spooky_log * log = spooky_log_acquire();
  log = log->ctor(log);

  const spooky_console * console = spooky_console_acquire();
  console = console->ctor(console, context, renderer);

  console->push_str(console, "> " PACKAGE_NAME " " PACKAGE_VERSION " <\n");
  const spooky_debug * debug = spooky_debug_acquire();
  debug = debug->ctor(debug, context);

  const spooky_help * help = spooky_help_acquire();
  help = help->ctor(help, context);

  objects[0] = (const spooky_base *)console;
  objects[1] = (const spooky_base *)debug;
  objects[2] = (const spooky_base *)help;

  console->as_base(console)->set_z_order(console->as_base(console), 9999997);
  help->as_base(help)->set_z_order(help->as_base(help), 9999998);
  debug->as_base(debug)->set_z_order(debug->as_base(debug), 9999999);

  spooky_base_z_sort(objects, (sizeof objects / sizeof * objects));

  if(debug->as_base(debug)->add_child(debug->as_base(debug), help->as_base(help), ex) != SP_SUCCESS) { goto err1; }

  SDL_Texture * sprite_texture = NULL;
  spooky_gui_load_texture(renderer, "./res/dwarf.png", strlen("./res/dwarf.png"), &sprite_texture);

  const spooky_sprite * sprite = spooky_sprite_acquire();
  sprite = sprite->ctor(sprite, sprite_texture);

  int temp_w, temp_h;
  SDL_GetWindowSize(window, &temp_w, &temp_h);
  int sprite_w = 8 * 4;
  int sprite_h = 16 * 4;
  int center_x = (temp_w / 2) - (sprite_w / 2);
  int center_y = (temp_h / 4) - (sprite_h / 2);
  SDL_Rect box0_rect = { .x = center_x, .y = center_y, .w = sprite_w, .h = sprite_h };
  const spooky_box * box0 = spooky_box_acquire();
  box0 = box0->ctor(box0, context, box0_rect);

  const spooky_config * config = context->get_config(context);

  SDL_ClearError();
  SDL_Texture * landscape = SDL_CreateTexture(renderer
      , SDL_GetWindowPixelFormat(window)
      , SDL_TEXTUREACCESS_TARGET
      , config->get_canvas_width(config)
      , config->get_canvas_height(config)
      );
  if(!landscape || spooky_is_sdl_error(SDL_GetError())) { abort(); }
  bool update_landscape = true;

  log->prepend(log, "Logging enabled\n", SLS_INFO);
  int x_dir = 30, y_dir = 30;
  double interpolation = 0.0;
  int seconds_to_save = 0;
  spooky_vector cursor = { .x = MAX_TILES_ROW_LEN / 2, .y = MAX_TILES_COL_LEN / 2, .z = 0 };

  const spooky_base * box = box0->as_base(box0);
  while(spooky_context_get_is_running(context)) {
    SDL_SetRenderTarget(renderer, context->get_canvas(context));

    SDL_Rect debug_rect = { 0 };
    int update_loops = 0;
    now = sp_get_time_in_us();
    while((now - last_update_time > TIME_BETWEEN_UPDATES && update_loops < MAX_UPDATES_BEFORE_RENDER)) {
      if(spooky_is_sdl_error(SDL_GetError())) { log->prepend(log, SDL_GetError(), SLS_INFO); }

      SDL_Event evt = { 0 };
      SDL_ClearError();
      while(SDL_PollEvent(&evt)) {
        if(spooky_is_sdl_error(SDL_GetError())) { log->prepend(log, SDL_GetError(), SLS_INFO); }

        /* NOTE: SDL_PollEvent can set the Error message returned by SDL_GetError; so clear it, here: */
        SDL_ClearError();
#ifdef __APPLE__
        /* On OS X Mojave, resize of a non-maximized window does not correctly update the aspect ratio */
        if (evt.type == SDL_WINDOWEVENT && (
             evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
          || evt.window.event == SDL_WINDOWEVENT_MOVED
          || evt.window.event == SDL_WINDOWEVENT_RESIZED
          || evt.window.event == SDL_WINDOWEVENT_SHOWN
          /* Only happens when clicking About in OS X Mojave */
          || evt.window.event == SDL_WINDOWEVENT_FOCUS_LOST
        ))
#elif __unix__
        if (evt.type == SDL_WINDOWEVENT && (
          evt.window.event == SDL_WINDOWEVENT_RESIZED
        ))
#endif
        {
          /* Resizing logic removed */
        }

        /* Handle top-level global events */
        switch(evt.type) {
          case SDL_QUIT:
            spooky_context_set_is_running(context, false);
            goto end_of_running_loop;
          case SDL_KEYDOWN:
            {
              SDL_Keycode sym = evt.key.keysym.sym;
              switch(sym) {
                case SDLK_h:
                case SDLK_l:
                  if(sym == SDLK_h) {
                    if(cursor.x - 1 > cursor.x) { cursor.x = 0; }
                    else {
                      cursor.x--;
                    }
                  }
                  else if(sym == SDLK_l) {
                    cursor.x++;
                    if(cursor.x >= MAX_TILES_ROW_LEN - 1) { cursor.x = MAX_TILES_ROW_LEN - 1; }
                  }
                  break;
                case SDLK_k:
                case SDLK_j:
                  if(sym == SDLK_j) {
                    cursor.y++;
                    if(cursor.y >= MAX_TILES_COL_LEN - 1) { cursor.y = MAX_TILES_COL_LEN - 1; }
                  }
                  else if(sym == SDLK_k) {
                    if(cursor.y - 1 > cursor.y) { cursor.y = 0; }
                    else {
                      cursor.y--;
                    }
                  }
                  break;
                case SDLK_COMMA: /* DOWN */
                case SDLK_PERIOD: /* UP */
                  if(sym == SDLK_COMMA) {
                    if(cursor.z - 1 > cursor.z) { cursor.z = 0; }
                    else {
                      cursor.z--;
                    }
                  }
                  else if(sym == SDLK_PERIOD) {
                    cursor.z++;
                    if(cursor.z >= MAX_TILES_DEPTH_LEN - 1) { cursor.z = MAX_TILES_DEPTH_LEN - 1; }
                  }
                  update_landscape = true;
                  break;
                case SDLK_q:
                  {
                    /* ctrl-q to quit */
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      spooky_context_set_is_running(context, false);
                      goto end_of_running_loop;
                    }
                  }
                  break;
                default:
                  break;
              }
            }
            break;
          case SDL_KEYUP:
            {
              SDL_Keycode sym = evt.key.keysym.sym;
              switch(sym) {
                case SDLK_F12: /* fullscreen window */
                  {
                    bool previous_is_fullscreen = context->get_is_fullscreen(context);
                    context->set_is_fullscreen(context, !previous_is_fullscreen);
                    SDL_ClearError();
                    if(SDL_SetWindowFullscreen(window, context->get_is_fullscreen(context) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
                      if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
                      /* on failure, reset to previous is_fullscreen value */
                      context->set_is_fullscreen(context,  previous_is_fullscreen);
                    }
                  }
                  break;
                default:
                  break;
              } /* >> switch(sym ... */
            }
            break;
          default:
            break;
        } /* >> switch(evt.type ... */

        /* handle base events */
        const spooky_base ** event_iter = last - 1;
        do {
          const spooky_base * obj = *event_iter;
          if(obj != NULL && obj->handle_event != NULL) {
            if(obj->handle_event(obj, &evt)) {
              break;
            }
          }
        } while(--event_iter >= first);

        box->handle_event(box, &evt);
      } /* >> while(SDL_PollEvent(&evt)) */

      {
        /* check the console command; execute it if it exists */
        const char * command;
        if(console->as_base(console)->get_focus(console->as_base(console)) && (command = console->get_current_command(console)) != NULL) {
          spooky_command_parser(context, console, log, command);
          console->clear_current_command(console);
        }
      }

      {
        ((const spooky_base *)debug)->get_bounds((const spooky_base *)debug, &debug_rect, NULL);

        int w = 0, h = 0;
        SDL_GetRendererOutputSize(renderer, &w, &h);
        if(debug_rect.x <= 0) {
          x_dir = abs(x_dir);
        } else if(debug_rect.x + debug_rect.w >= w) {
          x_dir = -x_dir;
        }
        if(debug_rect.y <= 0) {
          y_dir = abs(y_dir);
        } else if(debug_rect.y + debug_rect.h >= h) {
          y_dir = -y_dir;
        }
      }

      const spooky_base ** delta_iter = first;
      do {
        const spooky_base * obj = *delta_iter;
        if(obj->handle_delta != NULL) { obj->handle_delta(obj, last_update_time, interpolation); }
      } while(++delta_iter < last);

      last_update_time += TIME_BETWEEN_UPDATES;
      update_loops++;

    } /* >> while ((now - last_update_time ... */

    interpolation = fmin(1.0f, (double)(now - last_update_time) / (double)(TIME_BETWEEN_UPDATES));
    if (now - last_update_time > TIME_BETWEEN_UPDATES) {
      last_update_time = now - TIME_BETWEEN_UPDATES;
    }

    uint64_t this_second = (uint64_t)(last_update_time / BILLION);

    {
      const SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
      const spooky_gui_rgba_context * rgba = spooky_gui_push_draw_color(renderer, &c);
      {
        SDL_RenderFillRect(renderer, NULL); /* screen color */
        spooky_gui_pop_draw_color(rgba);
      }
    }

    if(update_landscape) {
      SDL_SetRenderTarget(renderer, landscape);
      spooky_render_landscape(renderer, context, tiles, tiles_len, &cursor);
      update_landscape = false;
      SDL_SetRenderTarget(renderer, context->get_canvas(context));
    }

    SDL_Rect landscape_rect = {
      .x = 0,
      .y = 0,
      .w = config->get_canvas_width(config),
      .h = config->get_canvas_height(config)
    };
    //context->translate_rect(context, &landscape_rect);
    SDL_RenderCopy(renderer, landscape, NULL, &landscape_rect);

    const SDL_Color cursor_color = { 255, 0, 255, 255};
    const spooky_gui_rgba_context * cursor_rgba = spooky_gui_push_draw_color(renderer, &cursor_color);
    {
      SDL_Rect cursor_rect = {
        .x = (int)(cursor.x * (size_t)VOXEL_WIDTH),
        .y = (int)(cursor.y * (size_t)VOXEL_HEIGHT),
        .w = (int)VOXEL_WIDTH,
        .h = (int)VOXEL_HEIGHT
      };

      if(cursor_rect.x >= (int)(VOXEL_WIDTH * MAX_TILES_ROW_LEN)) { cursor.x = (int)(VOXEL_WIDTH * MAX_TILES_ROW_LEN); }
      if(cursor_rect.x < 0) { cursor_rect.x = 0; }

      if(cursor_rect.y >= (int)(VOXEL_HEIGHT * MAX_TILES_COL_LEN)) { cursor.y = (int)(VOXEL_HEIGHT * MAX_TILES_COL_LEN); }
      if(cursor_rect.y < 0) { cursor_rect.y = 0; }

      spooky_box_draw_style style = box0->get_draw_style(box0);
      box0->set_draw_style(box0, SBDS_OUTLINE);
      box->set_rect(box, &cursor_rect, NULL);
      box->render(box, renderer);
      box0->set_draw_style(box0, style);
      spooky_gui_pop_draw_color(cursor_rgba);
    }

    /* render bases */
    const spooky_base ** render_iter = first;
    do {
      const spooky_base * obj = *render_iter;
      if(obj->render != NULL) { obj->render(obj, renderer); }
    } while(++render_iter < last);

    {
      (void)debug_rect;
      SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    }

    {
      /* Tile Details */
      const spooky_tile * current_tile = &(tiles[SP_OFFSET(cursor.x, cursor.y, cursor.z)]);
      static char tile_info[1920] = { 0 };
      int info_len = 0;
      const char * info = spooky_tile_info(current_tile, tile_info, sizeof(tile_info), &info_len);
      info_len += snprintf((tile_info + info_len), 1920 - info_len, "\nx: %lu, y: %lu, z: %lu, offset: %lu", cursor.x, cursor.y, cursor.z, SP_OFFSET(cursor.x, cursor.y, cursor.z));
      const spooky_font * font = context->get_font(context);
      static const SDL_Color white = { 255, 255, 255, 255 };
      SDL_Point info_point = { 0 };
      info_point.x = font->get_m_dash(font);
      info_point.y = font->get_line_skip(font);
      font->write_to_renderer(font, renderer, &info_point, &white, info, (size_t)info_len, NULL, NULL);
    }
    SDL_SetRenderTarget(renderer, NULL);

    // TODO: Center canvas on window.
    float renderer_scale = context->get_renderer_to_window_scale_factor(context);
    int canvas_width = config->get_canvas_width(config);
    int canvas_height = config->get_canvas_height(config);
    int window_width = (int)floor((float)config->get_window_width(config) / renderer_scale);
    int window_height = (int)floor((float)config->get_window_height(config) / renderer_scale);
    SDL_Rect center_rect = {
      .x = (window_width / 2),
      .y = (window_height / 2),
      .w = canvas_width,
      .h = canvas_height
    };
    SDL_RenderCopy(renderer, context->get_canvas(context), NULL, &center_rect);

    SDL_RenderPresent(renderer);

    last_render_time = now;
    frame_count++;

    if(this_second > last_second_time) {
      /*fprintf(stdout, "%i, %i, %i, %i--%i, %i, %i,%i--%i, %i, %i, %i\n",
        context->get_scaled_rect(context)->x, context->get_scaled_rect(context)->y, context->get_scaled_rect(context)->w, context->get_scaled_rect(context)->h,
        context->get_native_rect(context)->x, context->get_native_rect(context)->y, context->get_native_rect(context)->w, context->get_native_rect(context)->h,
        center_rect.x, center_rect.y, center_rect.w, center_rect.h
        ); */
      seconds_to_save++;
      if(seconds_to_save >= 300) {
        /* Autosave every 5 minutes */
        log->prepend(log, "Autosaving...", SLS_INFO);
        db->save_game(db, "(Autosave)", NULL/* TODO: (spooky_save_game *)&state */);
        seconds_to_save = 0;
      }
      /* Every second, update FPS: */
      fps = frame_count;
      frame_count = 0;
      last_second_time = this_second;
      seconds_since_start++;
      spooky_debug_update(debug, fps, seconds_since_start, interpolation);
    }

    /* Try to be friendly to the OS: */
    while (now - last_render_time < TARGET_TIME_BETWEEN_RENDERS && now - last_update_time < TIME_BETWEEN_UPDATES) {
      sp_sleep(0); /* Needed? */
      now = sp_get_time_in_us();
    }
end_of_running_loop: ;
  } /* >> while(spooky_context_get_is_running(context)) */

  if(db->close(db) != 0) {
    fprintf(stderr, "Unable to close save game storage; something broke but this is not catastrophic.\n");
  };

  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

  free(tiles), tiles = NULL;

  spooky_help_release(help);
  spooky_console_release(console);
  spooky_log_release(log);
  spooky_wm_release(wm);
  spooky_db_release(db);

  return SP_SUCCESS;

err1:
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

err0:
  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }

  return SP_FAILURE;
}

errno_t spooky_command_parser(spooky_context * context, const spooky_console * console, const spooky_log * log, const char * command) {

  if(strncmp(command, "clear", sizeof("clear")) == 0) {
    console->clear_console(console);
  } else if(strncmp(command, "help", sizeof("help")) == 0) {
    const char help[] =
      "DIAGNOSTIC CONSOLE\n"
      "Comands:\n"
      "  help  -- show this help screen\n"
      "  clear -- clear the console window\n"
      "  info  -- diagnostic information\n"
      "  log   -- show log\n"
      "  quit  -- exit the game\n";
    console->push_str(console, help);
  } else if(strncmp(command, "quit", sizeof("quit")) == 0) {
    console->push_str(console, "Quitting...\n");
    context->set_is_running(context, false);
  } else if(strncmp(command, "info", sizeof("info")) == 0) {
    char info[4096] = { 0 };
    char * out = info;
    struct rusage usage;

    long int tv_usec = 0, tv_sec = 0;
#ifdef __linux__
    long ru_maxrss = 0;
    long ru_minflt = 0, ru_majflt = 0;
#endif
    if(getrusage(RUSAGE_SELF, &usage) == 0) {
      tv_usec = usage.ru_utime.tv_usec;
      tv_sec = usage.ru_utime.tv_sec;
#ifdef __linux__
      ru_maxrss = usage.ru_maxrss;
      ru_minflt = usage.ru_minflt;
      ru_majflt = usage.ru_majflt;
#endif
    }

#ifdef __APPLE__
    out += snprintf(out, 4096 - (out - info),
        PACKAGE_STRING " :: "
        "Time: %ld.%06ld\n"
        "Logging entries: %zu\n"
        , tv_sec, tv_usec
        , log->get_entries_count(log)
    );
#endif
#ifdef __linux__
    out += snprintf(out, 4096 - (size_t)(out - info),
        PACKAGE_STRING " :: "
        "Time: %ld.%06ld\n"
        "Resident set: %ld\n"
        "Faults: (%ld) %ld\n"
        "Logging entries: %zu\n"
        , tv_sec, tv_usec
        , ru_maxrss
        , ru_minflt, ru_majflt
        , log->get_entries_count(log)
    );
#endif
    console->push_str(console, info);
  } else if(strncmp(command, "log", sizeof("log")) == 0) {
    log->dump(log, console);
  }

  return SP_SUCCESS;
}

static errno_t spooky_parse_args(int argc, char ** argv, spooky_options * options) {
  if(argc <= 1) { goto err0; }
  for(int i = 0; i < argc; i++) {
    if(argv[i][0] == '-') {
      if(i + 1 > argc) { goto err0; }
      switch (argv[i][1]) {
        /* case 'p': { options->gen_primes = true; return SP_SUCCESS; }
        case 'E': { options->exercise_hash = true; return SP_SUCCESS; }
        case 'i': options->ifile = argv[i + 1]; break;
        case 'o': options->ofile = argv[i + 1]; break; */
        case 'L': options->print_licenses = true; break;
        default: goto err0;
      }
    }
  }

  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

void Resize() {
  // TODO: resize the world/window
}

static void spooky_create_tile(spooky_tile * tiles, size_t tiles_len, size_t x, size_t y, size_t z, spooky_tile_type type) {
  assert(tiles_len > 0);

  const spooky_tile * tiles_end = tiles + tiles_len;

  size_t offset = SP_OFFSET(x, y, z);
  tiles[offset] = spooky_global_tiles[type];
  spooky_tile * tile = &(tiles[offset]);
  assert(tile >= tiles && tile < tiles_end);
}

static void spooky_generate_tiles(spooky_tile * tiles, size_t tiles_len) {
  const spooky_tile * tiles_end = tiles + tiles_len; (void)tiles_end;
  /* basic biom layout */
  // unsigned int seed = randombytes_uniform(100);
  for(uint32_t x = 0; x < MAX_TILES_ROW_LEN; ++x) {
    for(uint32_t y = 0; y < MAX_TILES_COL_LEN; ++y) {
      for(uint32_t z = 0; z < MAX_TILES_DEPTH_LEN; ++z) {
        // static const size_t level_ground = MAX_TILES_DEPTH_LEN / 2;

        spooky_tile_type type = STT_EMPTY;

        /* Bedrock is only found on the bottom 4 levels of the ground.
           Bedrock is impassible. */

        /* can't go any deeper than this. */
        if(z == 0 && (type = STT_BEDROCK) == STT_BEDROCK) { goto create_tile; }
        if(z <= 4) {
          static const int max_loops = 5;

          /* generate a random type of rock */
          spooky_tile_type new_type = STT_EMPTY;
          int loops = 0;
          do {
            uint32_t percentage = randombytes_uniform(101);
            new_type = randombytes_uniform((uint32_t)STT_METAMORPHIC + 1);
            assert(new_type >= STT_EMPTY && new_type <= STT_METAMORPHIC);
            /* Bedrock becomes more common the lower we dig.
               The lowest level, 0, is 100% bedrock, with the following
               distribution as we advance upwards:

              Bedrock Distribution Levels:
                - 0 = 100% bedrock
                - 1 = 77% bedrock
                - 2 = 33% bedrock
                - 3 = 17% bedrock
                - 4 = 2% bedrock

              Bedrock rises from the ground, forming simple stalagmites.
              Viewed from the side, we might witness something like:

              Level 5:
              Level 4:        =
              Level 3:        =
              Level 2:      ===
              Level 1:  ========
              Level 0: ==========
            */
            if(z == 1 && percentage <= 77) { new_type = STT_BEDROCK; }
            if(z == 2 && percentage <= 33) { new_type = STT_BEDROCK; }
            if(z == 3 && percentage <= 17) { new_type = STT_BEDROCK; }
            if(z == 4 && percentage <=  2) { new_type = STT_BEDROCK; }
            while(new_type == STT_BEDROCK) {
              /* we only want bedrock if the block below is also bedrock */
              size_t under_offset = SP_OFFSET(x, y, z - 1);
              const spooky_tile * under = &(tiles[under_offset]);
              if(under->type != STT_BEDROCK) {
                new_type = randombytes_uniform((uint32_t)STT_METAMORPHIC + 1);
                assert(new_type >= STT_EMPTY && new_type <= STT_METAMORPHIC);
              } else {
                break;
              }
            }
          } while(++loops < max_loops && new_type <= STT_EMPTY);

          if(new_type > STT_BEDROCK) { new_type = STT_EMPTY; }
          /* clean up type */
          type = new_type;
        }

create_tile:
        spooky_create_tile(tiles, tiles_len, x, y, z, type);
      }
    }
  }
}

const char * spooky_tile_type_as_string(spooky_tile_type type) {
  switch(type) {
    case STT_EMPTY: return "empty";
    case STT_BEDROCK: return "bedrock";
    case STT_CLAY: return "clay";
    case STT_GRAVEL: return "gravel";
    case STT_IGNEOUS: return "igneous";
    case STT_METAMORPHIC: return "metamorphic";
    case STT_SEDIMENTARY: return "sedimentary";
    case STT_SILT: return "silt";
    case STT_WATER: return "water";
    case STT_TREE: return "tree";
    case STT_SAND: return "sand";
    case STT_LOAM: return "loam";
    case STT_EOE:
    default:
      return "error";
  }
}

static const char * spooky_tile_info(const spooky_tile * tile, char * buf, size_t buf_len, int * buf_len_out) {
  const char * type = spooky_tile_type_as_string(tile->type);

  *buf_len_out = snprintf(buf, buf_len, "type: '%s'", type);
  return buf;
}

static void spooky_tile_color(spooky_tile_type type, SDL_Color * color) {
  if(!color) { return; }
  switch(type) {
    case STT_EMPTY:
      *color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = 255 }; /* Black */
      break;
    case STT_BEDROCK:
      *color = (SDL_Color){ .r = 64, .g = 64, .b = 64, .a = 255 }; /* Dark gray */
      break;
    case STT_METAMORPHIC:
      *color = (SDL_Color){ .r = 112, .g = 128, .b = 144, .a = 255 }; /* Slate gray color */
      break;
    case STT_IGNEOUS:
      *color = (SDL_Color){ .r = 255, .g = 253, .b = 208, .a = 255 };/* Cream color */
      break;
    case STT_SEDIMENTARY:
      *color = (SDL_Color){ .r = 213, .g = 194, .b = 165, .a = 255 };/* Sandstone color */
      break;
    case STT_TREE:
      *color = (SDL_Color){ .r = 0, .g = 128, .b = 0, .a = 255 }; /* Green tree */
      break;
    case STT_WATER:
      *color = (SDL_Color){ .r = 0, .g = 0, .b = 255, .a = 255 }; /* Blue water */
      break;
    case STT_SAND:
    case STT_SILT:
    case STT_CLAY:
    case STT_LOAM:
    case STT_GRAVEL:
      *color = (SDL_Color){ .r = 44, .g = 33, .b = 24, .a = 255 }; /* Mud color */
      break;
    case STT_EOE:
    default:
      *color = (SDL_Color){ .r = 255, .g = 0, .b = 0, .a = 255 }; /* Red Error */
      break;
  }
}

static void spooky_render_landscape(SDL_Renderer * renderer, const spooky_context * context, spooky_tile * tiles, size_t tiles_len, const spooky_vector * cursor) {
  static const spooky_box * box0 = NULL;
  static const spooky_base * box = NULL;
  if(!box0 && !box) {
    SDL_Rect box0_rect = { 0 };
    box0 = spooky_box_acquire();
    box0 = box0->ctor(box0, context, box0_rect);
    box = box0->as_base(box0);
  }

  (void)tiles_len;

  box->set_w(box, VOXEL_WIDTH);
  box->set_h(box, VOXEL_HEIGHT);
  box->set_x(box, 0);
  box->set_y(box, 0);

  for(size_t x = 0; x < MAX_TILES_ROW_LEN; x++) {
    for(size_t y = 0; y < MAX_TILES_COL_LEN; y++) {
      SDL_Color block_color = { 0 };
      {
        size_t offset = SP_OFFSET(x, y, cursor->z);
        spooky_tile_type type = tiles[offset].type;
        spooky_tile_color(type, &block_color);
      }

      const spooky_gui_rgba_context * tile_rgba = spooky_gui_push_draw_color(renderer, &block_color);
      {
        box->render(box, renderer);
        {
          const SDL_Color black = { 0, 0, 0, 255 };
          const spooky_gui_rgba_context * outline = spooky_gui_push_draw_color(renderer, &black);
          {

            spooky_box_draw_style style = box0->get_draw_style(box0);
            box0->set_draw_style(box0, SBDS_OUTLINE);
            box->render(box, renderer);
            box0->set_draw_style(box0, style);
            spooky_gui_pop_draw_color(outline);
          }
        }
        spooky_gui_pop_draw_color(tile_rgba);
      }
      box->set_y(box, box->get_y(box) + (int)VOXEL_HEIGHT);
    }

    box->set_x(box, box->get_x(box) + (int)VOXEL_WIDTH);
    box->set_y(box, 0);
  }
}
