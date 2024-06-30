#include <stdlib.h>
#include <time.h>

#include <grrlib.h>
#include <ogc/gx.h>

#define NUM_DROPS           600 // Defines rain density
#define MAX_THUNDER_OPACITY 100 // More is too brutal

typedef struct {
  int x, y;
  int dx, dy;
} Rain;

static Rain drops[NUM_DROPS];

static u16 k      = 0;
static u32 ndrops = 1;

static bool thunderOn = false;
static int thunder    = 1;
static int thAlpha    = MAX_THUNDER_OPACITY;

void setupRain()
{
  srand(time(NULL));
  for (k = 0; k < NUM_DROPS; k++) {
    // random place
    drops[k].x = rand() % (640 - 5);
    drops[k].y = -(rand() % 480); // rain drops are loaded above the screen
    // horizontal speed always set to 1, vertical speed between 4 and 6
    drops[k].dx = 1;
    drops[k].dy = (rand() & 2) + 3;
  }
}

void drawRain()
{
  // rain background
  for (k = 0; k < ndrops; k++) { // Draw all drops
    drops[k].x += drops[k].dx;
    drops[k].y += drops[k].dy;

    // check for collision with the screen boundaries
    if (drops[k].x > 640)
      drops[k].x -= 640;

    if (drops[k].y > 480) {
      drops[k].x = rand() % (640 - 5);
      drops[k].y -= 480;
    }

    if (drops[k].dy > 3) {
      GX_Begin(GX_LINES, GX_VTXFMT0, 2);
      GX_Position3f32(drops[k].x, drops[k].y, 0.0f);
      GX_Color1u32(0xffffff00);
      GX_Position3f32(drops[k].x + 3, drops[k].y + 10, 0.0f);
      GX_Color1u32(0xffffff80);
      GX_End();
    } else {
      GX_Begin(GX_LINES, GX_VTXFMT0, 2);
      GX_Position3f32(drops[k].x, drops[k].y, 0.0f);
      GX_Color1u32(0xffffff00);
      GX_Position3f32(drops[k].x + 3, drops[k].y + 10, 0.0f);
      GX_Color1u32(0xffffffff);
      GX_End();
    }
  }
  if (ndrops < NUM_DROPS)
    ndrops++;

  // thunder effect
  if (thunder == 55)
    thunderOn = true;

  else if (!thunderOn) {
    thunder = rand() % 500; // Probability of 1/200 per frame to hit thunder
    thAlpha = MAX_THUNDER_OPACITY;
  }

  if (thunderOn) {
    GRRLIB_FillScreen(RGBA(255, 255, 255, thAlpha));
    if (thAlpha > 5)
      thAlpha -= 5;
    else {
      thunderOn = false;
      thunder   = rand() % 200;
    }
  }
}