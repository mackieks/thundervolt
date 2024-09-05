#include <stdint.h>
#include <string.h>

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#include "gpio.h"
#include "i2c.h"
#include "i2c/thundervolt.h"
#include "i2c_target.h"
#include "led.h"

// Device power states
enum device_state { STATE_STANDBY, STATE_POWERED };

// Software revision - increment this for each firmware release
static const uint8_t SOFTWARE_REV = 1;

// GPIO pin definitions
static const gpio_t EN       = {&PORTA, 1};
static const gpio_t SAFEMODE = {&PORTA, 3};
static const gpio_t ALERT    = {&PORTA, 4};
static const gpio_t DIRECT   = {&PORTA, 7};
static const gpio_t U10      = {&PORTB, 3};
static const gpio_t LED      = {&PORTC, 0};

// EEPROM signature, used to detect if the EEPROM has been initialized
static const uint16_t *EEPROM_SIGNATURE_ADDR = 0x00FE;
static const uint16_t EEPROM_SIGNATURE       = 0xCAFE;

// LED effect timings (in milliseconds)
static const uint16_t LED_BREATHE_PERIOD = 2000;
static const uint16_t LED_SOS_PATTERN[]  = {
    200, 200, 200, 200, 200, 600, // S
    600, 200, 600, 200, 600, 600, // O
    200, 200, 200, 200, 200, 1400 // S
};

// RTC millisecond counter
static volatile uint32_t millis = 0;

// Device state
static volatile enum device_state device_state = STATE_STANDBY;

// Register memory space
// For convenience we're also using the same addresses for values persisted in EEPROM
static volatile uint8_t registers[THUNDERVOLT_NUM_REGISTERS];

// Set EEPROM values to their defaults
static void reset_eeprom()
{
  // Write the default CONFIG register value
  eeprom_write_byte(THUNDERVOLT_REG_CONFIG, THUNDERVOLT_LED);

  // Write the default "stock" voltage values
  eeprom_write_word(THUNDERVOLT_REG_VPERS_1V0_L, THUNDERVOLT_STOCK_VOLTAGE_1V0);
  eeprom_write_word(THUNDERVOLT_REG_VPERS_1V15_L, THUNDERVOLT_STOCK_VOLTAGE_1V15);
  eeprom_write_word(THUNDERVOLT_REG_VPERS_1V8_L, THUNDERVOLT_STOCK_VOLTAGE_1V8);
  eeprom_write_word(THUNDERVOLT_REG_VPERS_3V3_L, THUNDERVOLT_STOCK_VOLTAGE_3V3);

  // Write the default over-temperature shutdown temperature
  eeprom_write_byte(THUNDERVOLT_REG_OTSD_TEMP, THUNDERVOLT_DEFAULT_OTSD_LIMIT);
}

// Initialize the EEPROM if it has never been initialized
static void initialize_eeprom()
{
  // Return early if the EEPROM has already been initialized
  if (eeprom_read_word(EEPROM_SIGNATURE_ADDR) == EEPROM_SIGNATURE)
    return;

  // Write the default "stock" voltage values
  reset_eeprom();

  // Write the signature
  eeprom_write_word(EEPROM_SIGNATURE_ADDR, EEPROM_SIGNATURE);
}

// Check if the specified register is read-only
static inline bool is_read_only_register(uint8_t reg_addr)
{
  return reg_addr == THUNDERVOLT_REG_STATUS || reg_addr == THUNDERVOLT_REG_HWREV || reg_addr == THUNDERVOLT_REG_SWREV;
}

// Get the value of a 16-bit register
static inline uint16_t get_word_register(uint8_t addr)
{
  return registers[addr] | (registers[addr + 1] << 8);
}

// Populate the registers with persistent values from the EEPROM
static void load_persisted_registers()
{
  for (uint8_t i = 0; i < THUNDERVOLT_NUM_REGISTERS; i++) {
    if (is_read_only_register(i))
      continue;

    registers[i] = eeprom_read_byte((uint8_t *)i);
  }
}

