/*
 * Menu
 *
 * Attempt at a flexible & extensible hierarchical menu system
 *
 */

#include <asndlib.h>
#include <grrlib.h>
#include <mp3player.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/pad.h>
#include <wiiuse/wpad.h>

#include "i2c/thundervolt.h"

#include "assets.h"
#include "input.h"
#include "menu.h"

// UI colors
enum color {
  white      = 0xffffffff,
  grey       = 0xffffff80,
  light_grey = 0xffffffe0,
  yellow     = 0xffbf00ff,
};

// Value limits
struct limits {
  int min;
  int max;
  int increment;
};

// Voltage limits
static struct limits VOLTAGE_LIMITS[] = {
    {750, 1000, 5},
    {850, 1150, 5},
    {1300, 1800, 10},
    {3000, 3300, 25},
};

// Overtemp shutdown limits
static struct limits OTSD_LIMITS = {50, 100, 1};

// Alignment constants for controls in right column
static const int COL_WIDTH = 100;
static const int COL_END   = 550;
static const int COL_START = COL_END - COL_WIDTH;
static const int COL_MID   = COL_START + COL_WIDTH / 2;

// Font
static GRRLIB_ttfFont *myFont = NULL;

// Textures
static GRRLIB_texImg *logo      = NULL;
static GRRLIB_texImg *note      = NULL;
static GRRLIB_texImg *arrow     = NULL;
static GRRLIB_texImg *cursor    = NULL;
static GRRLIB_texImg *bullet    = NULL;
static GRRLIB_texImg *toggleOn  = NULL;
static GRRLIB_texImg *toggleOff = NULL;
static GRRLIB_texImg *icon      = NULL;

// Menu state
static menu *prevMenu               = NULL;
static menu *currentMenu            = NULL;
static uint8_t currentNumEntries    = 0;
static uint8_t prevNumEntries       = 0;
static uint8_t selectedEntry        = 0;
static uint8_t firstSelectableEntry = 4;
static uint8_t lastSelectableEntry  = 9;
static uint8_t firstVisibleEntry    = 0;
static uint8_t lastVisibleEntry     = 9;
static uint8_t prevSelectedEntry    = 0;

// Audio state
static bool musicPaused = false;

// Thundervolt state
static bool thundervoltPresent  = false;
static uint16_t liveVoltages[4] = {0};
static int8_t liveOvertemp      = 0;
static bool overtempShutdown    = false;

// Board temperature
static float temp            = 0.0;
static uint64_t prevTempTime = 0;

//
// Common menu functions
//

void getSelectedEntry()
{
  for (int i = 0; i < currentNumEntries; i++) {
    if (currentMenu[i].selected) {
      selectedEntry = i;
      break;
    }
  }
}

void getFirstVisibleEntry()
{
  for (int i = 0; i < currentNumEntries; i++) {
    if (currentMenu[i].visible) {
      firstVisibleEntry = i;
      break;
    }
  }
}

void getLastVisibleEntry()
{
  for (int i = currentNumEntries - 1; i >= 0; i--) {
    if (currentMenu[i].visible) {
      lastVisibleEntry = i;
      break;
    }
  }
}

void getFirstSelectableEntry()
{
  for (int i = 0; i < currentNumEntries; i++) {
    if (currentMenu[i].selectable) {
      firstSelectableEntry = i;
      break;
    }
  }
}

void getLastSelectableEntry()
{
  for (int i = currentNumEntries - 1; i >= 0; i--) {
    if (currentMenu[i].selectable) {
      lastSelectableEntry = i;
      break;
    }
  }
}

int getNextSelectable()
{
  for (int i = selectedEntry + 1; i < currentNumEntries; i++) {
    if (currentMenu[i].selectable)
      return (i);
  }

  return 0;
}

int getPrevSelectable()
{
  for (int i = selectedEntry - 1; i >= 0; i--) {
    if (currentMenu[i].selectable)
      return (i);
  }

  return 0;
}

