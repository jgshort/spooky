#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>

#include "config.h"
#include "sp_error.h"
#include "sp_math.h"
#include "sp_pak.h"
#include "sp_gui.h"
#include "sp_font.h"
#include "sp_base.h"
#include "sp_console.h"
#include "sp_debug.h"
#include "sp_help.h"
#include "sp_context.h"
#include "sp_time.h"

static errno_t spooky_loop(spooky_context * context);

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  spooky_pack_tests();
  
  spooky_context context = { 0 };

  if(spooky_init_context(&context) != SP_SUCCESS) { goto err0; }
  if(spooky_test_resources(&context) != SP_SUCCESS) { goto err0; }
  if(spooky_loop(&context) != SP_SUCCESS) { goto err1; }
  if(spooky_quit_context(&context) != SP_SUCCESS) { goto err2; }

  fprintf(stdout, "\nThank you for playing! Happy gaming!\n");
  fflush(stdout);
  return SP_SUCCESS;

err2:
  fprintf(stderr, "A fatal error occurred during shutdown.\n");
  goto err;

err1:
  fprintf(stderr, "A fatal error occurred in the main loop.\n");
  spooky_release_context(&context);
  goto err;

err0:
  fprintf(stderr, "A fatal error occurred during initialization.\n");
  goto err;

err:
  return SP_FAILURE;
}

errno_t spooky_loop(spooky_context * context) {
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

  SDL_Window * window = context->get_window(context);
  SDL_Renderer * renderer = context->get_renderer(context);

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

  bool running = true;

  const spooky_base * objects[4] = { 0 };
  const spooky_base ** first = objects;
  const spooky_base ** last = objects + ((sizeof objects / sizeof * objects) - 1);

  const spooky_console * console = spooky_console_acquire();
  console = console->ctor(console, renderer);

  const spooky_debug * debug = spooky_debug_acquire();
  debug = debug->ctor(debug, context);

  const spooky_help * help = spooky_help_acquire();
  help = help->ctor(help, context);

  objects[0] = (const spooky_base *)console;
  objects[1] = (const spooky_base *)debug;
  objects[2] = (const spooky_base *)help;

  objects[2]->set_z_order(objects[2], 99999);

  spooky_base_z_sort(objects, (sizeof objects / sizeof * objects) - 1);
  
  double interpolation = 0.0;
  bool is_done = false, is_up = false, is_down = false;

  while(running) {
    int update_loops = 0;
    now = sp_get_time_in_us();
    while((now - last_update_time > TIME_BETWEEN_UPDATES && update_loops < MAX_UPDATES_BEFORE_RENDER)) {
      if(spooky_is_sdl_error(SDL_GetError())) {
        fprintf(stderr, "%s\n", SDL_GetError());
      }

      SDL_Event evt = { 0 };
      SDL_ClearError();
      while(SDL_PollEvent(&evt)) {
        if(spooky_is_sdl_error(SDL_GetError())) { fprintf(stderr, "> %s\n", SDL_GetError()); }
        
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
          
          /* TODO: Update console width on window resize */
          // SDL_GetRendererOutputSize(context->renderer, &w, &h);
          // console.rect.w = w - 200;
        
          /*
          const spooky_font * font = context->get_font(context);
          int new_point_size = font->get_point_size(font);
          spooky_context_scale_font(context, new_point_size);
          */
        }

        switch(evt.type) {
          case SDL_QUIT:
            running = false;
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
                    spooky_context_scale_font_up(context, &is_done);
                    is_up = true;
                    is_down = false;
                  }
                  break;
                case SDLK_MINUS:
                  {
                    spooky_context_scale_font_down(context, &is_done);
                    is_down = true;
                    is_up = false;
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
                      running = false;
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
      }

      /* handle base events */
      const spooky_base ** event_iter = first;
      do {
        const spooky_base * obj = *event_iter;
        if(obj->handle_event != NULL) { obj->handle_event(obj, &evt); }
      } while(++event_iter < last);

      last_update_time += TIME_BETWEEN_UPDATES;
      update_loops++;
    } /* >> while ((now - last_update_time ... */

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

    interpolation = fmin(1.0f, (double)(now - last_update_time) / (double)(TIME_BETWEEN_UPDATES));

    /* handle base deltas */
    const spooky_base ** delta_iter = first;
    do {
      const spooky_base * obj = *delta_iter;
      if(obj->handle_delta != NULL) { obj->handle_delta(obj, interpolation); }
    } while(++delta_iter < last);

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
  
    SDL_RenderCopy(renderer, background, NULL, NULL);
    
    /* render bases */
    const spooky_base ** render_iter = first;
    do {
      const spooky_base * obj = *render_iter;
      if(obj->render != NULL) { obj->render(obj, renderer); }
    } while(++render_iter < last);

    //SDL_SetRenderDrawColor(renderer, c0.r, c0.g, c0.b, c0.a);
    SDL_RenderPresent(renderer);

    last_render_time = now;
    frame_count++;

    if(this_second > last_second_time) {
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
  } /* >> while(running) */

  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

  spooky_console_release(console);
  
  return SP_SUCCESS;

err1:
  if(letterbox_background != NULL) { SDL_DestroyTexture(letterbox_background), letterbox_background = NULL; }

err0:
  if(background != NULL) { SDL_DestroyTexture(background), background = NULL; }

  return SP_FAILURE;
}


