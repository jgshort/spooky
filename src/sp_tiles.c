#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sodium.h>

#include "sp_pak.h"
#include "sp_math.h"
#include "sp_z.h"
#include "sp_io.h"
#include "sp_gui.h"
#include "sp_context.h"
#include "sp_tiles.h"

const int SPOOKY_TILES_VISIBLE_VOXELS_WIDTH = (2 * 26);
const int SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT = (2 * 16);
const uint32_t SPOOKY_TILES_MAX_TILES_ROW_LEN = 64;
const uint32_t SPOOKY_TILES_MAX_TILES_COL_LEN = 64;
const uint32_t SPOOKY_TILES_MAX_TILES_DEPTH_LEN = 64;
const uint32_t SPOOKY_TILES_VOXEL_WIDTH = 16;
const uint32_t SPOOKY_TILES_VOXEL_HEIGHT = 16;

static const size_t SPOOKY_TILES_ALLOCATED_INCREMENT = 4096;

static uint32_t SP_OFFSET(uint32_t x, uint32_t y, uint32_t z) {
  return x + (y * SPOOKY_TILES_MAX_TILES_ROW_LEN) + (z * SPOOKY_TILES_MAX_TILES_ROW_LEN * SPOOKY_TILES_MAX_TILES_COL_LEN);
}

const spooky_tiles_tile_meta spooky_tiles_global_tiles_meta[STT_EOE + 1] = {
  [STT_EMPTY]   = { .type = STT_EMPTY, .unbreakable = true },
  [STT_BEDROCK] = { .type = STT_BEDROCK, .unbreakable = true },
  /* Rocks */
  [STT_IGNEOUS]     = { .type = STT_IGNEOUS, .unbreakable = false },
  [STT_SEDIMENTARY] = { .type = STT_SEDIMENTARY, .unbreakable = false },
  [STT_METAMORPHIC] = { .type = STT_METAMORPHIC, .unbreakable = false },
  /* Dirt */
  [STT_SAND]   = { .type = STT_SAND, .unbreakable = false } ,
  [STT_SILT]   = { .type = STT_SILT, .unbreakable = false },
  [STT_CLAY]   = { .type = STT_CLAY, .unbreakable = false },
  [STT_LOAM]   = { .type = STT_LOAM, .unbreakable = false },
  [STT_GRAVEL] = { .type = STT_GRAVEL, .unbreakable = false },
  /* Aqua */
  [STT_WATER] = { .type = STT_WATER, .unbreakable = false },
  /* Ground */
  [STT_TREE]  = { .type = STT_TREE, .unbreakable = false },

  /* Invalid */
  [STT_EOE] = { .type = STT_EOE, .unbreakable = true }
};

static spooky_tile spooky_tiles_global_empty_tile = {
  .meta = &(spooky_tiles_global_tiles_meta[STT_EMPTY]),
  .offset = UINT_MAX,
  .free = false
};

typedef struct spooky_tiles_manager_data {
  const spooky_context * context;
  spooky_tile * active_tile;

  spooky_tile * allocated_tiles;
  size_t allocated_tiles_capacity;
  size_t allocated_tiles_len;

  spooky_tile ** tiles;
  size_t tiles_len;

  spooky_tile ** rotated_tiles; /* tiles rotated around an axis */
  size_t rotated_tiles_len;

  spooky_vector_3df world_pov;

  spooky_view_perspective perspective;
  bool test_world;
  char padding[3];
} spooky_tiles_manager_data;

static void spooky_tiles_manager_generate_tiles(const spooky_tiles_manager * self);
static spooky_tile * spooky_tiles_manager_set_empty(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z);
static const spooky_tile * spooky_tiles_manager_create_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z, spooky_tiles_tile_type type);
static const spooky_tile * spooky_tiles_manager_get_tiles(const spooky_tiles_manager * self);
static const spooky_tile * spooky_tiles_manager_get_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z);
static const spooky_tile * spooky_tiles_get_active_tile(const spooky_tiles_manager * self);
static void spooky_tiles_set_active_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z);
static void spooky_tiles_rotate_perspective(const spooky_tiles_manager * self, spooky_view_perspective new_perspective);
static errno_t spooky_tiles_read_tiles(const spooky_tiles_manager * self);
static errno_t spooky_tiles_write_tiles(const spooky_tiles_manager * self);
static const spooky_vector_3df * spooky_tiles_get_world_pov(const spooky_tiles_manager * self);

