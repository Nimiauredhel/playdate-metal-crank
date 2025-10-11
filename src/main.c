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
#define BITMAP_COUNT (10)

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
    BITMAP_PLAYER = 7,
    BITMAP_DOOR_H = 8,
    BITMAP_DOOR_V = 9,
} BitmapIndices_t;

typedef enum TileFlags
{
    TILEFLAG_NONE = 0x00,
    TILEFLAG_WALKABLE = 0x01,
    TILEFLAG_DOOR_H = 0x02,
    TILEFLAG_DOOR_V = 0x04,
} TileFlags_t;

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

typedef struct Entity
{
    Vector2Int_t position_px;
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
    Vector2Int_t mov_speed;
    GlobalEntity_t *player_ptr;
    Room_t *current_room_ptr;
    Room_t *adjacent_room_ptrs[4];
    RoomDrawPositions_t room_draw_positions;
    LCDFont* font;
    LCDBitmap* bitmaps[BITMAP_COUNT];
} EphemeralState_t;

static int update(void* userdata);

static const char bitmap_paths[BITMAP_COUNT][16] =
{
    "floor00.png",
    "floor01.png",
    "floor02.png",
    "floor03.png",
    "wall.png",
    "table.png",
    "crate.png",
    "player.png",
    "doorh.png",
    "doorv.png",
};
static const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
static const int mov_accel_min = 15;
static const int mov_accel_max = 30;
static const int mov_speed_min = 125;
static const int mov_speed_max = 250;
static const Vector2Int_t default_camera_offset = { 200, 120 };
static const Vector2Int_t adjacent_room_offsets[4] =
{
    {-(ROOM_WIDTH*TILE_SIZE_PX), 0},
    {0, -(ROOM_HEIGHT*TILE_SIZE_PX)},
    {+(ROOM_WIDTH*TILE_SIZE_PX), 0},
    {0, +(ROOM_HEIGHT*TILE_SIZE_PX)},
};

static PlaydateAPI *pd_s = NULL;
static SerializableState_t ser = {0};
static EphemeralState_t eph = {0};

#ifdef _WINDLL
__declspec(dllexport)
#endif

static int8_t sign(int num)
{
    return (num > 0) - (num < 0);
}

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

    eph.adjacent_room_ptrs[0] = eph.current_room_ptr->coord.x > LEVEL_MIN_X ? eph.current_room_ptr-1 : NULL;
    eph.adjacent_room_ptrs[1] = eph.current_room_ptr->coord.y > LEVEL_MIN_Y ? eph.current_room_ptr-LEVEL_WIDTH : NULL;
    eph.adjacent_room_ptrs[2] = eph.current_room_ptr->coord.x < LEVEL_MAX_X ? eph.current_room_ptr+1 : NULL;
    eph.adjacent_room_ptrs[3] = eph.current_room_ptr->coord.y < LEVEL_MAX_Y ? eph.current_room_ptr+LEVEL_WIDTH : NULL;
}

static TileFlags_t tile_flags_at_pos(Room_t *room, int tile_x, int tile_y)
{
    if (tile_x < ROOM_MIN_X || tile_x > ROOM_MAX_X
     || tile_y < ROOM_MIN_Y || tile_y > ROOM_MAX_Y) return 0;
    return room->tiles[tile_x + (tile_y * ROOM_WIDTH)].flags;
}

