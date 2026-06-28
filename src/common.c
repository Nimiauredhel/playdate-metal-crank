#include "common_defs.h"
#include "pd_api.h"

const char bitmap_paths[BITMAP_COUNT][16] =
{
    "floor00.png",
    "floor01.png",
    "floor02.png",
    "floor03.png",
    "wall.png",
    "table.png",
    "crate.png",
    "doorh.png",
    "doorv.png",
    "player.png",
    "npc.png",
};
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
const int mov_accel_min = 15;
const int mov_accel_max = 30;
const int mov_speed_min = 125;
const int mov_speed_max = 250;
const Vector2Int_t default_camera_offset = { 200, 120 };

const Vector2Int_t direction_vectors[4] =
{
    { .x = -1, .y = 0 },
    { .x = 0, .y = -1 },
    { .x = 1, .y = 0 },
    { .x = 0, .y = 1 },
};
const Vector2Int_t direction_vectors_tile_px[4] =
{
    { .x = -1*TILE_SIZE_PX, .y = 0 },
    { .x = 0, .y = -1*TILE_SIZE_PX },
    { .x = 1*TILE_SIZE_PX, .y = 0 },
    { .x = 0, .y = 1*TILE_SIZE_PX },
};
const Vector2Int_t adjacent_room_offsets[4] =
{
    {-(ROOM_WIDTH*TILE_SIZE_PX), 0},
    {0, -(ROOM_HEIGHT*TILE_SIZE_PX)},
    {+(ROOM_WIDTH*TILE_SIZE_PX), 0},
    {0, +(ROOM_HEIGHT*TILE_SIZE_PX)},
};

PlaydateAPI *pd_s = NULL;
SerializableState_t ser = {0};
EphemeralState_t eph = {0};