static const spooky_vector_3df * spooky_tiles_get_world_pov(const spooky_tiles_manager * self) {
  return &(self->data->world_pov);
}

static spooky_view_perspective spooky_tiles_get_perspective(const spooky_tiles_manager * self) {
  return self->data->perspective;
}

static void spooky_tiles_move_left(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->x -= 1.0f;
  }
  if(pov->x < 0.f) { pov->x = 0.f; }
  if(pov->x >= (double)(SPOOKY_TILES_MAX_TILES_ROW_LEN - 1)) { pov->x = (double)(SPOOKY_TILES_MAX_TILES_ROW_LEN - 1); }
}

static void spooky_tiles_move_right(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->x += 1.0f;
  }
  if(pov->x < 0.f) { pov->x = 0.f; }
  if(pov->x >= (double)(SPOOKY_TILES_MAX_TILES_ROW_LEN - 1)) { pov->x = (double)(SPOOKY_TILES_MAX_TILES_ROW_LEN - 1); }
}

static void spooky_tiles_move_up(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->y += 1.0f;
  }
  if(pov->y < 0.f) { pov->y = 0.f; }
  if(pov->y >= (double)(SPOOKY_TILES_MAX_TILES_COL_LEN - 1)) { pov->y = (double)(SPOOKY_TILES_MAX_TILES_COL_LEN - 1); }
}

static void spooky_tiles_move_down(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->y -= 1.0f;
  }
  if(pov->y < 0.f) { pov->y = 0.f; }
  if(pov->y >= (double)(SPOOKY_TILES_MAX_TILES_COL_LEN - 1)) { pov->y = (double)(SPOOKY_TILES_MAX_TILES_COL_LEN - 1); }
}

static void spooky_tiles_move_forward(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->z -= 1.0f;
  }
  if(pov->z < 0.f) { pov->z = 0.f; }
  if(pov->z >= (double)(SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1)) { pov->z = (double)(SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1); }
}

static void spooky_tiles_move_backward(const spooky_tiles_manager * self) {
  spooky_vector_3df * pov = &(self->data->world_pov);
  switch(self->data->perspective) {
    case SPOOKY_SVP_X:
    case SPOOKY_SVP_Y:
    case SPOOKY_SVP_Z:
    case SPOOKY_SVP_EOE:
    default:
      pov->z += 1.0f;
  }
  if(pov->z < 0.f) { pov->z = 0.f; }
  if(pov->z >= (double)(SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1)) { pov->z = (double)(SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1); }
}

static void spooky_tiles_set_perspective(const spooky_tiles_manager * self, spooky_view_perspective perspective) {
  assert(perspective >= SPOOKY_SVP_DEFAULT && perspective < SPOOKY_SVP_EOE);
  self->data->perspective = perspective;
}

const spooky_tiles_manager * spooky_tiles_manager_alloc() {
  spooky_tiles_manager * self = calloc(1, sizeof * self);
  return self;
}

const spooky_tiles_manager * spooky_tiles_manager_init(spooky_tiles_manager * self) {
  assert(self);

  self->ctor = &spooky_tiles_manager_ctor;
  self->dtor = &spooky_tiles_manager_dtor;
  self->free = &spooky_tiles_manager_free;
  self->release = &spooky_tiles_manager_release;
  self->create_tile = &spooky_tiles_manager_create_tile;
  self->set_empty = &spooky_tiles_manager_set_empty;
  self->generate_tiles = &spooky_tiles_manager_generate_tiles;
  self->get_tiles = &spooky_tiles_manager_get_tiles;
  self->get_tile = &spooky_tiles_manager_get_tile;
  self->set_active_tile = &spooky_tiles_set_active_tile;
  self->get_active_tile = &spooky_tiles_get_active_tile;
  self->set_perspective = &spooky_tiles_set_perspective;
  self->get_perspective = &spooky_tiles_get_perspective;
  self->rotate_perspective = &spooky_tiles_rotate_perspective;
  self->write_tiles = &spooky_tiles_write_tiles;
  self->read_tiles = &spooky_tiles_read_tiles;

  self->move_left = &spooky_tiles_move_left;
  self->move_right = &spooky_tiles_move_right;
  self->move_up = &spooky_tiles_move_up;
  self->move_down = &spooky_tiles_move_down;
  self->move_forward = &spooky_tiles_move_forward;
  self->move_backward = &spooky_tiles_move_backward;

  self->get_world_pov = &spooky_tiles_get_world_pov;

  self->data = NULL;
  return self;
}

