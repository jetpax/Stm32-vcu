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

#include "drivers/uja1023.h"
#include <string.h>
#include <stdint.h>

// Command IDs for our LIN protocol (adjust as needed)
#define CMD_SET_PIN        0x30
#define CMD_GET_PIN        0x31
#define CMD_CONFIG_PIN     0x32
#define CMD_GET_PIN_CONFIG 0x33

// ---------------------------------------------------------------------
// External bus I/O functions are assumed to be implemented elsewhere.
// For example:
//   void busio_transmit(const uint8_t* data, uint8_t len);
//   bool busio_receive(uint8_t* buffer, uint8_t expectedLen);
// ---------------------------------------------------------------------
extern void busio_transmit(const uint8_t* data, uint8_t len);
extern bool busio_receive(uint8_t* buffer, uint8_t expectedLen);

// ---------------------------------------------------------------------
// Constructor (serial port parameters are stored but not used here)
// ---------------------------------------------------------------------
Uja1023::Uja1023(uint32_t usart_base, int baudrate)
    : usart(usart_base), baud(baudrate)
{
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memset(recvBuffer, 0, sizeof(recvBuffer));
}

// init() is a stub since serial port init is handled externally.
void Uja1023::init() {
    // Nothing to do here.
}

// ---------------------------------------------------------------------
// Mode control functions (these simply send a one-byte command)
// ---------------------------------------------------------------------
void Uja1023::setConfigMode() {
    sendBuffer[0] = 0x80; // Example command for config mode.
    busio_transmit(sendBuffer, 1);
}

void Uja1023::setNormalMode() {
    sendBuffer[0] = 0x00; // Example command for normal mode.
    busio_transmit(sendBuffer, 1);
}

void Uja1023::setSleepMode() {
    sendBuffer[0] = 0x20; // Example command for sleep mode.
    busio_transmit(sendBuffer, 1);
}

void Uja1023::reset() {
    sendBuffer[0] = 0xC0; // Example reset command.
    busio_transmit(sendBuffer, 1);
}

// ---------------------------------------------------------------------
// LIN frame helper functions
// ---------------------------------------------------------------------
uint8_t Uja1023::calculateParity(uint8_t id) {
    // Compute LIN parity bits (P0 and P1) per LIN spec.
    bool p1 = !(((id & 0x2) > 0) ^ ((id & 0x8) > 0) ^ ((id & 0x10) > 0) ^ ((id & 0x20) > 0));
    bool p0 = ((id & 0x1) > 0) ^ ((id & 0x2) > 0) ^ ((id & 0x4) > 0) ^ ((id & 0x10) > 0);
    return id | (p1 << 7) | (p0 << 6);
}

uint8_t Uja1023::calculateChecksum(uint8_t pid, const uint8_t* data, int len) {
    uint8_t checksum = pid;
    for (int i = 0; i < len; i++) {
        uint16_t tmp = (uint16_t)checksum + data[i];
        if (tmp > 256)
            tmp -= 255;
        checksum = (uint8_t)tmp;
    }
    return checksum ^ 0xff;
}

// ---------------------------------------------------------------------
// sendMessage: Build a LIN frame and send it using the external bus I/O.
// ---------------------------------------------------------------------
void Uja1023::sendMessage(uint8_t id, const uint8_t* data, uint8_t len) {
    if (len > (BUFFER_SIZE - 3)) return; // Protect against buffer overrun.
    sendBuffer[0] = 0x55;  // Sync byte.
    sendBuffer[1] = calculateParity(id);
    for (uint8_t i = 0; i < len; i++) {
        sendBuffer[i + 2] = data[i];
    }
    sendBuffer[len + 2] = calculateChecksum(sendBuffer[1], data, len);
    busio_transmit(sendBuffer, len + 3);
}

// ---------------------------------------------------------------------
// hasReceived: Poll for a LIN frame (using the external bus I/O).
// ---------------------------------------------------------------------
bool Uja1023::hasReceived(uint8_t id, uint8_t requiredLen) {
    // expected frame length: 1 (sync) + 1 (PID) + requiredLen (payload) + 1 (checksum)
    uint8_t expectedFrameLen = requiredLen + 3;
    if (!busio_receive(recvBuffer, expectedFrameLen))
        return false;

    // Check that the PID matches.
    uint8_t expectedPid = calculateParity(id);
    if (recvBuffer[1] != expectedPid)
        return false;

    // Verify checksum.
    uint8_t computed = calculateChecksum(recvBuffer[1], &recvBuffer[2], requiredLen);
    return (computed == recvBuffer[requiredLen + 2]);
}

// ---------------------------------------------------------------------
// High-level I/O functions
// ---------------------------------------------------------------------
void Uja1023::setPin(uint8_t pin, bool state) {
    // Build payload: [pin, state]
    uint8_t payload[2];
    payload[0] = pin;
    payload[1] = state ? 1 : 0;
    sendMessage(CMD_SET_PIN, payload, 2);
    // Optionally, one might wait for an ACK.
}

bool Uja1023::getPin(uint8_t pin) {
    // Build payload: [pin]
    uint8_t payload[1];
    payload[0] = pin;
    sendMessage(CMD_GET_PIN, payload, 1);

    // Wait for a response. Here we poll for a valid response.
    const int expectedPayloadLen = 2; // Response: [pin, state]
    const int timeout = 1000000;
    for (int i = 0; i < timeout; i++) {
        if (hasReceived(CMD_GET_PIN, expectedPayloadLen)) {
            // Check that the returned pin number matches our request.
            if (recvBuffer[2] == pin) {
                return (recvBuffer[3] != 0);
            }
        }
    }
    // On timeout or error, return false.
    return false;
}

void Uja1023::configurePin(uint8_t pin, bool isOutput) {
    // Build payload: [pin, mode] where mode=1 for output, 0 for input.
    uint8_t payload[2];
    payload[0] = pin;
    payload[1] = isOutput ? 1 : 0;
    sendMessage(CMD_CONFIG_PIN, payload, 2);
    // Optionally, one might wait for an acknowledgment.
}

bool Uja1023::isPinConfiguredAsOutput(uint8_t pin) {
    // Build payload: [pin]
    uint8_t payload[1];
    payload[0] = pin;
    sendMessage(CMD_GET_PIN_CONFIG, payload, 1);

    const int expectedPayloadLen = 2; // Expected response: [pin, mode]
    const int timeout = 1000000;
    for (int i = 0; i < timeout; i++) {
        if (hasReceived(CMD_GET_PIN_CONFIG, expectedPayloadLen)) {
            if (recvBuffer[2] == pin) {
                return (recvBuffer[3] != 0);
            }
        }
    }
    // On timeout or error, assume not configured as output.
    return false;
}