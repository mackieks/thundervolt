#pragma once

#include <stdbool.h>
#include <stdint.h>

// I2C target mode address
#define THUNDERVOLT_I2C_ADDR            0x48

// I2C target mode registers
#define THUNDERVOLT_REG_CONFIG          0x00 // Configuration register (RW)
#define THUNDERVOLT_REG_STATUS          0x01 // Status register (R)
#define THUNDERVOLT_REG_VPERS_1V0_L     0x02 // 1.0V rail persisted voltage [7:0] (RW)
#define THUNDERVOLT_REG_VPERS_1V0_H     0x03 // 1.0V rail persisted voltage [15:8] (RW)
#define THUNDERVOLT_REG_VPERS_1V15_L    0x04 // 1.15V rail persisted voltage [7:0] (RW)
#define THUNDERVOLT_REG_VPERS_1V15_H    0x05 // 1.15V rail persisted voltage [15:8] (RW)
#define THUNDERVOLT_REG_VPERS_1V8_L     0x06 // 1.8V rail persisted voltage [7:0] (RW)
#define THUNDERVOLT_REG_VPERS_1V8_H     0x07 // 1.8V rail persisted voltage [15:8] (RW)
#define THUNDERVOLT_REG_VPERS_3V3_L     0x08 // 3.3V rail persisted voltage [7:0] (RW)
#define THUNDERVOLT_REG_VPERS_3V3_H     0x09 // 3.3V rail persisted voltage [15:8] (RW)
#define THUNDERVOLT_REG_OTSD_TEMP       0x0A // Over-temperature shutdown temperature (RW)
#define THUNDERVOLT_REG_HWREV           0x0B // Hardware revision (R)
#define THUNDERVOLT_REG_SWREV           0x0C // Software revision (R)
#define THUNDERVOLT_NUM_REGISTERS       0x0D // Number of registers

// CONFIG register
#define THUNDERVOLT_LED                 (1 << 2) // Bit 2: Enable the onboard LED
#define THUNDERVOLT_OTSD                (1 << 1) // Bit 1: Enable over-temperature shutdown
#define THUNDERVOLT_CLEAR               (1 << 0) // Bit 0: Clear persisted values

// STATUS register
#define THUNDERVOLT_SAFEMODE            (1 << 0) // Bit 0: Safe mode is active

// Stock voltages for each rail, in mV
#define THUNDERVOLT_STOCK_VOLTAGE_1V0   1000
#define THUNDERVOLT_STOCK_VOLTAGE_1V15  1150
#define THUNDERVOLT_STOCK_VOLTAGE_1V8   1800
#define THUNDERVOLT_STOCK_VOLTAGE_3V3   3300

// Minimum voltages for each rail, in mV
#define THUNDERVOLT_MIN_VOLTAGE_1V0     750
#define THUNDERVOLT_MIN_VOLTAGE_1V15    850
#define THUNDERVOLT_MIN_VOLTAGE_1V8     1300
#define THUNDERVOLT_MIN_VOLTAGE_3V3     2950

// Default over-temperature limit, in degrees C
#define THUNDERVOLT_DEFAULT_OTSD_LIMIT  70

// Hardware variants
enum {
  THUNDERVOLT_HW1 = 1,
  THUNDERVOLT_HW2,
  THUNDERVOLT_LITE,
};

// Regulator rails
enum {
  THUNDERVOLT_RAIL_1V0,
  THUNDERVOLT_RAIL_1V15,
  THUNDERVOLT_RAIL_1V8,
  THUNDERVOLT_RAIL_3V3,
};

// Error codes
enum {
  THUNDERVOLT_ERR_INVALID_VOLTAGE = 10,
  THUNDERVOLT_ERR_INVALID_RAIL,
  THUNDERVOLT_ERR_NOT_SUPPORTED,
};

// Get the hardware revision of Thundervolt
int thundervolt_get_hardware_revision(uint8_t *hw_rev);

// Check if all regulators are present
bool thundervolt_regs_present();

// Get the current voltage for the specified rail, in mV
int thundervolt_get_voltage(uint8_t rail, uint16_t *voltage);

// Set the voltage for the specified rail, in mV
int thundervolt_set_voltage(uint8_t rail, uint16_t voltage);

// Get the current for the specified rail, in mA
int thundervolt_get_current(uint8_t rail, uint16_t *current);

// Get the power for the specified rail, in uW
int thundervolt_get_power(uint8_t rail, uint32_t *power);

// Get the temperture of the device, in degrees C
int thundervolt_get_temp(float *temp);

// Get the over-temperature limit, in degrees C
int thundervolt_get_otsd_limit(int8_t *temp);

// Set the over-temperature limit, in degrees C
int thundervolt_set_otsd_limit(int8_t temp);

// Check if this hardware variant supports power monitoring
bool thundervolt_has_power_monitoring();

//
// Functions only available in homebrew mode
//

#ifdef HW_RVL
// Check if Thundervolt is present on the I2C bus
bool thundervolt_is_present();

// Check if safe mode is enabled
int thundervolt_get_safemode_enabled(bool *safemode);

// Get the persisted voltage for the specified rail, in mV
int thundervolt_get_persisted_voltage(uint8_t rail, uint16_t *voltage);

// Set the persisted voltage for the specified rail, in mV
int thundervolt_set_persisted_voltage(uint8_t rail, uint16_t voltage);

// Clear the persisted voltages and over-temperature limit
int thundervolt_clear_persisted_values();

// Check if over-temperature shutdown is enabled
int thundervolt_get_otsd_enabled(bool *enable);

// Enable or disable over-temperature shutdown
int thundervolt_set_otsd_enabled(bool enable);

// Get the persisted over-temperature limit, in degrees C
int thundervolt_get_persisted_otsd_limit(int8_t *temp);

// Set the persisted over-temperature limit, in degrees C
int thundervolt_set_persisted_otsd_limit(int8_t temp);

// Get the software revision of Thundervolt
int thundervolt_get_software_revision(uint8_t *sw_rev);

// Check if the LED is enabled
int thundervolt_get_led_enabled(bool *enable);

// Enable or disable the LED
int thundervolt_set_led_enabled(bool enable);
#endif // HW_RVL
