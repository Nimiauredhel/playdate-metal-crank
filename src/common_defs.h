#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include "pd_api.h"
#include <stdint.h>

#define TILE_SIZE_PX (40)
#define TILE_OFFSET_PX (TILE_SIZE_PX / 2)
#define TILE_COLL_PX (16)

#define TEXT_WIDTH (86)
#define TEXT_HEIGHT (16)

#define LEVEL_WIDTH (16)
#define LEVEL_HEIGHT (16)
#define LEVEL_MIN_X (0)
#define LEVEL_MIN_Y (0)
#define LEVEL_MAX_X (LEVEL_WIDTH-1)
#define LEVEL_MAX_Y (LEVEL_HEIGHT-1)

#define ROOM_COUNT (LEVEL_WIDTH*LEVEL_HEIGHT)
#define ROOM_WIDTH (16)
#define ROOM_HEIGHT (16)
#define ROOM_MIN_X (0)
#define ROOM_MIN_Y (0)
#define ROOM_MAX_X (ROOM_WIDTH-1)
#define ROOM_MAX_Y (ROOM_HEIGHT-1)

#define ROOM_MID_X (ROOM_WIDTH/2)
#define ROOM_MID_Y (ROOM_HEIGHT/2)

#define ENTITIES_GLOBAL_MAX (16)
#define ENTITIES_LOCAL_MAX (4)
#define BITMAP_SIZE (419)
#define BITMAP_COUNT (11)

typedef enum CellType
{
    CELL_BORDER = -2,
    CELL_CLOSED = -1,
    CELL_PATH_0 = 0,
    CELL_PATH_1 = 1,
    CELL_PATH_2 = 2,
    CELL_PATH_3 = 3,
} CellType_t;

typedef enum Direction
{
    DIR_NONE = -1,
    DIR_LEFT = 0,
    DIR_UP = 1,
    DIR_RIGHT = 2,
    DIR_DOWN = 3,
    DIR_COUNT = 4,
} Direction_t;

typedef enum BitmapIndices
{
    BITMAP_FLOOR_00 = 0,
    BITMAP_FLOOR_01 = 1,
    BITMAP_FLOOR_02 = 2,
    BITMAP_FLOOR_03 = 3,
    BITMAP_WALL = 4,
    BITMAP_TABLE = 5,
    BITMAP_CRATE = 6,
    BITMAP_DOOR_H = 7,
    BITMAP_DOOR_V = 8,
    BITMAP_PLAYER = 9,
    BITMAP_NPC = 10,
} BitmapIndices_t;

typedef enum TileFlags
{
    TILEFLAG_NONE = 0x00,
    TILEFLAG_WALKABLE = 0x01,
    TILEFLAG_DOOR_H = 0x02,
    TILEFLAG_DOOR_V = 0x04,
} TileFlags_t;

typedef enum GamePhase
{
    PHASE_PREINIT = 0,
    PHASE_GAMEPLAY = 1,
    PHASE_PAUSED = 2,
    PHASE_TERMINATING = 3,
} GamePhase_t;

typedef struct Vector2Int
{
    int x;
    int y;
} Vector2Int_t;

typedef struct Vector2
{
    float x;
    float y;
} Vector2_t;

typedef struct Vector3
{
    float x;
    float y;
    float z;
} Vector3_t;

typedef struct Triangle2D
{
    Vector2Int_t a;
    Vector2Int_t b;
    Vector2Int_t c;
} Triangle2D_t;

typedef struct Entity
{
    Vector2Int_t position_px;
    Vector2Int_t mov_speed;
    Direction_t heading;
    int bitmap_idx;
} Entity_t;

typedef struct GlobalEntity
{
    uint16_t current_room_idx;
    Entity_t entity;
} GlobalEntity_t;

typedef struct Tile
{
    TileFlags_t flags;
    int bitmap_idx;
} Tile_t;

typedef struct Room
{
    Vector2Int_t coord;
    Tile_t tiles[ROOM_WIDTH*ROOM_HEIGHT];
    uint8_t local_entity_count;
    Entity_t entities[ENTITIES_LOCAL_MAX];
} Room_t;

typedef struct RoomDrawPositions
{
    int32_t x[ROOM_WIDTH];
    int32_t y[ROOM_HEIGHT];
} RoomDrawPositions_t;

typedef struct Level
{
    Room_t rooms[LEVEL_WIDTH*LEVEL_HEIGHT];
} Level_t;

typedef struct SerializableState
{
    uint16_t current_room_idx;
    Level_t level;

    uint8_t global_entity_count;
    int8_t player_entity_idx;
    GlobalEntity_t global_entities[ENTITIES_GLOBAL_MAX];
} SerializableState_t;

typedef struct EphemeralState
{
    GamePhase_t phase;
    float delta_time;
    Vector2Int_t screen_size;
    PDButtons buttons_current;
    PDButtons buttons_pushed;
    PDButtons buttons_released;
    Vector3_t accelerometer_center;
    Vector3_t accelerometer_raw;
    Vector2_t crank;
    Vector2Int_t camera_offset_target;
    Vector2Int_t camera_offset;
    Vector2Int_t camera_peek_offset;
    GlobalEntity_t *player_ptr;
    Room_t *current_room_ptr;
    Room_t *adjacent_room_ptrs[4];
    RoomDrawPositions_t room_draw_positions;
    LCDFont* font;
    uint8_t bitmaps_buffer[BITMAP_COUNT][BITMAP_SIZE];
    LCDBitmap *bitmaps[BITMAP_COUNT];
} EphemeralState_t;

#endif
