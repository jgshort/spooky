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
#include "sp_tiles.h"

static void spooky_render_hud(SDL_Renderer * renderer, const spooky_context * context, const spooky_tiles_manager * tiles_manager);
static void spooky_render_landscape(SDL_Renderer * renderer, const spooky_context * context, const spooky_tiles_manager * tiles_manager, const spooky_vector * cursor);
static errno_t spooky_loop(spooky_context * context, const spooky_ex ** ex);
static errno_t spooky_command_parser(spooky_context * context, const spooky_console * console, const spooky_log * log, const char * command) ;

typedef struct spooky_options {
  bool print_licenses;
  char padding[7];
} spooky_options;

static errno_t spooky_parse_args(int argc, char ** argv, spooky_options * options);

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
  static const double HERTZ = 30.0;
  static const int TARGET_FPS = 60;
  static const int BILLION = 1000000000;

  const int64_t TIME_BETWEEN_UPDATES = (int64_t) (BILLION / HERTZ);
  static const int MAX_UPDATES_BEFORE_RENDER = 5;
  static const int TARGET_TIME_BETWEEN_RENDERS = BILLION / TARGET_FPS;

  const spooky_tiles_manager * tiles_manager = spooky_tiles_manager_acquire();
  tiles_manager = tiles_manager->ctor(tiles_manager, context);

  if(tiles_manager->read_tiles(tiles_manager) != SP_SUCCESS) {
    tiles_manager->generate_tiles(tiles_manager);
    if(tiles_manager->write_tiles(tiles_manager) != SP_SUCCESS) {
      fprintf(stderr, "Unable to persist world :(\n");
    }
  }

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

  // SDL_Texture * sprite_texture = NULL;
  // spooky_gui_load_texture(renderer, "./res/dwarf.png", strlen("./res/dwarf.png"), &sprite_texture);

  // const spooky_sprite * sprite = spooky_sprite_acquire();
  // sprite = sprite->ctor(sprite, sprite_texture);

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
      , ((int)SPOOKY_TILES_VOXEL_WIDTH * SPOOKY_TILES_VISIBLE_VOXELS_WIDTH)
      , ((int)SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT)
      );
  if(!landscape || spooky_is_sdl_error(SDL_GetError())) { abort(); }
  bool update_landscape = true;

  SDL_ClearError();
  SDL_Texture * hud = SDL_CreateTexture(renderer
      , SDL_GetWindowPixelFormat(window)
      , SDL_TEXTUREACCESS_TARGET
      , ((int)SPOOKY_TILES_VOXEL_WIDTH * SPOOKY_TILES_VISIBLE_VOXELS_WIDTH)
      , ((int)SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT)
      );
  if(!hud || spooky_is_sdl_error(SDL_GetError())) { abort(); }
  SDL_SetTextureBlendMode(hud, SDL_BLENDMODE_ADD);
  bool update_hud = true;

  SDL_ClearError();
  SDL_Texture * info = SDL_CreateTexture(renderer
      , SDL_GetWindowPixelFormat(window)
      , SDL_TEXTUREACCESS_TARGET
      , config->get_window_width(config)
      , config->get_window_height(config) - ((int)SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT)
      );
  if(!info || spooky_is_sdl_error(SDL_GetError())) { abort(); }

  log->prepend(log, "Logging enabled\n", SLS_INFO);

  int x_dir = 30, y_dir = 30;
  double interpolation = 0.0;
  int seconds_to_save = 0;

  spooky_vector screen_cursor = { .x = 0, .y = 0, .z = SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 6 };

  SDL_Texture * canvas = context->get_canvas(context);
  /* center rect to center canvas */
  /*
  int canvas_width = config->get_canvas_width(config);
  int canvas_height = config->get_canvas_height(config);
  SDL_Rect center_rect = {
    .x = 1,
    .y = 1,
    .w = canvas_width,
    .h = canvas_height
  };
  */
  SDL_Rect landscape_rect = {
    .x = 1,
    .y = 1,
    .w = ((int)SPOOKY_TILES_VOXEL_WIDTH * SPOOKY_TILES_VISIBLE_VOXELS_WIDTH),
    .h = ((int)SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT)
  };

  SDL_Rect info_rect = {
    .x = 1,
    .y = landscape_rect.h,
    .w = config->get_window_width(config) - 1,
    .h = config->get_window_height(config) - landscape_rect.h
  };

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
                case SDLK_h: /* LEFT */
                  tiles_manager->move_left(tiles_manager);
                  update_landscape = true;
                  break;
                case SDLK_l: /* RIGHT */
                  tiles_manager->move_right(tiles_manager);
                  update_landscape = true;
                  break;
                case SDLK_k: /* UP */
                  tiles_manager->move_up(tiles_manager);
                  update_landscape = true;
                  break;
                case SDLK_j: /* DOWN */
                  tiles_manager->move_down(tiles_manager);
                  update_landscape = true;
                  break;
                case SDLK_COMMA: /* FOWARD */
                  if((SDL_GetModState() & KMOD_SHIFT) != 0) {
                    tiles_manager->move_forward(tiles_manager);
                    update_landscape = true;
                  }
                  break;
                case SDLK_PERIOD: /* BACK */
                  if((SDL_GetModState() & KMOD_SHIFT) != 0) {
                    tiles_manager->move_backward(tiles_manager);
                    update_landscape = true;
                  }
                  break;
                case SDLK_q:
                  /* ctrl-q to quit */
                  if((SDL_GetModState() & KMOD_CTRL) != 0) {
                    spooky_context_set_is_running(context, false);
                    goto end_of_running_loop;
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
                case SDLK_PERIOD:
                  if((SDL_GetModState() & KMOD_SHIFT) == 0) {
                    tiles_manager->set_active_tile(tiles_manager, screen_cursor.x, screen_cursor.y, screen_cursor.z);
                    update_landscape = true;
                  }
                  break;
                case SDLK_x:
                case SDLK_y:
                case SDLK_z:
                case SDLK_EQUALS:
                  {
                    spooky_view_perspective perspective =
                      sym == SDLK_x ? SPOOKY_SVP_X
                        : sym == SDLK_y ? SPOOKY_SVP_Y
                        : SPOOKY_SVP_Z
                        ;
                    tiles_manager->rotate_perspective(tiles_manager, perspective);
                    update_landscape = true;
                    update_hud = true;
                  }
                  break;
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
      spooky_render_landscape(renderer, context, tiles_manager, &screen_cursor);
      SDL_SetRenderTarget(renderer, canvas);
      update_landscape = false;
    }

    { /* Render the landscape */
      if(tiles_manager->get_perspective(tiles_manager) == SPOOKY_SVP_X) {
        /* rotate camera 180 degress */
        SDL_RenderCopyEx(renderer, landscape, NULL, &landscape_rect, 0, NULL, SDL_FLIP_VERTICAL);
      } else {
        SDL_RenderCopy(renderer, landscape, NULL, &landscape_rect);
      }
    }

    if(update_hud) {
      SDL_SetRenderTarget(renderer, hud);
      spooky_render_hud(renderer, context, tiles_manager);
      SDL_SetRenderTarget(renderer, canvas);
      update_hud = false;
    }
    /* Render the landscape overlay (hud): */
    SDL_RenderCopy(renderer, hud, NULL, &landscape_rect);

    { /* Render the borders */
      SDL_Color outline_color = { .r = 255, .g = 255, .b = 0, .a = 255 };
      const spooky_gui_rgba_context * outline_rgba = spooky_gui_push_draw_color(renderer, &outline_color);
      {
        SDL_RenderDrawRect(renderer, &landscape_rect);
        spooky_gui_pop_draw_color(outline_rgba);
      }
    }

    const SDL_Color cursor_color = { 255, 0, 255, 255};
    const spooky_gui_rgba_context * cursor_rgba = spooky_gui_push_draw_color(renderer, &cursor_color);
    {
      SDL_Rect cursor_rect = {
        .x = (int)(screen_cursor.x * SPOOKY_TILES_VOXEL_WIDTH),
        .y = (int)(screen_cursor.y * SPOOKY_TILES_VOXEL_HEIGHT),
        .w = (int)SPOOKY_TILES_VOXEL_WIDTH,
        .h = (int)SPOOKY_TILES_VOXEL_HEIGHT
      };

      if(cursor_rect.x >= (int)(SPOOKY_TILES_VOXEL_WIDTH * SPOOKY_TILES_MAX_TILES_ROW_LEN)) { screen_cursor.x = (SPOOKY_TILES_VOXEL_WIDTH * SPOOKY_TILES_MAX_TILES_ROW_LEN); }
      if(cursor_rect.x < 0) { cursor_rect.x = 0; }

      if(cursor_rect.y >= (int)(SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_MAX_TILES_COL_LEN)) { screen_cursor.y = (SPOOKY_TILES_VOXEL_HEIGHT * SPOOKY_TILES_MAX_TILES_COL_LEN); }
      if(cursor_rect.y < 0) { cursor_rect.y = 0; }

      spooky_box_draw_style style = box0->get_draw_style(box0);
      box0->set_draw_style(box0, SBDS_OUTLINE);
      box->set_rect(box, &cursor_rect, NULL);
      box->render(box, renderer);
      box0->set_draw_style(box0, style);
      spooky_gui_pop_draw_color(cursor_rgba);
    }

    SDL_RenderCopy(renderer, info, NULL, &info_rect);

    {
      SDL_Color outline_color = { .r = 255, .g = 255, .b = 0, .a = 255 };
      const spooky_gui_rgba_context * outline_rgba = spooky_gui_push_draw_color(renderer, &outline_color);
      {
        SDL_RenderDrawRect(renderer, &info_rect);
        spooky_gui_pop_draw_color(outline_rgba);
      }
    }
    /* render bases */
    const spooky_base ** render_iter = first;
    do {
      const spooky_base * obj = *render_iter;
      if(obj->render != NULL) { obj->render(obj, renderer); }
    } while(++render_iter < last);

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, canvas, NULL, NULL /* &center_rect to center */);

    SDL_RenderPresent(renderer);

    last_render_time = now;
    frame_count++;

    if(this_second > last_second_time) {
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

  spooky_tiles_manager_release(tiles_manager);
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

static void spooky_render_landscape(SDL_Renderer * renderer, const spooky_context * context, const spooky_tiles_manager * tiles_manager, const spooky_vector * cursor) {
  static const spooky_box * box = NULL;
  static const spooky_base * box_base = NULL;
  if(!box && !box_base) {
    SDL_Rect box_rect = { 0 };
    box = spooky_box_acquire();
    box = box->ctor(box, context, box_rect);
    box_base = box->as_base(box);
  }

  (void)cursor;

  box_base->set_w(box_base, (int)SPOOKY_TILES_VOXEL_WIDTH);
  box_base->set_h(box_base, (int)SPOOKY_TILES_VOXEL_HEIGHT);
  box_base->set_x(box_base, 0);
  box_base->set_y(box_base, 0);

  SDL_BlendMode old_blend_mode;
  SDL_GetRenderDrawBlendMode(renderer, &old_blend_mode);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  SDL_RenderClear(renderer);

  const spooky_vector_3df * world_pov = tiles_manager->get_world_pov(tiles_manager);
  uint32_t world_z = (uint32_t)floor(world_pov->z);

  uint32_t i = SPOOKY_TILES_MAX_TILES_ROW_LEN;
  while(i--) {
    uint32_t j = SPOOKY_TILES_MAX_TILES_COL_LEN;
    while(j--) {
      const spooky_tile * tile = tiles_manager->get_tile(tiles_manager, i, j, world_z);
      assert(tile && tile->meta);
      spooky_tiles_tile_type type = tile->meta->type;
      SDL_Color block_color = { 0 };
      spooky_tiles_get_tile_color(type, &block_color);

      /*
      TODO: Implement 'hidden' blocks above current depth
      if(tiles_manager->get_perspective(tiles_manager) == SPOOKY_SVP_Z && world_z + 1 < world_z) {
        uint32_t over_offset = world_z + 1;
        if(tiles_manager->get_tile(tiles_manager, i, j, over_offset) != NULL) {
          block_color = (SDL_Color){ 0 };
        }
      }
      */

      const spooky_gui_rgba_context * tile_rgba = spooky_gui_push_draw_color(renderer, &block_color);
      {
        box_base->render(box_base, renderer);

        uint32_t depth = 4;
        uint32_t under_offset = world_z + depth;
        do {
          const spooky_tile * under = NULL;
          under = tiles_manager->get_tile(tiles_manager, i, j, under_offset);
          if(tile->meta->type == STT_EMPTY && under != NULL && under->meta->type != STT_EMPTY) {
            /* try to draw the tile under the current tile if it's not empty */
            SDL_Color under_color = { 0 };
            spooky_tiles_get_tile_color(under->meta->type, &under_color);
            bool render_under = true;
            switch(depth) {
              case 1: spooky_gui_color_darken(&under_color, 20); break;
              case 2: spooky_gui_color_darken(&under_color, 35); break;
              case 3: spooky_gui_color_darken(&under_color, 50); break;
              case 4: spooky_gui_color_darken(&under_color, 65); break;
              default: render_under = false; break;
            }
            if(render_under) {
              const spooky_gui_rgba_context * under_rgba = spooky_gui_push_draw_color(renderer, &under_color);
              {
                box_base->render(box_base, renderer);
                spooky_gui_pop_draw_color(under_rgba);
              }
            }
          }
          depth--;
          under_offset = world_z + depth;
        } while(depth > 0);

        {
          SDL_Color black = { 0, 0, 0, 255 };
          const spooky_gui_rgba_context * outline = spooky_gui_push_draw_color(renderer, &black);
          {
            spooky_box_draw_style style = box->get_draw_style(box);
            box->set_draw_style(box, SBDS_OUTLINE);
            box_base->render(box_base, renderer);
            box->set_draw_style(box, style);
            spooky_gui_pop_draw_color(outline);
          }

          const spooky_tile * active_tile = tiles_manager->get_active_tile(tiles_manager);
          {
            /* highlight active tile */
            if(active_tile && tile == active_tile) {
              SDL_Color highlight_color = { .r = 255, .g = 0, .b = 0, .a = 255 };
              const spooky_gui_rgba_context * highlight = spooky_gui_push_draw_color(renderer, &highlight_color);
              {
                spooky_box_draw_style style = box->get_draw_style(box);
                box->set_draw_style(box, SBDS_FILL);
                box_base->render(box_base, renderer);
                box->set_draw_style(box, style);
                spooky_gui_pop_draw_color(highlight);
              }
            }
          }
        }
        spooky_gui_pop_draw_color(tile_rgba);
      }

      box_base->set_y(box_base, box_base->get_y(box_base) + (int)SPOOKY_TILES_VOXEL_HEIGHT);
    }

    box_base->set_x(box_base, box_base->get_x(box_base) + (int)SPOOKY_TILES_VOXEL_WIDTH);
    box_base->set_y(box_base, 0);
  }

  SDL_SetRenderDrawBlendMode(renderer, old_blend_mode);
}

static void spooky_render_hud(SDL_Renderer * renderer, const spooky_context * context, const spooky_tiles_manager * tiles_manager) {
  static const SDL_Color color = { 255, 255, 255, 255 };

  int w = -1, h, axis_height, axis_width;
  const spooky_font * font = context->get_font(context);
  SDL_GetRendererOutputSize(renderer, &w, &h);

  SDL_RenderClear(renderer);

  const char * axis = NULL;
  switch(tiles_manager->get_perspective(tiles_manager)) {
    case SPOOKY_SVP_X: axis = "x"; break;
    case SPOOKY_SVP_Y: axis = "y"; break;
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default: axis = "z"; break;
  }
  font->measure_text(font, axis, 1, &axis_width, &axis_height);
  SDL_Point dest = { axis_width - 4, (h - axis_height) - ((int)SPOOKY_TILES_VOXEL_HEIGHT / 2) + 1 };

  bool is_drop_shadow = font->get_is_drop_shadow(font);
  font->set_is_drop_shadow(font, true);
  font->write_to_renderer(font, renderer, &dest, &color, axis, 1, NULL, NULL);
  font->set_is_drop_shadow(font, is_drop_shadow);
}