void playSound(const uint8_t *sound, const size_t size)
{
  ASND_SetVoice(ASND_GetFirstUnusedVoice(), VOICE_STEREO_16BIT, 48000, 0, (void *)sound, size, 255, 255, NULL);
}

int enterSubmenu(menu *submenu, size_t numEntries)
{
  playSound(enter_raw, enter_raw_size);

  // Save selectedEntry in main menu
  getSelectedEntry();
  prevSelectedEntry = selectedEntry;

  prevMenu       = currentMenu;
  prevNumEntries = currentNumEntries;

  currentMenu       = submenu;
  currentNumEntries = numEntries;

  getFirstSelectableEntry();
  getLastSelectableEntry();

  return 1;
}

int exitSubmenu(menu *self, uint8_t action)
{
  playSound(back_raw, back_raw_size);

  currentMenu       = prevMenu;
  currentNumEntries = prevNumEntries;

  // Update selectedEntry flags
  getFirstSelectableEntry();
  getLastSelectableEntry();

  // revert to previously-selected entry
  selectedEntry = prevSelectedEntry;

  return 1;
}

int toggleOption(menu *self, uint8_t action)
{
  if (self->value)
    playSound(back_raw, back_raw_size);
  else
    playSound(enter_raw, enter_raw_size);

  if (self->type == TOGGLE)
    self->value = !(self->value);

  return 1;
}

int adjustValue(menu *self, uint8_t action)
{
  struct limits *limits = (struct limits *)self->data;

  if (action == DIRECTION_LEFT) {
    if (self->value - limits->increment >= limits->min) {
      self->value -= limits->increment;
      playSound(move_raw, move_raw_size);
    }
  } else if (action == DIRECTION_RIGHT) {
    if (self->value + limits->increment <= limits->max) {
      self->value += limits->increment;
      playSound(move_raw, move_raw_size);
    }
  }

  return 1;
}

int exitToPad(menu *self, uint8_t action)
{
  return 0;
}

int dummy(menu *self, uint8_t action)
{
  return 1;
}

//
// Main menu
//

int enterUndervoltMenu(menu *self, uint8_t action);
int enterOvertempMenu(menu *self, uint8_t action);
int enterCreditsMenu(menu *self, uint8_t action);

// Menu entries
static menu mainMenu[] = {
    {"[detection status]           ", 4, 1, 0, 0, 1, 1, 0, white, dummy},
    {"[safe mode status]           ", 4, 1, 0, 0, 1, 1, 2, white, dummy},
    {"board temp: ? °C             ", 4, 1, 0, 0, 1, 1, 3, white, dummy},
    {"board power: ? mW            ", 4, 1, 0, 0, 1, 1, 4, grey, dummy},
    {"configure undervolt          ", 0, 1, 1, 1, 1, 1, 6, white, enterUndervoltMenu},
    {"configure overtemp protection", 0, 1, 1, 0, 1, 1, 7, white, enterOvertempMenu},
    {"power monitor                ", 0, 1, 0, 0, 1, 1, 8, grey, dummy},
    {"stress test                  ", 0, 1, 0, 0, 1, 1, 9, grey, dummy},
    {"credits                      ", 0, 1, 1, 0, 1, 1, 11, white, enterCreditsMenu},
    {"exit                         ", 2, 1, 1, 0, 1, 1, 12, white, exitToPad},
};

//
// Undervolt submenu
//

int setLiveUndervolt(menu *self, uint8_t action);
int setPersistedUndervolt(menu *self, uint8_t action);

