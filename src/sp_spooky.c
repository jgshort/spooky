#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "config.h"
#include "sp_error.h"
#include "sp_gui.h"
#include "sp_time.h"

typedef struct sp_game_context {
  SDL_Window * window;
  SDL_Renderer * renderer;
  SDL_GLContext glContext;

  float scale_factor_x;
  float scale_factor_y;

  int logical_width;
  int logical_height;
} sp_game_context;

static errno_t spooky_init_game_context(sp_game_context * context);
static errno_t spooky_quit_game_context(sp_game_context * context);
static errno_t spooky_test_game_resources(SDL_Renderer * renderer);
static errno_t spooky_loop(sp_game_context * context);

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  sp_game_context context = { 0 };

  if(spooky_init_game_context(&context) != SP_SUCCESS) { goto err0; }
  if(spooky_test_game_resources(context.renderer) != SP_SUCCESS) { goto err0; }

  spooky_loop(&context);

  /* clean up */
  if(spooky_quit_game_context(&context) != SP_SUCCESS) { goto err1; }

  fprintf(stdout, "\nThank you for playing! Happy gaming!\n");
  return SP_SUCCESS;

err1:
  fprintf(stderr, "A fatal error occurred on shutdown.\n");
  goto err;

err0:
  fprintf(stderr, "A fatal error occurred on initialization.\n");
  goto err;

err:
  return SP_FAILURE;
}

errno_t spooky_init_game_context(sp_game_context * context) {
  assert(!(context == NULL));

  if(context == NULL) { return SP_FAILURE; }

  fprintf(stdout, "Initializing...");
  /* allow high-DPI windows */
  if(!SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0")) { goto err0; }

  if(SDL_Init(SDL_INIT_VIDEO) != 0) { goto err1; }
  if(TTF_Init() != 0) { goto err2; };

  if(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) { goto err3; }
  if(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) != 0) { goto err3; }
  if(SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) != 0) { goto err3; }
  if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) != 0) { goto err3; }
  if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2) != 0) { goto err3; }

  bool spooky_gui_is_fullscreen = false;
  uint32_t window_flags = 
    spooky_gui_is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
    | SDL_WINDOW_OPENGL
    | SDL_WINDOW_HIDDEN
    | SDL_WINDOW_ALLOW_HIGHDPI
    | SDL_WINDOW_RESIZABLE
    ;

  int window_width = spooky_window_default_width;
  int window_height = spooky_window_default_height;

  SDL_ClearError();
  SDL_Window * window = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags);
  if(window == NULL) { goto err4; }

  uint32_t renderer_flags =
    SDL_RENDERER_ACCELERATED
    | SDL_RENDERER_PRESENTVSYNC
    ;
  SDL_ClearError();
  SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, renderer_flags);
  if(renderer == NULL) { goto err5; }

  SDL_ClearError();
  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if(glContext == NULL) { goto err6; }

  context->scale_factor_x = 1.0f;
  context->scale_factor_y = 1.0f;
  context->logical_width = 320;
  context->logical_height = 200;

  SDL_RenderSetLogicalSize(renderer, context->logical_width, context->logical_height);
  SDL_RenderSetScale(renderer, context->scale_factor_x, context->scale_factor_y);

  context->renderer = renderer;
  context->window = window;
  context->glContext = glContext;

  fprintf(stdout, " Done!\n");

  return SP_SUCCESS;

err6:
  /* unable to create GL context */
  SDL_DestroyRenderer(renderer), renderer = NULL;

err5:
  /* unable to create SDL renderer */
  SDL_DestroyWindow(window), window = NULL;

err4:
  /* unable to create SDL window */

err3:
  /* unable to set GL attributes */
  TTF_Quit();

err2:
  /* unable to initialize TTF */
  SDL_Quit();

err1:
  /* unable to initialize SDL */

err0:
  /* unable to enable high DPI hint */
  goto err;