static bool generate_maze(CellType_t cell_grid[ROOM_WIDTH][ROOM_HEIGHT], const bool path_bools[4])
{
    // limiting a single walk to a sensible amount
    // TODO: figure out possible implications
    static const uint16_t walk_max_len = (ROOM_WIDTH*ROOM_HEIGHT);

    int first_path = rand() % 4;

    bool success = true;

    int paths_connected[4] = { -1, -1, -1, -1, };

    Vector2Int_t path_starts[4] =
    {
        (Vector2Int_t){ROOM_MIN_X+1, ROOM_MID_Y},
        (Vector2Int_t){ROOM_MID_X, ROOM_MIN_Y+1},
        (Vector2Int_t){ROOM_MAX_X-1, ROOM_MID_Y},
        (Vector2Int_t){ROOM_MID_X, ROOM_MAX_Y-1},
    };

    // 1. initialization

    // - first close every cell
    for (uint16_t x = 0; x < ROOM_WIDTH; x++)
    {
        for (uint16_t y = 0; y < ROOM_HEIGHT; y++)
        {
            cell_grid[x][y] = CELL_CLOSED;
        }
    }

    // - set the bounding walls to BORDER so they don't get overriden
    for (uint16_t x = 0; x < ROOM_WIDTH; x++)
    {
        cell_grid[x][ROOM_MIN_Y] = CELL_BORDER;
        cell_grid[x][ROOM_MAX_Y] = CELL_BORDER;
    }

    for (uint16_t y = 0; y < ROOM_HEIGHT; y++)
    {
        cell_grid[ROOM_MIN_X][y] = CELL_BORDER;
        cell_grid[ROOM_MAX_X][y] = CELL_BORDER;
    }

    // - add random border tiles in the room interior to discourage the tendency towards 'open space'
    uint8_t num_rand_border = rand() % ((ROOM_WIDTH+ROOM_HEIGHT)/8);
    for (uint8_t i = 0; i < num_rand_border; i++)
    {
        cell_grid[rand() % ((ROOM_MIN_X+2)+((ROOM_MAX_X-ROOM_MIN_X)-2))][rand() % ((ROOM_MIN_Y+2)+((ROOM_MAX_Y-ROOM_MIN_Y)-2))] = CELL_BORDER;
    }

    for (int path_num = 0; path_num < 4; path_num++)
    {
        CellType_t path_idx = (first_path + path_num) % 4;

        if (!path_bools[path_idx]) continue;

        cell_grid[path_starts[path_idx].x][path_starts[path_idx].y] = (CellType_t)path_idx;
    }

    Vector2Int_t coord_stack[(ROOM_WIDTH*ROOM_HEIGHT)] = {0};
    Direction_t move_stack[(ROOM_WIDTH*ROOM_HEIGHT)] = {0};

    bool reverse_path_order = (rand() % 2) > 0;

    // four random walks, until all four paths are connected to a single maze.
    for (CellType_t path_num = 0; path_num < 4; path_num++)
    {
        CellType_t path_idx = (first_path + path_num) % 4;
        if (reverse_path_order) path_idx = CELL_PATH_3 - path_idx;

        if (!path_bools[path_idx]) continue;

        int32_t walk_idx = 0;
        // 2. start walk from pre-determined path start.
        move_stack[0] = DIR_NONE;
        coord_stack[0].x = path_starts[path_idx].x;
        coord_stack[0].y = path_starts[path_idx].y;

        // 3. walk and set cells 'open' until reaching either a foreign open cell (LINK), a self open cell (LOOP), or a DEAD END
        while (walk_idx >= 0)
        {
            Vector2Int_t curr = coord_stack[walk_idx];
            // - set current cell to current path index; we will change this to CLOSED if a loop is detected.
            // - in either case, it is now marked as resolved and will not be checked again.
            cell_grid[curr.x][curr.y] = (CellType_t)path_idx;

            // - check valid directions from current cell
            bool dirs_in_bounds[DIR_COUNT] =
            {
                curr.x > ROOM_MIN_X && move_stack[walk_idx] != DIR_RIGHT,
                curr.y > ROOM_MIN_Y && move_stack[walk_idx] != DIR_DOWN,
                curr.x < ROOM_MAX_X && move_stack[walk_idx] != DIR_LEFT,
                curr.y < ROOM_MAX_Y && move_stack[walk_idx] != DIR_UP,
            };

            CellType_t dirs_types[DIR_COUNT] =
            {
                dirs_in_bounds[DIR_LEFT]  ? cell_grid[curr.x-1][curr.y] : CELL_BORDER,
                dirs_in_bounds[DIR_UP]    ? cell_grid[curr.x][curr.y-1] : CELL_BORDER,
                dirs_in_bounds[DIR_RIGHT] ? cell_grid[curr.x+1][curr.y] : CELL_BORDER,
                dirs_in_bounds[DIR_DOWN]  ? cell_grid[curr.x][curr.y+1] : CELL_BORDER,
            };

            Vector2Int_t dirs_coords[DIR_COUNT] =
            {
                dirs_in_bounds[DIR_LEFT]  ? (Vector2Int_t){curr.x-1, curr.y} : (Vector2Int_t){0, 0},
                dirs_in_bounds[DIR_UP]    ? (Vector2Int_t){curr.x, curr.y-1} : (Vector2Int_t){0, 0},
                dirs_in_bounds[DIR_RIGHT] ? (Vector2Int_t){curr.x+1, curr.y} : (Vector2Int_t){0, 0},
                dirs_in_bounds[DIR_DOWN]  ? (Vector2Int_t){curr.x, curr.y+1} : (Vector2Int_t){0, 0},
            };

            int dir_count = 0;
            Direction_t dir_indices[DIR_COUNT] = {DIR_NONE};

            for (int i = DIR_LEFT; i < DIR_COUNT; i++)
            {
                if (!dirs_in_bounds[i]) continue;

                if (dirs_types[i] == (CellType_t)path_idx
                || (dirs_types[i] >= 0 && paths_connected[dirs_types[i]] == path_idx))
                {
                    // ** LOOP **
                    // skip it as if it were a border.
                    //pd_s->system->logToConsole("Loop [%d,%d] reached in walk between [0][%d,%d] and [%d][%d,%d].",
                    //    dirs_coords[i].x, dirs_coords[i].y, coord_stack[0].x, coord_stack[0].y, walk_idx, coord_stack[walk_idx].x, coord_stack[walk_idx].y);
                    continue;
                }

                switch(dirs_types[i])
                {
                    case CELL_CLOSED:
                        // cell is unresolved, count and list it as option for next step
                        dir_indices[dir_count] = i;
                        dir_count++;
                        break;
                    case CELL_BORDER:
                        // cell is not counted as an option
                        continue;
                    case CELL_PATH_0:
                    case CELL_PATH_1:
                    case CELL_PATH_2:
                    case CELL_PATH_3:
                        // ** LINK ** 
                        //pd_s->system->logToConsole("Link [%d,%d] reached in walk between [0][%d,%d] and [%d][%d,%d].",
                        //        dirs_coords[i].x, dirs_coords[i].y, coord_stack[0].x, coord_stack[0].y, walk_idx, coord_stack[walk_idx].x, coord_stack[walk_idx].y);
                        // end walk
                        paths_connected[path_idx] = false;
                        paths_connected[dirs_types[i]] = true;
                        walk_idx = -1;
                        break;
                }

                if (walk_idx == -1) break;
            }

            if (walk_idx == -1) continue;

            // check for dead end
            if (dir_count == 0)
            {
                // ** DEAD END **
                // backtrack one.
                //pd_s->system->logToConsole("Dead end reached in walk between [0][%d,%d] and [%d][%d,%d].",
                //    coord_stack[0].x, coord_stack[0].y, walk_idx, coord_stack[walk_idx].x, coord_stack[walk_idx].y);
                walk_idx--;
                continue;
            }

            // check if current walk exceeded allowed length;
            // NOTE: this check must NEVER happen before the other stop conditions are tested.
            if (walk_idx >= walk_max_len)
            {
                // ** WALK LIMIT REACHED **
                // end walk.
                //pd_s->system->logToConsole("Limit reached in walk between [0][%d,%d] and [%d][%d,%d].",
                //    coord_stack[0].x, coord_stack[0].y, walk_idx, coord_stack[walk_idx].x, coord_stack[walk_idx].y);
                walk_idx = -1;
            }

            // ** WALK CONTINUES **
            Direction_t chosen_dir = DIR_NONE;

            // if only one direction is valid, select it
            if (dir_count == 1)
            {
                chosen_dir = dir_indices[0];
            }
            // else, select randomly from valid options
            else
            {
                chosen_dir = dir_indices[rand() % dir_count];
            }

            walk_idx++;
            move_stack[walk_idx] = chosen_dir;

            switch(chosen_dir)
            {
                case DIR_LEFT:
                case DIR_UP:
                case DIR_RIGHT:
                case DIR_DOWN:
                    coord_stack[walk_idx].x = dirs_coords[chosen_dir].x;
                    coord_stack[walk_idx].y = dirs_coords[chosen_dir].y;
                    break;
                case DIR_NONE:
                case DIR_COUNT:
                  // should not happen, end walk.
                  walk_idx = -1;
                  pd_s->system->logToConsole("!! Invalid 'chosen direction' in maze generation !!");
                  success = false;
                  break;
            }
        }
    }

    // 4. presumably all paths have now been linked and the caller can now use the maze grid.
    return success;
}