// Initialize the registers to their "reset" state
static void reset_registers()
{
  // Initialize the register space to 0
  memset(registers, 0, THUNDERVOLT_NUM_REGISTERS);

  // Initialize the fixed registers
  registers[THUNDERVOLT_REG_HWREV] = THUNDERVOLT_HWREV;
  registers[THUNDERVOLT_REG_SWREV] = SOFTWARE_REV;

  // Populate the registers with persistent values from EEPROM
  load_persisted_registers();
}

// Handle register reads from an I2C controller when in I2C target mode
static int handle_register_read(uint8_t reg_addr, uint8_t *value)
{
  // Return 0x00 for out-of-bounds register reads
  if (reg_addr >= THUNDERVOLT_NUM_REGISTERS) {
    *value = 0x00;
    return -1;
  }

  // Read the register value
  *value = registers[reg_addr];

  return 0;
}

// Handle register writes from an I2C controller when in I2C target mode
static int handle_register_write(uint8_t reg_addr, uint8_t value)
{
  // Ignore writes to read-only registers and out-of-bounds registers
  if (is_read_only_register(reg_addr) || reg_addr >= THUNDERVOLT_NUM_REGISTERS)
    return -1;

  // Handle CONFIG register writes
  if (reg_addr == THUNDERVOLT_REG_CONFIG) {
    // Handle the CLEAR bit
    if (value & THUNDERVOLT_CLEAR) {
      // Clear persisted registers
      reset_eeprom();
      load_persisted_registers();

      // We just cleared CONFIG, so load the updated value
      value = registers[THUNDERVOLT_REG_CONFIG];
    }

    // Handle the LED bit
    if (value & THUNDERVOLT_LED) {
      led_effect_breathe(LED_BREATHE_PERIOD);
    } else {
      led_off();
    }
  }

  // Update the register
  registers[reg_addr] = value;

  // Persist the value to the EEPROM
  eeprom_write_byte((uint8_t *)reg_addr, value);

  return 0;
}

// Initialize the GPIO pins
static void gpio_init()
{
  // Safe mode jumper, active low (when bridged)
  gpio_input(SAFEMODE);
  gpio_config(SAFEMODE, PORT_PULLUPEN_bm);

  // Temperature sensor alert pin, active low
  gpio_input(ALERT);
  gpio_config(ALERT, PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc);

  // Regulator enable pin, active high
  gpio_output(EN);
  gpio_set_low(EN);

  // Pinstrapping to detect U10 Direct Mode vs FET Mode
  gpio_input(DIRECT);
  gpio_config(DIRECT, PORT_PULLUPEN_bm);

  // Check pinstrapping to see if board has U10 FET
  bool u10_direct_mode = gpio_read(DIRECT);

  // U10 emulation pin
  gpio_output(U10);

  // Assert U10 (make it low)
  if (u10_direct_mode) {
    gpio_set_low(U10); // faux open-drain (direct mode)
  } else {
    gpio_set_high(U10); // U10 NFET gate is active-high
  }

  // LED, default off
  gpio_output(LED);
  gpio_set_low(LED);
}

// Initialize the RTC for periodic interrupts
void rtc_init()
{
  RTC.CLKSEL     = RTC_CLKSEL_INT32K_gc;
  RTC.PITINTCTRL = RTC_PI_bm;
  RTC.PITCTRLA   = RTC_PERIOD_CYC32_gc | RTC_PITEN_bm;
}

// Handle periodic RTC interrupts (every ~1ms)
ISR(RTC_PIT_vect)
{
  // Update the "milliseconds since boot" counter
  millis++;

  // Update the LED effect
  led_effect_update(millis);

  // Clear the interrupt flag
  RTC.PITINTFLAGS = RTC_PI_bm;
}

// Handle gpio interrupts on PORTA
ISR(PORTA_PORT_vect)
{
  // Handle the temperature sensor alert
  if (gpio_read_intflag(ALERT)) {
    // Shutdown the regulators if over-temp shutdown is enabled
    if (device_state == STATE_POWERED && registers[THUNDERVOLT_REG_CONFIG] & THUNDERVOLT_OTSD) {
      // Disable the regulators
      gpio_set_low(EN);

      // Enable the SOS LED effect
      led_effect_blink_pattern(LED_SOS_PATTERN, sizeof(LED_SOS_PATTERN) / sizeof(LED_SOS_PATTERN[0]));
    }
  }

  // Clear the interrupt flags
  PORTA.INTFLAGS = 0xFF;
}

