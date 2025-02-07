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

#ifndef TIC12400_H
#define TIC12400_H

#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/f4/nvic.h>
#include <cstdint>

/* TIC12400 Register Addresses */
constexpr uint8_t TIC12400_INPUT_CFG_BASE      = 0x00;  // Input configuration registers (per channel)
constexpr uint8_t TIC12400_THRESHOLD_CFG_BASE  = 0x10;  // Threshold configuration registers (per channel)
constexpr uint8_t TIC12400_INPUT_STATUS_BASE   = 0x50;  // Input status registers (per channel)
constexpr uint8_t TIC12400_DEVICE_CTRL         = 0x80;  // Global device control register
constexpr uint8_t TIC12400_RESET_CTRL          = 0x81;  // Reset register
constexpr uint8_t TIC12400_DEVICE_ID           = 0xFF;  // Device ID register
constexpr uint8_t TIC12400_INTERRUPT_STATUS    = 0x70;  // Interrupt status register

/* Input Modes */
enum class Tic12400Mode : uint16_t {
    FLOATING   = 0x0000,
    PULLUP     = 0x1000,
    PULLDOWN   = 0x2000,
    ANALOG     = 0x3000,
    DIGITAL    = 0x4000
};

/* Callback for interrupt events.
   The parameter is the value read from the interrupt status register. */
typedef void (*Tic12400InterruptCallback)(uint16_t status);

class TIC12400 {
private:
    uint32_t _spi_bus;   // e.g. SPI1, SPI2, etc.
    uint32_t _cs_port;    // GPIO port for chip select (CS)
    uint16_t _cs_pin;     // GPIO pin for CS

    uint32_t _int_port;   // GPIO port for the TIC12400 interrupt pin
    uint16_t _int_pin;    // GPIO pin for the interrupt

    Tic12400InterruptCallback interrupt_callback_;

    // Private helper functions for SPI communication:
    void cs_low();
    void cs_high();
    uint16_t spi_transfer(uint16_t data);
    uint16_t read_register(uint8_t reg);
    void write_register(uint8_t reg, uint16_t value);

    // Configure the GPIO and EXTI for the TIC12400 interrupt pin.
    void configure_interrupt_gpio();

public:
    /**
     * Constructor.
     * @param spi         The SPI peripheral (e.g. SPI1)
     * @param cs_gpio     The GPIO port for CS (e.g. GPIOA)
     * @param cs_pin      The GPIO pin for CS (e.g. GPIO4)
     * @param int_gpio    The GPIO port for the interrupt pin.
     * @param int_pin     The GPIO pin for the interrupt (must be a single-bit mask).
     */
    TIC12400(uint32_t spi, uint32_t cs_gpio, uint16_t cs_pin,
             uint32_t int_gpio, uint16_t int_pin);

    /**
     * Initialize the device: reset, enable, and configure the interrupt pin.
     */
    void init();

    /**
     * Configure an individual channel.
     * @param pin       Channel number (0–15)
     * @param mode      Mode (analog, digital, pullup, pulldown, floating)
     * @param threshold Optional threshold value (default 0)
     */
    void configure(uint8_t pin, Tic12400Mode mode, uint16_t threshold = 0);

    /**
     * Get the status (or value) of an individual channel.
     * @param pin   Channel number (0–15)
     * @return      16-bit value read from the corresponding input status register.
     */
    uint16_t get(uint8_t pin);

    // Compute the EXTI line from the int_pin bit mask (assumes a single-bit value).
    uint8_t get_exti_line();

    /**
     * Register a callback that will be called when an interrupt occurs.
     * @param cb    Callback function pointer.
     */
    void set_interrupt_callback(Tic12400InterruptCallback cb);

    /**
     * This method is invoked from the EXTI ISR.
     * It reads the interrupt status register and calls the user callback if set.
     */
    void handle_interrupt();
};

#endif // TIC12400_H