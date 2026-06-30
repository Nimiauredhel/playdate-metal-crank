#ifndef COMMON_H
#define COMMON_H

#include "pd_api.h"
#include "common_defs.h"

extern const char bitmap_paths[BITMAP_COUNT][16];
extern const char* fontpath;
extern const int mov_accel_min;
extern const int mov_accel_max;
extern const int mov_speed_min;
extern const int mov_speed_max;
extern const Vector2Int_t default_camera_offset;

extern const Vector2Int_t direction_vectors[4];
extern const Vector2Int_t direction_vectors_tile_px[4];
extern const Vector2Int_t adjacent_room_offsets[4];

extern PlaydateAPI *pd_s;
extern SerializableState_t ser;
extern EphemeralState_t eph;
extern bool map;

#endif