static menu undervoltMenu[] = {
    {"undervolt configuration      ", 4, 1, 0, 0, 1, 1, 0, light_grey, dummy},
    {"1V (GPU)                     ", 3, 1, 1, 1, 1, 1, 2, white, adjustValue, &VOLTAGE_LIMITS[0]},
    {"1.15V (CPU)                  ", 3, 1, 1, 0, 1, 1, 4, white, adjustValue, &VOLTAGE_LIMITS[1]},
    {"1.8V (DDR)                   ", 3, 1, 1, 0, 1, 1, 6, white, adjustValue, &VOLTAGE_LIMITS[2]},
    {"3.3V (NAND/IO)               ", 3, 1, 1, 0, 1, 1, 8, white, adjustValue, &VOLTAGE_LIMITS[3]},
    {"apply changes now            ", 2, 1, 1, 0, 1, 1, 10, white, setLiveUndervolt},
    {"apply and save to eeprom     ", 2, 1, 1, 0, 1, 1, 11, white, setPersistedUndervolt},
    {"back                         ", 2, 1, 1, 0, 1, 1, 13, white, exitSubmenu},
};

int enterUndervoltMenu(menu *self, uint8_t action)
{
  return enterSubmenu(undervoltMenu, sizeof(undervoltMenu) / sizeof(menu));
}

int setLiveUndervolt(menu *self, uint8_t action)
{
  for (int i = THUNDERVOLT_RAIL_1V0; i <= THUNDERVOLT_RAIL_3V3; i++) {
    // write display voltages to Thundervolt (live apply), and update cached live voltages
    thundervolt_set_voltage(i, undervoltMenu[i + 1].value);
    thundervolt_get_voltage(i, &liveVoltages[i]);
  }

  playSound(enter_raw, enter_raw_size);

  return 1;
}

int setPersistedUndervolt(menu *self, uint8_t action)
{
  for (int i = THUNDERVOLT_RAIL_1V0; i <= THUNDERVOLT_RAIL_3V3; i++) {
    // write display voltages to Thundervolt (live apply), and update cached live voltages
    thundervolt_set_voltage(i, undervoltMenu[i + 1].value);
    thundervolt_get_voltage(i, &liveVoltages[i]);

    // write display voltages to Thundervolt EEPROM
    thundervolt_set_persisted_voltage(i, undervoltMenu[i + 1].value);
  }

  playSound(enter_raw, enter_raw_size);

  return 1;
}

//
// Overtemp submenu
//

int setLiveOvertemp(menu *self, uint8_t action);
int setPersistedOvertemp(menu *self, uint8_t action);

static menu overtempMenu[] = {
    {"overtemp configuration       ", 4, 1, 0, 0, 1, 1, 0, light_grey, dummy},
    {"current temperature:         ", 4, 1, 0, 0, 1, 1, 2, white, dummy},
    {"overtemp threshold           ", 3, 1, 1, 1, 1, 1, 4, white, adjustValue, &OTSD_LIMITS},
    {"enable overtemp shutdown     ", 1, 1, 1, 0, 1, 1, 6, white, toggleOption},
    {"apply changes now            ", 2, 1, 1, 0, 1, 1, 8, white, setLiveOvertemp},
    {"apply and save to eeprom     ", 2, 1, 1, 0, 1, 1, 9, white, setPersistedOvertemp},
    {"back                         ", 2, 1, 1, 0, 1, 1, 11, white, exitSubmenu},
};

int enterOvertempMenu(menu *self, uint8_t action)
{
  return enterSubmenu(overtempMenu, sizeof(overtempMenu) / sizeof(menu));
}

int setLiveOvertemp(menu *self, uint8_t action)
{
  // write overtemp threshold to TMP1075N
  thundervolt_set_otsd_limit(overtempMenu[2].value);
  thundervolt_get_otsd_limit(&liveOvertemp);

  // write overtempShutdown state to thundervolt
  thundervolt_set_otsd_enabled(overtempMenu[3].value);
  thundervolt_get_otsd_enabled(&overtempShutdown);
  overtempMenu[3].value = overtempShutdown;

  playSound(enter_raw, enter_raw_size);

  return 1;
}

