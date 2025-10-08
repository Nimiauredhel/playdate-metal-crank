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
#define BITMAP_COUNT (5)

typedef enum BitmapIndices
{
    BITMAP_WALL = 0,
    BITMAP_FLOOR = 1,
    BITMAP_PLAYER = 2,
    BITMAP_DOOR_H = 3,
    BITMAP_DOOR_V = 4,
} BitmapIndices_t;

typedef enum TileFlags
{
    TILEFLAG_NONE = 0x00,
    TILEFLAG_WALKABLE = 0x01,
    TILEFLAG_DOOR_H = 0x02,
    TILEFLAG_DOOR_V = 0x04,
} TileFlags_t;

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
    Vector2_t coord;
    Tile_t tiles[ROOM_WIDTH*ROOM_HEIGHT];
    uint8_t local_entity_count;
    Entity_t entities[ENTITIES_LOCAL_MAX];
} Room_t;

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
    PDButtons buttons_current;
    PDButtons buttons_pushed;
    PDButtons buttons_released;
    Vector2_t camera_offset_target;
    Vector2_t camera_offset;
    GlobalEntity_t *player_ptr;
    Room_t *current_room_ptr;
    LCDFont* font;
    LCDBitmap* bitmaps[BITMAP_COUNT];
} EphemeralState_t;

static int update(void* userdata);

static const char bitmap_paths[BITMAP_COUNT][16] =
{
    "wall.png",
    "floor.png",
    "player.png",
    "doorh.png",
    "doorv.png",
};
static const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
static const int mov_speed = 200;
static const Vector2_t default_camera_offset = { 200, 120 };

static PlaydateAPI *pd_s = NULL;
static SerializableState_t ser = {0};
static EphemeralState_t eph = {0};

#ifdef _WINDLL
__declspec(dllexport)
#endif

static void set_player_room(uint16_t room_idx)
{
    if (eph.player_ptr == NULL) return;
    pd_s->system->logToConsole("Moving player to room #%d.", room_idx);
    eph.player_ptr->current_room_idx = room_idx;
}

static void set_current_room(uint16_t room_idx)
{
    pd_s->system->logToConsole("Setting current room to #%d.", room_idx);
    ser.current_room_idx = room_idx;
    eph.current_room_ptr = ser.level.rooms+room_idx;
}

static TileFlags_t tile_flags_at_pos(Room_t *room, int tile_x, int tile_y)
{
    if (tile_x < ROOM_MIN_X || tile_x > ROOM_MAX_X
     || tile_y < ROOM_MIN_Y || tile_y > ROOM_MAX_Y) return 0;
    return room->tiles[tile_x + (tile_y * ROOM_WIDTH)].flags;
}

static void populate_room(uint16_t level_x, uint16_t level_y, bool player_start)
{
    pd_s->system->logToConsole("Populating room [%d,%d].", level_x, level_y);

    uint16_t level_idx = level_x + (level_y * LEVEL_WIDTH);
    Room_t *room = ser.level.rooms + level_idx;
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
            Tile_t *tile = room->tiles+(x + (ROOM_WIDTH * y));

             /**
             * All fields are expected to be zero-initialized and adhere to zero-as-default,
             * which should have the same effect as if this code were here:
             *
             * tile->bitmap_idx = BITMAP_WALL;
             * tile->flags = TILEFLAG_NONE;
             *
             * TODO: create room generation tests to enforce this assumption.
             **/

            // left/right doors
            if ((x == ROOM_MID_X)
                && ((y == 0 && level_y > 0)
                ||  (y == ROOM_MAX_Y && level_y < LEVEL_MAX_Y)))
            {
                tile->bitmap_idx = BITMAP_DOOR_V;
                tile->flags |= (TILEFLAG_DOOR_V | TILEFLAG_WALKABLE);
            }
            // up/down doors
            else if ((y == ROOM_MID_Y)
                && ((x == 0 && level_x > 0)
                ||  (x == ROOM_MAX_X && level_x < LEVEL_MAX_X)))
            {
                tile->bitmap_idx = BITMAP_DOOR_H;
                tile->flags |= (TILEFLAG_DOOR_H | TILEFLAG_WALKABLE);
            }
            else if (x > ROOM_MIN_X && x < ROOM_MAX_X
                    && y > ROOM_MIN_Y && y < ROOM_MAX_Y)
            {
                if(walls_count < max_walls && (rand() % 100) > 70)
                {
                    walls_count++;
                    // nothing to set since unwalkable wall == 0, 0
                    //tile->bitmap_idx = 0;
                    //tile->type_idx = 1;
                }
                else
                {
                    tile->bitmap_idx = BITMAP_FLOOR;
                    tile->flags |= TILEFLAG_WALKABLE;

                    if (player_start && (!placed_player || (rand() % 100) > 80))
                    {
                        player_coord.x = x;
                        player_coord.y = y;
                        placed_player = true;
                    }
                }
            }
        }
    }

    if (player_start && eph.player_ptr != NULL)
    {
        // place player
        uint16_t room_idx = level_x + (level_y * LEVEL_WIDTH);
        eph.player_ptr->entity.position_px.x = player_coord.x * TILE_SIZE_PX;
        eph.player_ptr->entity.position_px.y = player_coord.y * TILE_SIZE_PX;
        set_player_room(room_idx);
        set_current_room(room_idx);
    }
}

