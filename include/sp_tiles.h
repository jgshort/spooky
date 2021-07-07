#pragma once

#ifndef SP_TILES__H
#define SP_TILES__H

#include <stdbool.h>

#include "sp_error.h"
#include "sp_gui.h"
#include "sp_context.h"

typedef enum spooky_tiles_tile_type {
  STT_EMPTY         = 0,
  /* Impassible */
  STT_BEDROCK       = 1 << 1,

  /* Rock */
  STT_IGNEOUS       = 1 << 2,
  STT_SEDIMENTARY   = 1 << 3,
  STT_METAMORPHIC   = 1 << 4,

  /* Dirt */
  STT_SAND          = 1 << 5,
  STT_SILT          = 1 << 6,
  STT_CLAY          = 1 << 7,
  STT_LOAM          = 1 << 8,
  STT_GRAVEL        = 1 << 9,

  /* Aqua */
  STT_WATER         = 1 << 10,

  /* Ground */
  STT_TREE          = 1 << 11,

  STT_RLE_MARKER    = 1 << 20,

  STT_EOE
} spooky_tiles_tile_type;

typedef enum spooky_tiles_biom {
  SB_EMPTY,
  SB_TUNDRA,
  SB_CONIFEROUS_FOREST,
  SB_TEMPERATE_DECIDUOUS_FOREST,
  SB_GRASSLAND,
  SB_DESERT,
  SB_SHRUBLAND,
  SB_RAINFOREST,
  SB_EOE
} spooky_tiles_biom;

const uint32_t SPOOKY_TILES_MAX_TILES_ROW_LEN;
const uint32_t SPOOKY_TILES_MAX_TILES_COL_LEN;
const uint32_t SPOOKY_TILES_MAX_TILES_DEPTH_LEN;
const uint32_t SPOOKY_TILES_VOXEL_WIDTH;
const uint32_t SPOOKY_TILES_VOXEL_HEIGHT;

typedef struct spooky_tiles_tile_meta spooky_tiles_tile_meta;
typedef struct spooky_tiles_tile_meta {
  spooky_tiles_tile_type type;
  bool unbreakable;
  char padding[7];
} spooky_tiles_tile_meta;

typedef struct spooky_tile spooky_tile;
typedef struct spooky_tile {
  const spooky_tiles_tile_meta * meta;
  uint32_t offset;
  bool free;
  char padding[3];
} spooky_tile;

const spooky_tiles_tile_meta spooky_tiles_global_tiles_meta[STT_EOE + 1];

typedef struct spooky_tiles_manager_data spooky_tiles_manager_data;
typedef struct spooky_tiles_manager spooky_tiles_manager;

typedef struct spooky_tiles_manager {
  const spooky_tiles_manager * (*ctor)(const spooky_tiles_manager * /* self */, const spooky_context * /* context */);
  const spooky_tiles_manager * (*dtor)(const spooky_tiles_manager * /* self */);
  void (*free)(const spooky_tiles_manager * /* self */);
  void (*release)(const spooky_tiles_manager * /* self */);

  void (*generate_tiles)(const spooky_tiles_manager * /* self */);
  const spooky_tile * (*create_tile)(const spooky_tiles_manager * /* self */, uint32_t /* x */, uint32_t /* y */, uint32_t /* z */, spooky_tiles_tile_type /* type */);
  spooky_tile * (*set_empty)(const spooky_tiles_manager * /* self */, uint32_t /* x */, uint32_t /* y */, uint32_t /* z */);
  const spooky_tile * (*get_tiles)(const spooky_tiles_manager * /* self */);
  const spooky_tile * (*get_tile)(const spooky_tiles_manager * /* self */, uint32_t /* x */, uint32_t /* y */, uint32_t /* z */);

  const spooky_tile * (*get_active_tile)(const spooky_tiles_manager * /* self */);
  void (*set_active_tile)(const spooky_tiles_manager * /* self */, uint32_t /* x */, uint32_t /* y */, uint32_t /* z */);

  spooky_view_perspective (*get_perspective)(const spooky_tiles_manager * /* self */);
  void (*set_perspective)(const spooky_tiles_manager * /* self */, spooky_view_perspective /* perspective */);
  void (*rotate_perspective)(const spooky_tiles_manager * /* self */, spooky_view_perspective /* new_perspective */);

  errno_t (*read_tiles)(const spooky_tiles_manager * /* self */);
  errno_t (*write_tiles)(const spooky_tiles_manager * /* self */);

  spooky_tiles_manager_data * data;
} spooky_tiles_manager;

/* Allocate (malloc) interface */
const spooky_tiles_manager * spooky_tiles_manager_alloc();
/* Initialize interface methods */
const spooky_tiles_manager * spooky_tiles_manager_init(spooky_tiles_manager * /* self */);
/* Allocate and initialize interface methods */
const spooky_tiles_manager * spooky_tiles_manager_acquire();
/* Construct data */
const spooky_tiles_manager * spooky_tiles_manager_ctor(const spooky_tiles_manager * /* self */, const spooky_context * /* context */);
/* Destruct (dtor) data */
const spooky_tiles_manager * spooky_tiles_manager_dtor(const spooky_tiles_manager * /* self */);
/* Free interface */
void spooky_tiles_manager_free(const spooky_tiles_manager * /* self */);
/* Destruct and free interface */
void spooky_tiles_manager_release(const spooky_tiles_manager * /* self */);

void spooky_tiles_get_tile_color(spooky_tiles_tile_type /* type */, SDL_Color * /* color */);
const char * spooky_tiles_tile_type_as_string(spooky_tiles_tile_type /* type */);
void spooky_tiles_generate_tiles(spooky_tile * /* tiles_in */, size_t /* tiles_len */);
const char * spooky_tiles_get_tile_info(const spooky_tile * /* tile */, char * /* buf */, size_t /* buf_len */, int * /* buf_len_out */);



#endif /* SP_TILES__H */