int setPersistedOvertemp(menu *self, uint8_t action)
{
  // write overtemp threshold to TMP1075N
  thundervolt_set_otsd_limit(overtempMenu[2].value);
  thundervolt_get_otsd_limit(&liveOvertemp);

  // write overtemp threshold to thundervolt eeprom
  thundervolt_set_persisted_otsd_limit(overtempMenu[2].value);

  // write overtempShutdown state to thundervolt
  thundervolt_set_otsd_enabled(overtempMenu[3].value);
  thundervolt_get_otsd_enabled(&overtempShutdown);
  overtempMenu[3].value = overtempShutdown;

  playSound(enter_raw, enter_raw_size);

  return 1;
}

//
// Credits submenu
//

static menu creditsMenu[] = {
    {"credits                      ", 4, 1, 0, 0, 1, 1, 0, light_grey, dummy},
    {"thundervolt software by:     ", 4, 1, 0, 0, 1, 1, 2, white, dummy},
    {"  YveltalGriffin (UI, app)   ", 4, 1, 0, 0, 1, 1, 3, white, dummy},
    {"  loopj (i2c, app)           ", 4, 1, 0, 0, 1, 1, 4, white, dummy},
    {"  Alex/supertazon (graphics) ", 4, 1, 0, 0, 1, 1, 5, white, dummy},
    {"music by ShockSlayer         ", 4, 1, 0, 0, 1, 1, 6, white, dummy},
    {"thundervolt firmware by loopj", 4, 1, 0, 0, 1, 1, 8, white, dummy},
    {"thundervolt hardware designed by YveltalGriffin", 4, 1, 0, 0, 1, 1, 10, white, dummy},
    {"back                         ", 2, 1, 1, 1, 1, 1, 12, white, exitSubmenu},
};

int enterCreditsMenu(menu *self, uint8_t action)
{
  return enterSubmenu(creditsMenu, sizeof(creditsMenu) / sizeof(menu));
}

const char *getHardwareName(uint8_t hw_variant)
{
  switch (hw_variant) {
    case THUNDERVOLT_HW1:
      return "thundervolt 1";
    case THUNDERVOLT_HW2:
      return "thundervolt 2";
    case THUNDERVOLT_LITE:
      return "thundervolt lite";
    default:
      return "unknown";
  }
}

void setupMenu()
{
  // Load font
  myFont = GRRLIB_LoadTTF(Glass_TTY_VT220_ttf, Glass_TTY_VT220_ttf_size);

  // Load textures
  logo      = GRRLIB_LoadTexture(tv_png);
  note      = GRRLIB_LoadTexture(note_png);
  arrow     = GRRLIB_LoadTexture(arrow_png);
  cursor    = GRRLIB_LoadTexture(cursor_png);
  bullet    = GRRLIB_LoadTexture(bullet_png);
  toggleOn  = GRRLIB_LoadTexture(toggle_on_png);
  toggleOff = GRRLIB_LoadTexture(toggle_off_png);
  icon      = GRRLIB_LoadTexture(dolphin_png);

  // set up main menu
  currentMenu       = mainMenu;
  currentNumEntries = sizeof(mainMenu) / sizeof(menu);

  getFirstSelectableEntry();
  getLastSelectableEntry();

  // Detect version of Thundervolt connected. We need to update menu entry colors
  // and selectability based on board type before rendering menu
  if (thundervolt_is_present()) {
    thundervoltPresent = true;

    // Print the hardware and software revisions of Thundervolt
    uint8_t hw_rev, sw_rev;
    thundervolt_get_hardware_revision(&hw_rev);
    thundervolt_get_software_revision(&sw_rev);
    snprintf(mainMenu[0].name, 50, "%s%s", getHardwareName(hw_rev), " detected!");

    // Check if safe mode is enabled
    bool safemode;
    thundervolt_get_safemode_enabled(&safemode);
    snprintf(mainMenu[1].name, 50, "%s%s", "safe mode ", safemode ? "enabled" : "disabled");

    // Check if power monitoring is supported
    if (thundervolt_has_power_monitoring()) {
      mainMenu[3].color      = white;
      mainMenu[3].selectable = true;
      mainMenu[6].color      = white;
      mainMenu[6].selectable = true;
    }

    // grab persisted voltages & live voltages
    for (int i = THUNDERVOLT_RAIL_1V0; i <= THUNDERVOLT_RAIL_3V3; i++) {
      thundervolt_get_voltage(i, &liveVoltages[i]);
      undervoltMenu[i + 1].value = liveVoltages[i];
    }

    // grab overtemp shutdown state
    thundervolt_get_otsd_enabled(&overtempShutdown);
    overtempMenu[3].value = overtempShutdown;

    // grab persisted overtemp threshold & live overtemp threshold
    thundervolt_get_otsd_limit(&liveOvertemp);
    overtempMenu[2].value = liveOvertemp;

  } else {

    snprintf(mainMenu[0].name, 50, "thundervolt not detected!");
    snprintf(mainMenu[1].name, 50, "safe mode unavailable");

    // disable menu entries
    mainMenu[2].color      = grey;
    mainMenu[4].color      = grey;
    mainMenu[4].selectable = false;
    mainMenu[4].selected   = false;
    mainMenu[5].color      = grey;
    mainMenu[5].selectable = false;

    mainMenu[8].selected = true;
  }
}

