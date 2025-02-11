/*
 * This file is part of the Headless Zombie project.
 *
 * Copyright (C) 2025 Jonathan Peace <jep@retrovms.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "drivers/DRV8912.h"


// DRV8912 Registers
#define DRV8912_STATUS_REG   0x00
#define DRV8912_CONTROL_REG  0x01
#define DRV8912_FAULT_REG    0x02
#define DRV8912_MODE_REG     0x03
#define DRV8912_PWM_REG_BASE 0x10 // First PWM register


// Constructor
DRV8912::DRV8912(uint32_t spi, uint32_t cs_port, uint16_t cs_pin, uint32_t int_port, uint16_t int_pin)
    : _spi(spi), _cs_port(cs_port), _cs_pin(cs_pin), _int_port(int_port), _int_pin(int_pin) {}

// Initialize DRV8912
void DRV8912::init()
{
    // Configure CS pin
    gpio_mode_setup(_cs_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, _cs_pin);
    gpio_set(_cs_port, _cs_pin);

    // Configure INT pin
    gpio_mode_setup(_int_port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, _int_pin);

    // spi_reset(_spi);
    // spi_init_master(_spi, SPI_CR1_BAUDRATE_FPCLK_DIV_16, SPI_CR1_CPOL_0, SPI_CR1_CPHA_0, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
    // spi_enable(_spi);
}

// Configure channel mode (HS, LS, Half-Bridge, Full-Bridge)
// Set Channel Mode (High-Side, Low-Side, Half-Bridge, Full-Bridge)
void DRV8912::setChannelMode(uint8_t channel, OutputMode mode, bool enablePWM) {
  if (channel > 11) return;

  // Configure the channel mode (High-Side, Low-Side, Half-Bridge, Full-Bridge)
  uint16_t modeValue = readRegister(DRV8912_CONFIG_CTRL);

  switch (mode) {
      case OutputMode::HIGH_SIDE:
          modeValue |= (1 << (channel * 2));
          modeValue &= ~(1 << (channel * 2 + 1));
          break;
      case OutputMode::LOW_SIDE:
          modeValue &= ~(1 << (channel * 2));
          modeValue |= (1 << (channel * 2 + 1));
          break;
      case OutputMode::HALF_BRIDGE:
          modeValue |= (1 << (channel * 2));
          modeValue |= (1 << (channel * 2 + 1));
          break;
      case OutputMode::FULL_BRIDGE:
          if (channel % 2 == 0) {
              modeValue |= (1 << (channel * 2));
              modeValue |= (1 << ((channel + 1) * 2 + 1));
          }
          break;
  }

  writeRegister(DRV8912_CONFIG_CTRL, modeValue);

  // Enable or disable PWM mode for the channel
  uint8_t pwmCtrlReg = DRV8912_PWM_CTRL_1 + (channel / 3);
  uint16_t pwmCtrlValue = readRegister(pwmCtrlReg);
  uint8_t shift = (channel % 3) * 2;

  pwmCtrlValue &= ~(0x03 << shift);  // Clear the 2 bits for this channel

  if (enablePWM) {
      pwmCtrlValue |= (0x01 << shift);  // Enable PWM mode (01)
  }

  writeRegister(pwmCtrlReg, pwmCtrlValue);
}

// Enable a specific channel
void DRV8912::enableChannel(uint8_t channel) {
    if (channel > 11) return;
    uint16_t value = readRegister(DRV8912_OP_CTRL_1);
    value |= (1 << channel);
    writeRegister(DRV8912_OP_CTRL_1, value);
}

// Disable a specific channel
void DRV8912::disableChannel(uint8_t channel) {
    if (channel > 11) return;
    uint16_t value = readRegister(DRV8912_OP_CTRL_1);
    value &= ~(1 << channel);
    writeRegister(DRV8912_OP_CTRL_1, value);
}

// Set the frequency for one of the 4 global PWM generators
void DRV8912::setPWMFrequency(uint8_t pwmChannel, uint8_t frequencySetting) {
  if (pwmChannel > 3 || frequencySetting > 3) return;  // Ensure valid PWM channel and frequency setting

  uint16_t pwmFreqValue = readRegister(DRV8912_PWM_FREQ_CTRL);
  uint8_t shift = pwmChannel * 2;

  pwmFreqValue &= ~(0x03 << shift);  // Clear the current frequency setting for the channel
  pwmFreqValue |= (frequencySetting << shift);  // Set the new frequency

  writeRegister(DRV8912_PWM_FREQ_CTRL, pwmFreqValue);
}

// Set the duty cycle for one of the 4 global PWM generators
void DRV8912::setPWMDutyCycle(uint8_t pwmChannel, uint8_t dutyCycle) {
  if (pwmChannel > 3) return;  // Ensure valid PWM channel
  writeRegister(DRV8912_PWM_DUTY_CTRL_1 + pwmChannel, dutyCycle);
}

// Map a specific channel to one of the 4 global PWM generators
void DRV8912::setPWMChannelMapping(uint8_t channel, uint8_t pwmChannel) {
  if (channel > 11 || pwmChannel > 3) return;  // Ensure valid channel and PWM selection

  uint8_t pwmMapReg = DRV8912_PWM_MAP_CTRL_1 + (channel / 4);
  uint16_t pwmMapValue = readRegister(pwmMapReg);
  uint8_t shift = (channel % 4) * 2;

  pwmMapValue &= ~(0x03 << shift);  // Clear the previous mapping for this channel
  pwmMapValue |= (pwmChannel << shift);  // Map the channel to the selected PWM generator

  writeRegister(pwmMapReg, pwmMapValue);
}

// Read Status Register
uint16_t DRV8912::readStatus() {
  return readRegister(DRV8912_IC_STAT);
}

// Clear Faults
void DRV8912::clearFaults() {
  writeRegister(DRV8912_OLD_STAT_1, 0xFFFF);
}

// Write to Register
void DRV8912::writeRegister(uint8_t reg, uint16_t value) {
  gpio_clear(_cs_port, _cs_pin);
  spi_send(_spi, reg);
  spi_send(_spi, (value >> 8) & 0xFF);
  spi_send(_spi, value & 0xFF);
  gpio_set(_cs_port, _cs_pin);
}

// Read from Register
uint16_t DRV8912::readRegister(uint8_t reg) {
  gpio_clear(_cs_port, _cs_pin);
  spi_send(_spi, reg | 0x80);
  uint16_t value = (spi_read(_spi) << 8);
  value |= spi_read(_spi);
  gpio_set(_cs_port, _cs_pin);
  return value;
}

