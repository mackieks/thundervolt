/*
 * I2C driver for INA700 digital power monitor
 * https://www.ti.com/lit/ds/symlink/ina700.pdf
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// I2C device details
#define INA700_I2C_ADDR_START       0x44
#define INA700_I2C_ADDR_END         0x47

// Register map
#define INA700_REG_CONFIG           0x00
#define INA700_REG_ADC_CONFIG       0x01
#define INA700_REG_VBUS             0x05
#define INA700_REG_DIETEMP          0x06
#define INA700_REG_CURRENT          0x07
#define INA700_REG_POWER            0x08
#define INA700_REG_ENERGY           0x09
#define INA700_REG_CHARGE           0x0A
#define INA700_REG_ALERT_DIAG       0x0B
#define INA700_REG_COL              0x0C
#define INA700_REG_CUL              0x0D
#define INA700_REG_BOVL             0x0E
#define INA700_REG_BUVL             0x0F
#define INA700_REG_TEMP_LIMIT       0x10
#define INA700_REG_PWR_LIMIT        0x11
#define INA700_REG_MANUFACTURER_ID  0x3E

#define INA700_MANFID               0x5449

// Check if an INA700 is present on the I2C bus at the given address
bool ina700_is_present(uint8_t addr);

// Get the measured bus voltage, in mV
int ina700_get_bus_voltage(uint8_t addr, uint16_t *voltage);

// Get the die temperature, in mC
int ina700_get_temp(uint8_t addr, uint16_t *temp);

// Get the measured current, in mA
int ina700_get_current(uint8_t addr, uint16_t *current);

// Get the measured power, in uW
int ina700_get_power(uint8_t addr, uint32_t *power);
