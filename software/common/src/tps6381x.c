#include "i2c.h"

#include "i2c/tps6381x.h"

// Get the current voltage of the VOUT1 or VOUT2 register, in mV
static int tps6381x_get_vout(uint8_t reg, uint16_t *voltage)
{
  int rcode;

  // Get the current voltage range
  uint8_t range;
  if ((rcode = tps6381x_get_range(&range)) != 0)
    return rcode;

  // Read the VOUT value
  uint8_t vout;
  if ((rcode = i2c_reg_read_byte(TPS6381X_I2C_ADDR, reg, &vout)) != 0)
    return rcode;

  // Convert the VOUT hex value to mV, based on the range
  if (range == TPS6381X_RANGE_LOW) {
    // Low range (1.8V - 4.975V), 25mV steps
    *voltage = vout * TPS6381X_VOUT_RESOLUTION + TPS6381X_VOUT_START_LOW;
  } else {
    // High range (2.025V - 5.2V), 25mV steps
    *voltage = vout * TPS6381X_VOUT_RESOLUTION + TPS6381X_VOUT_START_HIGH;
  }

  return 0;
}

// Set the output voltage of the VOUT1 or VOUT2 register
static int tps6381x_set_vout(uint8_t reg, uint16_t voltage)
{
  int rcode;

  // Get the current voltage range
  uint8_t range;
  if ((rcode = tps6381x_get_range(&range)) != 0)
    return rcode;

  // Convert mV value to a VOUT hex value
  uint8_t vout;
  if (range == TPS6381X_RANGE_LOW) {
    // Low range (1.8V - 4.975V), 25mV steps
    vout = (voltage - TPS6381X_VOUT_START_LOW) / TPS6381X_VOUT_RESOLUTION;
  } else {
    // High range (2.025V - 5.2V), 25mV steps
    vout = (voltage - TPS6381X_VOUT_START_HIGH) / TPS6381X_VOUT_RESOLUTION;
  }

  // Write the value to the register
  return i2c_reg_write_byte(TPS6381X_I2C_ADDR, reg, vout);
}

bool tps6381x_is_present()
{
  int rcode;

  // Fetch the device ID register
  uint8_t device_id;
  if ((rcode = i2c_reg_read_byte(TPS6381X_I2C_ADDR, TPS6381X_REG_DEVID, &device_id)) != 0)
    return false;

  // Check device ID is expected value
  if (device_id != TPS6381X_DEVID)
    return false;

  return true;
}

int tps6381x_set_slew_rate(uint8_t slew_rate)
{
  return i2c_reg_update_byte(TPS6381X_I2C_ADDR, TPS6381X_REG_CONTROL, TPS6381X_SLEW, slew_rate);
}

int tps6381x_enable(bool enable)
{
  return i2c_reg_update_byte(TPS6381X_I2C_ADDR, TPS6381X_REG_CONTROL, TPS6381X_ENABLE,
                             enable ? TPS6381X_ENABLE : 0);
}

int tps6381x_get_range(uint8_t *range)
{
  int rcode;

  // Read the CONTROL register
  uint8_t control;
  if ((rcode = i2c_reg_read_byte(TPS6381X_I2C_ADDR, TPS6381X_REG_CONTROL, &control)) != 0)
    return rcode;

  // Mask out the RANGE field
  *range = control & TPS6381X_RANGE;

  return 0;
}

int tps6381x_set_range(uint8_t range)
{
  return i2c_reg_update_byte(TPS6381X_I2C_ADDR, TPS6381X_REG_CONTROL, TPS6381X_RANGE, range);
}

int tps6381x_get_vout1(uint16_t *voltage)
{
  return tps6381x_get_vout(TPS6381X_REG_VOUT1, voltage);
}

int tps6381x_get_vout2(uint16_t *voltage)
{
  return tps6381x_get_vout(TPS6381X_REG_VOUT2, voltage);
}

int tps6381x_set_vout1(uint16_t voltage)
{
  return tps6381x_set_vout(TPS6381X_REG_VOUT1, voltage);
}

int tps6381x_set_vout2(uint16_t voltage)
{
  return tps6381x_set_vout(TPS6381X_REG_VOUT2, voltage);
}