#include <assert.h>
#include <stdlib.h>
#include <sodium.h>

#include "sp_gui.h"
#include "sp_tiles.h"

const uint32_t SPOOKY_TILES_MAX_TILES_ROW_LEN = 64;
const uint32_t SPOOKY_TILES_MAX_TILES_COL_LEN = 64;
const uint32_t SPOOKY_TILES_MAX_TILES_DEPTH_LEN = 64;
const uint32_t SPOOKY_TILES_VOXEL_WIDTH = 8;
const uint32_t SPOOKY_TILES_VOXEL_HEIGHT = 8;

const spooky_tiles_tile_meta spooky_tiles_global_tiles_meta[STT_EOE + 1] = {
  { .type = STT_EMPTY, .is_breakable = false },
  { .type = STT_BEDROCK, .is_breakable = false },
  { .type = STT_IGNEOUS, .is_breakable = true },
  { .type = STT_SEDIMENTARY, .is_breakable = true },
  { .type = STT_METAMORPHIC, .is_breakable = true },
  /* Dirt */
  { .type = STT_SAND, .is_breakable = true } ,
  { .type = STT_SILT, .is_breakable = true },
  { .type = STT_CLAY, .is_breakable = true },
  { .type = STT_LOAM, .is_breakable = true },
  { .type = STT_GRAVEL, .is_breakable = true },
  /* Aqua */
  { .type = STT_WATER, .is_breakable = true },
  /* Ground */
  { .type = STT_TREE, .is_breakable = true },
  { .type = STT_EOE, .is_breakable = false }
};

static spooky_tile spooky_tiles_global_empty_tile = {
  .meta = &(spooky_tiles_global_tiles_meta[STT_EMPTY])
};

typedef struct spooky_tiles_manager_data {
  spooky_tile * allocated_tiles;
  size_t allocated_tiles_capacity;
  size_t allocated_tiles_len;

  spooky_tile * tiles;
  size_t tiles_capacity;
  size_t tiles_len;
} spooky_tiles_manager_data;

static void spooky_tiles_manager_generate_tiles(const spooky_tiles_manager * self);
static spooky_tile * spooky_tiles_manager_set_empty(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z);
static const spooky_tile * spooky_tiles_manager_create_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z, spooky_tiles_tile_type type);
static const spooky_tile * spooky_tiles_manager_get_tiles(const spooky_tiles_manager * self);

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
  self->data = NULL;
  return self;
}

const spooky_tiles_manager * spooky_tiles_manager_acquire() {
  return spooky_tiles_manager_init(((spooky_tiles_manager *)(uintptr_t)spooky_tiles_manager_alloc()));
}
const spooky_tiles_manager * spooky_tiles_manager_ctor(const spooky_tiles_manager * self) {
  assert(self);

  spooky_tiles_manager_data * data = calloc(1, sizeof * data);

  /* Free store from which tiles are allocated; the allocated
     array is resized as needed (a pointer to structure). */
  data->allocated_tiles_capacity = 128;
  data->allocated_tiles_len = 0;
  data->allocated_tiles = calloc(data->allocated_tiles_capacity, sizeof * (data->allocated_tiles));

  /* Pointers to the allocated tiles; using 'empty', we can save
     some memory when no tile is allocated (a pointer to pointers). */
  data->tiles_capacity = SPOOKY_TILES_MAX_TILES_ROW_LEN * SPOOKY_TILES_MAX_TILES_COL_LEN * SPOOKY_TILES_MAX_TILES_DEPTH_LEN;
  data->tiles_len = data->tiles_capacity;
  data->tiles = calloc(SPOOKY_TILES_MAX_TILES_ROW_LEN * SPOOKY_TILES_MAX_TILES_COL_LEN * SPOOKY_TILES_MAX_TILES_DEPTH_LEN, sizeof data->tiles);

  ((spooky_tiles_manager *)(uintptr_t)self)->data = data;

  return self;
}

