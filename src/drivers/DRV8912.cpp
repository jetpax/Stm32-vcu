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

    // SPI Setup
    // spi_reset(_spi);
    // spi_init_master(_spi, SPI_CR1_BAUDRATE_FPCLK_DIV_16, SPI_CR1_CPOL_0, SPI_CR1_CPHA_0, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
    // spi_enable(_spi);
}

// Configure channel mode (HS, LS, Half-Bridge, Full-Bridge)
void DRV8912::setChannelMode(uint8_t channel, OutputMode mode)
{
    if (channel > 11) return;

    uint16_t modeValue = readRegister(DRV8912_MODE_REG);

    switch (mode)
    {
        case OutputMode::HIGH_SIDE:
            modeValue |= (1 << (channel * 2));  // HS bit
            modeValue &= ~(1 << (channel * 2 + 1)); // Clear LS bit
            break;
        case OutputMode::LOW_SIDE:
            modeValue &= ~(1 << (channel * 2));  // Clear HS bit
            modeValue |= (1 << (channel * 2 + 1)); // LS bit
            break;
        case OutputMode::HALF_BRIDGE:
            modeValue |= (1 << (channel * 2));  // Enable HS
            modeValue |= (1 << (channel * 2 + 1)); // Enable LS
            break;
        case OutputMode::FULL_BRIDGE:
            if (channel % 2 == 0)  // Full bridge uses pairs (0-1, 2-3, ...)
            {
                modeValue |= (1 << (channel * 2));  // HS on first
                modeValue |= (1 << ((channel + 1) * 2 + 1)); // LS on second
            }
            break;
    }

    writeRegister(DRV8912_MODE_REG, modeValue);
}

// Enable a motor channel
void DRV8912::enableChannel(uint8_t channel)
{
    if (channel > 11) return;
    uint16_t value = readRegister(DRV8912_CONTROL_REG);
    value |= (1 << channel);
    writeRegister(DRV8912_CONTROL_REG, value);
}

// Disable a motor channel
void DRV8912::disableChannel(uint8_t channel)
{
    if (channel > 11) return;
    uint16_t value = readRegister(DRV8912_CONTROL_REG);
    value &= ~(1 << channel);
    writeRegister(DRV8912_CONTROL_REG, value);
}

// Set PWM duty cycle
void DRV8912::setPWM(uint8_t channel, uint8_t dutyCycle)
{
    if (channel > 11) return;
    writeRegister(DRV8912_PWM_REG_BASE + channel, dutyCycle);
}

// Read status register
uint16_t DRV8912::readStatus()
{
    return readRegister(DRV8912_STATUS_REG);
}

// Clear fault register
void DRV8912::clearFaults()
{
    writeRegister(DRV8912_FAULT_REG, 0xFFFF);
}

// Write to a register
void DRV8912::writeRegister(uint8_t reg, uint16_t value)
{
    gpio_clear(_cs_port, _cs_pin);
    spi_send(_spi, reg);
    spi_send(_spi, (value >> 8) & 0xFF);
    spi_send(_spi, value & 0xFF);
    gpio_set(_cs_port, _cs_pin);
}


// Read from a register
uint16_t DRV8912::readRegister(uint8_t reg)
{
    gpio_clear(_cs_port, _cs_pin);
    spi_send(_spi, reg | 0x80);
    uint16_t value = (spi_read(_spi) << 8);
    value |= spi_read(_spi);
    gpio_set(_cs_port, _cs_pin);
    return value;
}

