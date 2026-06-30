#include "pd_api.h"
#include "common_defs.h"
#include "common.h"
#include "raycasting.h"

void draw_room(PlaydateAPI *pd, Room_t *room_ptr, Vector2Int_t offset)
{
    static const int draw_min = -TILE_SIZE_PX;

    const Vector2Int_t draw_max = { eph.screen_size.x - 1, eph.screen_size.y - 1 };

    Vector2Int_t draw_pos = {0};

    for (int x = 0; x < ROOM_WIDTH; x++)
    {
        draw_pos.x = eph.room_draw_positions.x[x] + offset.x;

        if (draw_pos.x < draw_min) continue;
        if (draw_pos.x > draw_max.x) break;

        for (int y = 0; y < ROOM_HEIGHT; y++)
        {
            draw_pos.y = eph.room_draw_positions.y[y] + offset.y;

            if (draw_pos.y < draw_min) continue;
            if (draw_pos.y > draw_max.y) break;

            Tile_t *tile = room_ptr->tiles+(x + (ROOM_WIDTH * y));
            pd->graphics->drawBitmap(eph.bitmaps[tile->bitmap_idx],
                    draw_pos.x, draw_pos.y, kBitmapUnflipped);
        }
    }

    Entity_t *entity = NULL;
    Triangle2D_t vision_cone = {0};

    for (uint8_t i = 0; i < room_ptr->local_entity_count; i++)
    {
        entity = room_ptr->entities+i;

        draw_pos.x = TILE_OFFSET_PX + entity->position_px.x + offset.x;
        if (draw_pos.x < draw_min || draw_pos.x > draw_max.x) continue;

        draw_pos.y = TILE_OFFSET_PX + entity->position_px.y + offset.y;
        if (draw_pos.y < draw_min || draw_pos.y > draw_max.y) continue;

        pd->graphics->drawBitmap(eph.bitmaps[entity->bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);

        vision_cone.a.x = draw_pos.x + TILE_OFFSET_PX;
        vision_cone.a.y = draw_pos.y + TILE_OFFSET_PX;
        vision_cone.b.x = vision_cone.a.x + ((direction_vectors[entity->heading].x + direction_vectors[entity->heading].y) * TILE_SIZE_PX * 2);
        vision_cone.b.y = vision_cone.a.y + ((direction_vectors[entity->heading].y - direction_vectors[entity->heading].x) * TILE_SIZE_PX * 2);
        vision_cone.c.x = vision_cone.a.x + ((direction_vectors[entity->heading].x - direction_vectors[entity->heading].y) * TILE_SIZE_PX * 2);
        vision_cone.c.y = vision_cone.a.y + ((direction_vectors[entity->heading].y + direction_vectors[entity->heading].x) * TILE_SIZE_PX * 2);

        pd->graphics->drawLine(vision_cone.a.x, vision_cone.a.y,
                              vision_cone.b.x, vision_cone.b.y,
                              4, kColorBlack);
        pd->graphics->drawLine(vision_cone.a.x, vision_cone.a.y,
                              vision_cone.c.x, vision_cone.c.y,
                              4, kColorBlack);
        pd->graphics->drawLine(vision_cone.b.x, vision_cone.b.y,
                              vision_cone.c.x, vision_cone.c.y,
                              4, kColorWhite);
    }

    uint16_t room_idx = room_ptr->coord.x + ((room_ptr->coord.y) * LEVEL_WIDTH);

    for (uint8_t i = 0; i < ser.global_entity_count; i++)
    {
        if (ser.global_entities[i].current_room_idx == room_idx)
        {
            entity = &(ser.global_entities+i)->entity;

            draw_pos.x = TILE_OFFSET_PX + entity->position_px.x + offset.x;
            if (draw_pos.x < draw_min || draw_pos.x > draw_max.x) continue;

            draw_pos.y = TILE_OFFSET_PX + entity->position_px.y + offset.y;
            if (draw_pos.y < draw_min || draw_pos.y > draw_max.y) continue;

            pd->graphics->drawBitmap(eph.bitmaps[entity->bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);
        }
    }
}

void draw_adjacent_rooms(PlaydateAPI *pd, Vector2Int_t offset)
{
    Vector2Int_t neighbour_offset = offset;

    for (uint8_t i = 0; i < 4; i++)
    {
        if (eph.adjacent_room_ptrs[i] != NULL)
        {
            neighbour_offset.x = offset.x+adjacent_room_offsets[i].x;
            neighbour_offset.y = offset.y+adjacent_room_offsets[i].y;
            draw_room(pd, eph.adjacent_room_ptrs[i], neighbour_offset);
        }
    }
}

void gameplay_draw(void)
{
    // draw gfx
    static char text_buff[32] = {0};

	pd_s->graphics->clear(kColorWhite);
	pd_s->graphics->setFont(eph.font);

    if (map)
    {
        if (eph.current_room_ptr != NULL)
        {
            draw_room(pd_s, eph.current_room_ptr, eph.camera_offset);
            draw_adjacent_rooms(pd_s, eph.camera_offset);

            snprintf(text_buff, sizeof(text_buff), "Room [%d,%d]", eph.current_room_ptr->coord.x, eph.current_room_ptr->coord.y);
            pd_s->graphics->fillRect(0, 48, TEXT_WIDTH, TEXT_HEIGHT, kColorWhite);
            pd_s->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 0, 48);
        }
    }
    else
    {
        raycasting_draw();
        snprintf(text_buff, sizeof(text_buff), "Angle [%.2f]", eph.PlayerAngle);
        pd_s->graphics->fillRect(0, 48, TEXT_WIDTH, TEXT_HEIGHT, kColorWhite);
        pd_s->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 0, 48);
    }

	pd_s->system->drawFPS(0,0);
}