static bool populate_room(uint16_t level_x, uint16_t level_y, bool player_start)
{
    static const uint8_t doorh_count = 2;
    static const uint8_t doorv_count = 2;

    static CellType_t maze_grid[ROOM_WIDTH][ROOM_HEIGHT] = {0};

    pd_s->system->logToConsole("Populating room [%d,%d].", level_x, level_y);

    uint16_t level_idx = level_x + (level_y * LEVEL_WIDTH);
    Room_t *room = ser.level.rooms + level_idx;
    room->coord.x = level_x;
    room->coord.y = level_y;

    Tile_t *tile = NULL;
    bool placed_player = false;
    Vector2Int_t player_coord = {0};

    const bool door_bools[4] =
    {
        level_x > LEVEL_MIN_X,
        level_y > LEVEL_MIN_Y,
        level_x < LEVEL_MAX_X,
        level_y < LEVEL_MAX_Y,
    };

    const int32_t doorh_indices[2] =
    {
        door_bools[0] ? ROOM_MIN_X + (ROOM_WIDTH*ROOM_MID_Y) : -1,
        door_bools[2] ? ROOM_MAX_X + (ROOM_WIDTH*ROOM_MID_Y) : -1,
    };

    const int32_t doorv_indices[2] =
    {
        door_bools[1] ? ROOM_MID_X + (ROOM_WIDTH*ROOM_MIN_Y) : -1,
        door_bools[3] ? ROOM_MID_X + (ROOM_WIDTH*ROOM_MAX_Y) : -1,
    };

    bool maze_success = generate_maze(maze_grid, door_bools);
    if (!maze_success) return false;

    for (int x = 0; x < ROOM_WIDTH; x++)
    {
        for (int y = 0; y < ROOM_HEIGHT; y++)
        {
            tile = room->tiles+(x + (ROOM_WIDTH * y));

             /**
             * All fields are expected to be zero-initialized and adhere to zero-as-default,
             * which should have the same effect as if this code were here:
             *
             * tile->bitmap_idx = BITMAP_WALL;
            * tile->flags = TILEFLAG_NONE;
             *
             * TODO: create room generation tests to enforce this assumption.
             **/

            uint8_t adj_space =
            (x == (ROOM_MIN_X+1) || maze_grid[x-1][y] >= CELL_PATH_0)
             + (x == (ROOM_MAX_X-1) || maze_grid[x+1][y] >= CELL_PATH_0)
             + (y == (ROOM_MIN_Y+1) || maze_grid[x][y-1] >= CELL_PATH_0)
             + (y == (ROOM_MAX_Y-1) || maze_grid[x][y+1] >= CELL_PATH_0);

            switch(maze_grid[x][y])
            {
            case CELL_PATH_0:
            case CELL_PATH_1:
            case CELL_PATH_2:
            case CELL_PATH_3:
                tile->bitmap_idx = BITMAP_FLOOR_00 + maze_grid[x][y];
                tile->flags |= TILEFLAG_WALKABLE;

                if (player_start && (!placed_player || (rand() % 100) > 80))
                {
                    player_coord.x = x;
                    player_coord.y = y;
                    placed_player = true;
                }

                break;
            case CELL_CLOSED:
            case CELL_BORDER:
                tile->bitmap_idx = adj_space >= 4 ? BITMAP_TABLE
                    : adj_space >= 3 ? BITMAP_CRATE : BITMAP_WALL;
                tile->flags = TILEFLAG_NONE;
                break;
            default:
                pd_s->system->logToConsole("!! Unresolved cell in returned maze grid !!");
                tile->bitmap_idx = BITMAP_PLAYER;
                tile->flags = TILEFLAG_NONE;
                // shouldn't happen
                break;
            }
        }
    }

    for (uint8_t i = 0; i < doorh_count; i++)
    {
        if (doorh_indices[i] < 0) continue;
        room->tiles[doorh_indices[i]].bitmap_idx = BITMAP_DOOR_H;
        room->tiles[doorh_indices[i]].flags |= (TILEFLAG_DOOR_H | TILEFLAG_WALKABLE);
    }

    for (uint8_t i = 0; i < doorv_count; i++)
    {
        if (doorv_indices[i] < 0) continue;
        room->tiles[doorv_indices[i]].bitmap_idx = BITMAP_DOOR_V;
        room->tiles[doorv_indices[i]].flags |= (TILEFLAG_DOOR_V | TILEFLAG_WALKABLE);
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

    return true;
}

bool populate_level(void)
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
            bool room_success = populate_room(x, y, x == start_x && y == start_y);
            if (!room_success) return false;
        }
    }

    return true;
}

