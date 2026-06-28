#include "pd_api.h"
#include "common_defs.h"
#include "common.h"

typedef struct ray_hit
{
    uint16_t index;
    uint16_t type;
    uint16_t side_px;
    uint16_t tx;
    uint16_t ty;
    float rx;
    float ry;
    float dist;

} ray_hit_t;

#define TAU (6.283185f)
#define PI (3.14159f)
#define HALFPI (PI*0.5f)

static const float PlayerAngle = 0.0;
static const float PlayerDirX = 1.0;
static const float PlayerDirY = 0.0;
static const float PlayerLatX = 1.0;
static const float PlayerLatY = 0.0;

static const float FieldOfView = 55.0;
static float Ratio;
static float Cone;

static uint16_t hit_count = 0;
static ray_hit_t ray_hits[400] = {0};
static uint16_t horizon_y = 90;

static void gather_rays(void)
{
	uint8_t dof_max = 32;
    uint16_t ray_count = eph.screen_size.x;
	float ray_inc = Cone / ray_count;
	float ray_angle = PlayerAngle - Cone/2.0f;

    hit_count = 0;

    for (uint8_t ray = 0; ray < ray_count; ray++)
    {
		if (ray_angle < 0.0f) ray_angle = ray_angle + TAU;
		else if (ray_angle > TAU) ray_angle -= TAU;

		uint16_t dof = 0;

		float xoff = cosf(ray_angle);
		float yoff = sinf(ray_angle);
		float x_delta, y_delta, x_side, y_side = 0.0f;
        uint16_t x_step, y_step = 1;

		if (xoff == 0) x_delta = 1e30;
		else x_delta = fabsf(1.0f/xoff);

		if (yoff == 0) y_delta = 1e30;
		else y_delta = fabsf(1.0f/yoff);

        uint16_t PlayerX = eph.player_ptr->entity.position_px.x;
        uint16_t PlayerY = eph.player_ptr->entity.position_px.y;

		if (xoff < 0.0f)
        {
			x_side = (PlayerX - floorf(PlayerX)) * x_delta;
			x_step = -1;
        }
		else x_side = (floorf(PlayerX) + 1.0f - PlayerX) * x_delta;

		if (yoff < 0.0f)
        {
			y_side = (PlayerY - floorf(PlayerY)) * y_delta;
			y_step = -1;
        }
		else y_side = (floorf(PlayerY) + 1.0f - PlayerY) * y_delta;

		uint16_t tx = floorf(PlayerX);
        uint16_t ty = floorf(PlayerY);
		uint8_t side = 0;

		while (dof < dof_max)
        {
			dof = dof + 1;
			if (x_side < y_side)
            {
				side = 0;
				x_side += x_delta;
				tx += x_step;
            }
			else
            {
				side = 1;
				y_side += y_delta;
				ty += y_step;
            }
			if (tx < ROOM_WIDTH && ty < ROOM_HEIGHT && tx >=0 && ty >= 0)
            {
				uint8_t hit = eph.current_room_ptr->tiles[tx+(ty*ROOM_WIDTH)].bitmap_idx;

				if (hit > 0)
                {
					dof = dof_max;
					float dist = 0.0f;
					float wall_x = 0.0f;
					uint16_t side_px = 1;

					if (side == 0) dist = x_side - x_delta;
					else dist = y_side - y_delta;

					side_px = 1+((dist-floorf(dist))*64);

					if (dist < 0.001f) dist = 0.001f;

					float rx = PlayerX + xoff * dist;
					float ry = PlayerY + yoff * dist;

					if (side == 0) wall_x = ry;
					else wall_x = rx;

					wall_x -= floorf(wall_x);
					side_px = 1+wall_x * 64;

                    ray_hits[hit_count].index = ray;
                    ray_hits[hit_count].type = hit;
                    ray_hits[hit_count].rx = rx;
                    ray_hits[hit_count].ry = ry;
                    ray_hits[hit_count].tx = tx;
                    ray_hits[hit_count].ty = ty;
                    ray_hits[hit_count].dist = dist;
                    ray_hits[hit_count].side_px = side_px;
					hit_count++;
                }
            }
        }
        ray_angle = ray_angle + ray_inc;
    }
}

static void draw_constants(void)
{
    pd_s->graphics->fillRect(0, 0, eph.screen_size.x, horizon_y, kColorBlack);
}

static void draw_raycast(void)
{
}

static void draw_sprites(void)
{
}

static void draw_map(void)
{
}

void raycasting_draw(void)
{
    horizon_y = eph.screen_size.y * 0.6;
    Ratio = (float)eph.screen_size.x/(float)eph.screen_size.y;
    Cone = (FieldOfView / 360.0f) * TAU * Ratio;
    gather_rays();
    draw_constants();
    draw_raycast();
    draw_sprites();
    draw_map();
}
/*
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

    if (eph.current_room_ptr != NULL)
    {
        draw_room(pd_s, eph.current_room_ptr, eph.camera_offset);
        draw_adjacent_rooms(pd_s, eph.camera_offset);

        snprintf(text_buff, sizeof(text_buff), "Room [%d,%d]", eph.current_room_ptr->coord.x, eph.current_room_ptr->coord.y);
        pd_s->graphics->fillRect(0, 48, TEXT_WIDTH, TEXT_HEIGHT, kColorWhite);
        pd_s->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 0, 48);
    }

	pd_s->system->drawFPS(0,0);
}
*/
