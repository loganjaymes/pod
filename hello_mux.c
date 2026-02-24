// TEST FILE TO DEBUG MUXING FOR QUAD BILATERAL SWITCH (please just get an 8x1 mux...)
// for testing: change `pico_enable_stdio_usb(hello_midi 0)` to `pico_enable_stdio_uart(hello_adc 0), pico_enable_stdio_usb(hello_adc 1)`
//    change cmake file as well at `${CMAKE_CURRENT_LIST_DIR}/hello_midi.c` to `${CMAKE_CURRENT_LIST_DIR}/hello_mux.c`
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

const int MUX_PINS[] = {12, 13, 14, 15};

// store in struct to keep persistent
typedef struct {
    int index;
    float value;
} MuxResult;

MuxResult get_max(float* pins) {
    MuxResult res = {0, pins[0]};
    
    for (int i = 1; i < 4; ++i) {
        if (pins[i] > res.value) {
            res.value = pins[i];
            res.index = i;
        }
    }
    return res;
}

void read_mux(float* out_buffer) {
    for (int i = 0; i < 4; ++i) {
        // switch mux OFF
        for (int j = 0; j < 4; j++) gpio_put(MUX_PINS[j], 0);

        // FIXME discharge step, but not "bleeding" fast enough
        gpio_init(26);
        gpio_set_dir(26, GPIO_OUT);
        gpio_put(26, 0); 
        sleep_us(5); // buffer

        // use as ADV
        adc_gpio_init(26);

        // open each channel
        gpio_put(MUX_PINS[i], 1);
        sleep_us(20); // Slightly longer settling time
        
        out_buffer[i] = (float)adc_read();
        
        gpio_put(MUX_PINS[i], 0);
    }
}

int main() {
    stdio_init_all();
    adc_init();

    for (int i = 0; i < 4; i++) {
        gpio_init(MUX_PINS[i]);
        gpio_set_dir(MUX_PINS[i], GPIO_OUT);
        gpio_put(MUX_PINS[i], 0); // start with everything disconnected
    }

    adc_gpio_init(26);
    adc_select_input(0);
    const float conversion_factor = 3.3f / (1 << 12);
    const float bit = ((float)((1 << 7) - 1))/((float)((1 << 12) - 1));
    // uint16_t velocity;
    

    float mux_values[] = {0, 0, 0, 0};

    while (1) {
        printf("MUX ARR:\n");
        printf("[\n0: %f\n", mux_values[0] * conversion_factor);
        printf("1: %f\n", mux_values[1] * conversion_factor);
        printf("2: %f\n", mux_values[2] * conversion_factor);
        printf("3: %f\n]\n", mux_values[3] * conversion_factor);
        read_mux(mux_values);
        
        MuxResult max_data = get_max(mux_values);

        printf("max val of %f on channel %d\n", max_data.value, max_data.index);
        
        sleep_ms(800);
    }
}