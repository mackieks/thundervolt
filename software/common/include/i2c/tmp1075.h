/*
 * I2C driver for TMP1075 temperature sensor
 * https://www.ti.com/lit/ds/symlink/tmp1075.pdf
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Register map
#define TMP1075_REG_TEMP    0x00
#define TMP1075_REG_CFGR    0x01
#define TMP1075_REG_LLIM    0x02
#define TMP1075_REG_HLIM    0x03
#define TMP1075_REG_DIEID   0x0F

// CFGR register mask
#define TMP1075_OS  (1 << 15)
#define TMP1075_R   (1 << 14) | (1 << 13)
#define TMP1075_F   (1 << 12) | (1 << 11)
#define TMP1075_POL (1 << 10)
#define TMP1075_TM  (1 << 9)
#define TMP1075_SD  (1 << 8)

// Conversion rate values
#define TMP1075_CONV_RATE_27_5  0x0000 // 27.5ms, default on TMP1075
#define TMP1075_CONV_RATE_55    0x2000 // 55ms
#define TMP1075_CONV_RATE_110   0x4000 // 110ms
#define TMP1075_CONV_RATE_220   0x6000 // 220ms (35ms, default on TMP1075N)

// Fault count values
#define TMP1075_FAULT_COUNT_1   0x0000 // 1 fault, default
#define TMP1075_FAULT_COUNT_2   0x0800 // 2 faults
#define TMP1075_FAULT_COUNT_3   0x1000 // 3 faults (4 faults on TMP1075N)
#define TMP1075_FAULT_COUNT_4   0x1800 // 4 faults (6 faults on TMP1075N)

// Alert mode
#define TMP1075_ALERT_MODE_COMPARATOR   0
#define TMP1075_ALERT_MODE_INTERRUPT    (1 << 9)

// Power mode
#define TMP1075_POWER_MODE_CONTINUOUS   0
#define TMP1075_POWER_MODE_SHUTDOWN     (1 << 8)

// Check if the TMP1075 is present on the I2C bus
bool tmp1075_is_present(uint8_t addr);

// Get temperature of last conversion, in deg C
int tmp1075_get_temp(uint8_t addr, float *temp);

// Start a one-shot conversion
int tmp1075_start_conversion(uint8_t addr);

// Set conversion rate (default is 27.5ms on TMP1075, 220ms on TMP1075N)
int tmp1075_set_conversion_rate(uint8_t addr, uint16_t rate);

// Set fault count to trigger alert (default is 1)
int tmp1075_set_fault_count(uint8_t addr, uint16_t count);

// Set polarity of the output pin (default is active low)
int tmp1075_set_alert_polarity(uint8_t addr, bool polarity);

// Set alert mode (comparator or interrupt)
int tmp1075_set_alert_mode(uint8_t addr, uint16_t mode);

// Set power mode (shutdown or continuous conversion)
int tmp1075_set_power_mode(uint8_t addr, uint16_t mode);

// Get low temperature limit, in deg C
int tmp1075_get_low_limit(uint8_t addr, float *temp);

// Set low temperature limit, in deg C
int tmp1075_set_low_limit(uint8_t addr, float temp);

// Get high temperature limit, in deg C
int tmp1075_get_high_limit(uint8_t addr, float *temp);

// Set high temperature limit, in deg C
int tmp1075_set_high_limit(uint8_t addr, float temp);