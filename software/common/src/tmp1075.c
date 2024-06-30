#include "i2c.h"

#include "i2c/tmp1075.h"

// Read a 16-bit register value from the TMP1075
static int tmp1075_read_reg(uint8_t addr, uint8_t reg, uint16_t *value)
{
  int rcode;

  // Read the register value
  uint8_t buf[2];
  if ((rcode = i2c_write_read(addr, &reg, 1, buf, 2)) < 0)
    return rcode;

  // Combine the two bytes into a big-endian word
  *value = (buf[0] << 8) | buf[1];

  return 0;
}

// Write a 16-bit register value to the TMP1075
static inline int tmp1075_write_reg(uint8_t addr, uint8_t reg, uint16_t value)
{
  uint8_t buf[] = {reg, value >> 8, value & 0xFF};
  return i2c_write(addr, buf, 3);
}

// Update a 16-bit register value in the TMP1075
static int tmp1075_update_reg(uint8_t addr, uint8_t reg, uint16_t mask, uint16_t value)
{
  int rcode;

  // Read the current register value
  uint16_t reg_val;
  if ((rcode = tmp1075_read_reg(addr, reg, &reg_val)) != 0)
    return rcode;

  // Update the register value
  reg_val = (reg_val & ~mask) | (value & mask);

  // Write the updated register value
  return tmp1075_write_reg(addr, reg, reg_val);
}

// Read a temperature value from the TMP1075
static int tmp1075_read_temp(uint8_t addr, uint8_t reg, float *temp)
{
  int rcode;

  // Read the temperature register
  uint16_t reg_value;
  if ((rcode = tmp1075_read_reg(addr, reg, &reg_value)) != 0)
    return rcode;

  // Convert the register value to a temperature
  int16_t signed_temp = reg_value;
  *temp               = (signed_temp >> 4) * 0.0625f;

  return 0;
}

// Write a temperature value to the TMP1075
static int tmp1075_write_temp(uint8_t addr, uint8_t reg, float temp)
{
  // Convert the temperature to a register value and write it
  uint16_t reg_value = temp / 0.0625f;
  return tmp1075_write_reg(addr, reg, reg_value << 4);
}

bool tmp1075_is_present(uint8_t addr)
{
  // NOTE: Device ID only available in the TMP1075 not the TMP1075N

  return i2c_detect(addr);
}

int tmp1075_get_temp(uint8_t addr, float *temp)
{
  return tmp1075_read_temp(addr, TMP1075_REG_TEMP, temp);
}

int tmp1075_start_conversion(uint8_t addr)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_OS, TMP1075_OS);
}

int tmp1075_set_conversion_rate(uint8_t addr, uint16_t rate)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_R, rate);
}

int tmp1075_set_fault_count(uint8_t addr, uint16_t count)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_F, count);
}

int tmp1075_set_alert_polarity(uint8_t addr, bool polarity)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_POL, polarity ? TMP1075_POL : 0);
}

int tmp1075_set_alert_mode(uint8_t addr, uint16_t mode)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_TM, mode);
}

int tmp1075_set_power_mode(uint8_t addr, uint16_t mode)
{
  return tmp1075_update_reg(addr, TMP1075_REG_CFGR, TMP1075_SD, mode);
}

int tmp1075_get_low_limit(uint8_t addr, float *temp)
{
  return tmp1075_read_temp(addr, TMP1075_REG_LLIM, temp);
}

int tmp1075_set_low_limit(uint8_t addr, float temp)
{
  return tmp1075_write_temp(addr, TMP1075_REG_LLIM, temp);
}

int tmp1075_get_high_limit(uint8_t addr, float *temp)
{
  return tmp1075_read_temp(addr, TMP1075_REG_HLIM, temp);
}

int tmp1075_set_high_limit(uint8_t addr, float temp)
{
  return tmp1075_write_temp(addr, TMP1075_REG_HLIM, temp);
}