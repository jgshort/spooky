#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "sp_error.h"
#include "sp_gui.h"

typedef struct sp_game_context {
  SDL_Window * window;
  SDL_Renderer * renderer;
  SDL_GLContext glContext;
} sp_game_context;

static errno_t spooky_init_game_context(sp_game_context * context);
static errno_t spooky_quit_game_context(sp_game_context * context);

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  sp_game_context context;

  if(spooky_init_game_context(&context) != SP_SUCCESS) { goto err0; }

  SDL_Renderer * renderer = context.renderer;

  /* check surface resource path and method */
  SDL_Surface * test_surface = NULL;
  if(spooky_load_image("res/test0.png", 13, &test_surface) == SP_FAILURE) { goto err0; }
  if(test_surface == NULL) goto err0;
  SDL_FreeSurface(test_surface), test_surface = NULL;

  /* check texture method */
  SDL_Texture * test_texture = NULL;
  if(spooky_load_texture(renderer, "res/test0.png", 13, &test_texture) == SP_FAILURE) { goto err0; }
  if(test_texture == NULL) { goto err0; }
  SDL_DestroyTexture(test_texture), test_texture = NULL;

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

static errno_t spooky_init_game_context(sp_game_context * context) {
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

  SDL_Window * window = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, window_flags);
  if(window == NULL) { goto err4; }

  SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(renderer == NULL) { goto err5; }

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if(glContext == NULL) { goto err6; }

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

static errno_t spooky_quit_game_context(sp_game_context * context) {
  assert(!(context == NULL));
  if(context == NULL) { return SP_FAILURE; }

  fprintf(stdout, "Freeing resources...");
  SDL_GL_DeleteContext(context->glContext), context->glContext = NULL;
  SDL_DestroyRenderer(context->renderer), context->renderer = NULL;
  SDL_DestroyWindow(context->window), context->window = NULL;
  TTF_Quit();
  SDL_Quit();
  fprintf(stdout, " Done!\n");
  return SP_SUCCESS;
}
