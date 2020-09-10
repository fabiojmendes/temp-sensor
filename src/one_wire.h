#pragma once

#include <stdint.h>
#include <stdbool.h>

// Perform a 1-Wire reset cycle. Returns 1 if a device responds
// with a presence pulse.  Returns 0 if there is no device or the
// bus is shorted or otherwise held low for more than 250uS
bool ow_reset(uint32_t pin);

// Issue a 1-Wire rom select command, you do the reset first.
void ow_select(uint32_t pin, const uint8_t rom[8]);

// Issue a 1-Wire rom skip command, to address all on bus.
void ow_skip(uint32_t pin);

// Write a byte. If 'power' is one then the wire is held high at
// the end for parasitically powered devices. You are responsible
// for eventually depowering it by calling depower() or doing
// another read or write.
void ow_write(uint32_t pin, uint8_t v, uint8_t power);

void ow_write_bytes(uint32_t pin, const uint8_t *buf, uint16_t count, bool power);

// Read a byte.
uint8_t ow_read(uint32_t pin);

void ow_read_bytes(uint32_t pin, uint8_t *buf, uint16_t count);

// Write a bit. The bus is always left powered at the end, see
// note in write() about that.
void ow_write_bit(uint32_t pin, uint8_t v);

// Read a bit.
uint8_t ow_read_bit(uint32_t pin);

// Stop forcing power onto the bus. You only need to do this if
// you used the 'power' flag to write() or used a write_bit() call
// and aren't about to do another read or write. You would rather
// not leave this powered if you don't have to, just in case
// someone shorts your bus.
void ow_depower(uint32_t pin);

// Clear the search state so that if will start from the beginning again.
void ow_reset_search();

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
void ow_target_search(uint8_t family_code);

// Look for the next device. Returns 1 if a new address has been
// returned. A zero might mean that the bus is shorted, there are
// no devices, or you have already retrieved all of them.  It
// might be a good idea to check the CRC to make sure you didn't
// get garbage.  The order is deterministic. You will always get
// the same devices in the same order.
bool ow_search(uint32_t pin, uint8_t *newAddr, bool search_mode);

// Compute a Dallas Semiconductor 8 bit CRC, these are used in the
// ROM and scratchpad registers.
uint8_t ow_crc8(const uint8_t *addr, uint8_t len);
