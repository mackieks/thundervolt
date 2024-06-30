/*
 * I2C Wii bit-banging core.
 */

#if defined(HW_RVL) && !defined(DOLPHIN)

#include <gctypes.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#include "i2c.h"

// Wii GPIO registers
#define HW_GPIO_BASE    0xCD0000C0
#define HW_GPIOB_OUT    (*((vu32*)(HW_GPIO_BASE + 0x00)))
#define HW_GPIOB_DIR    (*((vu32*)(HW_GPIO_BASE + 0x04)))
#define HW_GPIOB_IN     (*((vu32*)(HW_GPIO_BASE + 0x08)))

// Wii GPIO pins
#define GPIO_AVE_SCL    (1 << 14)
#define GPIO_AVE_SDA    (1 << 15)

// Convert nanoseconds to ticks, adjust for function call overhead, rise times
#define NS_TO_TICKS(ticks) (nanosecs_to_ticks(ticks) - 18)

// Drive modes for SCL line
enum { I2C_DRIVE_PUSH_PULL, I2C_DRIVE_OPEN_DRAIN };

// Is the I2C bus configured yet?
static bool configured = false;

// Configured I2C timings (in ticks)
static uint32_t delay; // Half SCL period
static uint32_t half_delay; // Quarter SCL period

// The current drive mode of the SCL line
// Default to push/pull since an unmodified Wii has no pull-up resistor on SCL
static bool drive_mode = I2C_DRIVE_PUSH_PULL;

// Set the state of the SCL line
static inline void i2c_set_scl(int state)
{
  if (state) {
    if (drive_mode == I2C_DRIVE_OPEN_DRAIN) {
      // Set as input, allow pull-up resistor to pull the line high
      HW_GPIOB_DIR &= ~GPIO_AVE_SCL;

      // Wait for the target to release the SCL line to support clock stretching
      // TODO: Add a timeout
      while (!(HW_GPIOB_IN & GPIO_AVE_SCL));
    } else {
      // Set as output, and pull the line high
      HW_GPIOB_OUT |= GPIO_AVE_SCL;
      HW_GPIOB_DIR |= GPIO_AVE_SCL;
    }
  } else {
    // Set as output, and pull the line low
    HW_GPIOB_OUT &= ~GPIO_AVE_SCL;
    HW_GPIOB_DIR |= GPIO_AVE_SCL;
  }
}

// Set the state of the SDA line
static inline void i2c_set_sda(int state)
{
  if (state) {
    // Set SDA as input, allow pull-up resistor to pull the line high
    HW_GPIOB_DIR &= ~GPIO_AVE_SDA;
  } else {
    // Set SDA low, and as an output
    HW_GPIOB_OUT &= ~GPIO_AVE_SDA;
    HW_GPIOB_DIR |= GPIO_AVE_SDA;
  }
}

// Get the current state of the SDA line
static inline bool i2c_get_sda()
{
  // Set SDA as input and return the state
  HW_GPIOB_DIR &= ~GPIO_AVE_SDA;
  return HW_GPIOB_IN & GPIO_AVE_SDA;
}

// Delay for a number of ticks
static inline void i2c_delay(unsigned int ticks)
{
  uint32_t start = gettick();
  while (gettick() - start < ticks);
}

// I2C start condition
static inline void i2c_start()
{
  i2c_set_sda(0);
  i2c_delay(half_delay);

  i2c_set_scl(0);
  i2c_delay(half_delay);
}

// I2C repeated start condition
static inline void i2c_repeated_start()
{
  i2c_set_sda(1);
  i2c_delay(half_delay);

  i2c_set_scl(1);
  i2c_delay(half_delay);

  i2c_start();
}

// I2C stop condition
static inline void i2c_stop()
{
  i2c_set_sda(0);
  i2c_delay(half_delay);

  i2c_set_scl(1);
  i2c_delay(delay);

  i2c_set_sda(1);
  i2c_delay(delay);
}

// Write a single bit
static inline void i2c_write_bit(int bit)
{
  i2c_set_sda(bit);
  i2c_delay(half_delay);

  i2c_set_scl(1);
  i2c_delay(delay);

  i2c_set_scl(0);
  i2c_delay(half_delay);
}

// Read a single bit
static inline int i2c_read_bit()
{
  i2c_set_sda(1);
  i2c_delay(half_delay);

  i2c_set_scl(1);
  i2c_delay(delay);

  int bit = i2c_get_sda();

  i2c_set_scl(0);
  i2c_delay(half_delay);

  return bit;
}