int main(void)
{
  // Enable prescaler, and set prescaler division to 4 to run at 5MHz
  CPU_CCP           = CCP_IOREG_gc;
  CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm;

  // Set I2C target interrupts as the highest priority
  CPUINT.LVL1VEC = TWI0_TWIS_vect_num;

  // Enable interrupts (for I2C target comms and overtemp alerting)
  sei();

  // Initalize the GPIOs
  gpio_init();

  // Initialize the RTC
  rtc_init();

  // Initialize the LED
  led_init();

  // Initialize as an I2C controller
  i2c_configure(I2C_MODE_STANDARD);

  // Initialize the EEPROM with stock values if it has never been initialized
  initialize_eeprom();

  // Initialize the registers to their "reset" state
  reset_registers();

  // Check if we are in safe mode
  bool in_safe_mode = !gpio_read(SAFEMODE);
  if (in_safe_mode) {
    registers[THUNDERVOLT_REG_STATUS] |= THUNDERVOLT_SAFEMODE;
  }

  // Registers cannot be read/written via I2C on the TPS62868x until startup is finished,
  // so we must enable the regulators and wait a short time (at least ~1100us) first
  gpio_set_high(EN);
  _delay_ms(10);

  // Determine the startup voltages
  uint16_t voltages[4];
  if (in_safe_mode) {
    // Use stock voltage values
    voltages[THUNDERVOLT_RAIL_1V0]  = THUNDERVOLT_STOCK_VOLTAGE_1V0;
    voltages[THUNDERVOLT_RAIL_1V15] = THUNDERVOLT_STOCK_VOLTAGE_1V15;
    voltages[THUNDERVOLT_RAIL_1V8]  = THUNDERVOLT_STOCK_VOLTAGE_1V8;
    voltages[THUNDERVOLT_RAIL_3V3]  = THUNDERVOLT_STOCK_VOLTAGE_3V3;
  } else {
    // Use persisted voltage values
    voltages[THUNDERVOLT_RAIL_1V0]  = get_word_register(THUNDERVOLT_REG_VPERS_1V0_L);
    voltages[THUNDERVOLT_RAIL_1V15] = get_word_register(THUNDERVOLT_REG_VPERS_1V15_L);
    voltages[THUNDERVOLT_RAIL_1V8]  = get_word_register(THUNDERVOLT_REG_VPERS_1V8_L);
    voltages[THUNDERVOLT_RAIL_3V3]  = get_word_register(THUNDERVOLT_REG_VPERS_3V3_L);
  }

  // Set the voltage on each regulator
  for (uint8_t i = THUNDERVOLT_RAIL_1V0; i <= THUNDERVOLT_RAIL_3V3; i++) { thundervolt_set_voltage(i, voltages[i]); }

  // Set the over-temperature limit based on the persisted value
  thundervolt_set_otsd_limit(registers[THUNDERVOLT_REG_OTSD_TEMP]);

  // Wait 200ms to emulate the U10 delay
  _delay_ms(200);

  // Check pinstrapping to see if board has U10 FET
  bool u10_direct_mode = gpio_read(DIRECT);

  // Deassert U10 (make it high-impedance) so Hollywood can boot
  if (u10_direct_mode) {
    gpio_input(U10); // faux open-drain (direct mode)
  } else {
    gpio_set_low(U10); // Turn off U10 NFET
  }

  // Update the device state
  device_state = STATE_POWERED;

  // Enable the breathing LED effect if the LED is enabled
  if (registers[THUNDERVOLT_REG_CONFIG] & THUNDERVOLT_LED) {
    led_effect_breathe(LED_BREATHE_PERIOD);
  }

  // Initialize as an I2C target device, and listen for commands
  i2c_target_init(THUNDERVOLT_I2C_ADDR, handle_register_read, handle_register_write);

  // Main loop
  while (1);
}