/*
    Menu
*/

#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum entry_type { SUBMENU, TOGGLE, FUNCTION, ADJUSTABLE, INERT };

typedef struct menu_s menu;

struct menu_s {
  char name[50];
  enum entry_type type;
  bool visible;
  bool selectable; // control for items cursor can be next to
  bool selected;
  int value;
  bool enabled;
  int index; // GUI entry location (max = 14?)
  uint32_t color; // 32-bit RGBA value
  int (*run)(menu *self, uint8_t action);
  void *data;
};

int toggleOption(menu *self, uint8_t action);

int exitToPad(menu *self, uint8_t action);

int dummy(menu *self, uint8_t action);

int exitSubmenu(menu *self, uint8_t action);

void setupMenu(void);

int handleMenuInput(void);

void drawMenu(void);