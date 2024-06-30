#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <ogc/lwp_watchdog.h>
#include <ogc/pad.h>
#include <wiiuse/wpad.h>

#include "input.h"

// Input delays and thresholds
static const uint32_t KEY_REPEAT_DELAY_INITIAL = 300000;
static const uint32_t KEY_REPEAT_DELAY         = 100000;
static const int8_t STICK_THRESHOLD            = 50;

// Input state
static uint64_t prevDirectionFire = 0;
static uint64_t keyRepeatDelay    = KEY_REPEAT_DELAY_INITIAL;
static uint32_t padPressed        = 0;
static uint32_t padHeld           = 0;
static uint32_t wpadPressed       = 0;
static uint32_t wpadHeld          = 0;
static int8_t sX                  = 0;
static int8_t sY                  = 0;
static bool stickPressFired       = false;

void updateInput(void)
{
  // Update input state
  PAD_ScanPads();
  WPAD_ScanPads();

  // Store latest input states
  padPressed  = PAD_ButtonsDown(0);
  padHeld     = PAD_ButtonsHeld(0);
  wpadPressed = WPAD_ButtonsDown(0);
  wpadHeld    = WPAD_ButtonsHeld(0);
  sX          = PAD_StickX(0);
  sY          = PAD_StickY(0);
}

bool directionPressed(enum direction direction)
{
  // Determine which button to check based on direction
  u32 padButton, wpadButton;
  bool stickHeld;
  switch (direction) {
    case DIRECTION_UP:
      padButton  = PAD_BUTTON_UP;
      wpadButton = WPAD_BUTTON_UP;
      stickHeld  = (sY >= STICK_THRESHOLD);
      break;
    case DIRECTION_DOWN:
      padButton  = PAD_BUTTON_DOWN;
      wpadButton = WPAD_BUTTON_DOWN;
      stickHeld  = (sY <= -STICK_THRESHOLD);
      break;
    case DIRECTION_LEFT:
      padButton  = PAD_BUTTON_LEFT;
      wpadButton = WPAD_BUTTON_LEFT;
      stickHeld  = (sX <= -STICK_THRESHOLD);
      break;
    case DIRECTION_RIGHT:
      padButton  = PAD_BUTTON_RIGHT;
      wpadButton = WPAD_BUTTON_RIGHT;
      stickHeld  = (sX >= STICK_THRESHOLD);
      break;
    default:
      return false;
  }

  // Check if button is pressed or held
  if (wpadHeld & wpadButton || padHeld & padButton || stickHeld) {
    // Initial key press
    if (wpadPressed & wpadButton || padPressed & padButton || (stickHeld && !stickPressFired)) {
      stickPressFired   = true;
      prevDirectionFire = gettime();
      keyRepeatDelay    = KEY_REPEAT_DELAY_INITIAL;
      return true;
    }

    // Repeat key press
    u64 now = gettime();
    if (diff_usec(prevDirectionFire, now) > keyRepeatDelay) {
      prevDirectionFire = now;

      if (keyRepeatDelay == KEY_REPEAT_DELAY_INITIAL) {
        keyRepeatDelay = KEY_REPEAT_DELAY;
      }

      return true;
    }
  }

  // Reset stickPressFired if analog stick is released
  if (abs(sX) < STICK_THRESHOLD && abs(sY) < STICK_THRESHOLD)
    stickPressFired = false;

  return false;
}

bool padButtonPressed(uint32_t button)
{
  return padPressed & button;
}

bool padButtonHeld(uint32_t button)
{
  return padHeld & button;
}

bool wpadButtonPressed(uint32_t button)
{
  return wpadPressed & button;
}

bool wpadButtonHeld(uint32_t button)
{
  return wpadHeld & button;
}