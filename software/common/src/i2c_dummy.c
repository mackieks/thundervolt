/*
 * I2C dummy core for testing I2C devices.
 */

#if defined(HW_RVL) && defined(DOLPHIN)

#include <stddef.h>

#include "i2c.h"

// I2C transaction states
enum i2c_state {
  IDLE,
  NEW_TRANSACTION,
  RECEIVED_ADDRESS,
  RECEIVED_PARTIAL_DATA,
  RECEIVED_DATA,
  SENT_PARTIAL_DATA,
  SENT_DATA
};

// Forward declaration of I2C device structure
struct i2c_device;

// I2C device read and write function pointers
typedef uint8_t (*read_fn)(struct i2c_device *device);
typedef int (*write_fn)(struct i2c_device *device, uint8_t data);

// I2C device structure
struct i2c_device {
  // Device address
  uint8_t addr;

  // Device read and write handlers
  read_fn read_byte;
  write_fn write_byte;

  // Device register storage
  void *registers;

  // Register pointer
  uint8_t register_pointer;

  // Transaction state
  enum i2c_state state;
};

// Storage for dummy I2C devices
static struct i2c_device devices[16];
static uint8_t num_devices = 0;

// Handle generic pass-through byte reads for 8-bit registers
static uint8_t reg8_read_byte(struct i2c_device *device)
{
  uint8_t *registers = device->registers;
  if (device->state == RECEIVED_ADDRESS || device->state == SENT_DATA) {
    device->state = SENT_DATA;
    return registers[device->register_pointer++];
  }

  return 0;
}

// Handle generic pass-through byte writes for 8-bit registers
static int reg8_write_byte(struct i2c_device *device, uint8_t data)
{
  uint8_t *registers = device->registers;
  if (device->state == NEW_TRANSACTION) {
    device->state            = RECEIVED_ADDRESS;
    device->register_pointer = data;
  } else if (device->state == RECEIVED_ADDRESS || device->state == RECEIVED_DATA) {
    device->state                         = RECEIVED_DATA;
    registers[device->register_pointer++] = data;
  }

  return 0;
}

// Handle generic pass-through byte reads for 16-bit, big-endian registers
static uint8_t reg16be_read_byte(struct i2c_device *device)
{
  uint16_t *registers = device->registers;
  if (device->state == RECEIVED_ADDRESS || device->state == SENT_DATA) {
    device->state = SENT_PARTIAL_DATA;
    return registers[device->register_pointer] >> 8;
  } else if (device->state == SENT_PARTIAL_DATA) {
    device->state = SENT_DATA;
    return registers[device->register_pointer++] & 0xFF;
  }

  return 0;
}

// Handle generic pass-through byte writes for 16-bit, big-endian registers
static int reg16be_write_byte(struct i2c_device *device, uint8_t data)
{
  uint16_t *registers = device->registers;
  if (device->state == NEW_TRANSACTION) {
    device->state            = RECEIVED_ADDRESS;
    device->register_pointer = data;
  } else if (device->state == RECEIVED_ADDRESS || device->state == RECEIVED_DATA) {
    device->state                       = RECEIVED_PARTIAL_DATA;
    registers[device->register_pointer] = data << 8;
  } else if (device->state == RECEIVED_PARTIAL_DATA) {
    device->state = RECEIVED_DATA;
    registers[device->register_pointer++] |= data;
  }

  return 0;
}

// Add a dummy device to the list of devices
static void add_device(uint8_t addr, void *registers, read_fn read_byte, write_fn write_byte)
{
  struct i2c_device *device = &devices[num_devices++];
  device->addr              = addr;
  device->read_byte         = read_byte;
  device->write_byte        = write_byte;
  device->registers         = registers;
  device->register_pointer  = 0;
  device->state             = IDLE;
}

// Get a dummy I2C device by its address
static struct i2c_device *get_i2c_device(uint8_t addr)
{
  for (uint8_t i = 0; i < num_devices; i++) {
    if (devices[i].addr == addr) {
      return &devices[i];
    }
  }

  return NULL;
}

// Handle an I2C start condition
static void i2c_start(struct i2c_device *device)
{
  device->state = NEW_TRANSACTION;
}

// Handle an I2C stop condition
static void i2c_stop(struct i2c_device *device)
{
  device->state            = IDLE;
  device->register_pointer = 0;
}

// Read a byte from an I2C device
static uint8_t i2c_read_byte(struct i2c_device *device)
{
  return device->read_byte(device);
}

// Write a byte to an I2C device
static int i2c_write_byte(struct i2c_device *device, uint8_t data)
{
  return device->write_byte(device, data);
}

// Dummy device registers
static uint8_t thundervolt_regs[]   = {0x04, 0x00, 0xE8, 0x03, 0x7E, 0x04, 0x08,
                                       0x07, 0xE4, 0x0C, 0x46, 0x01, 0x01};
static uint8_t tps6286x_1v0_regs[]  = {0x00, 0x78, 0x78, 0x00, 0x00, 0x00};
static uint8_t tps6286x_1v15_regs[] = {0x00, 0x96, 0x96, 0x00, 0x00, 0x00};
static uint8_t tps6286x_1v8_regs[]  = {0x00, 0x64, 0x64, 0x00, 0x00, 0x00};
static uint8_t tps6381x_regs[]      = {0x00, 0x00, 0x00, 0x04, 0x3C, 0x42};
static uint16_t tmp1075_regs[]      = {0x3000, 0x00FF, 0x4100, 0x4600};

int i2c_configure(uint8_t mode)
{
  // Add dummy I2C devices to the "bus"
  num_devices = 0;
  add_device(0x48, thundervolt_regs, reg8_read_byte, reg8_write_byte);
  add_device(0x49, tmp1075_regs, reg16be_read_byte, reg16be_write_byte);
  add_device(0x43, tps6286x_1v0_regs, reg8_read_byte, reg8_write_byte);
  add_device(0x46, tps6286x_1v15_regs, reg8_read_byte, reg8_write_byte);
  add_device(0x41, tps6286x_1v8_regs, reg8_read_byte, reg8_write_byte);
  add_device(0x75, tps6381x_regs, reg8_read_byte, reg8_write_byte);

  return 0;
}

int i2c_transfer(uint8_t addr, struct i2c_msg *msgs, uint8_t num_msgs)
{
  struct i2c_device *device = get_i2c_device(addr);
  if (!device)
    return -1;

  // Always start with a start condition
  unsigned int flags = I2C_MSG_RESTART;

  // Return early if there are no messages
  if (!num_msgs)
    return 0;

  do {
    // Send stop condition from previous message, if needed
    if (flags & I2C_MSG_STOP) {
      i2c_stop(device);
    }

    // Forget old flags, except for start
    flags &= I2C_MSG_RESTART;

    // Send start or repeated start condition
    if (flags & I2C_MSG_RESTART) {
      i2c_start(device);
    }

    // Get flags for new message
    flags |= msgs->flags;

    // Send address after start condition
    if (flags & I2C_MSG_RESTART) {
      flags &= ~I2C_MSG_RESTART;
    }

    // Transfer data
    uint8_t *buf     = msgs->buf;
    uint8_t *buf_end = buf + msgs->len;
    if (flags & I2C_MSG_READ) {
      while (buf < buf_end) { *buf++ = i2c_read_byte(device); }
    } else {
      while (buf < buf_end) { i2c_write_byte(device, *buf++); }
    }

    // Next message
    msgs++;
    num_msgs--;
  } while (num_msgs);

  // Send final stop condition
  i2c_stop(device);
  return 0;
}

#endif