#include "i2c.h"

#include "i2c/ina700.h"

// Resolutions and conversion factors
#define INA700_BUS_VOLTAGE_LSB  3125 // 3.125 mV/lsb
#define INA700_DIE_TEMP_LSB     125 // 125 mC/lsb
#define INA700_CURRENT_LSB      480 // 480 μA/lsb
#define INA700_POWER_LSB        96 // 96 μW/lsb

// Read a 16-bit register value from the INA700
static int ina700_reg_read_16(uint8_t addr, uint8_t reg, uint16_t *value)
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

// Read a 24-bit register value from the INA700
static int ina700_reg_read_24(uint8_t addr, uint8_t reg, uint32_t *value)
{
  int rcode;

  // Read the register value
  uint8_t buf[3];
  if ((rcode = i2c_write_read(addr, &reg, 1, buf, 3)) < 0)
    return rcode;

  // Combine the three bytes into a big-endian value
  *value = (buf[0] << 16) | (buf[1] << 8) | buf[2];

  return 0;
}

bool ina700_is_present(uint8_t addr)
{
  int rcode;

  // Read the manufacturer ID
  uint16_t manfid;
  if ((rcode = ina700_reg_read_16(addr, INA700_REG_MANUFACTURER_ID, &manfid)) < 0)
    return false;

  // Check device ID is expected value
  if (manfid != INA700_MANFID)
    return false;

  return true;
}

int ina700_get_bus_voltage(uint8_t addr, uint16_t *voltage)
{
  int rcode;

  // Read the raw register value
  uint16_t regval;
  if ((rcode = ina700_reg_read_16(addr, INA700_REG_VBUS, &regval)) < 0)
    return rcode;

  // Convert the raw register value to mV
  *voltage = (regval * INA700_BUS_VOLTAGE_LSB) / 1000;

  return 0;
}

int ina700_get_temp(uint8_t addr, uint16_t *temp)
{
  int rcode;

  // Read the raw register value
  uint16_t regval;
  if ((rcode = ina700_reg_read_16(addr, INA700_REG_DIETEMP, &regval)) < 0)
    return rcode;

  // Convert the raw register value to mC
  *temp = regval * INA700_DIE_TEMP_LSB;

  return 0;
}

int ina700_get_current(uint8_t addr, uint16_t *current)
{
  int rcode;

  // Read the raw register value
  uint16_t regval;
  if ((rcode = ina700_reg_read_16(addr, INA700_REG_CURRENT, &regval)) < 0)
    return rcode;

  // Convert the raw register value to mA
  *current = (regval * INA700_CURRENT_LSB) / 1000;

  return 0;
}

int ina700_get_power(uint8_t addr, uint32_t *power)
{
  int rcode;

  // Read the raw register value
  uint32_t regval;
  if ((rcode = ina700_reg_read_24(addr, INA700_REG_POWER, &regval)) < 0)
    return rcode;

  // Convert the raw register value to uW
  *power = regval * INA700_POWER_LSB;

  return 0;
}
