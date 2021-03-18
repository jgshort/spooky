#pragma once

#ifndef SPOOKY_TYPES__H
#define SPOOKY_TYPES__H

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

#endif /* SPOOKY_TYPES__H */
