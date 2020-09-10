#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf_delay.h"
#include "boards.h"

#include "one_wire.h"

//DS18B20 stuff
#define DS18B20PIN NRF_GPIO_PIN_MAP(0, 31)

#define RED     1
#define GREEN   2
#define BLUE    3

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    /* Configure board. */
    bsp_board_init(BSP_INIT_LEDS);

    while (true) {
        //Blink to start a new reading
        bsp_board_led_off(0);
        for (int i = 0; i < 4; i++) {
            bsp_board_led_invert(0);
            nrf_delay_ms(100);
        }

        uint8_t addr[8];
        if (!ow_search(DS18B20PIN, addr, true)) {
            continue;
        }

        bool present = ow_reset(DS18B20PIN);
        if (present) {
            ow_select(DS18B20PIN, addr);
            ow_write(DS18B20PIN, 0x44, false); // start conversion

            nrf_delay_ms(1000);

            ow_reset(DS18B20PIN);
            ow_select(DS18B20PIN, addr);
            ow_write(DS18B20PIN, 0xBE, false); // Read Scratchpad

            uint8_t data[12];
            for (int i = 0; i < 9; i++) {           // we need 9 bytes
                data[i] = ow_read(DS18B20PIN);
            }

            // Clear for new render
            bsp_board_leds_off();
            if (ow_crc8(data, 8)) {
                bsp_board_led_on(0);
                // Convert the data to actual temperature
                // because the result is a 16 bit signed integer, it should
                // be stored to an "int16_t" type, which is always 16 bits
                // even when compiled on a 32 bit processor.
                int16_t raw = (data[1] << 8) | data[0];
                uint8_t cfg = (data[4] & 0x60);
                // at lower res, the low bits are undefined, so let's zero them
                if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
                else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
                else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms

                float temperature = (float)raw / 16.0;
                if (temperature < 20.0) {
                    bsp_board_led_on(BLUE);
                } else if (temperature > 30.0) {
                    bsp_board_led_on(RED);
                } else {
                    bsp_board_led_on(GREEN);
                }
            }
        }
    }
}
