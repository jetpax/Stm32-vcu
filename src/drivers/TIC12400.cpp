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
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>


// Global pointer used by the EXTI ISR (we assume only one instance)
static TIC12400* tic12400_instance = nullptr;

// -------- Private Helper Functions --------

uint16_t TIC12400::spi_transfer(uint16_t data) {
  return spi_xfer(_spi_bus, data);
}

uint16_t TIC12400::read_register(uint8_t reg) {
    // For a read, the command is the register (in the high byte)
    uint16_t cmd = (reg << 8);
    uint16_t response;
    gpio_clear(_cs_port, _cs_pin);
    spi_transfer(cmd);              // Send register address
    response = spi_transfer(0xFFFF);  // Read response (dummy write)
    gpio_set(_cs_port, _cs_pin);
    return response;
}

void TIC12400::write_register(uint8_t reg, uint16_t value) {
    // For a write, send the register address (high byte) and then data.
    uint16_t cmd = (reg << 8) | ((value >> 8) & 0xFF);
    uint16_t data = value & 0xFF;
    gpio_clear(_cs_port, _cs_pin);
    spi_transfer(cmd);
    spi_transfer(data);
    gpio_set(_cs_port, _cs_pin);
}

/**
 * Compute the EXTI line number from the int_pin mask.
 * Assumes int_pin is a single-bit mask (e.g. GPIO0, GPIO1, etc.).
 */
uint8_t TIC12400::get_exti_line() {
    uint8_t line = 0;
    uint16_t pin_mask = _int_pin;
    while (!(pin_mask & 1)) {
        pin_mask >>= 1;
        line++;
    }
    return line;
}

/**
 * Configure the GPIO and EXTI settings for the interrupt pin.
 */
void TIC12400::configure_interrupt_gpio() {
    // Configure the interrupt pin as an input with pull-up.
#ifdef STM32F1
    gpio_set_mode(_int_port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, int_pin);
    gpio_set(_int_port, int_pin);  // Enable pull-up (assuming active-low interrupt)
#else
    gpio_mode_setup(_int_port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, _int_pin);
#endif
    // Set up the EXTI line.
    uint8_t exti_line = get_exti_line();
    // Link the EXTI line to the proper GPIO port.
    exti_select_source(exti_line, _int_port);
    // Trigger on falling edge (adjust if your device uses a different polarity)
    exti_set_trigger(exti_line, EXTI_TRIGGER_FALLING);
    exti_enable_request(exti_line);


    if (exti_line == 0) {
        nvic_enable_irq(NVIC_EXTI0_IRQ);
    } else if (exti_line == 1) {
        nvic_enable_irq(NVIC_EXTI1_IRQ);
    } else if (exti_line == 2) {
        nvic_enable_irq(NVIC_EXTI2_IRQ);
    } else if (exti_line == 3) {
        nvic_enable_irq(NVIC_EXTI3_IRQ);
    } else if (exti_line >= 4 && exti_line <= 15) {
        nvic_enable_irq(NVIC_EXTI9_5_IRQ);
    }
}

// -------- Public Methods --------

TIC12400::TIC12400(uint32_t spi, uint32_t cs_gpio, uint16_t cs_pin,
                   uint32_t int_gpio, uint16_t int_pin)
    : _spi_bus(spi), _cs_port(cs_gpio), _cs_pin(cs_pin),
      _int_port(int_gpio), _int_pin(int_pin), interrupt_callback_(nullptr)
{
    // Save a pointer to this instance for the ISR.
    tic12400_instance = this;
}

void TIC12400::init() {
    // Reset and enable the device.
    write_register(TIC12400_RESET_CTRL, 0x0001);
    write_register(TIC12400_DEVICE_CTRL, 0x0001);
    // Configure the interrupt GPIO/EXTI.
    configure_interrupt_gpio();
}

void TIC12400::configure(uint8_t pin, Tic12400Mode mode, uint16_t threshold) {
    if (pin > 15)
        return;  // Out of range (TIC12400 supports 16 channels)
    uint8_t input_cfg_reg     = TIC12400_INPUT_CFG_BASE + pin;
    uint8_t threshold_cfg_reg = TIC12400_THRESHOLD_CFG_BASE + pin;
    write_register(input_cfg_reg, static_cast<uint16_t>(mode));
    write_register(threshold_cfg_reg, threshold);
}

uint16_t TIC12400::get(uint8_t pin) {
    if (pin > 15)
        return 0xFFFF;  // Invalid channel
    uint8_t status_reg = TIC12400_INPUT_STATUS_BASE + pin;
    return read_register(status_reg);
}

void TIC12400::set_interrupt_callback(Tic12400InterruptCallback cb) {
    interrupt_callback_ = cb;
}

/**
 * This method is invoked from the EXTI ISR.
 * It reads the device’s interrupt status register and, if a callback
 * is registered, passes the status value to the user.
 */
void TIC12400::handle_interrupt() {
    uint16_t status = read_register(TIC12400_INTERRUPT_STATUS);
    if (interrupt_callback_) {
        interrupt_callback_(status);
    }
}

// -------- EXTI Interrupt Service Routine --------
// For simplicity, we assume that the interrupt pin is on an EXTI line that falls
// within the NVIC_EXTI0_1_IRQ or NVIC_EXTI4_15_IRQ handler.

extern "C" void exti0_1_isr(void) {
    if (tic12400_instance) {
        uint8_t exti_line = tic12400_instance->get_exti_line();
        if (exti_get_flag_status(exti_line)) {
            tic12400_instance->handle_interrupt();
            exti_reset_request(exti_line);
        }
    }
}

extern "C" void exti4_15_isr(void) {
    if (tic12400_instance) {
        uint8_t exti_line = tic12400_instance->get_exti_line();
        if (exti_get_flag_status(exti_line)) {
            tic12400_instance->handle_interrupt();
            exti_reset_request(exti_line);
        }
    }
}