err:
  return SP_FAILURE;
}

errno_t spooky_quit_game_context(sp_game_context * context) {
  assert(!(context == NULL));
  if(context == NULL) { return SP_FAILURE; }

  fprintf(stdout, "Shutting down...");

  if(context->glContext != NULL) {
    SDL_ClearError();
    SDL_GL_DeleteContext(context->glContext), context->glContext = NULL;
    const char * error = SDL_GetError();
    if(spooky_is_sdl_error(error)) {
      fprintf(stderr, "Non-fatal error: Unable to release GL context, '%s'.\n", error);
    }
  }

  if(context->renderer != NULL) {
    SDL_ClearError();
    SDL_DestroyRenderer(context->renderer), context->renderer = NULL;
    const char * error = SDL_GetError();
    if(spooky_is_sdl_error(error)) {
      fprintf(stderr, "Non-fatal error: Unable to destroy renderer, '%s'.\n", error);
    }
  }

  if(context->window != NULL) {
    SDL_ClearError();
    SDL_DestroyWindow(context->window), context->window = NULL;
    const char * error = SDL_GetError();
    if(spooky_is_sdl_error(error)) {
      fprintf(stderr, "Non-fatal error: Unable to destroy window, '%s'.\n", error);
    }
  }

  TTF_Quit();
  SDL_Quit();
  fprintf(stdout, " Done!\n");

  return SP_SUCCESS;
}

