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

#ifndef UJA1023_H
#define UJA1023_H

#include <cstdint>

/**
 * @brief UJA1023 driver with individual pin control over LINbus.
 *
 * This driver sends and receives LIN frames (via an external busio layer)
 * to configure and control I/O pins on the UJA1023 transceiver.
 */
class Uja1023 {
public:
    /**
     * @brief Create a UJA1023 object.
     *
     * @param usart_base Not used internally (serial port is already initialized).
     * @param baudrate    Not used here.
     */
    Uja1023(uint32_t usart_base, int baudrate);

    /**
     * @brief Stub initialization function.
     *
     * The serial port/DMA init is assumed to be done elsewhere.
     */
    void init();

    // Mode control (if required)
    void setConfigMode();
    void setNormalMode();
    void setSleepMode();
    void reset();

    /**
     * @brief Transmit a LIN message.
     *
     * This builds a LIN frame with the following format:
     *   Byte 0: Sync (0x55)
     *   Byte 1: Protected identifier (PID) computed from the LIN id
     *   Bytes 2 .. (len+1): Payload
     *   Byte (len+2): Checksum (over PID and payload)
     *
     * @param id   LIN identifier (without parity)
     * @param data Pointer to payload bytes.
     * @param len  Number of payload bytes.
     */
    void sendMessage(uint8_t id, const uint8_t* data, uint8_t len);

    /**
     * @brief Poll for a received LIN message.
     *
     * The expected frame format is the same as in sendMessage().
     *
     * @param id          LIN identifier to look for.
     * @param requiredLen Expected payload length.
     * @return true if a valid frame was received, false otherwise.
     */
    bool hasReceived(uint8_t id, uint8_t requiredLen);

    // --- High-level I/O functions over LINbus ---

    /**
     * @brief Set the state of a pin.
     *
     * Sends a LIN message with ID 0x30 and payload: [pin, state].
     *
     * @param pin   The pin number.
     * @param state true for high, false for low.
     */
    void setPin(uint8_t pin, bool state);

    /**
     * @brief Get the state of a pin.
     *
     * Sends a LIN request with ID 0x31 (payload: [pin]) and polls for a response
     * whose payload is [pin, state].
     *
     * @param pin The pin number.
     * @return true if the pin is high; false if low or on error.
     */
    bool getPin(uint8_t pin);

    /**
     * @brief Configure a pin as input or output.
     *
     * Sends a LIN message with ID 0x32 and payload: [pin, mode],
     * where mode = 1 (output) or 0 (input).
     *
     * @param pin      The pin number.
     * @param isOutput true to configure as output; false as input.
     */
    void configurePin(uint8_t pin, bool isOutput);

    /**
     * @brief (Optional) Query the current pin configuration.
     *
     * Sends a LIN request with ID 0x33 and payload: [pin], then polls for a response
     * whose payload is [pin, mode]. Returns true if the pin is configured as output.
     *
     * @param pin The pin number.
     * @return true if the pin is configured as output; false if input or on error.
     */
    bool isPinConfiguredAsOutput(uint8_t pin);

private:
    // Although the serial port (and DMA) are initialized elsewhere,
    // we retain these parameters for completeness.
    uint32_t usart;
    int baud;

    // Internal LIN frame buffers.
    static const int BUFFER_SIZE = 16;
    uint8_t sendBuffer[BUFFER_SIZE];
    uint8_t recvBuffer[BUFFER_SIZE];

    // LIN frame helpers.
    uint8_t calculateChecksum(uint8_t pid, const uint8_t* data, int len);
    uint8_t calculateParity(uint8_t id);
};

#endif // UJA1023_H