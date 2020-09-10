#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nrf_delay.h"
#include "boards.h"

//DS18B20 stuff
#define DS18B20PIN NRF_GPIO_PIN_MAP(0, 31)
#define TIMER_SHORT 5
#define TIMER_RESET 15
#define TIMER_LONG 20

void ds18b20_read(void *data_ptr, unsigned int num_bytes) {
    char ch; //Current reading byte buffer
    char *data_buf = data_ptr;

    int i = 0, u = 0;
    for (i = 0; i < num_bytes; i++)
    {
        ch = 0;
        for (u = 0; u < 8; u++)
        {
            //Form read slot
            nrf_gpio_cfg_output(DS18B20PIN);
            nrf_gpio_pin_write(DS18B20PIN, 0);
            nrf_delay_us(5);
            nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
            nrf_delay_us(5);
            if (nrf_gpio_pin_read(DS18B20PIN) > 0)
            {
                ch |= 1 << u; //There is 1 on the bus
            }
            else
            {
                ch &= ~(1 << u); //There us 0 on the bus
            }

            //Apply "long" timer for make sure that timeslot is end
            nrf_delay_us(60);
        }
        data_buf[i] = ch;
    }
    nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
}

void ds18b20_write(void *data_ptr, unsigned int num_bytes) {
    char ch; //Current reading byte buffer
    char *data_buf = data_ptr;

    int i = 0, u = 0;
    for (i = 0; i < num_bytes; i++)
    {
        ch = data_buf[i];
        for (u = 0; u < 8; u++)
        {
            //Form write slot
            nrf_gpio_cfg_output(DS18B20PIN);
            nrf_gpio_pin_write(DS18B20PIN, 0);
            nrf_delay_us(1);
            //write 1 - pull bus to HIGH just after short timer
            if (ch & (1 << u))
            {
                nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
            }
            //Apply "long" timer for make sure that timeslot is end
            nrf_delay_us(60);
            //Release bus, if this wasn't done before
            nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
            data_buf[i] = ch;
        }
    }
    nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
}

//Perform reset of the bus, and then wait for the presence pulse
bool ds18b20_reset_and_check() {
    int res = 0;
    //Form reset pulse

    nrf_gpio_cfg_output(DS18B20PIN);
    nrf_gpio_pin_write(DS18B20PIN, 0);
    nrf_delay_us(500);
    //Release bus and wait 15-60MS
    nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
    nrf_delay_us(500);

    //Read from bus
    res = nrf_gpio_pin_read(DS18B20PIN);
    if (res == 0)
    {
        nrf_delay_us(500);
        return true;
    }
    return false;
}
/**
 *@}
 **/
float ds18b20_read_remp() {
    char buf[16];
    double f;
    int16_t raw_temp = 0;

    ds18b20_reset_and_check();

    //Read ROM
    buf[0] = 0x33;
    ds18b20_write(&buf, 1);

    //Read the results

    ds18b20_read(&buf, 8);
    memset(&buf, 0, 16);

    //Send convert TX cmd
    buf[0] = 0x44; //Convert temp
    ds18b20_write(&buf, 1);
    nrf_delay_ms(1000); //Wait for finishing of the conversion

    ds18b20_reset_and_check();
    buf[0] = 0x33; //Read ROM
    ds18b20_write(&buf, 1);
    //Read the results
    ds18b20_read(&buf, 8);
    memset(&buf, 0, 16);
    buf[0] = 0xBE; //Read scratchpad
    ds18b20_write(&buf, 1);
    //Read the results
    ds18b20_read(&buf, 9);
    raw_temp = (buf[1] << 8) | buf[0];

    //memcpy(&f,&buf,8);
    f = (float)raw_temp / 16.0;
    return (f);
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    /* Configure board. */
    bsp_board_init(BSP_INIT_LEDS);


    // int pin = NRF_GPIO_PIN_MAP(1, 10);
    // nrf_gpio_cfg_output(pin);

    for (int i = 0; i < 6; i++)
    {
        bsp_board_led_invert(0);
        nrf_delay_ms(500);
    }


    /* Toggle LEDs. */
    while (true) {
        // nrf_gpio_pin_toggle(pin);

        float temperature = ds18b20_read_remp();
        if (temperature < 20.0) {
            // Blue led
            bsp_board_led_on(3);
        } else if (temperature > 30.0) {
            // Red led
            bsp_board_led_on(1);
        } else {
            // Green
            bsp_board_led_on(2);
        }

        // bsp_board_led_invert(0);
        // nrf_delay_ms(1000);
        // bsp_board_led_invert(0);

        // nrf_gpio_cfg_output(DS18B20PIN);
        // nrf_gpio_pin_write(DS18B20PIN, 0);
        // nrf_delay_ms(500);
        // nrf_gpio_cfg_input(DS18B20PIN, NRF_GPIO_PIN_NOPULL);
        // nrf_delay_ms(500);
        // nrf_gpio_pin_read(DS18B20PIN);
    }
}