errno_t spooky_test_game_resources(SDL_Renderer * renderer) {
  assert(!(renderer == NULL));
  if(renderer == NULL) { goto err0; }

  fprintf(stdout, "Testing resources...");

  /* check surface resource path and method */
  SDL_Surface * test_surface = NULL;
  if(spooky_load_image("res/test0.png", 13, &test_surface) != SP_SUCCESS) { goto err0; }
  if(test_surface == NULL) goto err0;
  SDL_FreeSurface(test_surface), test_surface = NULL;

  /* check texture method */
  SDL_Texture * test_texture = NULL;
  if(spooky_load_texture(renderer, "res/test0.png", 13, &test_texture) != SP_SUCCESS) { goto err0; }
  if(test_texture == NULL) { goto err0; }
  SDL_DestroyTexture(test_texture), test_texture = NULL;

  fprintf(stdout, " Done!\n");
  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

errno_t spooky_loop(sp_game_context * context) {
  const double HERTZ = 30.0;
  const int TARGET_FPS = 60;
  const int BILLION = 1000000000;

  const int64_t TIME_BETWEEN_UPDATES = (int64_t) (BILLION / HERTZ);
  const int MAX_UPDATES_BEFORE_RENDER = 5;
  const int TARGET_TIME_BETWEEN_RENDERS = BILLION / TARGET_FPS;

  int64_t now;
  int64_t last_render_time = sp_get_time_in_us();
  int64_t last_update_time = sp_get_time_in_us();

  int64_t frame_count = 0,
          fps = 0,
          seconds_since_start = 0;

  uint64_t last_second_time = (uint64_t) (last_update_time / BILLION);

  SDL_Window * window = context->window;
  SDL_Renderer * renderer = context->renderer;

#ifdef __APPLE__ 
  /* On OS X Mojave, screen will appear blank until a call to PumpEvents.
   * This temporarily fixes the blank screen problem */
  SDL_PumpEvents();
#endif

  /* Showing the window here prevented flickering on Linux Mint */
  SDL_ShowWindow(window);

  SDL_Texture * background = NULL;
  if(spooky_load_texture(renderer, "./res/bg3.png", 13, &background) != SP_SUCCESS) { goto err0; }
  assert(background != NULL);

  bool running = true;
  //bool linux_resize = false;
  while(running) {
    int update_loops = 0;

    now = sp_get_time_in_us();
    while ((now - last_update_time > TIME_BETWEEN_UPDATES && update_loops < MAX_UPDATES_BEFORE_RENDER)) {
      SDL_Event evt;
      while (SDL_PollEvent(&evt)) {
#ifdef __APPLE__ 
        /* On OS X Mojave, resize of a non-maximized window does not correctly update the aspect ratio */
        if (evt.type == SDL_WINDOWEVENT && (
              evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED 
              || evt.window.event == SDL_WINDOWEVENT_MOVED 
              || evt.window.event == SDL_WINDOWEVENT_RESIZED
              /* Only happens when clicking About in OS X Mojave */
              || evt.window.event == SDL_WINDOWEVENT_FOCUS_LOST
              ))
        {
          SDL_RenderSetLogicalSize(renderer, context->logical_width, context->logical_height);

          SDL_RenderSetScale(renderer, context->scale_factor_x, context->scale_factor_y);
          if(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) {
            context->scale_factor_x = context->scale_factor_y = 1.0f;
          }
        }
#elif __unix__
        if (evt.type == SDL_WINDOWEVENT && (
              evt.window.event == SDL_WINDOWEVENT_RESIZED
              ))
        {
#if 1==0
          if(linux_resize) {
            spooky_gui_init_gui(win, renderer);

            logical_width = (int)(((float)spooky_window_width / 2) * spooky_gui_get_scale_factor());
            logical_height = (int)(((float)spooky_window_height / 2) * spooky_gui_get_scale_factor());

            SDL_RenderSetLogicalSize(renderer, logical_width, logical_height);
            SDL_RenderSetScale(renderer, spooky_gui_get_scale_factor(), spooky_gui_get_scale_factor());
          }
          linux_resize = true;
#endif
        }
#endif

        if (evt.type == SDL_QUIT) {
          running = false;
          goto end_of_running_loop;
        }
      }

      last_update_time += TIME_BETWEEN_UPDATES;
      update_loops++;
    } /* >> while ((now - last_update_time ... */

    if (now - last_update_time > TIME_BETWEEN_UPDATES) {
      last_update_time = now - TIME_BETWEEN_UPDATES;
    }

    //float interpolation = spooky_float_min(1.0f, (float)(now - last_update_time) / (float)(TIME_BETWEEN_UPDATES));
    //(void)interpolation;
    uint64_t this_second = (uint64_t)(last_update_time / BILLION);

    SDL_Color c0;
    SDL_GetRenderDrawColor(renderer, &c0.r, &c0.g, &c0.b, & c0.a);
    {
      SDL_Color bg = { .r = 20, .g = 1, .b = 36, .a = 255 };
      SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderClear(renderer); /* letterbox color */

      SDL_Color c = { .r = 1, .g = 20, .b = 36, .a = 255 };
      SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(renderer, NULL); /* screen color */
    }

    SDL_RenderCopy(renderer, background, NULL, NULL);

    SDL_SetRenderDrawColor(renderer, c0.r, c0.g, c0.b, c0.a);
    SDL_RenderPresent(renderer);

    last_render_time = now;
    frame_count++;

    if (this_second > last_second_time) {
      /* Every second, calculate the following: */
      /* TODO: Anything that needs to update every second */

      int mouse_x, mouse_y;
      SDL_GetMouseState(&mouse_x, &mouse_y);

      /* Every second, update FPS: */
      fps = frame_count;
      (void)fps;

      frame_count = 0;
      last_second_time = this_second;
      seconds_since_start++;
    }

    /* Try to be friendly to the OS: */
    while (now - last_render_time < TARGET_TIME_BETWEEN_RENDERS && now - last_update_time < TIME_BETWEEN_UPDATES) {
      sp_sleep(1); /* Needed? */
      now = sp_get_time_in_us();
    }
end_of_running_loop: ;
  } /* >> while(running) */

  if(background != NULL) {
    SDL_DestroyTexture(background), background = NULL;
  }

  return SP_SUCCESS;

err0:
  return SP_FAILURE;
}