const spooky_tiles_manager * spooky_tiles_manager_acquire() {
  return spooky_tiles_manager_init(((spooky_tiles_manager *)(uintptr_t)spooky_tiles_manager_alloc()));
}
const spooky_tiles_manager * spooky_tiles_manager_ctor(const spooky_tiles_manager * self, const spooky_context * context) {
  assert(self);

  spooky_tiles_manager_data * data = calloc(1, sizeof * data);

  /* Pointers to the allocated tiles; using 'empty', we can save
     some memory when no tile is allocated (a pointer to pointers). */
  data->tiles_len = SPOOKY_TILES_MAX_TILES_ROW_LEN * SPOOKY_TILES_MAX_TILES_COL_LEN * SPOOKY_TILES_MAX_TILES_DEPTH_LEN;
  data->tiles = calloc(data->tiles_len, sizeof * data->tiles);
  if(!data->tiles) { abort(); }

  data->rotated_tiles_len = data->tiles_len;//;(size_t)(SPOOKY_TILES_VISIBLE_VOXELS_WIDTH * SPOOKY_TILES_VISIBLE_VOXELS_HEIGHT);

  /* Free store from which tiles are allocated; the allocated
     array is resized as needed (a pointer to structure). */
  data->allocated_tiles_capacity = SPOOKY_TILES_ALLOCATED_INCREMENT;
  data->allocated_tiles_len = 0;
  data->allocated_tiles = calloc(data->allocated_tiles_capacity, sizeof * data->allocated_tiles);
  if(!data->allocated_tiles) { abort(); }

  data->context = context;
  data->active_tile = NULL;
  data->test_world = false;

  data->world_pov.x = 0.f;
  data->world_pov.y = 0.f;
  data->world_pov.z = (double)(SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 5);

  ((spooky_tiles_manager *)(uintptr_t)self)->data = data;

  spooky_tiles_rotate_perspective(self, SPOOKY_SVP_X);

  return self;
}

const spooky_tiles_manager * spooky_tiles_manager_dtor(const spooky_tiles_manager * self) {
  if(self && self->data) {
    self->data->tiles_len = 0;
    free(self->data->tiles), self->data->tiles = NULL;
    free(self->data->rotated_tiles), self->data->rotated_tiles = NULL;

    self->data->allocated_tiles_capacity = 0;
    self->data->allocated_tiles_len = 0;
    free(self->data->allocated_tiles), self->data->allocated_tiles = NULL;

    free(self->data), ((spooky_tiles_manager *)(uintptr_t)self)->data = NULL;
  }
  return self;
}

void spooky_tiles_manager_free(const spooky_tiles_manager * self) {
  free(((spooky_tiles_manager *)(uintptr_t)self)), self = NULL;
}

void spooky_tiles_manager_release(const spooky_tiles_manager * self) {
  self->free(self->dtor(self));
}

static spooky_tile * spooky_tiles_manager_set_empty_by_offset(const spooky_tiles_manager * self, uint32_t offset) {
  self->data->tiles[offset] = &spooky_tiles_global_empty_tile;
  return self->data->tiles[offset];
}

static spooky_tile * spooky_tiles_manager_set_empty(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z) {
  uint32_t offset = SP_OFFSET(x, y, z);
  return spooky_tiles_manager_set_empty_by_offset(self, offset);
}

