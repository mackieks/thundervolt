#include "i2c.h"

#include "i2c/tps6286x.h"

// Get the voltage scale based on the chip type
static int tps6286x_get_scale(uint8_t chip_type, float *scale)
{
  switch (chip_type) {
    case TPS6286X0A:
      *scale = 0.5;
      break;
    case TPS6286X1A:
      *scale = 1;
      break;
    case TPS6286X2A:
      *scale = 2;
      break;
    default:
      return -TPS6286X_ERR_INVALID_SCALE;
  }

  return 0;
}

// Get the current voltage of the VOUT1 or VOUT2 register
static int tps6286x_get_vout(uint8_t addr, uint8_t reg, uint8_t chip_type, uint16_t *voltage)
{
  int rcode;

  // Read the VOUT value
  uint8_t vout_byte;
  if ((rcode = i2c_reg_read_byte(addr, reg, &vout_byte)) != 0)
    return rcode;

  // Get the voltage scale from the chip type
  float scale;
  if ((rcode = tps6286x_get_scale(chip_type, &scale)) != 0)
    return rcode;

  // Convert to mV, scale appropriately
  *voltage = (vout_byte * TPS6286X_VOUT_STEP + TPS6286X_VOUT_BASE) * scale;

  return 0;
}

// Set the output voltage of the VOUT1 or VOUT2 register
static int tps6286x_set_vout(uint8_t addr, uint8_t reg, uint8_t chip_type, uint16_t voltage)
{
  int rcode;

  // Get the voltage scale from the chip type
  float scale;
  if ((rcode = tps6286x_get_scale(chip_type, &scale)) != 0)
    return rcode;

  // Convert mV to hex value
  uint8_t vout_byte = (voltage / scale - TPS6286X_VOUT_BASE) / TPS6286X_VOUT_STEP;

  // Write the value to the register
  return i2c_reg_write_byte(addr, reg, vout_byte);
}

bool tps6286x_is_present(uint8_t addr)
{
  // NOTE: The TPS6286X does not have a device ID register

  return i2c_detect(addr);
}

int tps6286x_enable(uint8_t addr, bool enabled)
{
  return i2c_reg_update_byte(addr, TPS6286X_REG_CONTROL, TPS6286X_ENABLE,
                             enabled ? TPS6286X_ENABLE : 0);
}

int tps6286x_set_slew_rate(uint8_t addr, uint8_t slew_rate)
{
  return i2c_reg_update_byte(addr, TPS6286X_REG_CONTROL, TPS6286X_SLEW, slew_rate);
}

int tps6286x_get_vout1(uint8_t addr, uint8_t device_option, uint16_t *voltage)
{
  return tps6286x_get_vout(addr, TPS6286X_REG_VOUT1, device_option, voltage);
}

int tps6286x_get_vout2(uint8_t addr, uint8_t device_option, uint16_t *voltage)
{
  return tps6286x_get_vout(addr, TPS6286X_REG_VOUT2, device_option, voltage);
}

int tps6286x_set_vout1(uint8_t addr, uint8_t device_option, uint16_t voltage)
{
  return tps6286x_set_vout(addr, TPS6286X_REG_VOUT1, device_option, voltage);
}

int tps6286x_set_vout2(uint8_t addr, uint8_t device_option, uint16_t voltage)
{
  return tps6286x_set_vout(addr, TPS6286X_REG_VOUT2, device_option, voltage);
}