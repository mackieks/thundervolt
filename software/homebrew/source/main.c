/*===========================================
        TrueType Font demo
============================================*/
#include <grrlib.h>

#include <asndlib.h>
#include <mp3player.h>
#include <ogc/pad.h>
#include <stdio.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "i2c.h"

#include "input.h"
#include "menu.h"
#include "rain.h"

s8 hardwareButton = -1;

void wiiResetPressed()
{
  hardwareButton = SYS_RETURNTOMENU;
}

void wiiPowerPressed()
{
  hardwareButton = SYS_POWEROFF_STANDBY;
}

void wiimotePowerPressed(s32 chan __attribute__((unused)))
{
  hardwareButton = SYS_POWEROFF_STANDBY;
}

int main(int argc, char **argv)
{

  // Initialise the Graphics & Video subsystem
  GRRLIB_Init();

  // Initialise the Wii Remotes
  PAD_Init();
  WPAD_Init();

  ASND_Init();
  MP3Player_Init();

  // Initialize the I2C bus
  i2c_configure(I2C_MODE_STANDARD);

  // Black background
  GRRLIB_SetBackgroundColour(0x00, 0x00, 0x00, 0x00);

  // Set the power and reset callbacks
  SYS_SetResetCallback(wiiResetPressed);
  SYS_SetPowerCallback(wiiPowerPressed);
  WPAD_SetPowerButtonCallback(wiimotePowerPressed);

  // Initialise the rain effect
  setupRain();

  // Initialise the menu
  setupMenu();

  // Loop forever
  while (1) {
    // Fetch the latest input states
    updateInput();

    // Update the menu, and check if it has told us to exit
    bool menuExit = !handleMenuInput();
    if (menuExit)
      break;

    // Draw the rain effect
    drawRain();

    // Draw the menu
    drawMenu();

    // Check if the power or reset button was pressed
    if (hardwareButton != -1)
      break;
  }

  // GRRLIB_FreeTTF(myFont);
  GRRLIB_Exit(); // Be a good boy, clear the memory allocated by GRRLIB

  // Power off or return to the Wii menu
  if (hardwareButton != -1)
    SYS_ResetSystem(hardwareButton, 0, 0);

#if defined(DOLPHIN)
  // Workaround for crash-on-exit bug in Dolphin
  SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);
#endif

  return 0;
}