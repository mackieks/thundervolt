/*
 * I2C driver for TPS62868 and TPS62869 programmable buck regulators
 * https://www.ti.com/lit/ds/symlink/tps62868.pdf
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// I2C device details
#define TPS6286X_I2C_ADDR_START 0x40
#define TPS6286X_I2C_ADDR_END   0x4F
#define TPS6286X_I2C_ADDR_FIXED 0x42

// Register map
#define TPS6286X_REG_VOUT1      0x01
#define TPS6286X_REG_VOUT2      0x02
#define TPS6286X_REG_CONTROL    0x03
#define TPS6286X_REG_STATUS     0x05

// CONTROL register mask
#define TPS6286X_RESET          (1 << 7)
#define TPS6286X_FORCE_FPWM     (1 << 6)
#define TPS6286X_ENABLE         (1 << 5)
#define TPS6286X_FPWM           (1 << 4)
#define TPS6286X_DISCHARGE      (1 << 3)
#define TPS6286X_HICCUP_EN      (1 << 2)
#define TPS6286X_SLEW           0x03 // [0:1]

// STATUS register mask
#define TPS6286X_THERM          (1 << 4)
#define TPS6286X_HICCUP         (1 << 3)
#define TPS6286X_UVLO           (1 << 0)

// Slew rate values (mV/us)
#define TPS6286X_SLEW_RATE_20   0x0
#define TPS6286X_SLEW_RATE_10   0x1
#define TPS6286X_SLEW_RATE_5    0x2
#define TPS6286X_SLEW_RATE_1    0x3 // Default

// VOUT calculation constants (in mV)
#define TPS6286X_VOUT_STEP 5
#define TPS6286X_VOUT_BASE 400

// Chip types, for determining voltage scaling factor
#define TPS6286X0A  0
#define TPS6286X1A  1
#define TPS6286X2A  2

// Error codes
enum {
  TPS6286X_ERR_INVALID_SCALE = 10,
  TPS6286X_ERR_INVALID_VOLTAGE,
};

// Check if a TPS6286x is present on the I2C bus at the given address
bool tps6286x_is_present(uint8_t addr);

// Enable or disable the regulator (enabled by default)
int tps6286x_enable(uint8_t addr, bool enabled);

// Set slew rate using TPS6286X_SLEW_RATE__xxx values
int tps6286x_set_slew_rate(uint8_t addr, uint8_t slew_rate);

// Get voltage in mV when VSET is LOW
int tps6286x_get_vout1(uint8_t addr, uint8_t chip_type, uint16_t *voltage);

// Get voltage in mV when VSET is HIGH
int tps6286x_get_vout2(uint8_t addr, uint8_t chip_type, uint16_t *voltage);

// Set voltage in mV when VSET is LOW
int tps6286x_set_vout1(uint8_t addr, uint8_t chip_type, uint16_t voltage);

// Set voltage in mV when VSET is HIGH
int tps6286x_set_vout2(uint8_t addr, uint8_t chip_type, uint16_t voltage);