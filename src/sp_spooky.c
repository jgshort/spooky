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

#include "../config.h"
#include "../include/sp_types.h"
#include "../include/sp_str.h"
#include "../include/sp_error.h"
#include "../include/sp_math.h"
#include "../include/sp_hash.h"
#include "../include/sp_pak.h"
#include "../include/sp_gui.h"
#include "../include/sp_font.h"
#include "../include/sp_base.h"
#include "../include/sp_console.h"
#include "../include/sp_debug.h"
#include "../include/sp_help.h"
#include "../include/sp_context.h"
#include "../include/sp_menu.h"
#include "../include/sp_wm.h"
#include "../include/sp_log.h"
#include "../include/sp_limits.h"
#include "../include/sp_time.h"
#include "../include/sp_db.h"
#include "../include/sp_box.h"
#include "../include/sp_config.h"
#include "../include/sp_text.h"

static errno_t spooky_loop(spooky_context * context, const spooky_ex ** ex);
static errno_t spooky_command_parser(spooky_context * context, const spooky_console * console, const char * command) ;
static void spooky_print_licenses(const spooky_hash_table * hash);
static FILE * spooky_open_pak_file(char ** argv);

typedef struct spooky_options {
  bool print_licenses;
  char padding[7];
} spooky_options;

static errno_t spooky_parse_args(int argc, char ** argv, spooky_options * options);

int main(int argc, char **argv) {
  spooky_options options = { 0 };

  spooky_parse_args(argc, argv, &options);

#ifdef DEBUG
  spooky_pack_tests();
#endif

  FILE * fp = spooky_open_pak_file(argv);

  spooky_context context = { 0 };
  const spooky_ex * ex = NULL;

  spooky_log_startup();
  SP_LOG(SLS_INFO, "Logging enabled.\n");

  if(spooky_init_context(&context, fp) != SP_SUCCESS) { goto err0; }
  if(spooky_test_resources(&context) != SP_SUCCESS) { goto err0; }

  const spooky_hash_table * hash = context.get_hash(&context);

  fclose(fp);

  {
#ifdef DEBUG
    /* Loading a font from the resource pak example */
    void * temp = NULL;
    spooky_pack_item_file * font = NULL;
    hash->find(hash, "pr.number", strnlen("pr.number", SPOOKY_MAX_STRING_LEN), &temp);
    font = (spooky_pack_item_file *)temp;

    SDL_RWops * src = SDL_RWFromMem(font->data, (int)font->data_len);
    assert(src);
    TTF_Init();
    TTF_Font * ttf = TTF_OpenFontRW(src, 0, 10);

    assert(ttf);
    fprintf(stdout, "Okay!\n");
#endif
  }

  if(options.print_licenses) {
    spooky_print_licenses(hash);
  }

#ifdef DEBUG
  /* Print out the pak file resources: */
  fseek(fp, 0, SEEK_SET);
  spooky_pack_print_resources(stdout, fp);
#endif

  if(spooky_loop(&context, &ex) != SP_SUCCESS) { goto err1; }
  if(spooky_quit_context(&context) != SP_SUCCESS) { goto err2; }

  spooky_log_shutdown();

  fprintf(stdout, "\nThank you for playing! Happy gaming!\n");
  fflush(stdout);
  return SP_SUCCESS;

err2:
  fprintf(stderr, "A fatal error occurred during shutdown.\n");
  goto err;

err1:
  fprintf(stderr, "A fatal error occurred in the main loop.\n");
  if(ex) {
    fprintf(stderr, "%s\n", ex->msg);
  }
  spooky_release_context(&context);
  goto err;

err0:
  fprintf(stderr, "A fatal error occurred during initialization.\n");
  goto err;

err:
  if(fp) { fclose(fp); }
  return SP_FAILURE;
}