int handleMenuInput()
{
  menu *entry = &currentMenu[selectedEntry];

  // Check for directional input
  if (directionPressed(DIRECTION_UP)) {
    // if not at first selectable entry, move cursor up
    if (selectedEntry > firstSelectableEntry) {
      menu *prevEntry = &currentMenu[getPrevSelectable()];

      entry->selected     = false;
      prevEntry->selected = true;

      playSound(move_raw, move_raw_size);
    }
  } else if (directionPressed(DIRECTION_DOWN)) {
    // if not at last selectable entry, move cursor down
    if (selectedEntry < lastSelectableEntry) {
      menu *nextEntry = &currentMenu[getNextSelectable()];

      entry->selected     = false;
      nextEntry->selected = true;

      playSound(move_raw, move_raw_size);
    }
  } else if (directionPressed(DIRECTION_LEFT)) {
    // if on an adjustable entry, pass "left" action to entry's run function
    if (entry->enabled && entry->type == ADJUSTABLE && entry->run) {
      return entry->run(entry, DIRECTION_LEFT);
    }
  } else if (directionPressed(DIRECTION_RIGHT)) {
    // if on an adjustable entry, pass "right" action to entry's run function
    if (entry->enabled && entry->type == ADJUSTABLE && entry->run) {
      return entry->run(entry, DIRECTION_RIGHT);
    }
  } else if (padButtonPressed(PAD_TRIGGER_Z) || wpadButtonPressed(WPAD_BUTTON_1)) {
    // toggle music
    musicPaused = !musicPaused;
  } else if (padButtonPressed(PAD_BUTTON_A) || wpadButtonPressed(WPAD_BUTTON_A)) {
    // if entry is enabled, run its function
    if (entry->enabled && entry->run) {
      return entry->run(entry, 0);
    }
  } else if (padButtonPressed(PAD_BUTTON_B) || wpadButtonPressed(WPAD_BUTTON_B)) {
    // exit submenu if on a submenu
    if (currentMenu != mainMenu) {
      exitSubmenu(NULL, 0);
    }
  }
  
  /* Entries' functions should all return 1 except for mainMenu.exitToPad,
  which should return 0 and result in a break from this while loop. */
  return 1;
}

