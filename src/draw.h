#ifndef DRAW_H
#define DRAW_H

#include "pd_api.h"
#include "common_defs.h"

void draw_room(PlaydateAPI *pd, Room_t *room_ptr, Vector2Int_t offset);
void draw_adjacent_rooms(PlaydateAPI *pd, Vector2Int_t offset);
void gameplay_draw(void);

#endif
