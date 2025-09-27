#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

typedef struct Vector2
{
    int x;
    int y;
} Vector2_t;

typedef struct Entity
{
    Vector2_t position;
    int bitmap_idx;
} Entity_t;

static int update(void* userdata);

static const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
static const int mov_speed = 1;

static LCDFont* font = NULL;

static Entity_t entities[64] = {0};
static LCDBitmap* bitmaps[32] = {0};

static int bitmap_count = 0;
static int entity_count = 0;

static PDButtons buttons_current = {0};
static PDButtons buttons_pushed = {0};
static PDButtons buttons_released = {0};

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if (event == kEventInit)
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);

		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

        bitmaps[0] = pd->graphics->loadBitmap("player.png", &err);
        // TODO: handle error like the above font loading
        bitmap_count = 1;

        entities[0].bitmap_idx = 0;
        entities[0].position.x = 5;
        entities[0].position.y = 5;
        entity_count = 1;

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}


#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

int x = (400-TEXT_WIDTH)/2;
int y = (240-TEXT_HEIGHT)/2;
int dx = 1;
int dy = 2;

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;

    // get input
    pd->system->getButtonState(&buttons_current, &buttons_pushed, &buttons_released);

    // process input
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

    entities[0].position.x += mov_delta.x;
    entities[0].position.y += mov_delta.y;
	
    // draw gfx
	pd->graphics->clear(kColorWhite);
	pd->graphics->setFont(font);
	pd->graphics->drawText("Hello World!", strlen("Hello World!"), kASCIIEncoding, x, y);

    for (int i = 0; i < entity_count; i++)
    {
        pd->graphics->drawBitmap(bitmaps[entities[i].bitmap_idx], entities[i].position.x, entities[i].position.y, kBitmapUnflipped);
    }

	x += dx;
	y += dy;
	
	if ( x < 0 || x > LCD_COLUMNS - TEXT_WIDTH )
		dx = -dx;
	
	if ( y < 0 || y > LCD_ROWS - TEXT_HEIGHT )
		dy = -dy;
        
	pd->system->drawFPS(0,0);

	return 1;
}