// Write a single byte
static inline int i2c_write_byte(uint8_t data)
{
  for (uint8_t i = 0; i < 8; i++) {
    i2c_write_bit(data & 0x80); // write the most-significant bit
    data <<= 1;
  }

  // Return inverted ACK bit - 'true' for ACK, 'false' for NACK
  return !i2c_read_bit();
}

// Read a single byte
static inline uint8_t i2c_read_byte()
{
  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data <<= 1;
    data |= i2c_read_bit();
  }

  return data;
}

// Perform an I2C bit-banged transfer
static inline int i2c_bitbang_transfer(uint8_t addr, struct i2c_msg *msgs, uint8_t num_msgs)
{
  // Always start with a start condition
  unsigned int flags = I2C_MSG_RESTART;

  // Return early if there are no messages
  if (!num_msgs)
    return 0;

  do {
    // Send stop condition from previous message, if needed
    if (flags & I2C_MSG_STOP) {
      i2c_stop();
    }

    // Forget old flags, except for start
    flags &= I2C_MSG_RESTART;

    // Send start or repeated start condition
    if (flags & I2C_MSG_RESTART) {
      i2c_start();
    } else if (msgs->flags & I2C_MSG_RESTART) {
      i2c_repeated_start();
    }

    // Get flags for new message
    flags |= msgs->flags;

    // Send address after start condition
    if (flags & I2C_MSG_RESTART) {
      // Adjust address to include read/write bit
      uint8_t addr_rw = (addr << 1) | (flags & I2C_MSG_READ);

      // Send address
      int ack = i2c_write_byte(addr_rw);

      // Check for NACK
      if (!ack) {
        i2c_stop();
        return -I2C_ERR;
      }

      flags &= ~I2C_MSG_RESTART;
    }

    // Transfer data
    uint8_t *buf     = msgs->buf;
    uint8_t *buf_end = buf + msgs->len;
    if (flags & I2C_MSG_READ) {
      // Read
      while (buf < buf_end) {
        // Read byte
        *buf++ = i2c_read_byte();

        // ACK the byte, except for the last one
        i2c_write_bit(buf == buf_end);
      }
    } else {
      // Write
      while (buf < buf_end) {
        // Write byte
        int ack = i2c_write_byte(*buf++);

        // Check for NACK
        if (!ack) {
          i2c_stop();
          return -I2C_ERR;
        }
      }
    }

    // Next message
    msgs++;
    num_msgs--;
  } while (num_msgs);

  // Send final stop condition
  i2c_stop();
  return 0;
}

// Determine the drive mode of the SCL line
static bool i2c_is_open_drain()
{
  // Store gpio directions
  uint32_t dir = HW_GPIOB_DIR;

  // Set SCL as input, wait for the line to settle
  HW_GPIOB_DIR &= ~GPIO_AVE_SCL;
  i2c_delay(half_delay);

  // Read the SCL line multiple times, if it is ever low, it is not open-drain
  bool open_drain = true;
  for (int i = 0; i < 100; i++) {
    if (!(HW_GPIOB_IN & GPIO_AVE_SCL)) {
      open_drain = false;
      break;
    }

    i2c_delay(half_delay);
  }

  // Restore original gpio directions
  HW_GPIOB_DIR = dir;

  return open_drain;
}

int i2c_configure(uint8_t mode)
{
  // Set the I2C timings
  switch (mode) {
    case I2C_MODE_STANDARD: // 100 KHz
      delay      = NS_TO_TICKS(5000);
      half_delay = NS_TO_TICKS(2500);
      break;

    case I2C_MODE_FAST: // 400 KHz
      delay      = NS_TO_TICKS(1250);
      half_delay = NS_TO_TICKS(625);
      break;

    default:
      return -I2C_ERR;
  }

  // Send a stop condition to ensure the SCL and SDA lines are high
  // libogc's VIDEO_init() function may have left them low
  i2c_stop();

  // Enable open-drain mode if supported
  if (i2c_is_open_drain())
    drive_mode = I2C_DRIVE_OPEN_DRAIN;

  // Set the configured flag
  configured = true;

  return 0;
}

int i2c_transfer(uint8_t addr, struct i2c_msg *msgs, uint8_t num_msgs)
{
  // Check if the I2C bus is configured
  if (!configured)
    return -I2C_ERR;

  // Disable interrupts
  uint32_t level;
  _CPU_ISR_Disable(level);

  // Perform the transfer
  int result = i2c_bitbang_transfer(addr, msgs, num_msgs);

  // Re-enable interrupts
  _CPU_ISR_Restore(level);

  return result;
}

#endif // defined(HW_RVL)