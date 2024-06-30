/*
 * Basic I2C target device library for working with byte registers.
 *
 * - Provides a simple interface for reading and writing byte registers
 * - Supports 7-bit I2C addresses
 * - Supports auto-incrementing register reads and writes
 * - No support for I2C general call addresses
 * - No support for matching on multiple addresses
 */

#pragma once

#include <stdint.h>

/**
 * Callback for reading a value from a single byte register at the specified address
 *
 * @param reg_addr The address of the register to read
 * @param value A pointer to the location to store the read value
 */
typedef int (*read_register_fn)(uint8_t reg_addr, uint8_t *value);

/**
 * Callback for writing a value to a single byte register at the specified address
 *
 * @param reg_addr The address of the register to write
 * @param value The value to write to the register
 */
typedef int (*write_register_fn)(uint8_t reg_addr, uint8_t value);

/*
 * Initialize as an I2C target device, with the provided access functions
 *
 * @param dev_addr The 7-bit I2C address of the device
 * @param read_fn The callback function for reading a register
 * @param write_fn The callback function for writing a register
 */
void i2c_target_init(uint8_t dev_addr, read_register_fn read_fn, write_register_fn write_fn);