static const spooky_tile * spooky_tiles_manager_create_tile_by_offset(const spooky_tiles_manager * self, uint32_t offset, spooky_tiles_tile_type type) {
  if(type == STT_EMPTY) {
    return spooky_tiles_manager_set_empty_by_offset(self, offset);
  } else {
    if(self->data->allocated_tiles_len + 1 > self->data->allocated_tiles_capacity) {
      /* reallocate allocated tiles */
      self->data->allocated_tiles_capacity += SPOOKY_TILES_ALLOCATED_INCREMENT;
      spooky_tile * temp = realloc(self->data->allocated_tiles, self->data->allocated_tiles_capacity * (sizeof * temp));
      if(!temp) { abort(); }

      for(size_t i = 0; i < self->data->allocated_tiles_len; ++i) {
        spooky_tile * tile = &(temp[i]);
        if(tile && tile->meta->type != STT_EMPTY && tile->offset != UINT_MAX) {
          self->data->tiles[tile->offset] = tile;
        }
      }

      self->data->allocated_tiles = temp;

      /*
      Diagnostics:
      if(self->data->allocated_tiles_capacity % SPOOKY_TILES_ALLOCATED_INCREMENT == 0) {
        fprintf(stdout, "Voxels to %lu\n", self->data->allocated_tiles_capacity);
        fflush(stdout);
      }
      */
    }

    spooky_tile * new_tile = &(self->data->allocated_tiles[self->data->allocated_tiles_len]);
    self->data->allocated_tiles_len++;

    assert(type > STT_EMPTY && type < STT_EOE);
    *new_tile = (spooky_tile){
      .meta = &(spooky_tiles_global_tiles_meta[type]),
      .offset = offset,
      .free = false,
      .padding = { 0 }
    };

    assert(new_tile->meta);
    self->data->tiles[offset] = new_tile;

    return new_tile;
  }
}

static const spooky_tile * spooky_tiles_manager_create_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z, spooky_tiles_tile_type type) {
  uint32_t offset = SP_OFFSET(x, y, z);
  return spooky_tiles_manager_create_tile_by_offset(self, offset, type);
}

const char * spooky_tiles_tile_type_as_string(spooky_tiles_tile_type type) {
  switch(type) {
    case STT_EMPTY: return "empty";
    case STT_BEDROCK: return "bedrock";
    case STT_CLAY: return "clay";
    case STT_GRAVEL: return "gravel";
    case STT_IGNEOUS: return "igneous";
    case STT_METAMORPHIC: return "metamorphic";
    case STT_SEDIMENTARY: return "sedimentary";
    case STT_SILT: return "silt";
    case STT_WATER: return "water";
    case STT_TREE: return "tree";
    case STT_SAND: return "sand";
    case STT_LOAM: return "loam";
    case STT_RLE_MARKER:
    case STT_EOE: /* fall-through */
    default: return "error";
  }
}

const char * spooky_tiles_get_tile_info(const spooky_tile * tile, char * buf, size_t buf_len, int * buf_len_out) {
  const char * type = spooky_tiles_tile_type_as_string(tile->meta->type);

  *buf_len_out = snprintf(buf, buf_len, "type: '%s'", type);

  return buf;
}

