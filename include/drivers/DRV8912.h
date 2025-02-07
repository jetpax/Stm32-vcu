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

#ifndef DRV8912_H
#define DRV8912_H

#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/gpio.h>
#include "hwdefs.h"

// DRV8912 Modes
enum class OutputMode {
    HIGH_SIDE,   // HS mode
    LOW_SIDE,    // LS mode
    HALF_BRIDGE, // Single channel as half-bridge
    FULL_BRIDGE  // Two channels as full bridge
};

class DRV8912
{
public:
    DRV8912(uint32_t spi, uint32_t cs_port, uint16_t cs_pin, uint32_t int_port, uint16_t int_pin);

    void init();

    // Motor Control
    void setChannelMode(uint8_t channel, OutputMode mode);
    void enableChannel(uint8_t channel);
    void disableChannel(uint8_t channel);
    void setPWM(uint8_t channel, uint8_t dutyCycle);
    
    // Status & Fault Handling
    uint16_t readStatus();
    void clearFaults();
    

private:
    uint32_t _spi;
    uint32_t _cs_port;
    uint16_t _cs_pin;
    uint32_t _int_port;
    uint16_t _int_pin;

    // Low-level SPI communication
    void writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t reg);
    void selectChip();
    void deselectChip();
};

#endif // DRV8912_H