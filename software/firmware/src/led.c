#include <stddef.h>

#include <avr/io.h>

#include "led.h"

// Effect types
enum effect_type {
  EFFECT_NONE,
  EFFECT_BLINK,
  EFFECT_BLINK_PATTERN,
  EFFECT_FADE_ON,
  EFFECT_FADE_OFF,
  EFFECT_BREATHE,
  EFFECT_CUSTOM,
};

// Data/settings storage for effects
union effect_data {
  // Data for fade, blink, and breathe effects
  uint16_t period;

  // Data for custom effects
  struct {
    led_effect_fn user_fn;
    void *user_data;
  };

  // Data for blink pattern effect
  struct {
    uint16_t *pattern;
    uint8_t length;
    uint8_t index;
    uint32_t last_change;
  };
};

// Time since the active effect started
static uint32_t start_time;

// Active effect type and data
static enum effect_type effect_type;
static union effect_data effect_data;

// Get the brightness for a fade-on effect
static inline uint8_t fade_on_brightness(uint32_t elapsed)
{
  return elapsed * 255 / effect_data.period;
}

// Get the brightness for a fade-off effect
static inline uint8_t fade_off_brightness(uint32_t elapsed)
{
  return 255 - fade_on_brightness(elapsed);
}

// Set the LED brightness directly
static inline void led_set_raw(uint8_t brightness)
{
  TCB0.CCMPH = brightness;
}

void led_init()
{
  // Select the alternative output pin for TCB0 (PC0)
  PORTMUX.CTRLD = PORTMUX_TCB0_bm;

  // Configure TCB0 for 8-bit PWM output
  TCB0.CTRLA = TCB_ENABLE_bm;
  TCB0.CTRLB = TCB_CCMPEN_bm | TCB_CNTMODE_PWM8_gc;

  // Set the PWM period
  TCB0.CCMPL = 0xFF;
}

void led_set(uint8_t brightness)
{
  effect_type = EFFECT_NONE;
  led_set_raw(brightness);
}

void led_effect_none()
{
  effect_type = EFFECT_NONE;
}

void led_effect_blink(uint16_t period)
{
  effect_type = EFFECT_BLINK;
  start_time  = 0;

  effect_data.period = period;
}

void led_effect_blink_pattern(uint16_t *pattern, uint8_t length)
{
  effect_type = EFFECT_BLINK_PATTERN;
  start_time  = 0;

  effect_data.pattern     = pattern;
  effect_data.length      = length;
  effect_data.index       = 0;
  effect_data.last_change = 0;
}

void led_effect_fade_on(uint16_t period)
{
  effect_type = EFFECT_FADE_ON;
  start_time  = 0;

  effect_data.period = period;
}

void led_effect_fade_off(uint16_t period)
{
  effect_type = EFFECT_FADE_OFF;
  start_time  = 0;

  effect_data.period = period;
}

void led_effect_breathe(uint16_t period)
{
  effect_type = EFFECT_BREATHE;
  start_time  = 0;

  effect_data.period = period;
}

void led_effect_custom(led_effect_fn user_fn, void *user_data)
{
  effect_type = EFFECT_CUSTOM;
  start_time  = 0;

  effect_data.user_fn   = user_fn;
  effect_data.user_data = user_data;
}

void led_effect_update(uint32_t millis)
{
  if (effect_type == EFFECT_NONE)
    return;

  // Initialize the start time
  if (start_time == 0)
    start_time = millis;

  // Calculate the elapsed time since the effect started
  uint32_t elapsed = millis - start_time;

  // Set the LED brightness based on the effect type
  switch (effect_type) {
    case EFFECT_BLINK: {
      uint32_t pos = elapsed % (effect_data.period * 2);
      led_set_raw(pos < effect_data.period ? 255 : 0);
      break;
    }

    case EFFECT_BLINK_PATTERN: {
      uint16_t period = effect_data.pattern[effect_data.index];
      if (effect_data.last_change == 0) {
        effect_data.last_change = elapsed;
      } else if (elapsed - effect_data.last_change >= period) {
        effect_data.last_change = elapsed;
        effect_data.index       = (effect_data.index + 1) % effect_data.length;
      }

      led_set_raw((effect_data.index % 2 == 0) ? 255 : 0);
      break;
    }

    case EFFECT_FADE_ON:
      led_set_raw(fade_on_brightness(elapsed));
      break;

    case EFFECT_FADE_OFF:
      led_set_raw(fade_off_brightness(elapsed));
      break;

    case EFFECT_BREATHE: {
      uint32_t pos = (elapsed % effect_data.period) * 2;
      led_set_raw(pos < effect_data.period ? fade_on_brightness(pos)
                                           : fade_off_brightness(pos - effect_data.period));
      break;
    }

    case EFFECT_CUSTOM:
      led_set_raw(effect_data.user_fn(elapsed, effect_data.user_data));
      break;
  }
}