static void spooky_tiles_manager_generate_tiles(const spooky_tiles_manager * self) {
  if(self->data->test_world) {
    for(uint32_t x = 0; x < SPOOKY_TILES_MAX_TILES_ROW_LEN; ++x) {
      for(uint32_t y = 0; y < SPOOKY_TILES_MAX_TILES_COL_LEN; ++y) {
        for(uint32_t z = 0; z < SPOOKY_TILES_MAX_TILES_DEPTH_LEN; ++z) {
          self->create_tile(self, x, y, z, STT_EMPTY);
        }
      }
    }

    static const uint32_t x_offset = 10; (void)x_offset;
    static const uint32_t y_offset = 10; (void)y_offset;

    static const spooky_tiles_tile_type demo[3][3][3] = {
      { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },
      { { 0, 0, 1 }, { 1, 1, 0 }, { 0, 0, 1 } },
      { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },
    };

    for(uint32_t x = 0; x < sizeof demo / sizeof demo[0]; ++x) {
      for(uint32_t y = 0; y < sizeof demo[0] / sizeof demo[0][0]; ++y) {
        for(uint32_t z = 0; z < sizeof demo[0][0] / sizeof demo[0][0][0]; ++z) {
          self->create_tile(self, x, y, z, demo[x][y][z]);
        }
      }
    }

    memmove(self->data->rotated_tiles, self->data->tiles, self->data->tiles_len * sizeof(void *));

    return;
  }

  /* basic biom layout */
  // unsigned int seed = randombytes_uniform(100);
  uint32_t x = SPOOKY_TILES_MAX_TILES_ROW_LEN;
  while(x--) {
    uint32_t y = SPOOKY_TILES_MAX_TILES_COL_LEN;
    while(y--) {
      uint32_t z = SPOOKY_TILES_MAX_TILES_DEPTH_LEN;
      while(z--) {
        // static const size_t level_ground = MAX_TILES_DEPTH_LEN / 2;
        spooky_tiles_tile_type type = STT_EMPTY;

        /* Bedrock is only found on the bottom 4 levels of the ground.
           Bedrock is impassible. */

        /* can't go any deeper than this. */
        if(z == SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1) {
          type = STT_BEDROCK;
          goto create_tile;
        }
        if(z > SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 5) {
          static const int max_loops = 5;

          /* generate a random type of rock */
          spooky_tiles_tile_type new_type = STT_EMPTY;
          int loops = 0;
          do {
            uint32_t percentage = randombytes_uniform(101);
            new_type = randombytes_uniform((uint32_t)STT_METAMORPHIC + 1);
            assert(new_type >= STT_EMPTY && new_type <= STT_METAMORPHIC);
            /* Bedrock becomes more common the lower we dig.
               The lowest level, 0, is 100% bedrock, with the following
               distribution as we advance upwards:

              Bedrock Distribution Levels:
                - 0 = 100% bedrock
                - 1 = 77% bedrock
                - 2 = 33% bedrock
                - 3 = 17% bedrock
                - 4 = 2% bedrock

              Bedrock rises from the ground, forming simple stalagmites.
              Viewed from the side, we might witness something like:

              Level 5:
              Level 4:        =
              Level 3:        =
              Level 2:      ===
              Level 1:  ========
              Level 0: ==========
            */
            if(z == SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 2 && percentage <= 77) { new_type = STT_BEDROCK; }
            if(z == SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 3 && percentage <= 33) { new_type = STT_BEDROCK; }
            if(z == SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 4 && percentage <= 17) { new_type = STT_BEDROCK; }
            if(z == SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 5 && percentage <=  2) { new_type = STT_BEDROCK; }
            while(new_type == STT_BEDROCK) {
              /* we only want bedrock if the block below is also bedrock */
              size_t under_offset = SP_OFFSET(x, y, z + 1);
              const spooky_tile * under = self->data->tiles[under_offset];
              if(under->meta->type != STT_BEDROCK) {
                new_type = randombytes_uniform((uint32_t)STT_METAMORPHIC + 1);
                assert(new_type >= STT_EMPTY && new_type <= STT_METAMORPHIC);
              } else {
                break;
              }
            }
          } while(++loops < max_loops && new_type <= STT_EMPTY);

          if(new_type > STT_BEDROCK) { new_type = STT_EMPTY; }
          /* clean up type */
          type = new_type;
        }

create_tile:
        self->create_tile(self, x, y, z, type);
      }
    }
  }

  self->rotate_perspective(self, self->data->perspective);

  /* Diagnostics */
  /* fprintf(stdout, "%lu total voxels, %lu allocated voxels\n", self->data->tiles_len, self->data->allocated_tiles_len); */
}

