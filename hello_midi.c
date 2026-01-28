#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

int main() {
    stdio_init_all();
    adc_init();

    adc_gpio_init(26); 
    adc_gpio_init(27); 
    adc_gpio_init(28); 
    

    while (1) {
        const float conversion_factor = 3.3f / (1 << 12);

        // might need to do this for each pin depending on how circuit structures multiplexor 
        adc_select_input(0);
        float val = adc_read();
        
        printf("Raw value: 0x%03x, voltage: %f V\n", val, val * conversion_factor);
        sleep_ms(500);
    }
}
