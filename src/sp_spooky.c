#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "sp_error.h"
#include "sp_gui.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

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

  SDL_Window * win = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, window_flags);
  if(win == NULL) { goto err4; }

  SDL_Renderer * renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(renderer == NULL) { goto err5; }

  SDL_GLContext glContext = SDL_GL_CreateContext(win);
  if(glContext == NULL) { goto err6; }

  SDL_Surface * surface = NULL;
  if(spooky_load_image("res/test0.png", 13, &surface) == SP_FAILURE) { goto err0; }
  if(surface == NULL) goto err0;
  SDL_FreeSurface(surface), surface = NULL;

  SDL_Texture * texture = NULL;
  if(spooky_load_texture(renderer, "res/test0.png", 13, &texture) == SP_FAILURE) { goto err0; }
  if(texture == NULL) { goto err0; }

  SDL_DestroyTexture(texture), texture = NULL;

  SDL_GL_DeleteContext(glContext), glContext = NULL;
  SDL_DestroyRenderer(renderer), renderer = NULL;
	SDL_DestroyWindow(win), win = NULL;
  TTF_Quit();
	SDL_Quit();

  fprintf(stdout, "Thank you for playing! Happy gaming!\n");
  return SP_SUCCESS;

err6:
  /* unable to create GL context */
  SDL_DestroyRenderer(renderer), renderer = NULL;

err5:
  /* unable to create SDL renderer */
  SDL_DestroyWindow(win), win = NULL;

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
  fprintf(stderr, "A fatal error occurred on initialization.\n");
  return SP_FAILURE;
}