void drawMenu()
{
  // Update music state
  if (musicPaused) {
    MP3Player_Stop();
  } else if (!MP3Player_IsPlaying()) {
    MP3Player_PlayBuffer(sample_mp3, sample_mp3_size, NULL);
  }

  // black rectangle background
  GRRLIB_Rectangle(50, 40, 540, 380, 0x000000C8, true);

  // draw the logo
  GRRLIB_DrawImg(73, 58, logo, 0, 1, 1, white);

#if defined(DOLPHIN)
  // draw dolphin mode icon
  GRRLIB_DrawImg(280, 69, icon, 0, 1, 1, white);
#endif

  // update the board temp periodically
  if (thundervoltPresent) {
    u64 now = gettime();
    if (diff_usec(prevTempTime, now) > 500000) {
      thundervolt_get_temp(&temp);
      snprintf(mainMenu[2].name, 50, "%s%.2f%s", "board temp: ", temp, "°C");
      snprintf(overtempMenu[1].name, 50, "%s%.2f%s", "current temperature:                   ", temp, "°C");

      prevTempTime = now;
    }
  }

  // determine where to draw cursor
  getSelectedEntry();

  // menu drawing
  for (int n = 0; n < currentNumEntries; n++) {
    menu *entry = &currentMenu[n];

    // draw the entry name
    GRRLIB_PrintfTTF(85, 120 + entry->index * 20, myFont, entry->name, 20, entry->color);

    // draw toggle menu entries
    if (entry->type == TOGGLE) {
      // hardcode "dirty" comparison for now
      bool dirty = false;
      if (entry == &overtempMenu[3]) {
        dirty = entry->value != overtempShutdown;
      }

      // draw toggle switch
      GRRLIB_texImg *img = entry->value ? toggleOn : toggleOff;
      GRRLIB_DrawImg(COL_MID - img->w / 2, 125 + entry->index * 20, img, 0, 1, 1, dirty ? yellow : white);
    }

    // draw adjustable menu entries
    if (entry->type == ADJUSTABLE) {
      struct limits *limits = (struct limits *)entry->data;

      // if value is not min, draw left arrow
      if (!limits || entry->value > limits->min) {
        GRRLIB_DrawImg(COL_START, 123 + entry->index * 20, arrow, 0, 1, 1, white);
      }

      // if value is not max, draw right arrow
      if (!limits || entry->value < limits->max) {
        GRRLIB_DrawImg(COL_END, 123 + entry->index * 20, arrow, 0, -1, 1, white);
      }

      // hardcode "dirty" comparison for now
      bool dirty = false;
      if (currentMenu == undervoltMenu) {
        dirty = entry->value != liveVoltages[n - 1];
      } else if (currentMenu == overtempMenu) {
        dirty = entry->value != liveOvertemp;
      }

      // hardcode string formatting for now
      char valueStr[16];
      if (currentMenu == undervoltMenu) {
        snprintf(valueStr, 16, "%d%s", entry->value, " mV");
      } else if (currentMenu == overtempMenu) {
        snprintf(valueStr, 16, "%d%s", entry->value, "°C");
      } else {
        snprintf(valueStr, 16, "%d", entry->value);
      }

      // draw value text
      u32 textWidth = GRRLIB_WidthTTF(myFont, valueStr, 20);
      GRRLIB_PrintfTTF(COL_MID - textWidth / 2, 120 + entry->index * 20, myFont, valueStr, 20, dirty ? yellow : white);
    }

    // draw bullet points on credits menu
    if (currentMenu == creditsMenu && (entry->index > 2 && entry->index < 6)) {
      GRRLIB_DrawImg(82, 126 + entry->index * 20, bullet, 0, 1, 1, white);
    }

    // draw cursor
    if (entry->selected) {
      GRRLIB_DrawImg(60, 126 + entry->index * 20, cursor, 0, 1, 1, yellow);
    }
  }

  // now playing indicator / URL
  if (currentMenu == creditsMenu) {
    GRRLIB_PrintfTTF(390, 360, myFont, "visit thundervo.lt!", 20, yellow);
  } else {
    GRRLIB_DrawImg(420, 380, note, 0, 1, 1, white); // Draw a png
    GRRLIB_PrintfTTF(450, 380, myFont, "Enough Blocks", 20, white);
  }

  // send the frame buffer to the screen
  GRRLIB_Render();
}