const spooky_tiles_manager * spooky_tiles_manager_dtor(const spooky_tiles_manager * self) {
  if(self && self->data) {
    self->data->tiles_capacity = 0;
    self->data->tiles_len = 0;
    free(self->data->tiles), self->data->tiles = NULL;

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

static spooky_tile * spooky_tiles_manager_set_empty(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z) {
  uint32_t offset = SP_OFFSET(x, y, z);
  self->data->tiles[offset] = spooky_tiles_global_empty_tile;
  return &(self->data->tiles[offset]);
}

static const spooky_tile * spooky_tiles_manager_create_tile(const spooky_tiles_manager * self, uint32_t x, uint32_t y, uint32_t z, spooky_tiles_tile_type type) {
  if(type == STT_EMPTY) {
    return self->set_empty(self, x, y, z);
  } else {
    uint32_t offset = SP_OFFSET(x, y, z);
    if(self->data->allocated_tiles_len + 1 >= self->data->allocated_tiles_capacity) {
      /* reallocate allocated tiles */
      self->data->allocated_tiles_capacity += 64;
      spooky_tile * temp = realloc(self->data->allocated_tiles, self->data->allocated_tiles_capacity * sizeof * self->data->allocated_tiles);
      if(!temp) { abort(); }
      self->data->allocated_tiles = temp;
    }
    spooky_tile * new_tile = &(self->data->allocated_tiles[self->data->allocated_tiles_len]);
    assert(type > STT_EMPTY && type < STT_EOE);
    new_tile->meta = &(spooky_tiles_global_tiles_meta[type]);
    ++self->data->allocated_tiles_len;
    self->data->tiles[offset] = *new_tile;
    return new_tile;
  }
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
  const spooky_tile * tiles_end = self->data->tiles + self->data->tiles_len; (void)tiles_end;
  /* basic biom layout */
  // unsigned int seed = randombytes_uniform(100);
  for(uint32_t x = 0; x < SPOOKY_TILES_MAX_TILES_ROW_LEN; ++x) {
    for(uint32_t y = 0; y < SPOOKY_TILES_MAX_TILES_COL_LEN; ++y) {
      for(uint32_t z = 0; z < SPOOKY_TILES_MAX_TILES_DEPTH_LEN; ++z) {
        // static const size_t level_ground = MAX_TILES_DEPTH_LEN / 2;

        spooky_tiles_tile_type type = STT_EMPTY;

        /* Bedrock is only found on the bottom 4 levels of the ground.
           Bedrock is impassible. */

        /* can't go any deeper than this. */
        if(z == 0 && (type = STT_BEDROCK) == STT_BEDROCK) { goto create_tile; }
        if(z <= 4) {
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
            if(z == 1 && percentage <= 77) { new_type = STT_BEDROCK; }
            if(z == 2 && percentage <= 33) { new_type = STT_BEDROCK; }
            if(z == 3 && percentage <= 17) { new_type = STT_BEDROCK; }
            if(z == 4 && percentage <=  2) { new_type = STT_BEDROCK; }
            while(new_type == STT_BEDROCK) {
              /* we only want bedrock if the block below is also bedrock */
              size_t under_offset = SP_OFFSET(x, y, z - 1);
              const spooky_tile * under = &(self->data->tiles[under_offset]);
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
}

void spooky_tiles_get_tile_color(spooky_tiles_tile_type type, SDL_Color * color) {
  if(!color) { return; }
  switch(type) {
    case STT_EMPTY:
      *color = (SDL_Color){ .r = 0, .g = 0, .b = 0, .a = 255 }; /* Black */
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
    case STT_EOE:
    default:
      *color = (SDL_Color){ .r = 255, .g = 0, .b = 0, .a = 255 }; /* Red Error */
      break;
  }
}

static const spooky_tile * spooky_tiles_manager_get_tiles(const spooky_tiles_manager * self) {
  return self->data->tiles;
}
