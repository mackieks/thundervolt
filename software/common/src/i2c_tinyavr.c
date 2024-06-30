/*
 * I2C tinyAVR0/1/2 core.
 */

#if defined(AVR)

#include <avr/io.h>
#include <util/delay.h>

#include "i2c.h"

// Is the I2C bus configured yet?
static bool configured = false;

// Calculate the value for the I2C baud rate register
// NOTE: This is approximate, and doesn't take into account rise time
static inline uint8_t i2c_baud(uint32_t frequency)
{
  int32_t baud = ((F_CPU / frequency) - 10) / 2;

  if (baud < 0)
    return 0;
  if (baud > 255)
    return 255;

  return (uint8_t)baud;
}

// Wait for bus to return to idle state
static inline void i2c_wait_for_idle()
{
  while (!(TWI0.MSTATUS & TWI_BUSSTATE_IDLE_gc));
}

// Send stop condition
static inline void i2c_stop()
{
  TWI0.MCTRLB |= TWI_MCMD_STOP_gc;
  i2c_wait_for_idle();
}

int i2c_configure(uint8_t mode)
{
  // Set the I2C frequency
  switch (mode) {
    case I2C_MODE_STANDARD:
      TWI0.MBAUD = i2c_baud(100000);
      break;
    case I2C_MODE_FAST:
      TWI0.MBAUD = i2c_baud(400000);
      break;
    default:
      return -I2C_ERR;
  }

  // Enable the I2C controller
  TWI0.MCTRLA = TWI_ENABLE_bm;

  // Set the bus state to idle
  TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;

  // Wait a small amount of time for the bus to be ready
  _delay_ms(5);

  // Set the configured flag
  configured = true;

  return 0;
}

int i2c_transfer(uint8_t addr, struct i2c_msg *msgs, uint8_t num_msgs)
{
  // Check if the I2C bus is configured
  if (!configured)
    return -I2C_ERR;

  // Always start with a start condition
  unsigned int flags = I2C_MSG_RESTART;

  // Return early if there are no messages
  if (!num_msgs)
    return 0;

  // Send the messages
  do {
    // Stop flag from previous message?
    if (flags & I2C_MSG_STOP) {
      i2c_stop();
    }

    // Get flags for new message, keep start flag if present
    flags = (flags & I2C_MSG_RESTART) | msgs->flags;

    // Start / repeated start condition
    if (flags & I2C_MSG_RESTART) {
      // Send start condition with address
      TWI0.MADDR = (addr << 1) | (flags & I2C_MSG_READ);

      // Wait for write or read interrupt flag
      while (!(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm)));

      // Check for errors
      if (TWI0.MSTATUS & TWI_ARBLOST_bm) {
        // Arbitration lost
        i2c_wait_for_idle();
        return -I2C_ERR;
      } else if (TWI0.MSTATUS & TWI_RXACK_bm) {
        // Address not acknowledged by client
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
        // Wait for read interrupt flag
        while (!(TWI0.MSTATUS & TWI_RIF_bm));

        // Read byte
        *buf++ = TWI0.MDATA;

        // ACK the byte, except for the last one
        if (buf == buf_end) {
          TWI0.MCTRLB = TWI_ACKACT_NACK_gc;
        } else {
          TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc;
        }
      }
    } else {
      // Write
      TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc;
      while (buf < buf_end) {
        // Send byte
        TWI0.MDATA = *buf++;

        // Wait for write to complete
        while (!(TWI0.MSTATUS & TWI_WIF_bm));

        // Check for errors
        if (TWI0.MSTATUS & (TWI_ARBLOST_bm | TWI_BUSERR_bm))
          return -I2C_ERR;

        // Check for NACK
        if (TWI0.MSTATUS & TWI_RXACK_bm) {
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

#endif // defined(AVR)