void populate_level(void)
{
    pd_s->system->logToConsole("Initializing level.");
    uint16_t start_x = rand() % LEVEL_WIDTH;
    uint16_t start_y = rand() % LEVEL_HEIGHT;

    ser.player_entity_idx = ser.global_entity_count;
    ser.global_entity_count++;
    eph.player_ptr = ser.global_entities+ser.player_entity_idx;
    eph.player_ptr->entity.bitmap_idx = BITMAP_PLAYER;

    for (uint16_t x = 0; x < LEVEL_WIDTH; x++)
    {
        for (uint16_t y = 0; y < LEVEL_HEIGHT; y++)
        {
            populate_room(x, y, x == start_x && y == start_y);
        }
    }
}

static void draw_room(PlaydateAPI *pd, Room_t *room_ptr, Vector2_t offset)
{
    Vector2_t draw_pos = {0};

    for (int x = 0; x < ROOM_WIDTH; x++)
    {
        for (int y = 0; y < ROOM_HEIGHT; y++)
        {
            Tile_t *tile = room_ptr->tiles+(x + (ROOM_WIDTH * y));
            draw_pos.x = TILE_OFFSET_PX + (x * TILE_SIZE_PX) + offset.x;
            draw_pos.y = TILE_OFFSET_PX + (y * TILE_SIZE_PX) + offset.y;
            pd->graphics->drawBitmap(eph.bitmaps[tile->bitmap_idx],
                    draw_pos.x, draw_pos.y, kBitmapUnflipped);
        }
    }

    Entity_t *entity = NULL;

    for (uint8_t i = 0; i < room_ptr->local_entity_count; i++)
    {
        entity = room_ptr->entities+i;
        draw_pos.x = TILE_OFFSET_PX + entity->position_px.x + offset.x;
        draw_pos.y = TILE_OFFSET_PX + entity->position_px.y + offset.y;
        pd->graphics->drawBitmap(eph.bitmaps[entity->bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);
    }

    uint16_t room_idx = room_ptr->coord.x + ((room_ptr->coord.y) * LEVEL_WIDTH);

    for (uint8_t i = 0; i < ser.global_entity_count; i++)
    {
        if (ser.global_entities[i].current_room_idx == room_idx)
        {
            entity = &(ser.global_entities+i)->entity;
            draw_pos.x = TILE_OFFSET_PX + entity->position_px.x + offset.x;
            draw_pos.y = TILE_OFFSET_PX + entity->position_px.y + offset.y;
            pd->graphics->drawBitmap(eph.bitmaps[entity->bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);
        }
    }
}

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if (event == kEventInit)
	{
		const char* err;
        pd_s = pd;

        pd->system->logToConsole("Initializing game.");

        srand(pd->system->getSecondsSinceEpoch(NULL));

        bzero(&ser, sizeof(ser));
        bzero(&eph, sizeof(eph));

        eph.camera_offset_target = default_camera_offset;
        eph.font = pd->graphics->loadFont(fontpath, &err);
		
		if ( eph.font == NULL )
        {
            pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);
        }

        for (uint16_t i = 0; i < BITMAP_COUNT; i++)
        {
            eph.bitmaps[i] = pd->graphics->loadBitmap(bitmap_paths[i], &err);

            if (eph.bitmaps[i] == NULL )
            {
                pd->system->error("%s:%i Couldn't load bitmap %s: %s", __FILE__, __LINE__, bitmap_paths[i], err);
            }
            else
            {
                pd->system->logToConsole("Loaded bitmap [%s] to index [%d].", bitmap_paths[i], i);
            }
        }

        populate_level();

        eph.camera_offset_target.x = (default_camera_offset.x - eph.player_ptr->entity.position_px.x) - TILE_SIZE_PX;
        eph.camera_offset_target.y = (default_camera_offset.y - eph.player_ptr->entity.position_px.y) - TILE_SIZE_PX;
        eph.camera_offset = eph.camera_offset_target;

        pd->display->setRefreshRate(50);
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
        pd->system->resetElapsedTime();
	}
	
	return 0;
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;

    float deltaTime = pd->system->getElapsedTime();
    pd->system->resetElapsedTime();

    char text_buff[32] = {0};

    // get input
    pd->system->getButtonState(&eph.buttons_current, &eph.buttons_pushed, &eph.buttons_released);

    // process input
    if (eph.player_ptr != NULL)
    {
        Vector2_t mov_delta = {0};

        if (eph.buttons_current & kButtonLeft)
        {
            mov_delta.x -= mov_speed * deltaTime;
        }

        if (eph.buttons_current & kButtonRight)
        {
            mov_delta.x += mov_speed * deltaTime;
        }

        if (eph.buttons_current & kButtonUp)
        {
            mov_delta.y -= mov_speed * deltaTime;
        }

        if (eph.buttons_current & kButtonDown)
        {
            mov_delta.y += mov_speed * deltaTime;
        }

        if (mov_delta.x != 0 || mov_delta.y != 0)
        {
            Vector2_t current_pos = { eph.player_ptr->entity.position_px.x, eph.player_ptr->entity.position_px.y };
            Vector2_t new_pos = { current_pos.x + mov_delta.x, current_pos.y + mov_delta.y };
            Vector2_t new_offset_pos = { new_pos.x + TILE_OFFSET_PX, new_pos.y + TILE_OFFSET_PX };

            Vector2_t eval_coll_tiles[4] =
            {
                { (new_offset_pos.x - TILE_COLL_PX) / TILE_SIZE_PX, (new_offset_pos.y - TILE_COLL_PX) / TILE_SIZE_PX },
                { (new_offset_pos.x + TILE_COLL_PX) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX },
                { (new_offset_pos.x - TILE_COLL_PX) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX },
                { (new_offset_pos.x + TILE_COLL_PX) / TILE_SIZE_PX, (new_offset_pos.y - TILE_COLL_PX) / TILE_SIZE_PX },
            };
            TileFlags_t eval_tile_flags[4] =
            {
                tile_flags_at_pos(eph.current_room_ptr, eval_coll_tiles[0].x, eval_coll_tiles[0].y),
                tile_flags_at_pos(eph.current_room_ptr, eval_coll_tiles[1].x, eval_coll_tiles[1].y),
                tile_flags_at_pos(eph.current_room_ptr, eval_coll_tiles[2].x, eval_coll_tiles[2].y),
                tile_flags_at_pos(eph.current_room_ptr, eval_coll_tiles[3].x, eval_coll_tiles[3].y),
            };

            //TileFlags_t sum_tile_flags = eval_tile_flags[0] | eval_tile_flags[1] | eval_tile_flags[2] | eval_tile_flags[3];
            TileFlags_t tile_flags = TILEFLAG_NONE;
            Vector2_t coll_tile = {0};

            for (uint8_t i = 0; i < 4; i++)
            {
                coll_tile = eval_coll_tiles[i];
                tile_flags = eval_tile_flags[i];

                // if a non-walkable tile is touched, the below (position-changing) flags do not apply
                // if applicable flags are added later, they should be tested above this one
                if (!(tile_flags & TILEFLAG_WALKABLE))
                {
                    break;
                }
                else if (tile_flags & TILEFLAG_DOOR_H
                        && ((new_pos.x >= (ROOM_MAX_X * TILE_SIZE_PX) || new_pos.x <= (ROOM_MIN_X * TILE_SIZE_PX))))
                {
                    if (coll_tile.x == ROOM_MIN_X)
                    {
                        set_current_room(ser.current_room_idx - 1);
                        new_pos.x = TILE_SIZE_PX * ROOM_MAX_X;
                        eph.camera_offset.x = (default_camera_offset.x - (TILE_SIZE_PX * ROOM_MAX_X) - TILE_SIZE_PX);
                    }
                    else if (coll_tile.x == ROOM_MAX_X)
                    {
                        set_current_room(ser.current_room_idx + 1);
                        new_pos.x = TILE_SIZE_PX * ROOM_MIN_X;
                        eph.camera_offset.x = (default_camera_offset.x - (TILE_SIZE_PX * ROOM_MIN_X) - TILE_SIZE_PX);
                    }

                    set_player_room(ser.current_room_idx);
                    break;
                }
                else if (tile_flags & TILEFLAG_DOOR_V
                        && ((new_pos.y >= (ROOM_MAX_Y * TILE_SIZE_PX) || new_pos.y <= (ROOM_MIN_Y * TILE_SIZE_PX))))
                {
                    if (coll_tile.y == ROOM_MIN_Y)
                    {
                        set_current_room(ser.current_room_idx - LEVEL_WIDTH);
                        new_pos.y = TILE_SIZE_PX * ROOM_MAX_Y;
                        eph.camera_offset.y = (default_camera_offset.y - (TILE_SIZE_PX * ROOM_MAX_Y) - TILE_SIZE_PX);
                    }
                    else if (coll_tile.y == ROOM_MAX_Y)
                    {
                        set_current_room(ser.current_room_idx + LEVEL_WIDTH);
                        new_pos.y = TILE_SIZE_PX * ROOM_MIN_Y;
                        eph.camera_offset.y = (default_camera_offset.y - (TILE_SIZE_PX * ROOM_MIN_Y) - TILE_SIZE_PX);
                    }

                    set_player_room(ser.current_room_idx);
                    break;
                }
            }

            if (tile_flags & TILEFLAG_WALKABLE)
            {
                eph.player_ptr->entity.position_px = new_pos;
                eph.camera_offset_target.x = (default_camera_offset.x - eph.player_ptr->entity.position_px.x) - TILE_SIZE_PX;
                eph.camera_offset_target.y = (default_camera_offset.y - eph.player_ptr->entity.position_px.y) - TILE_SIZE_PX;
            }
        }

        float camera_follow_speed = 6.5f * deltaTime;

        if (eph.camera_offset.x > eph.camera_offset_target.x)
        {
            eph.camera_offset.x -= (eph.camera_offset.x - eph.camera_offset_target.x) * camera_follow_speed;
        }
        else if (eph.camera_offset.x < eph.camera_offset_target.x)
        {
            eph.camera_offset.x += (eph.camera_offset_target.x - eph.camera_offset.x) * camera_follow_speed;
        }

        if (eph.camera_offset.y > eph.camera_offset_target.y)
        {
            eph.camera_offset.y -= (eph.camera_offset.y - eph.camera_offset_target.y) * camera_follow_speed;
        }
        else if (eph.camera_offset.y < eph.camera_offset_target.y)
        {
            eph.camera_offset.y += (eph.camera_offset_target.y - eph.camera_offset.y) * camera_follow_speed;
        }
    }
	
    // draw gfx
	pd->graphics->clear(kColorWhite);
	pd->graphics->setFont(eph.font);

    if (eph.current_room_ptr != NULL)
    {
        snprintf(text_buff, sizeof(text_buff), "Room [%d,%d]", eph.current_room_ptr->coord.x, eph.current_room_ptr->coord.y);
        pd->graphics->drawText(text_buff, strlen(text_buff), kASCIIEncoding, 0, 48);

        draw_room(pd, eph.current_room_ptr, eph.camera_offset);

        Vector2_t room_offset;

        if (ser.current_room_idx < (ROOM_COUNT-1))
        {
            room_offset.x = eph.camera_offset.x+(ROOM_WIDTH*TILE_SIZE_PX);
            room_offset.y = eph.camera_offset.y;
            draw_room(pd, eph.current_room_ptr+1, room_offset);

            if (ser.current_room_idx < ((ROOM_COUNT-1)-LEVEL_WIDTH))
            {
                room_offset.x = eph.camera_offset.x;
                room_offset.y = room_offset.y+(ROOM_HEIGHT*TILE_SIZE_PX);
                draw_room(pd, eph.current_room_ptr+LEVEL_WIDTH, room_offset);
            }
        }

        if (ser.current_room_idx > 0)
        {
            room_offset.x = eph.camera_offset.x-(ROOM_WIDTH*TILE_SIZE_PX);
            room_offset.y = eph.camera_offset.y;
            draw_room(pd, eph.current_room_ptr-1, room_offset);

            if (ser.current_room_idx > LEVEL_WIDTH)
            {
                room_offset.x = eph.camera_offset.x;
                room_offset.y = room_offset.y-(ROOM_HEIGHT*TILE_SIZE_PX);
                draw_room(pd, eph.current_room_ptr-LEVEL_WIDTH, room_offset);
            }
        }
    }

	pd->system->drawFPS(0,0);

	return 1;
}

