#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

#define TILE_SIZE_PX (40)
#define TILE_OFFSET_PX (TILE_SIZE_PX / 2)
#define TILE_COLL_PX (16)
#define TEXT_WIDTH (86)
#define TEXT_HEIGHT (16)

#define LEVEL_WIDTH (16)
#define LEVEL_HEIGHT (16)
#define ROOM_WIDTH (16)
#define ROOM_HEIGHT (16)

typedef struct Vector2
{
    int x;
    int y;
} Vector2_t;

typedef struct Entity
{
    Vector2_t position_px;
    int bitmap_idx;
} Entity_t;

typedef struct Tile
{
    int type_idx;
    int bitmap_idx;
} Tile_t;

typedef struct Room
{
    Vector2_t coord;
    Tile_t tiles[ROOM_WIDTH*ROOM_HEIGHT];
} Room_t;

typedef struct Level
{
    Room_t rooms[LEVEL_WIDTH*LEVEL_HEIGHT];
} Level_t;

static int update(void* userdata);

static const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
static const int mov_speed = 3;

static const Vector2_t default_camera_offset = { 200, 120 };
static Vector2_t camera_offset = default_camera_offset;

static LCDFont* font = NULL;

static Entity_t entities[64] = {0};
static LCDBitmap* bitmaps[32] = {0};

static Level_t level = {0};
uint16_t current_room_idx;
static Room_t *current_room = level.rooms+0;

static int bitmap_count = 0;
static int entity_count = 0;

static int player_entity_idx = -1;

static PDButtons buttons_current = {0};
static PDButtons buttons_pushed = {0};
static PDButtons buttons_released = {0};

int text_x = (400-TEXT_WIDTH)/2;
int text_y = (240-TEXT_HEIGHT)/2;
int dx = 1;
int dy = 2;

#ifdef _WINDLL
__declspec(dllexport)
#endif

static uint8_t tile_type_at_pos(Room_t *room, int tile_x, int tile_y)
{
    if (tile_x < 0 || tile_y < 0) return 1;
    return room->tiles[tile_x + (tile_y * ROOM_WIDTH)].type_idx;
}

static void populate_room(uint16_t level_x, uint16_t level_y, bool player_start)
{
    Room_t *room = level.rooms + level_x + (level_y * LEVEL_WIDTH);
    room->coord.x = level_x;
    room->coord.y = level_y;
    // randomly place walls in level
    uint16_t max_walls = (ROOM_WIDTH * ROOM_HEIGHT) / 2;
    uint16_t walls_count = 0;
    bool placed_player = false;
    Vector2_t player_coord = {0};

    for (int x = 0; x < ROOM_WIDTH; x++)
    {
        for (int y = 0; y < ROOM_HEIGHT; y++)
        {
            Tile_t *tile = room->tiles+(x + (ROOM_HEIGHT * y));

            if (x == 0 || y == 0 || x == ROOM_WIDTH - 1 || y == ROOM_HEIGHT - 1)
            {
                if (x == ROOM_WIDTH/2)
                {
                    tile->bitmap_idx = 4;
                    tile->type_idx = 2;
                }
                else if (y == ROOM_HEIGHT/2)
                {
                    tile->bitmap_idx = 3;
                    tile->type_idx = 2;
                }
                else
                {
                    tile->bitmap_idx = 2;
                    tile->type_idx = 1;
                }
            }
            else if(walls_count < max_walls && (rand() % 100) > 70)
            {
                walls_count++;
                tile->bitmap_idx = 2;
                tile->type_idx = 1;
            }
            else
            {
                tile->bitmap_idx = 1;
                tile->type_idx = 0;

                if (player_start && (!placed_player || (rand() % 100) > 80))
                {
                    player_coord.x = x;
                    player_coord.y = y;
                    placed_player = true;
                }
            }
        }
    }

    if (player_start)
    {
        // place player
        player_entity_idx = entity_count;
        entity_count++;
        entities[player_entity_idx].bitmap_idx = 0;
        entities[player_entity_idx].position_px.x = player_coord.x * TILE_SIZE_PX;
        entities[player_entity_idx].position_px.y = player_coord.y * TILE_SIZE_PX;
        current_room_idx = level_x + (level_y * LEVEL_WIDTH);
        current_room = room;
    }
}

void populate_level(void)
{
    uint16_t start_x = rand() % LEVEL_WIDTH;
    uint16_t start_y = rand() % LEVEL_HEIGHT;

    for (uint16_t x = 0; x < LEVEL_WIDTH; x++)
    {
        for (uint16_t y = 0; y < LEVEL_HEIGHT; y++)
        {
            populate_room(x, y, x == start_x && y == start_y);
        }
    }
}

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if (event == kEventInit)
	{
        srand(pd->system->getSecondsSinceEpoch(NULL));

		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);

		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

        bitmaps[0] = pd->graphics->loadBitmap("player.png", &err);
        bitmaps[1] = pd->graphics->loadBitmap("floor.png", &err);
        bitmaps[2] = pd->graphics->loadBitmap("wall.png", &err);
        bitmaps[3] = pd->graphics->loadBitmap("doorh.png", &err);
        bitmaps[4] = pd->graphics->loadBitmap("doorv.png", &err);
        // TODO: handle error like the above font loading
        bitmap_count = 5;

        populate_level();

        camera_offset.x = default_camera_offset.x - (entities[player_entity_idx].position_px.x);
        camera_offset.y = default_camera_offset.y - (entities[player_entity_idx].position_px.y);

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}

