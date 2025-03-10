#include "i2c.h"
#include "i2c/ina700.h"
#include "i2c/tmp1075.h"
#include "i2c/tps6286x.h"
#include "i2c/tps6381x.h"

#include "i2c/thundervolt.h"

// Child I2C device addresses
#define THUNDERVOLT_ADDR_HW1_REG_1V0    0x43
#define THUNDERVOLT_ADDR_HW1_REG_1V15   0x46
#define THUNDERVOLT_ADDR_HW2_REG_1V0    0x42
#define THUNDERVOLT_ADDR_HW2_REG_1V15   0x43
#define THUNDERVOLT_ADDR_REG_1V8        0x41
#define THUNDERVOLT_ADDR_REG_3V3        0x75
#define THUNDERVOLT_ADDR_TMP            0x49
#define THUNDERVOLT_ADDR_INA_1V0        0x44
#define THUNDERVOLT_ADDR_INA_1V15       0x45
#define THUNDERVOLT_ADDR_INA_1V8        0x46
#define THUNDERVOLT_ADDR_INA_3V3        0x47

// Get the I2C address of the specified regulator rail
static int get_regulator_i2c_addr(uint8_t rail)
{
  // Get the hardware revision
  uint8_t hw_rev;
  int rcode = thundervolt_get_hardware_revision(&hw_rev);
  if (rcode != 0)
    return rcode;

  // Determine the I2C address
  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
      switch (hw_rev) {
        case THUNDERVOLT_HW1:
        case THUNDERVOLT_LITE:
          return THUNDERVOLT_ADDR_HW1_REG_1V0;
        case THUNDERVOLT_HW2:
          return THUNDERVOLT_ADDR_HW2_REG_1V0;
        default:
          return -1;
      }
    case THUNDERVOLT_RAIL_1V15:
      switch (hw_rev) {
        case THUNDERVOLT_HW1:
        case THUNDERVOLT_LITE:
          return THUNDERVOLT_ADDR_HW1_REG_1V15;
        case THUNDERVOLT_HW2:
          return THUNDERVOLT_ADDR_HW2_REG_1V15;
        default:
          return -1;
      }
    case THUNDERVOLT_RAIL_1V8:
      return THUNDERVOLT_ADDR_REG_1V8;
    case THUNDERVOLT_RAIL_3V3:
      return THUNDERVOLT_ADDR_REG_3V3;
    default:
      return -THUNDERVOLT_ERR_INVALID_RAIL;
  }
}

// Get the I2C address of the specified power monitor rail
static int get_power_monitor_i2c_addr(uint8_t rail)
{
  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
      return THUNDERVOLT_ADDR_INA_1V0;
    case THUNDERVOLT_RAIL_1V15:
      return THUNDERVOLT_ADDR_INA_1V15;
    case THUNDERVOLT_RAIL_1V8:
      return THUNDERVOLT_ADDR_INA_1V8;
    case THUNDERVOLT_RAIL_3V3:
      return THUNDERVOLT_ADDR_INA_3V3;
    default:
      return -THUNDERVOLT_ERR_INVALID_RAIL;
  }
}

// Fetch the vpers register address for the specified rail
static int get_vpers_reg_addr(uint8_t rail)
{
  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
      return THUNDERVOLT_REG_VPERS_1V0_L;
    case THUNDERVOLT_RAIL_1V15:
      return THUNDERVOLT_REG_VPERS_1V15_L;
    case THUNDERVOLT_RAIL_1V8:
      return THUNDERVOLT_REG_VPERS_1V8_L;
    case THUNDERVOLT_RAIL_3V3:
      return THUNDERVOLT_REG_VPERS_3V3_L;
    default:
      return -THUNDERVOLT_ERR_INVALID_RAIL;
  }
}

// Check if the specified voltage is valid for the given rail
static bool is_valid_voltage(uint8_t rail, uint16_t voltage)
{
  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
      return voltage >= THUNDERVOLT_MIN_VOLTAGE_1V0 && voltage <= THUNDERVOLT_STOCK_VOLTAGE_1V0;
    case THUNDERVOLT_RAIL_1V15:
      return voltage >= THUNDERVOLT_MIN_VOLTAGE_1V15 && voltage <= THUNDERVOLT_STOCK_VOLTAGE_1V15;
    case THUNDERVOLT_RAIL_1V8:
      return voltage >= THUNDERVOLT_MIN_VOLTAGE_1V8 && voltage <= THUNDERVOLT_STOCK_VOLTAGE_1V8;
    case THUNDERVOLT_RAIL_3V3:
      return voltage >= THUNDERVOLT_MIN_VOLTAGE_3V3 && voltage <= THUNDERVOLT_STOCK_VOLTAGE_3V3;
    default:
      return false;
  }
}

