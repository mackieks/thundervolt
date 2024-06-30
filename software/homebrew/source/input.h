#pragma once

// Input directions
enum direction {
  DIRECTION_UP,
  DIRECTION_DOWN,
  DIRECTION_LEFT,
  DIRECTION_RIGHT,
};

void updateInput(void);
bool directionPressed(enum direction direction);
bool padButtonPressed(uint32_t button);
bool padButtonHeld(uint32_t button);
bool wpadButtonPressed(uint32_t button);
bool wpadButtonHeld(uint32_t button);