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
  const double HERTZ = 30.0;
  const int TARGET_FPS = 60;
  const int BILLION = 1000000000;

  const int64_t TIME_BETWEEN_UPDATES = (int64_t) (BILLION / HERTZ);
  const int MAX_UPDATES_BEFORE_RENDER = 5;
  const int TARGET_TIME_BETWEEN_RENDERS = BILLION / TARGET_FPS;

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
  if(spooky_load_texture(renderer, "./res/bg3.png", 13, &background) != SP_SUCCESS) { goto err0; }
  if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }

  SDL_Texture * letterbox_background = NULL;
  SDL_ClearError();
  if(spooky_load_texture(renderer, "./res/bg4.png", 13, &letterbox_background) != SP_SUCCESS) { goto err1; }
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

  bool is_done = false, is_up = false, is_down = false;

  if(((const spooky_base *)debug)->add_child((const spooky_base *)debug, (const spooky_base *)help, ex) != SP_SUCCESS) { goto err1; }

  log->prepend(log, "Logging enabled\n", SLS_INFO);
  int x_dir = 30, y_dir = 30;
  double interpolation = 0.0;
  int seconds_to_save = 0;
  while(spooky_context_get_is_running(context)) {
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
          /* Only happens when clicking About in OS X Mojave */
          || evt.window.event == SDL_WINDOWEVENT_FOCUS_LOST
        ))
#elif __unix__
        if (evt.type == SDL_WINDOWEVENT && (
          evt.window.event == SDL_WINDOWEVENT_RESIZED
        ))
#endif
        {
          int w = 0, h = 0;
          SDL_GetWindowSize(window, &w, &h);
          assert(w > 0 && h > 0);
          context->set_window_width(context, w);
          context->set_window_height(context, h);
        }

        switch(evt.type) {
          case SDL_QUIT:
            spooky_context_set_is_running(context, false);
            goto end_of_running_loop;
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
                case SDLK_EQUALS:
                  {
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      spooky_context_scale_font_up(context, &is_done);
                      is_up = true;
                      is_down = false;
                    }
                  }
                  break;
                case SDLK_MINUS:
                  {
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      spooky_context_scale_font_down(context, &is_done);
                      is_down = true;
                      is_up = false;
                    }
                  }
                  break;
                case SDLK_1:
                  {
                    if((SDL_GetModState() & KMOD_CTRL) != 0) {
                      context->next_font_type(context);
                    }
                  }
                  break;
                default:
                  break;
              } /* >> switch(sym ... */
            }
            break;
          case SDL_KEYDOWN:
            {
              SDL_Keycode sym = evt.key.keysym.sym;
              switch(sym) {
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
          default:
            break;
        } /* >> switch(evt.type ... */

        /* handle base events */
        const spooky_base ** event_iter = last - 1;
        do {
          const spooky_base * obj = *event_iter;
          if(obj != NULL && obj->handle_event != NULL) {
            if(obj->handle_event(obj, &evt)) {
              goto render_pipeline;
            }
          }
        } while(--event_iter >= first);
      } /* >> while(SDL_PollEvent(&evt)) */

      {
        /* check the console command; execute it if it exists */
        const char * command;
        if((command = console->get_current_command(console)) != NULL) {
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

      //bouncing console... for reasons:
      /*
      const SDL_Rect * r = ((const spooky_base *)debug)->get_rect((const spooky_base *)debug);
      SDL_Rect rr = { .x = r->x, .y =r->y, .w = r->w, .h = r->h };
      rr.x += x_dir * (int)floor((double)5 * interpolation);
      rr.y += y_dir * (int)floor((double)5 * interpolation);
      if(((const spooky_base *)debug)->set_rect((const spooky_base *)debug, &rr, ex) != SP_SUCCESS) { goto err1; }
      */
      last_update_time += TIME_BETWEEN_UPDATES;
      update_loops++;

      /* handle base deltas */
    } /* >> while ((now - last_update_time ... */

render_pipeline:
    interpolation = fmin(1.0f, (double)(now - last_update_time) / (double)(TIME_BETWEEN_UPDATES));
    if (now - last_update_time > TIME_BETWEEN_UPDATES) {
      last_update_time = now - TIME_BETWEEN_UPDATES;
    }

    if(!is_done) {
      if(is_up) {
        spooky_context_scale_font_up(context, &is_done);
      }
      else if(is_down) {
        spooky_context_scale_font_down(context, &is_done);
      }
    }

    uint64_t this_second = (uint64_t)(last_update_time / BILLION);

    {
      SDL_Color saved_color = { 0 };
      SDL_GetRenderDrawColor(renderer, &saved_color.r, &saved_color.g, &saved_color.b, &saved_color.a);
      /** If not rendering non-scaled background below, render the letterbox color instead:
      *** const SDL_Color bg = { .r = 20, .g = 1, .b = 36, .a = 255 };
      *** SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
      *** SDL_RenderClear(renderer); // letterbox color
      */
      const SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
      SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(renderer, NULL); /* screen color */
      SDL_SetRenderDrawColor(renderer, saved_color.r, saved_color.g, saved_color.b, saved_color.a);
    }

    /* Placeholder background */
    /* SDL_RenderCopy(renderer, background, NULL, NULL); */

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
      char buf[80] = { 0 };
      snprintf(buf, sizeof buf, "Delta: %f, FPS: %i", interpolation, (int)fps);

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