int thundervolt_get_hardware_revision(uint8_t *hw_rev)
{
#ifdef HW_RVL
  // Cache hardware revision lookups, they don't change at runtime
  static uint8_t cache = 0;
  if (cache == 0) {
    // Read the hardware revision from the device
    int rcode = i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_HWREV, &cache);
    if (rcode < 0)
      return rcode;
  }

  *hw_rev = cache;
#else
  *hw_rev = THUNDERVOLT_HWREV;
#endif

  return 0;
}

bool thundervolt_i2c_scan()
{
  return tps6286x_is_present(get_regulator_i2c_addr(THUNDERVOLT_RAIL_1V0)) &&
         tps6286x_is_present(get_regulator_i2c_addr(THUNDERVOLT_RAIL_1V15)) &&
         tps6286x_is_present(get_regulator_i2c_addr(THUNDERVOLT_RAIL_1V8)) && tps6381x_is_present() &&
         tmp1075_is_present(THUNDERVOLT_ADDR_TMP);
}

int thundervolt_get_voltage(uint8_t rail, uint16_t *voltage)
{
  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
    case THUNDERVOLT_RAIL_1V15: {
      // Determine I2C address based on HW revision
      int addr = get_regulator_i2c_addr(rail);
      if (addr < 0)
        return addr;

      return tps6286x_get_vout1(addr, TPS6286X1A, voltage);
    }
    case THUNDERVOLT_RAIL_1V8:
      return tps6286x_get_vout1(THUNDERVOLT_ADDR_REG_1V8, TPS6286X2A, voltage);
    case THUNDERVOLT_RAIL_3V3:
      return tps6381x_get_vout1(voltage);
    default:
      return -THUNDERVOLT_ERR_INVALID_RAIL;
  }
}

int thundervolt_set_voltage(uint8_t rail, uint16_t voltage)
{
  if (!is_valid_voltage(rail, voltage))
    return -THUNDERVOLT_ERR_INVALID_VOLTAGE;

  switch (rail) {
    case THUNDERVOLT_RAIL_1V0:
    case THUNDERVOLT_RAIL_1V15: {
      // Determine I2C address based on HW revision
      int addr = get_regulator_i2c_addr(rail);
      if (addr < 0)
        return addr;

      return tps6286x_set_vout1(addr, TPS6286X1A, voltage);
    }
    case THUNDERVOLT_RAIL_1V8:
      return tps6286x_set_vout1(THUNDERVOLT_ADDR_REG_1V8, TPS6286X2A, voltage);
    case THUNDERVOLT_RAIL_3V3:
      return tps6381x_set_vout1(voltage);
    default:
      return -THUNDERVOLT_ERR_INVALID_RAIL;
  }
}

int thundervolt_get_current(uint8_t rail, uint16_t *current)
{
  // Check if power monitoring is supported
  if (!thundervolt_has_power_monitoring())
    return -THUNDERVOLT_ERR_NOT_SUPPORTED;

  // Determine I2C address based on rail
  int addr = get_power_monitor_i2c_addr(rail);
  if (addr < 0)
    return addr;

  // Read the current from the INA700
  return ina700_get_current(addr, current);
}

int thundervolt_get_power(uint8_t rail, uint32_t *power)
{
  // Check if power monitoring is supported
  if (!thundervolt_has_power_monitoring())
    return -1;

  // Determine I2C address based on rail
  int addr = get_power_monitor_i2c_addr(rail);
  if (addr < 0)
    return addr;

  // Read the power from the INA700
  return ina700_get_power(addr, power);
}

int thundervolt_get_temp(float *temp)
{
  // Read the temperature from the TMP1075
  return tmp1075_get_temp(THUNDERVOLT_ADDR_TMP, temp);
}