void spooky_tiles_get_tile_color(spooky_tiles_tile_type type, SDL_Color * color) {
  if(!color) { return; }
  switch(type) {
    case STT_EMPTY:
      *color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = 255 }; /* Empty (?) */
      break;
    case STT_BEDROCK:
      *color = (SDL_Color){ .r = 64, .g = 64, .b = 64, .a = 255 }; /* Dark gray */
      break;
    case STT_METAMORPHIC:
      *color = (SDL_Color){ .r = 112, .g = 128, .b = 144, .a = 255 }; /* Slate gray color */
      break;
    case STT_IGNEOUS:
      *color = (SDL_Color){ .r = 255, .g = 253, .b = 208, .a = 255 };/* Cream color */
      break;
    case STT_SEDIMENTARY:
      *color = (SDL_Color){ .r = 213, .g = 194, .b = 165, .a = 255 };/* Sandstone color */
      break;
    case STT_TREE:
      *color = (SDL_Color){ .r = 0, .g = 128, .b = 0, .a = 255 }; /* Green tree */
      break;
    case STT_WATER:
      *color = (SDL_Color){ .r = 0, .g = 0, .b = 255, .a = 255 }; /* Blue water */
      break;
    case STT_SAND:
    case STT_SILT:
    case STT_CLAY:
    case STT_LOAM:
    case STT_GRAVEL:
      *color = (SDL_Color){ .r = 44, .g = 33, .b = 24, .a = 255 }; /* Mud color */
      break;
    case STT_RLE_MARKER:
    case STT_EOE:
    default:
      *color = (SDL_Color){ .r = 255, .g = 0, .b = 0, .a = 255 }; /* Red Error */
      break;
  }
}

static const spooky_tile * spooky_tiles_manager_get_tiles(const spooky_tiles_manager * self) {
  return *(self->data->rotated_tiles);
}

static const spooky_tile * spooky_tiles_manager_get_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z) {
  if(x > SPOOKY_TILES_MAX_TILES_ROW_LEN - 1) { return NULL; }
  if(y > SPOOKY_TILES_MAX_TILES_COL_LEN - 1) { return NULL; }
  if(z > SPOOKY_TILES_MAX_TILES_DEPTH_LEN - 1) { return NULL; }

  uint32_t offset = SP_OFFSET(x, y, z);
  return self->data->rotated_tiles[offset];
}

static const spooky_tile * spooky_tiles_get_active_tile(const spooky_tiles_manager * self) {
  return self->data->active_tile;
}

static void spooky_tiles_set_active_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z) {
  uint32_t offset = SP_OFFSET(x, y, z);
  spooky_tile * tile = self->data->rotated_tiles[offset];
  if(tile->meta->type != STT_EMPTY) {
    self->data->active_tile = tile;
  } else {
    self->data->active_tile = NULL;
  }
}

static void spooky_tiles_rotate_perspective(const spooky_tiles_manager * self, spooky_view_perspective new_perspective) {
  spooky_tile ** rotated = calloc(self->data->rotated_tiles_len, sizeof * rotated);
  if(!rotated) { abort(); }

  // Degrees / 57.29578 = radians
  static const float theta = 90.f / 57.29578f;
  const float sin_theta = (float)sin(theta);
  const float cos_theta = (float)cos(theta);
  for(uint32_t x = 0; x < SPOOKY_TILES_MAX_TILES_ROW_LEN; ++x) {
    for(uint32_t y = 0; y < SPOOKY_TILES_MAX_TILES_ROW_LEN; ++y) {
      for(uint32_t z = 0; z < SPOOKY_TILES_MAX_TILES_ROW_LEN; ++z) {
        uint32_t new_x = 0, new_y = 0, new_z = 0;
        switch(new_perspective) {
          case SPOOKY_SVP_X:
            new_x = x;
            new_y = (uint32_t)(/*abs*/((int)floor(((float)y * cos_theta) - ((float)z * sin_theta))));
            new_z = (uint32_t)(((int)floor(((float)y * sin_theta) + ((float)z * cos_theta))));
            break;
          case SPOOKY_SVP_Y:
            new_x = (uint32_t)(((int)floor(((float)x * cos_theta) + ((float)z * sin_theta))));
            new_y = y;
            new_z = (uint32_t)(((int)floor(((float)z * cos_theta) - ((float)x * sin_theta))));
            break;
          case SPOOKY_SVP_Z:
          case SPOOKY_SVP_EOE:
          default:
            new_x = (uint32_t)(((int)floor(((float)x * cos_theta) - ((float)y * sin_theta))));
            new_y = (uint32_t)(((int)floor(((float)x * sin_theta) + ((float)y * cos_theta))));
            new_z = z;
            break;
        }

        uint32_t offset = SP_OFFSET((uint32_t)abs((int)new_x), (uint32_t)abs((int)new_y), (uint32_t)abs((int)new_z));
        rotated[offset] = self->data->tiles[SP_OFFSET(x, y, z)];
      }
    }
  }
  free(self->data->rotated_tiles), self->data->rotated_tiles = NULL;
  self->data->rotated_tiles = rotated;
  self->data->perspective = new_perspective;
}

