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

 #include "drivers/TIC12400.h"
#include <cstring>  // For strcpy
#include <libopencm3/stm32/gpio.h>
#include <delay.h>
#include "utils.h"
#include "errormessage_prj.h"

TIC12400::TIC12400(uint32_t spi_bus, uint32_t cs_port, uint16_t cs_pin, uint32_t int_port, uint16_t int_pin)
    : _spi_bus(spi_bus), _cs_port(cs_port), _cs_pin(cs_pin), _int_port(int_port), _int_pin(int_pin), _lastInterruptStatus(0) {
    memset(_channels, 0, sizeof(_channels));  
}


bool TIC12400::init() {

  gpio_mode_setup(_int_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, _int_pin); // INT pin as input
  gpio_mode_setup(_cs_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, _cs_pin);  // CS pin as output
  gpio_set(_cs_port, _cs_pin);  // Ensure CS is high (inactive)

  // Wait for POR to complete (INT pin should go low)
  uint32_t timeout = 1000;  // Timeout counter (adjust as needed)
  while (gpio_get(_int_port, _int_pin) != 0 && --timeout) {
      uDelay(1);  // Delay 1 microsecond
  }
  if (timeout == 0) {
      utils::PostErrorIfRunning(ERR_TIC_INIT_ERROR_TIMEOUT);  
      return false;
  }

  // Check INT_STAT for POR and self-check errors
  uint16_t int_stat = read_register(REG_INT_STAT);

  if (int_stat & INT_STAT_POR_0) {

      if (int_stat & INT_STAT_CHK_FAIL_13) {
          write_register(REG_CONFIG, RESET_0);
          uDelay(10);

          // Wait for POR to complete again
          timeout = 1000;
          while (gpio_get(_int_port, _int_pin) != 0 && --timeout) {
              uDelay(1);
          }
          if (timeout == 0) {
              utils::PostErrorIfRunning(ERR_TIC_INIT_ERROR_TIMEOUT);
              return false;
          }

          int_stat = read_register(REG_INT_STAT);
          if (int_stat & INT_STAT_CHK_FAIL_13) {
              utils::PostErrorIfRunning(ERR_TIC_SELF_CHK_FAIL);
              return false;
          }
      }
  }

  // Configure interrupts for all channels
  uint16_t config = read_register(REG_CONFIG);
  config &= ~TRIGGER_11;  // Ensure TRIGGER = 0 for configuration
  write_register(REG_CONFIG, config);

  write_register(REG_INT_EN_COMP1, 0xFFFF);  // Enable interrupts for IN0–IN11
  write_register(REG_INT_EN_COMP2, 0xFFFF);  // Enable interrupts for IN12–IN23
  write_register(REG_INT_EN_CFG0, INT_EN_CFG0_TSD_EN_3 | INT_EN_CFG0_OV_EN_5 | INT_EN_CFG0_UV_EN_6);

  // Set TRIGGER = 1 to start monitoring
  config |= TRIGGER_11;
  write_register(REG_CONFIG, config);

  return true;
}


bool TIC12400::pollInterrupt() {
    if (!(gpio_get(_int_port, _int_pin))) {  
        _lastInterruptStatus = read_register(REG_INT_STAT);  // Read and clear interrupt
        updateChannelStates();
        return true;
    }
    return false;
}

void TIC12400::configureChannel(uint8_t channel, float threshold, Tic12400PullMode pullState) {
    if (channel >= MAX_CHANNELS) return;

    _channels[channel].channel = channel;
    _channels[channel].threshold = threshold;
    _channels[channel].pullMode = pullState;

    // Configure hardware registers
    uint8_t reg_threshold = REG_THRES_CFG0 + (channel / 4);  // 4 channels per register
    write_register(reg_threshold, static_cast<uint16_t>(threshold * 100));  

}

Tic12400Channel_t TIC12400::getChannelState(uint8_t channel) {
    if (channel >= MAX_CHANNELS) return {};

    uint8_t status_reg = REG_IN_STAT_COMP + (channel / 12);
    _channels[channel].state = (read_register(status_reg) >> (channel % 12)) & 0x01;

    uint8_t analog_reg = REG_ANA_STAT0 + (channel / 2);
    _channels[channel].valueVoltage = read_register(analog_reg);

    return _channels[channel];
}

void TIC12400::updateChannelStates() {
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
        if (_lastInterruptStatus & (1 << i)) {
            _channels[i].state = (read_register(REG_IN_STAT_COMP + (i / 12)) >> (i % 12)) & 0x01;
            _channels[i].valueVoltage = read_register(REG_ANA_STAT0 + (i / 2));
        }
    }
}