errno_t spooky_loop(spooky_context * context, const spooky_ex ** ex) {
  static const double SP_HERTZ = 30.0;
  static const unsigned int SP_TARGET_FPS = 60;
  static const unsigned int SP_MILLI = 1000;

  const int64_t SP_TIME_BETWEEN_UPDATES = (int64_t) (SP_MILLI / SP_HERTZ);
  static const int SP_MAX_UPDATES_BEFORE_RENDER = 5;
  static const int SP_TARGET_TIME_BETWEEN_RENDERS = SP_MILLI / SP_TARGET_FPS;

  uint64_t now = 0;
  uint64_t last_render_time = sp_get_time_in_ms();
  uint64_t last_update_time = sp_get_time_in_ms();

  int64_t frame_count = 0,
          fps = 0,
          seconds_since_start = 0;

  uint64_t last_second_time = (uint64_t) (last_update_time / SP_MILLI);

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

  SDL_Texture * canvas = context->get_canvas(context);
  SDL_SetRenderTarget(renderer, context->get_canvas(context));

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
  wm = wm->ctor(wm, "wm", context);

  const spooky_console * console = spooky_console_acquire();
  console = console->ctor(console, "console", context, renderer);
  console->as_base(console)->set_z_order(console->as_base(console), 9999999);

  console->push_str(console, "> " PACKAGE_NAME " " PACKAGE_VERSION " <\n");
  const spooky_debug * debug = spooky_debug_acquire();
  debug = debug->ctor(debug, "debug", context);

  const spooky_help * help = spooky_help_acquire();
  help = help->ctor(help, "help", context);
  help->as_base(help)->set_z_order(help->as_base(help), 9999998);

  SDL_Rect main_menu_rect = { .x = 0, .y = 0, .w = spooky_gui_window_default_logical_width, .h = 24 };
  const spooky_menu * main_menu = spooky_menu_load_from_file(context, main_menu_rect, "res/menus.mnudef");

  const spooky_text * text = spooky_text_acquire();
  text = text->ctor(text, "text", context, renderer);
  text->as_base(text)->set_is_modal(text->as_base(text), true);
  text->as_base(text)->set_z_order(text->as_base(text), 9999997);

  const spooky_base * objects[3] = { 0 };
  const spooky_base ** first = objects;
  const spooky_base ** last = objects + ((sizeof objects / sizeof objects[0]));

  objects[0] = console->as_base(console);
  objects[1] = help->as_base(help);
  objects[2] = text->as_base(text);

  spooky_base_z_sort(objects, (sizeof objects / sizeof objects[0]));

  if(debug->as_base(debug)->add_child(debug->as_base(debug), help->as_base(help), ex) != SP_SUCCESS) { goto err1; }

  int temp_w, temp_h;
  SDL_GetWindowSize(window, &temp_w, &temp_h);
  SDL_Rect box0_rect = { .x = 100, .y = 100, .w = 100, .h = 100 };
  const spooky_box * box0 = spooky_box_acquire();
  box0 = box0->ctor(box0, "box0", context, box0_rect);
  box0->set_draw_style(box0, SBDS_FILL);
  wm->register_window(wm, box0->as_base(box0));

  int x_dir = 30, y_dir = 30;
  double interpolation = 0.0;
  int seconds_to_save = 0;

  SDL_Event evt = { 0 };

  const spooky_base * wm_base = wm->as_base(wm);
  const spooky_base * menu_base = main_menu->as_base(main_menu);

  while(spooky_context_get_is_running(context)) {
    SDL_SetRenderTarget(renderer, context->get_canvas(context));

    SDL_Rect debug_rect = { 0 };
    int update_loops = 0;
    now = sp_get_time_in_ms();

    while((now - last_update_time > SP_TIME_BETWEEN_UPDATES && update_loops < SP_MAX_UPDATES_BEFORE_RENDER)) {
      if(spooky_is_sdl_error(SDL_GetError())) { SP_LOG(SLS_INFO, "Uncaught SDL error '%s'.", SDL_GetError()); }

      SDL_ClearError();
      while(SDL_PollEvent(&evt) > 0) {
        if(spooky_is_sdl_error(SDL_GetError())) { SP_LOG(SLS_INFO, "%s", SDL_GetError()); }

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
                case SDLK_EQUALS:
                  {
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      context->scale_font_up();
                    }
                  }
                  break;
                case SDLK_MINUS:
                  {
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      context->scale_font_down();
                    }
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
                default: ;
                         break;
              } /* >> switch(sym ... */
            }
            break;
          default: ;
                   break;
        } /* >> switch(evt.type ... */

        /* handle menu events */
        menu_base->handle_event(menu_base, &evt);

        /* handle debug/HUD events */
        spooky_debug_handle_event(debug->as_base(debug), &evt);

        /* handle base events */
        if(context->get_modal(context) != NULL) {
          // handle events from the modal; ignore everyone else...
          const spooky_base * modal = context->get_modal(context);
          if(modal && modal->handle_event != NULL) {
            if(modal->handle_event(modal, &evt)) {
              goto break_events;
            }
          }
        } else {
          /* No modal? Iterate registered objects... */
          const spooky_base ** event_iter = last - 1;
          do {
            const spooky_base * obj = *event_iter;
            if(obj != NULL && obj->handle_event != NULL) {
              if(obj->handle_event(obj, &evt)) {
                goto break_events;
              }
            }
          } while((--event_iter) >= first);
        } /* context->get_modal()... */

        /* handle window manager object events... */
        if(wm_base && wm_base->handle_event) {
          wm_base->handle_event(wm_base, &evt);
        }
      } /* >> while(SDL_PollEvent(&evt)) */

