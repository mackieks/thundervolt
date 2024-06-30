/**
 * LED effects library for AVR 0/1-series MCUs.
 *
 * This library provides a simple API for setting effects on non-addressable LEDs,
 * such as blinking, fading, and breathing.
 *
 * Currently only supports a single LED, connected to the TCB0 PWM output.
 * TODO: Support setting pin/port, active high/low, etc.
 */

#pragma once

#include <stdint.h>

/**
 * Custom LED effect function.
 *
 * @param millis The current time in milliseconds
 * @param data   User data for the effect, previously set with led_effect_custom
 *
 * @return The brightness of the LED, from 0 to 255
 */
typedef uint8_t (*led_effect_fn)(uint32_t millis, void *data);

/**
 * Initialize the LED
 */
void led_init();

/**
 * Set the LED brightness, and disable any effects.
 *
 * @param brightness The brightness of the LED, from 0 to 255
 */
void led_set(uint8_t brightness);

/**
 * Turn the LED on, and disable any effects.
 */
static inline void led_on()
{
  led_set(255);
}

/**
 * Turn the LED off, and disable any effects.
 */
static inline void led_off()
{
  led_set(0);
}

/**
 * Disable any active LED effect.
 */
void led_effect_none();

/**
 * Blink the LED on and off.
 *
 * @param duration The duration of the blink, in milliseconds
 */
void led_effect_blink(uint16_t duration);

/**
 * Blink the LED on and off with a custom pattern.
 *
 * @param pattern The pattern to blink, where each element is the duration in milliseconds
 *                for the LED to be on or off, starting with on
 * @param length  The length of the pattern
 */
void led_effect_blink_pattern(uint16_t *pattern, uint8_t length);

/**
 * Fade the LED on over time.
 *
 * @param duration The duration of the fade, in milliseconds
 */
void led_effect_fade_on(uint16_t duration);

/**
 * Fade the LED off over time.
 *
 * @param duration The duration of the fade, in milliseconds
 */
void led_effect_fade_off(uint16_t duration);

/**
 * Breathe the LED on and off.
 *
 * @param duration The duration of the breathe cycle, in milliseconds
 */
void led_effect_breathe(uint16_t duration);

/**
 * Run a custom LED effect.
 *
 * @param user_fn   The custom effect function
 * @param user_data User data to pass to the custom effect function
 */
void led_effect_custom(led_effect_fn user_fn, void *user_data);

/**
 * Update the LED effect.
 *
 * This function should be called periodically to update the LED effect,
 * typically from an periodic interrupt, or from the main loop.
 */
void led_effect_update(uint32_t millis);