#pragma once

#ifndef SP_TYPES__H
#define SP_TYPES__H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

  typedef struct spooky_save_game {
    const char * name;
    int save_game_version;
    int v1_id;
  } spooky_save_game;

  typedef struct spooky_save_game_v1 {
    spooky_save_game base;
    int seed;
    char padding[4];
    int turns;
    int deaths;
    double x;
    double y;
    double z;
  } spooky_save_game_v1;

#ifdef __cplusplus
}
#endif

#endif /* SP_TYPES__H */

