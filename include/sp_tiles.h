#pragma once

#ifndef SP_TILES__H
#define SP_TILES__H

#include <stdbool.h>
#include "sp_gui.h"

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

const uint32_t MAX_TILES_ROW_LEN;
const uint32_t MAX_TILES_COL_LEN;
const uint32_t MAX_TILES_DEPTH_LEN;
const uint32_t VOXEL_WIDTH;
const uint32_t VOXEL_HEIGHT;

typedef struct spooky_tile_meta spooky_tile_meta;
typedef struct spooky_tile_meta {
  spooky_tile_type type;
  char padding[7];
  bool is_breakable;
} spooky_tile_meta;

typedef struct spooky_tile spooky_tile;
typedef struct spooky_tile {
  const spooky_tile_meta * meta;
} spooky_tile;

const spooky_tile_meta spooky_global_tiles[STT_EOE + 1];
const spooky_tile spooky_global_empty;

typedef struct spooky_tiles_manager_data spooky_tiles_manager_data;
typedef struct spooky_tiles_manager spooky_tiles_manager;

typedef struct spooky_tiles_manager {
  const spooky_tiles_manager * (*ctor)(const spooky_tiles_manager * /* self */);
  const spooky_tiles_manager * (*dtor)(const spooky_tiles_manager * /* self */);
  void (*free)(const spooky_tiles_manager * /* self */);
  void (*release)(const spooky_tiles_manager * /* self */);

  spooky_tiles_manager_data * data;
} spooky_tiles_manager;

/* Allocate (malloc) interface */
const spooky_tiles_manager * spooky_tiles_manager_alloc();
/* Initialize interface methods */
const spooky_tiles_manager * spooky_tiles_manager_init(spooky_tiles_manager * /* self */);
/* Allocate and initialize interface methods */
const spooky_tiles_manager * spooky_tiles_manager_acquire();
/* Construct data */
const spooky_tiles_manager * spooky_tiles_manager_ctor(const spooky_tiles_manager * /* self */);
/* Destruct (dtor) data */
const spooky_tiles_manager * spooky_tiles_manager_dtor(const spooky_tiles_manager * /* self */);
/* Free interface */
void spooky_tiles_manager_free(const spooky_tiles_manager * /* self */);
/* Destruct and free interface */
void spooky_tiles_manager_release(const spooky_tiles_manager * /* self */);

void spooky_tile_color(spooky_tile_type /* type */, SDL_Color * /* color */);
const char * spooky_tile_type_as_string(spooky_tile_type /* type */);
void spooky_generate_tiles(spooky_tile * /* tiles_in */, size_t /* tiles_len */);
const char * spooky_tile_info(const spooky_tile * /* tile */, char * /* buf */, size_t /* buf_len */, int * /* buf_len_out */);

inline uint32_t SP_OFFSET(uint32_t x, uint32_t y, uint32_t z) {
  return x + (y * MAX_TILES_ROW_LEN) + (z * MAX_TILES_ROW_LEN * MAX_TILES_COL_LEN);
}

#endif /* SP_TILES__H */