static int update(void* userdata)
{
    static uint8_t prev_coll_type = 0;
    static Vector2_t coll_tiles[4] = {0};

	PlaydateAPI* pd = userdata;

    char text_buff[32] = {0};

    // get input
    pd->system->getButtonState(&buttons_current, &buttons_pushed, &buttons_released);

    // process input
    if (player_entity_idx >= 0)
    {
        Entity_t *player = entities+player_entity_idx;
        Vector2_t mov_delta = {0};

        if (buttons_current & kButtonLeft)
        {
            mov_delta.x -= mov_speed;
        }

        if (buttons_current & kButtonRight)
        {
            mov_delta.x += mov_speed;
        }

        if (buttons_current & kButtonUp)
        {
            mov_delta.y -= mov_speed;
        }

        if (buttons_current & kButtonDown)
        {
            mov_delta.y += mov_speed;
        }

        if (mov_delta.x != 0 || mov_delta.y != 0)
        {
            Vector2_t new_pos = { player->position_px.x + mov_delta.x, player->position_px.y + mov_delta.y };
            Vector2_t new_offset_pos = { new_pos.x + TILE_OFFSET_PX, new_pos.y + TILE_OFFSET_PX };

            coll_tiles[0].x = (new_offset_pos.x - TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[0].y = (new_offset_pos.y - TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[1].x = (new_offset_pos.x + TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[1].y = (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[2].x = (new_offset_pos.x - TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[2].y = (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[3].x = (new_offset_pos.x + TILE_COLL_PX) / TILE_SIZE_PX;
            coll_tiles[3].y = (new_offset_pos.y - TILE_COLL_PX) / TILE_SIZE_PX;

            uint8_t coll_type = 0;
            Vector2_t coll_tile = {0};

            for (int i = 0; i < 4; i++)
            {
                uint8_t coll = tile_type_at_pos(current_room, coll_tiles[i].x, coll_tiles[i].y);

                if (coll > coll_type)
                {
                    coll_type = coll;
                    coll_tile = coll_tiles[i];
                }
            }

            if (coll_type == 2 && prev_coll_type != 2)
            {
                if (coll_tile.x == 0)
                {
                    current_room_idx -= 1;
                    new_pos.x = TILE_SIZE_PX * (ROOM_WIDTH - 1);
                    new_pos.y = TILE_SIZE_PX * (ROOM_HEIGHT / 2);
                }
                else if (coll_tile.x == ROOM_WIDTH - 1)
                {
                    current_room_idx += 1;
                    new_pos.x = TILE_SIZE_PX * (0);
                    new_pos.y = TILE_SIZE_PX * (ROOM_HEIGHT / 2);
                }
                else if (coll_tile.y == 0)
                {
                    current_room_idx -= LEVEL_WIDTH;
                    new_pos.x = TILE_SIZE_PX * (ROOM_WIDTH / 2);
                    new_pos.y = TILE_SIZE_PX * (ROOM_HEIGHT -1);
                }
                else if (coll_tile.y == ROOM_HEIGHT - 1)
                {
                    current_room_idx += LEVEL_WIDTH;
                    new_pos.x = TILE_SIZE_PX * (ROOM_WIDTH / 2);
                    new_pos.y = TILE_SIZE_PX * (0);
                }

                current_room = &level.rooms[current_room_idx];
            }

            if (coll_type != 1)
            {
                player->position_px = new_pos;

                camera_offset.x = default_camera_offset.x - (entities[player_entity_idx].position_px.x);
                camera_offset.y = default_camera_offset.y - (entities[player_entity_idx].position_px.y);
            }

            prev_coll_type = coll_type;
        }
    }
	
    // draw gfx
    Vector2_t draw_pos = {0};

	pd->graphics->clear(kColorWhite);
	pd->graphics->setFont(font);
	pd->graphics->drawText("Hello World!", strlen("Hello World!"), kASCIIEncoding, text_x, text_y);

    if (current_room != NULL)
    {
        snprintf(text_buff, sizeof(text_buff), "[%d,%d]", coll_tiles[0].x, coll_tiles[0].y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 4, 16);
        snprintf(text_buff, sizeof(text_buff), "[%d,%d]", coll_tiles[1].x, coll_tiles[1].y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 40, 32);
        snprintf(text_buff, sizeof(text_buff), "[%d,%d]", coll_tiles[2].x, coll_tiles[2].y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 4, 32);
        snprintf(text_buff, sizeof(text_buff), "[%d,%d]", coll_tiles[3].x, coll_tiles[3].y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 40, 16);
        snprintf(text_buff, sizeof(text_buff), "Room [%d,%d]", current_room->coord.x, current_room->coord.y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 0, 48);

        for (int x = 0; x < ROOM_WIDTH; x++)
        {
            for (int y = 0; y < ROOM_HEIGHT; y++)
            {
                Tile_t *tile = current_room->tiles+(x + (ROOM_WIDTH * y));
                draw_pos.x = TILE_OFFSET_PX + (x * TILE_SIZE_PX) + camera_offset.x;
                draw_pos.y = TILE_OFFSET_PX + (y * TILE_SIZE_PX) + camera_offset.y;
                pd->graphics->drawBitmap(bitmaps[tile->bitmap_idx],
                        draw_pos.x, draw_pos.y, kBitmapUnflipped);
            }
        }

        for (int i = 0; i < entity_count; i++)
        {
            draw_pos.x = TILE_OFFSET_PX + entities[i].position_px.x + camera_offset.x;
            draw_pos.y = TILE_OFFSET_PX + entities[i].position_px.y + camera_offset.y;
            pd->graphics->drawBitmap(bitmaps[entities[i].bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);
        }
    }

	text_x += dx;
	text_y += dy;
	
	if ( text_x < 0 || text_x > LCD_COLUMNS - TEXT_WIDTH )
		dx = -dx;
	
	if ( text_y < 0 || text_y > LCD_ROWS - TEXT_HEIGHT )
		dy = -dy;
        
	pd->system->drawFPS(0,0);

	return 1;
}