int thundervolt_get_otsd_limit(int8_t *temp)
{
  // Read the high limit from the TMP1075
  float reg_val;
  int rcode = tmp1075_get_high_limit(THUNDERVOLT_ADDR_TMP, &reg_val);
  if (rcode != 0)
    return rcode;

  // Convert to integer degrees C
  *temp = (int8_t)reg_val;

  return 0;
}

int thundervolt_set_otsd_limit(int8_t temp)
{
  int rcode = tmp1075_set_high_limit(THUNDERVOLT_ADDR_TMP, temp);
  if (rcode != 0)
    return rcode;

  return tmp1075_set_low_limit(THUNDERVOLT_ADDR_TMP, temp - 5.0f);
}

bool thundervolt_has_power_monitoring()
{
  // Get the hardware revision
  uint8_t hw_rev;
  int rcode = thundervolt_get_hardware_revision(&hw_rev);
  if (rcode != 0)
    return false;

  // Only HW2 supports power monitoring
  return hw_rev == THUNDERVOLT_HW2;
}

#ifdef HW_RVL
bool thundervolt_is_present()
{
  return i2c_detect(THUNDERVOLT_I2C_ADDR);
}

int thundervolt_get_safemode_enabled(bool *safemode)
{
  int rcode;

  // Read the status register
  uint8_t status;
  if ((rcode = i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_STATUS, &status)) != 0)
    return rcode;

  // Extract the safe mode bit
  *safemode = status & THUNDERVOLT_SAFEMODE;

  return 0;
}

int thundervolt_get_persisted_voltage(uint8_t rail, uint16_t *voltage)
{
  // Fetch the VPERS register address for the specified rail
  int reg = get_vpers_reg_addr(rail);
  if (reg < 0)
    return reg;

  // Read the persisted voltage from the VPERS register
  return i2c_reg_read_word(THUNDERVOLT_I2C_ADDR, reg, voltage);
}

int thundervolt_set_persisted_voltage(uint8_t rail, uint16_t voltage)
{
  // Range check the voltage
  if (!is_valid_voltage(rail, voltage))
    return -THUNDERVOLT_ERR_INVALID_VOLTAGE;

  // Fetch the VPERS register address for the specified rail
  int reg = get_vpers_reg_addr(rail);
  if (reg < 0)
    return reg;

  // Write the voltage to the VPERS register
  return i2c_reg_write_word(THUNDERVOLT_I2C_ADDR, reg, voltage);
}

int thundervolt_clear_persisted_values()
{
  return i2c_reg_update_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_CONFIG, THUNDERVOLT_CLEAR, THUNDERVOLT_CLEAR);
}

int thundervolt_get_otsd_enabled(bool *enable)
{
  uint8_t config;
  int rcode = i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_CONFIG, &config);
  if (rcode < 0)
    return rcode;

  *enable = config & THUNDERVOLT_OTSD;

  return 0;
}

int thundervolt_set_otsd_enabled(bool enable)
{
  return i2c_reg_update_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_CONFIG, THUNDERVOLT_OTSD,
                             enable ? THUNDERVOLT_OTSD : 0);
}

int thundervolt_get_persisted_otsd_limit(int8_t *temp)
{
  int rcode;

  // Read the over-temperature limit from the device
  uint8_t reg_val;
  rcode = i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_OTSD_TEMP, &reg_val);
  if (rcode < 0)
    return rcode;

  *temp = (int8_t)reg_val;

  return 0;
}

int thundervolt_set_persisted_otsd_limit(int8_t temp)
{
  return i2c_reg_write_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_OTSD_TEMP, (uint8_t)temp);
}

int thundervolt_get_software_revision(uint8_t *sw_rev)
{
  return i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_SWREV, sw_rev);
}

int thundervolt_get_led_enabled(bool *enable)
{
  uint8_t config;
  int rcode = i2c_reg_read_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_CONFIG, &config);
  if (rcode < 0)
    return rcode;

  *enable = config & THUNDERVOLT_LED;

  return 0;
}

int thundervolt_set_led_enabled(bool enable)
{
  return i2c_reg_update_byte(THUNDERVOLT_I2C_ADDR, THUNDERVOLT_REG_CONFIG, THUNDERVOLT_LED,
                             enable ? THUNDERVOLT_LED : 0);
}
#endif // HW_RVL