break_events:
      {
        /* check the console command; execute it if it exists */
        const char * command = NULL;
        const spooky_base * console_base = console->as_base(console);
        if(console_base->get_focus(console_base) && (command = console->get_current_command(console)) != NULL) {
          spooky_command_parser(context, console, command);
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

      /* Currently, the debug/HUD display doesn't need to handle deltas. */

      /* Handle deltas for all other base objects... */
      const spooky_base ** delta_iter = first;
      do {
        const spooky_base * obj = *delta_iter;
        if(obj->handle_delta != NULL) {
          obj->handle_delta(obj, &evt, last_update_time, interpolation);
        }
      } while(++delta_iter < last);

      /* handle window manager object deltas... */
      wm_base->handle_delta(wm_base, &evt, last_update_time, interpolation);

      /* handle main menu deltas... */
      // menu_base->handle_delta(menu_base, &evt, last_update_time, interpolation);

      last_update_time += (unsigned long int)SP_TIME_BETWEEN_UPDATES;
      update_loops++;

    } /* >> while ((now - last_update_time ... */

    interpolation = fmin(1.0f, (double)(now - last_update_time) / (double)(SP_TIME_BETWEEN_UPDATES));
    if (now - last_update_time > SP_TIME_BETWEEN_UPDATES) {
      last_update_time = now - (unsigned long int)SP_TIME_BETWEEN_UPDATES;
    }

    uint64_t this_second = (uint64_t)(last_update_time / SP_MILLI);

    {
      const SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
      const spooky_gui_rgba_context * rgba = spooky_gui_push_draw_color(renderer, &c);
      {
        SDL_RenderFillRect(renderer, NULL); /* screen color */
        spooky_gui_pop_draw_color(rgba);
      }
    }

    /* render bases */
    const spooky_base ** render_iter = first;
    do {
      const spooky_base * obj = *render_iter;
      if(obj->render != NULL) { obj->render(obj, renderer); }
    } while(++render_iter < last);

    /* render window manager objects... */
    wm_base->render(wm_base, renderer);

    /* render main menu... */
    menu_base->render(menu_base, renderer);

    /* The debug/HUD is always on top... */
    spooky_debug_render(debug->as_base(debug), renderer);

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, canvas, NULL, NULL);

    SDL_RenderPresent(renderer);

    last_render_time = now;
    frame_count++;

    if(this_second > last_second_time) {
      seconds_to_save++;
      if(seconds_to_save >= 300) {
        /* Autosave every 5 minutes */
        SP_LOG(SLS_INFO, "Autosaving...");
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
    while (now - last_render_time < SP_TARGET_TIME_BETWEEN_RENDERS && now - last_update_time < SP_TIME_BETWEEN_UPDATES) {
      sp_sleep(0); /* Needed? */
      now = sp_get_time_in_ms();
    }
end_of_running_loop: ;
  } /* >> while(spooky_context_get_is_running(context)) */

  if(db->close(db) != 0) {
    fprintf(stderr, "Unable to close save game storage; something broke but this is not catastrophic.\n");
  };

  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

  spooky_text_release(text);
  spooky_help_release(help);
  spooky_console_release(console);
  spooky_wm_release(wm);
  spooky_db_release(db);

  return SP_SUCCESS;

err1:
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

err0:
  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }

  return SP_FAILURE;
}

errno_t spooky_command_parser(spooky_context * context, const spooky_console * console, const char * command) {

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
        , spooky_log_get_global_entries_count()
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
        , spooky_log_get_global_entries_count()
        );
#endif
    console->push_str(console, info);
  } else if(strncmp(command, "log", sizeof("log")) == 0) {
    spooky_log_dump_to_console(console);
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

static FILE * spooky_open_pak_file(char ** argv) {
  FILE * fp = NULL;

  long pak_offset = 0;
  uint64_t content_offset = 0,
           content_len = 0,
           index_entries = 0,
           index_offset = 0,
           index_len = 0
             ;
  const char * self_exec = argv[0];
  { /* check if we're a bundled exec + pak file */
    int fd = open(self_exec, O_RDONLY | O_EXCL, S_IRUSR | S_IWUSR);
    if(fd >= 0) {
      fp = fdopen(fd, "rb");
      if(fp) {
        errno_t is_valid = spooky_pack_is_valid_pak_file(fp, &pak_offset, &content_offset, &content_len, &index_entries, &index_offset, &index_len);
        if(is_valid != SP_SUCCESS) {
          /* not a valid bundle */
          fclose(fp), fp = NULL;
        }
      } else { fprintf(stderr, "Unable to open file %s\n", self_exec); }
    } else { fprintf(stderr, "Unable to open file %s\n", self_exec); }
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

#ifdef DEBUG
  fprintf(stdout,
      "SPDB Stats: Content Offset:  %" PRIu64 ", "
      "Content Len: %" PRIu64 ", "
      "Index Entries: %" PRIu64 ", "
      "Index Offset:  %" PRIu64 ", "
      "Index Len: %" PRIu64 "\n",
      content_offset,
      content_len,
      index_entries,
      index_offset,
      index_len
      );
#endif

  return fp;
}

static void spooky_print_licenses(const spooky_hash_table * hash) {
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