static void prepare_room_draw_positions(void)
{
    Vector2Int_t draw_pos_scratch = {-TILE_OFFSET_PX, -TILE_OFFSET_PX};

    for (int x = 0; x < ROOM_WIDTH; x++)
    {
        draw_pos_scratch.x += TILE_SIZE_PX;
        eph.room_draw_positions.x[x] = draw_pos_scratch.x;
    }

    for (int y = 0; y < ROOM_HEIGHT; y++)
    {
        draw_pos_scratch.y += TILE_SIZE_PX;
        eph.room_draw_positions.y[y] = draw_pos_scratch.y;
    }
}

static void draw_room(PlaydateAPI *pd, Room_t *room_ptr, Vector2Int_t offset)
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

    for (uint8_t i = 0; i < room_ptr->local_entity_count; i++)
    {
        entity = room_ptr->entities+i;

        draw_pos.x = TILE_OFFSET_PX + entity->position_px.x + offset.x;
        if (draw_pos.x < draw_min || draw_pos.x > draw_max.x) continue;

        draw_pos.y = TILE_OFFSET_PX + entity->position_px.y + offset.y;
        if (draw_pos.y < draw_min || draw_pos.y > draw_max.y) continue;

        pd->graphics->drawBitmap(eph.bitmaps[entity->bitmap_idx], draw_pos.x, draw_pos.y, kBitmapUnflipped);
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

static void draw_adjacent_rooms(PlaydateAPI *pd, Vector2Int_t offset)
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

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if (event == kEventInit)
	{
		const char* err;
        pd_s = pd;

        pd->system->logToConsole("Initializing game.");

        pd->system->setPeripheralsEnabled(kAccelerometer);
        srand(pd->system->getSecondsSinceEpoch(NULL));

        bzero(&ser, sizeof(ser));
        bzero(&eph, sizeof(eph));

        eph.screen_size.x = pd->display->getWidth();
        eph.screen_size.y = pd->display->getHeight();
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

        bool level_init_success = populate_level();

        prepare_room_draw_positions();

        eph.camera_offset_target.x = (default_camera_offset.x - eph.player_ptr->entity.position_px.x) - TILE_SIZE_PX;
        eph.camera_offset_target.y = (default_camera_offset.y - eph.player_ptr->entity.position_px.y) - TILE_SIZE_PX;
        eph.camera_offset = eph.camera_offset_target;

        // calibrate accelerometer
        pd->system->getAccelerometer(&eph.accelerometer_raw.x, &eph.accelerometer_raw.y, &eph.accelerometer_raw.z);
        memcpy(&eph.accelerometer_center, &eph.accelerometer_raw, sizeof(Vector3_t));

        pd->display->setRefreshRate(50);
        pd->system->resetElapsedTime();
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
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
    eph.crank.x = pd->system->getCrankAngle();
    eph.crank.y = pd->system->getCrankChange();
    pd->system->getAccelerometer(&eph.accelerometer_raw.x, &eph.accelerometer_raw.y, &eph.accelerometer_raw.z);

    // if crank is moving, calibrate accelerometer
    if (eph.crank.y != 0)
    {
        memcpy(&eph.accelerometer_center, &eph.accelerometer_raw, sizeof(Vector3_t));
    }

    eph.camera_peek_offset.x = (eph.accelerometer_raw.x - eph.accelerometer_center.x) * TILE_SIZE_PX;
    eph.camera_peek_offset.y = (eph.accelerometer_raw.y - eph.accelerometer_center.y) * TILE_SIZE_PX;

    // process input
    if (eph.player_ptr != NULL)
    {
        Vector2Int_t mov_delta = {0};
        float crank_value = powf(eph.crank.y, 2) * deltaTime;

        int target_speed = mov_speed_min + ((mov_speed_max - mov_speed_min) * crank_value);
        int mov_accel_val = mov_accel_min + ((mov_accel_max - mov_accel_min) * crank_value);

        if (target_speed > mov_speed_max) target_speed = mov_speed_max;
        if (mov_accel_val > mov_accel_max) mov_accel_val = mov_accel_max;

        Vector2Int_t directional_target_speed =
        {
            target_speed * sign(((eph.buttons_current & kButtonRight) - (eph.buttons_current & kButtonLeft))),
            target_speed * sign(((eph.buttons_current & kButtonDown) - (eph.buttons_current & kButtonUp))),
        };


        if (eph.mov_speed.x < directional_target_speed.x)
        {
            eph.mov_speed.x += mov_accel_val;
            if (eph.mov_speed.x > directional_target_speed.x) eph.mov_speed.x = directional_target_speed.x;
        }
        else if (eph.mov_speed.x > directional_target_speed.x)
        {
            eph.mov_speed.x -= mov_accel_val;
            if (eph.mov_speed.x < directional_target_speed.x) eph.mov_speed.x = directional_target_speed.x;
        }

        if (eph.mov_speed.y < directional_target_speed.y)
        {
            eph.mov_speed.y += mov_accel_val;
            if (eph.mov_speed.y > directional_target_speed.y) eph.mov_speed.y = directional_target_speed.y;
        }
        else if (eph.mov_speed.y > directional_target_speed.y)
        {
            eph.mov_speed.y -= mov_accel_val;
            if (eph.mov_speed.y < directional_target_speed.y) eph.mov_speed.y = directional_target_speed.y;
        }

        mov_delta.x = eph.mov_speed.x * deltaTime;
        mov_delta.y = eph.mov_speed.y * deltaTime;

        if (mov_delta.x != 0 || mov_delta.y != 0)
        {
            Vector2Int_t current_pos = { eph.player_ptr->entity.position_px.x, eph.player_ptr->entity.position_px.y };
            Vector2Int_t new_pos = { current_pos.x + mov_delta.x, current_pos.y + mov_delta.y };
            Vector2Int_t new_offset_pos = { new_pos.x + TILE_OFFSET_PX, new_pos.y + TILE_OFFSET_PX };

            Vector2Int_t eval_coll_tiles[4] =
            {
                { (new_offset_pos.x - TILE_COLL_PX/2) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX/2) / TILE_SIZE_PX },
                { (new_offset_pos.x + TILE_COLL_PX/2) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX },
                { (new_offset_pos.x - TILE_COLL_PX/2) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX) / TILE_SIZE_PX },
                { (new_offset_pos.x + TILE_COLL_PX/2) / TILE_SIZE_PX, (new_offset_pos.y + TILE_COLL_PX/2) / TILE_SIZE_PX },
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
            Vector2Int_t coll_tile = {0};

            for (uint8_t i = 0; i < 4; i++)
            {
                coll_tile = eval_coll_tiles[i];
                tile_flags = eval_tile_flags[i];

                // if a non-walkable tile is touched, the below (position-changing) flags do not apply
                // if applicable flags are added later, they should be tested above this one
                if (!(tile_flags & TILEFLAG_WALKABLE))
                {
                    eph.mov_speed.x *= -0.95f;
                    eph.mov_speed.y *= -0.95f;
                    break;
                }
                else if (tile_flags & TILEFLAG_DOOR_H
                        && ((new_pos.x >= (ROOM_MAX_X * TILE_SIZE_PX) || new_pos.x <= (ROOM_MIN_X * TILE_SIZE_PX))))
                {
                    if (coll_tile.x == ROOM_MIN_X)
                    {
                        set_current_room(ser.current_room_idx - 1);
                        new_pos.x = TILE_SIZE_PX * ROOM_MAX_X;
                        eph.camera_offset.x = (default_camera_offset.x - (new_pos.x + TILE_SIZE_PX*2));
                    }
                    else if (coll_tile.x == ROOM_MAX_X)
                    {
                        set_current_room(ser.current_room_idx + 1);
                        new_pos.x = TILE_SIZE_PX * ROOM_MIN_X;
                        eph.camera_offset.x = (default_camera_offset.x - new_pos.x);
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
                        eph.camera_offset.y = (default_camera_offset.y - (new_pos.y + TILE_SIZE_PX*2));
                    }
                    else if (coll_tile.y == ROOM_MAX_Y)
                    {
                        set_current_room(ser.current_room_idx + LEVEL_WIDTH);
                        new_pos.y = TILE_SIZE_PX * ROOM_MIN_Y;
                        eph.camera_offset.y = (default_camera_offset.y - new_pos.y);
                    }

                    set_player_room(ser.current_room_idx);
                    break;
                }
            }

            if (tile_flags & TILEFLAG_WALKABLE)
            {
                eph.player_ptr->entity.position_px = new_pos;
            }
        }

        float camera_follow_speed = 3.5f * deltaTime;

        eph.camera_offset_target.x = ((default_camera_offset.x - eph.player_ptr->entity.position_px.x) - TILE_SIZE_PX) - eph.camera_peek_offset.x;
        eph.camera_offset_target.y = ((default_camera_offset.y - eph.player_ptr->entity.position_px.y) - TILE_SIZE_PX) - eph.camera_peek_offset.y;

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
        draw_adjacent_rooms(pd, eph.camera_offset);
    }

	pd->system->drawFPS(0,0);

	return 1;
}