static errno_t spooky_tiles_read_tiles(const spooky_tiles_manager * self) {
  const spooky_config * config = self->data->context->get_config(self->data->context);
  const char * data_path = config->get_data_path(config);

  FILE * fp = NULL;
  int fd = 0;
  char * pak_file = spooky_io_alloc_concat_path(data_path, "/000001.spdb");

  if((fp = spooky_io_open_binary_file_for_reading(pak_file, &fd)) == NULL) { goto err0; }

  size_t tiles_len = self->data->tiles_len;
  for(uint32_t i = 0; i < tiles_len; i++) {
    uint32_t type = 0;
    bool success = spooky_read_uint32(fp, &type);
    assert(success);
    if(success && ((type & STT_RLE_MARKER) == STT_RLE_MARKER)) {
      /* unpack RLE */
      uint32_t unpacked_type = (uint32_t)((int)type & ~STT_RLE_MARKER);
      assert(unpacked_type >= STT_EMPTY && unpacked_type <= STT_TREE);
      uint32_t rle_count = 0;
      success = spooky_read_uint32(fp, &rle_count);
      assert(success && rle_count >= 5);
      for(uint32_t j = 0; j < rle_count; j++) {
        spooky_tiles_manager_create_tile_by_offset(self, i + j, unpacked_type);
      }
      i += rle_count - 1;
    } else if(success) {
      assert(type >= STT_EMPTY && type <= STT_TREE);
      spooky_tiles_manager_create_tile_by_offset(self, i, type);
    } else {
      goto err1;
    }
  }

  self->rotate_perspective(self, self->data->perspective);

  fclose(fp);
  free(pak_file), pak_file = NULL;

  return SP_SUCCESS;

err1:
  fclose(fp);

err0:
  free(pak_file), pak_file = NULL;
  return SP_FAILURE;
}

static errno_t spooky_tiles_write_tiles(const spooky_tiles_manager * self) {
  const spooky_config * config = self->data->context->get_config(self->data->context);
  const char * data_path = config->get_data_path(config);

  int fd = -1;
  char * pak_file = spooky_io_alloc_concat_path(data_path, "/000001.spdb");
  FILE * fp = NULL;
  if((fp = spooky_io_open_or_create_binary_file_for_writing(pak_file, &fd)) == NULL) { goto err0; };

  spooky_tiles_manager_data * data = self->data;
  size_t tiles_len = data->tiles_len;
  for(uint32_t i = 0; i < tiles_len; i++) {
    spooky_tile * tile = data->tiles[i];
    const spooky_tiles_tile_meta * meta = tile->meta;
    uint32_t rle_count = 0;
    for(uint32_t j = i + 1; j < tiles_len; j++) {
      spooky_tile * next_tile = data->tiles[j];
      if(next_tile->meta->type != meta->type) { break; }
      ++rle_count;
    }

    uint64_t bytes_written = 0;
    static const uint32_t min_contiguous_types = 5;
    if(rle_count >= min_contiguous_types) {
      uint32_t rte_marker = (meta->type | STT_RLE_MARKER);
      spooky_write_uint32(rte_marker, fp, &bytes_written);
      spooky_write_uint32(rle_count, fp, &bytes_written);
      i += rle_count - 1;
    } else {
      spooky_write_uint32(meta->type, fp, &bytes_written);
    }
  }

  fclose(fp);
  free(pak_file), pak_file = NULL;
  return SP_SUCCESS;

err0:
  free(pak_file), pak_file = NULL;
  return SP_FAILURE;
}
