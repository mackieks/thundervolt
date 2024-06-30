/**
 * I2C driver for TPS63810 and TPS63811 programmable buck/boost regulators
 * https://www.ti.com/lit/ds/symlink/tps63811.pdf
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// I2C device details
#define TPS6381X_I2C_ADDR       0x75
#define TPS6381X_DEVID          0x04

// Register map
#define TPS6381X_REG_CONTROL    0x01
#define TPS6381X_REG_STATUS     0x02
#define TPS6381X_REG_DEVID      0x03
#define TPS6381X_REG_VOUT1      0x04
#define TPS6381X_REG_VOUT2      0x05

// CONTROL register mask
#define TPS6381X_RANGE          (1 << 6)
#define TPS6381X_ENABLE         (1 << 5)
#define TPS6381X_FPWM           (1 << 3)
#define TPS6381X_RPWM           (1 << 2)
#define TPS6381X_SLEW           0x03 // [0:1]

// STATUS register mask
#define TPS6381X_TSD            (1 << 1)
#define TPS6381X_PG             (1 << 0)

// Slew rate values (V/ms)
#define TPS6381X_SLEW_RATE_1    0x0 // Default
#define TPS6381X_SLEW_RATE_2_5  0x1
#define TPS6381X_SLEW_RATE_5    0x2
#define TPS6381X_SLEW_RATE_10   0x3

// Voltage range values
#define TPS6381X_RANGE_LOW      (0 << 6) // Default, 1.800V to 4.975 V
#define TPS6381X_RANGE_HIGH     (1 << 6) // 2.025V to 5.200 V

// VOUT calculation constants (in mV)
#define TPS6381X_VOUT_RESOLUTION    25
#define TPS6381X_VOUT_START_LOW     1800
#define TPS6381X_VOUT_START_HIGH    2025
#define TPS6381X_VOUT_END_LOW       4975
#define TPS6381X_VOUT_END_HIGH      5200

// Check if the TPS6381x is present on the I2C bus
bool tps6381x_is_present();

// Set slew rate using TPS6381X_SLEW_RATE_xxx values
int tps6381x_set_slew_rate(uint8_t slew_rate);

// Enable or disable the regulator (default enabled on TPS63810, disabled on TPS63811)
int tps6381x_enable(bool enable);

// Get the current voltage range
int tps6381x_get_range(uint8_t *range);

// Set the voltage range using TPS6381X_RANGE_xxx values
int tps6381x_set_range(uint8_t range);

// Get the output voltage in mV, when VSEL is LOW
int tps6381x_get_vout1(uint16_t *voltage);

// Get the output voltage in mV, when VSEL is HIGH
int tps6381x_get_vout2(uint16_t *voltage);

// Set voltage in mV when VSEL is LOW
int tps6381x_set_vout1(uint16_t voltage);

// Set voltage in mV when VSEL is HIGH
int tps6381x_set_vout2(uint16_t voltage);