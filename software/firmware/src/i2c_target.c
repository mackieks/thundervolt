#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include "i2c_target.h"

// State machine for I2C target mode
static enum i2c_state { IDLE, NEW_TRANSACTION, RECEIVED_ADDRESS, RECEIVED_DATA, SENT_DATA };

// I2C target state variables
static volatile enum i2c_state i2c_state = IDLE;
static volatile uint8_t reg_index        = 0;

// Register access functions
static read_register_fn reg_read_fn   = NULL;
static write_register_fn reg_write_fn = NULL;

static inline void i2c_ack()
{
  TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc;
}

static inline void i2c_nack()
{
  TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc | TWI_ACKACT_NACK_gc;
}

static inline void i2c_complete()
{
  TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc;
}

static void i2c_target_end_transaction()
{
  i2c_state = IDLE;
  i2c_complete();
}

static void i2c_target_handle_address_match()
{
  i2c_state = NEW_TRANSACTION;
  i2c_ack();
}

static void i2c_target_handle_data()
{
  if (TWI0.SSTATUS & TWI_DIR_bm) {
    // Handle reads from a master
    if ((TWI0.SSTATUS & TWI_RXACK_bm) && i2c_state == SENT_DATA) {
      // Client NACK'd the last byte, so end the transaction
      i2c_complete();
    } else {
      // Send the contents of the current register
      reg_read_fn(reg_index++, &TWI0.SDATA);
      i2c_state = SENT_DATA;
      i2c_ack();
    }
  } else {
    // Handle writes from a master
    if (i2c_state == NEW_TRANSACTION) {
      // The first byte is the register address
      reg_index = TWI0.SDATA;
      i2c_state = RECEIVED_ADDRESS;
      i2c_ack();
    } else {
      // Subsequent bytes are data, write them to the current register
      reg_write_fn(reg_index++, TWI0.SDATA);
      i2c_state = RECEIVED_DATA;
      i2c_ack();
    }
  }
}

// I2C target mode interrupt handler
ISR(TWI0_TWIS_vect)
{
  if (TWI0.SSTATUS & (TWI_COLL_bm | TWI_BUSERR_bm)) {
    // Handle collisions and bus errors
    i2c_target_end_transaction();
  } else if (TWI0.SSTATUS & TWI_APIF_bm) {
    // Handle address match and stop condition interrupts
    if (TWI0.SSTATUS & TWI_AP_bm) {
      i2c_target_handle_address_match();
    } else {
      i2c_target_end_transaction();
    }
  } else if (TWI0.SSTATUS & TWI_DIF_bm) {
    // Handle data interrupts
    i2c_target_handle_data();
  }
}

void i2c_target_init(uint8_t addr, read_register_fn read_fn, write_register_fn write_fn)
{
  // Store the register access functions
  reg_read_fn  = read_fn;
  reg_write_fn = write_fn;

  // Set the I2C target address
  TWI0.SADDR = (addr << 1);

  // Enable I2C target mode, smart mode, and stop/address match/data interrupts
  TWI0.SCTRLA = TWI_DIEN_bm | TWI_APIEN_bm | TWI_PIEN_bm | TWI_SMEN_bm | TWI_ENABLE_bm;
}