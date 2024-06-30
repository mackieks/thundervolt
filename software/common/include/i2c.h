/**
 * Cross platform I2C library, heavily inspired by the Linux/Zephyr I2C APIs.
 *
 * Provides functions for configuring the I2C bus, sending messages, and reading/writing registers.
 *
 * It is up to the platform to implement both the `i2c_configure` and `i2c_transfer` functions.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * I2C modes.
 */

/** Standard mode (100 KHz) */
#define I2C_MODE_STANDARD       0

/** Fast mode (400 KHz) */
#define I2C_MODE_FAST           1

/**
 * I2C message flags.
 */

/** Write message to the I2C bus. */
#define I2C_MSG_WRITE           (0 << 0)

/** Read message from the I2C bus. */
#define I2C_MSG_READ            (1 << 0)

/** Send a STOP condition after the message. */
#define I2C_MSG_STOP            (1 << 1)

/** Send a RESTART condition before this message. */
#define I2C_MSG_RESTART         (1 << 2)

/**
 * A message to be sent or received on the I2C bus.
 */
struct i2c_msg {
  /** Data buffer */
  uint8_t *buf;

  /** Length of the buffer, in bytes */
  uint32_t len;

  /** Flags for the message */
  uint8_t flags;
};

/**
 * I2C error codes.
 */
enum i2c_error {
  I2C_ERR = 1,
};

/**
 * Initialize the I2C bus as a controller.
 *
 * @param mode I2C_MODE_STANDARD or I2C_MODE_FAST
 * @return 0 if successful, negative error code
 */
int i2c_configure(uint8_t mode);

/**
 * Send one or more messages on the I2C bus, in a single transfer.
 * STOP is issued to terminate the operation; each message begins with a START.
 *
 * @param addr     7-bit I2C address
 * @param msgs     Array of messages to send
 * @param num_msgs Number of messages to send
 * @return 0 if successful, negative error code otherwise
 */
int i2c_transfer(uint8_t addr, struct i2c_msg *msgs, uint8_t num_msgs);

/**
 * Detect if an I2C device is present at a given address.
 *
 * @param addr 7-bit I2C address of the target device
 * @return true if the device is present, false otherwise
 */
static inline bool i2c_detect(uint8_t addr)
{
  uint8_t tmp;

  struct i2c_msg msg = {
      .buf   = &tmp,
      .len   = 0,
      .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
  };

  return i2c_transfer(addr, &msg, 1) == 0;
}

/**
 * Write a set amount of data to an I2C device.
 *
 * @param addr 7-bit I2C address of the target device
 * @param buf Buffer containing the data to write
 * @param len Number of bytes to write
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_write(uint8_t addr, uint8_t *buf, uint32_t len)
{
  struct i2c_msg msg = {
      .buf   = buf,
      .len   = len,
      .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
  };

  return i2c_transfer(addr, &msg, 1);
}

/**
 * Read a set amount of data from an I2C device.
 *
 * @param addr 7-bit I2C address of the target device
 * @param buf Buffer to store the read data
 * @param len Number of bytes to read
 *
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_read(uint8_t addr, uint8_t *buf, uint32_t len)
{
  struct i2c_msg msg = {
      .buf   = buf,
      .len   = len,
      .flags = I2C_MSG_READ | I2C_MSG_STOP,
  };

  return i2c_transfer(addr, &msg, 1);
}

/**
 * Write then read from an I2C device.
 *
 * @param addr 7-bit I2C address of the target device
 * @param write_buf Buffer to write to the device
 * @param write_len Length of the write buffer
 * @param read_buf Buffer to read from the device
 * @param read_len Length of the read buffer
 */
static inline int i2c_write_read(uint8_t addr, uint8_t *write_buf, uint32_t write_len,
                                 uint8_t *read_buf, uint32_t read_len)
{
  struct i2c_msg msgs[] = {
      {
          .buf   = write_buf,
          .len   = write_len,
          .flags = I2C_MSG_WRITE,
      },
      {
          .buf   = read_buf,
          .len   = read_len,
          .flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP,
      },
  };

  return i2c_transfer(addr, msgs, 2);
}

/**
 * Read a single byte from an I2C device, at the specified register address.
 * This is identical to the SMBus "Read Byte" command.
 *
 * @param addr 7-bit I2C address of the target device
 * @param reg Register address to read from
 * @param data Pointer to store the read data
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_reg_read_byte(uint8_t addr, uint8_t reg, uint8_t *data)
{
  return i2c_write_read(addr, &reg, 1, data, 1);
}

/**
 * Write a single byte to an I2C device, at the specified register address.
 * This is identical to the SMBus "Write Byte" command.
 *
 * @param addr 7-bit I2C address of the target device
 * @param reg Register address to write to
 * @param value Value to write to the register
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_reg_write_byte(uint8_t addr, uint8_t reg, uint8_t value)
{
  uint8_t buf[] = {reg, value};
  return i2c_write(addr, buf, 2);
}

/**
 * Read a little-endian word from an I2C device, at the specified register address.
 * This is identical to the SMBus "Read Word" command.
 *
 * @param addr 7-bit I2C address of the target device
 * @param reg Register address to read from
 * @param value Pointer to store the read data
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_reg_read_word(uint8_t addr, uint8_t reg, uint16_t *value)
{
  int rcode;

  // Read the register value
  uint8_t buf[2];
  if ((rcode = i2c_write_read(addr, &reg, 1, buf, 2)) < 0)
    return rcode;

  // Combine the two bytes into a little-endian word
  *value = buf[0] | (buf[1] << 8);

  return 0;
}

/**
 * Write a little-endian word to an I2C device, at the specified register address.
 * This is identical to the SMBus "Write Word" command.
 *
 * @param addr 7-bit I2C address of the target device
 * @param reg Register address to write to
 * @param value Value to write to the register
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_reg_write_word(uint8_t addr, uint8_t reg, uint16_t value)
{
  uint8_t buf[] = {reg, value & 0xFF, value >> 8};
  return i2c_write(addr, buf, 3);
}

/**
 * Perform a read/modify/write operation on a single byte register of an I2C device.
 *
 * @param addr 7-bit I2C address of the target device
 * @param reg Register address to read from and write to
 * @param mask Bitmask to apply to the register value
 * @param value Value to write to the register
 * @return 0 if successful, negative error code otherwise
 */
static inline int i2c_reg_update_byte(uint8_t addr, uint8_t reg, uint8_t mask, uint8_t value)
{
  int rcode;

  // Read the current register value
  uint8_t old_value;
  if ((rcode = i2c_reg_read_byte(addr, reg, &old_value)) < 0)
    return rcode;

  // Update the register value
  uint8_t new_value = (old_value & ~mask) | (value & mask);
  if (new_value == old_value) {
    return 0;
  }

  // Write the updated register value
  return i2c_reg_write_byte(addr, reg, new_value);
}