void TIC12400::configureInputMode(uint8_t channel, Tic12400InputMode mode) {
  if (channel > 12) {
      return;
  }

  uint16_t cs_select = read_register(REG_CS_SELECT);
  uint16_t mode_select = read_register(REG_MODE);

  switch (mode) {
      case INPUT_MODE_SWITCH_TO_GND:
          cs_select |= (1 << channel);     // Enable current source (pull-up)
          mode_select &= ~(1 << channel);  // Set as digital input
          _channels[channel].pullMode = PULL_MODE_PULLUP;
          _channels[channel].inputMode = INPUT_MODE_SWITCH_TO_GND;
          break;

      case INPUT_MODE_SWITCH_TO_VBAT:
          cs_select &= ~(1 << channel);    // Enable current sink (pull-down)
          mode_select &= ~(1 << channel);  // Set as digital input
          _channels[channel].pullMode = PULL_MODE_PULLDOWN;
          _channels[channel].inputMode = INPUT_MODE_SWITCH_TO_VBAT;
          break;

      case INPUT_MODE_HIGH_IMPEDANCE:
          cs_select &= ~(1 << channel);    // Disable current source/sink
          mode_select |= (1 << channel);   // Set as high-impedance/analog input
          _channels[channel].pullMode = PULL_MODE_NONE;
          _channels[channel].inputMode = INPUT_MODE_HIGH_IMPEDANCE;
          break;
  }

  // Write back the updated configuration
  write_register(REG_CS_SELECT, cs_select);
  write_register(REG_MODE, mode_select);
}

void TIC12400::configureWettingCurrent(uint8_t channel, float current_mA) {
  if (channel >= MAX_CHANNELS) return;

  uint8_t reg_wetting = (channel < 12) ? REG_WC_CFG0 : REG_WC_CFG1;
  uint8_t bit_offset = (channel % 12) * 2;  // Each channel uses 2 bits for wetting current configuration

  uint16_t current_cfg = read_register(reg_wetting);

  if (current_mA == 0) {
      // Disable wetting current for this channel
      current_cfg &= ~(0x03 << bit_offset);
  } else {
      // Calculate wetting current value (assume scaling factor, e.g., 0.1 mA per step)
      uint8_t wetting_value = static_cast<uint8_t>(current_mA * 10) & 0x03;
      current_cfg &= ~(0x03 << bit_offset);        // Clear previous setting
      current_cfg |= (wetting_value << bit_offset); // Set new wetting current
  }

  write_register(reg_wetting, current_cfg);
  _channels[channel].wettingCurrent = current_mA;  // Update channel struct
}

float TIC12400::getWettingCurrent(uint8_t channel) {
  if (channel >= MAX_CHANNELS) return 0;

  uint8_t reg_wetting = (channel < 12) ? REG_WC_CFG0 : REG_WC_CFG1;
  uint8_t bit_offset = (channel % 12) * 2;

  uint16_t current_cfg = read_register(reg_wetting);
  uint8_t wetting_value = (current_cfg >> bit_offset) & 0x03;

  // Convert back to mA (assuming 0.1 mA per step)
  return (wetting_value == 0) ? 0 : (wetting_value * 0.1f);
}

uint16_t TIC12400::read_register(uint8_t reg) {
    uint16_t cmd = (reg << 8);
    uint16_t response;
    gpio_clear(_cs_port, _cs_pin);
    spi_xfer(_spi_bus, cmd);
    response = spi_xfer(_spi_bus, 0xFFFF);
    gpio_set(_cs_port, _cs_pin);
    return response;
}

void TIC12400::write_register(uint8_t reg, uint16_t value) {
    uint16_t cmd = (reg << 8) | ((value >> 8) & 0xFF);
    uint16_t data = value & 0xFF;
    gpio_clear(_cs_port, _cs_pin);
    spi_xfer(_spi_bus, cmd);
    spi_xfer(_spi_bus, data);
    gpio_set(_cs_port, _cs_pin);
}

bool TIC12400::pollDiagnostics() {
  uint16_t int_stat = read_register(REG_INT_STAT);  // Read and clear interrupt flags
  bool retval = true;

  if (int_stat & INT_STAT_OV_6) {
      utils::PostErrorIfRunning(ERR_TIC_OVERVOLTAGE);
      retval = false;
  }

  if (int_stat & INT_STAT_UV_7) {
      utils::PostErrorIfRunning(ERR_TIC_UNDERVOLTAGE);
      retval = false;
  }

  if (int_stat & INT_STAT_TSD_4) {
      utils::PostErrorIfRunning(ERR_TIC_THERMAL_SHUTDOWN);
      retval = false;
  }

  if (int_stat & INT_STAT_WET_DIAG_11) {
      utils::PostErrorIfRunning(ERR_TIC_WETTING_CURRENT_FAIL);
      retval = false;
  }

  return retval;  // Return true if no errors were found
}