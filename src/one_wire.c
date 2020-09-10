/*
    One wire lib ported from https://github.com/PaulStoffregen/OneWire
*/

#include "one_wire.h"

#include "nrf_delay.h"
#include "boards.h"

// global search state
uint8_t ROM_NO[8];
uint8_t LastDiscrepancy;
uint8_t LastFamilyDiscrepancy;
bool LastDeviceFlag;

// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
bool ow_reset(uint32_t pin)
{
    uint8_t retries = 125;

    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
    // wait until the wire is high... just in case
    do {
        if (--retries == 0)
            return 0;
        nrf_delay_us(2);
    } while (!nrf_gpio_pin_read(pin));

    nrf_gpio_cfg_output(pin);
    nrf_gpio_pin_write(pin, 0);
    nrf_delay_us(480);

    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
    nrf_delay_us(70);
    uint8_t reading = !nrf_gpio_pin_read(pin);
    nrf_delay_us(410);
    return reading;
}

//
// Write a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
void ow_write_bit(uint32_t pin, uint8_t v)
{
    nrf_gpio_cfg_output(pin);
    if (v & 1)
    {
        nrf_gpio_pin_write(pin, 0); // drive output low
        nrf_delay_us(10);
        nrf_gpio_pin_write(pin, 1); // drive output high
        nrf_delay_us(55);
    }
    else
    {
        nrf_gpio_pin_write(pin, 0); // drive output low
        nrf_delay_us(65);
        nrf_gpio_pin_write(pin, 1); // drive output high
        nrf_delay_us(5);
    }
}

//
// Read a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
uint8_t ow_read_bit(uint32_t pin)
{
    nrf_gpio_cfg_output(pin);
    nrf_gpio_pin_write(pin, 0); // drive output low
    nrf_delay_us(3);
    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL); // let pin float, pull up will raise
    nrf_delay_us(10);
    uint8_t reading = nrf_gpio_pin_read(pin);
    nrf_delay_us(53);
    return reading;
}

//
// Write a byte. The writing code uses the active drivers to raise the
// pin high, if you need power after the write (e.g. DS18S20 in
// parasite power mode) then set 'power' to 1, otherwise the pin will
// go tri-state at the end of the write to avoid heating in a short or
// other mishap.
//
void ow_write(uint32_t pin, uint8_t v, uint8_t power)
{
    uint8_t bitMask;

    for (bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        ow_write_bit(pin, (bitMask & v) ? 1 : 0);
    }
    if (!power)
    {
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
        nrf_gpio_pin_write(pin, 0); // drive output low
    }
}

void ow_write_bytes(uint32_t pin, const uint8_t *buf, uint16_t count, bool power)
{
    for (uint16_t i = 0; i < count; i++)
        ow_write(pin, buf[i], false);
    if (!power)
    {
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
        nrf_gpio_pin_write(pin, 0); // drive output low
    }
}

//
// Read a byte
//
uint8_t ow_read(uint32_t pin)
{
    uint8_t bitMask;
    uint8_t r = 0;

    for (bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        if (ow_read_bit(pin))
            r |= bitMask;
    }
    return r;
}

void ow_read_bytes(uint32_t pin, uint8_t *buf, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
        buf[i] = ow_read(pin);
}

//
// Do a ROM select
//
void ow_select(uint32_t pin, const uint8_t rom[8])
{
    uint8_t i;

    ow_write(pin, 0x55, false); // Choose ROM

    for (i = 0; i < 8; i++)
        ow_write(pin, rom[i], false);
}

//
// Do a ROM skip
//
void ow_skip(uint32_t pin)
{
    ow_write(pin, 0xCC, false); // Skip ROM
}

void ow_depower(uint32_t pin)
{
    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
}

//
// You need to use this function to start a search again from the beginning.
// You do not need to do it for the first search, though you could.
//
void ow_reset_search() {
  // reset the search state
  LastDiscrepancy = 0;
  LastDeviceFlag = false;
  LastFamilyDiscrepancy = 0;
  for(int i = 7; ; i--) {
    ROM_NO[i] = 0;
    if ( i == 0) break;
  }
}

//
// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
//
void ow_target_search(uint8_t family_code) {
   // set the search state to find SearchFamily type devices
   ROM_NO[0] = family_code;
   for (uint8_t i = 1; i < 8; i++)
      ROM_NO[i] = 0;
   LastDiscrepancy = 64;
   LastFamilyDiscrepancy = 0;
   LastDeviceFlag = false;
}

//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
bool ow_search(uint32_t pin, uint8_t *newAddr, bool search_mode) {
   uint8_t id_bit_number;
   uint8_t last_zero, rom_byte_number;
   bool    search_result;
   uint8_t id_bit, cmp_id_bit;

   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = false;

   // if the last call was not the last one
   if (!LastDeviceFlag) {
      // 1-Wire reset
      if (!ow_reset(pin)) {
         // reset the search
         LastDiscrepancy = 0;
         LastDeviceFlag = false;
         LastFamilyDiscrepancy = 0;
         return false;
      }

      // issue the search command
      if (search_mode == true) {
        ow_write(pin, 0xF0, false);   // NORMAL SEARCH
      } else {
        ow_write(pin, 0xEC, false);   // CONDITIONAL SEARCH
      }

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit = ow_read_bit(pin);
         cmp_id_bit = ow_read_bit(pin);

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1)) {
            break;
         } else {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit) {
               search_direction = id_bit;  // bit write value for search
            } else {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < LastDiscrepancy) {
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
               } else {
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == LastDiscrepancy);
               }
               // if 0 was picked then record its position in LastZero
               if (search_direction == 0) {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     LastFamilyDiscrepancy = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
              ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;

            // serial number search direction write bit
            ow_write_bit(pin, search_direction);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0) {
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!(id_bit_number < 65)) {
         // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
         LastDiscrepancy = last_zero;

         // check for last device
         if (LastDiscrepancy == 0) {
            LastDeviceFlag = true;
         }
         search_result = true;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[0]) {
      LastDiscrepancy = 0;
      LastDeviceFlag = false;
      LastFamilyDiscrepancy = 0;
      search_result = false;
   } else {
      for (int i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
   }
   return search_result;
}

// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (Use tiny 2x16 entry CRC table)
uint8_t ow_crc8(const uint8_t *addr, uint8_t len) {
	uint8_t crc = 0;
	while (len--) {
		uint8_t inbyte = *addr++;
		for (uint8_t i = 8; i; i--) {
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
