#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "bsp/board_api.h"
#include "tusb.h"

// DEFINE ALL NOTE POS
#define KICK 36
#define SNARE 38
#define CLOSED_HAT 60

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void midi_task(void);

/*------------- MAIN -------------*/
int main(void) {
  board_init();
  stdio_init_all();
  adc_init();

  adc_gpio_init(26);
  gpio_set_dir(0, false);
  
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
  }
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

void midi_task(void)
{
  // static uint32_t start_ms = 0;
  uint8_t const cable_num = 0; // MIDI jack associated with USB endpoint
  uint8_t const channel = 0; // 0 for channel 1

  // The MIDI interface always creates input and output port/jack descriptors
  // regardless of these being used or not. Therefore incoming traffic should be read
  // (possibly just discarded) to avoid the sender blocking in IO
  while (tud_midi_available()) {
    uint8_t packet[4];
    tud_midi_packet_read(packet);
  }

  // Select ADC input 0 (GPIO26)
  adc_select_input(0);
  uint16_t result = adc_read();
  
  // calculate velocity 
  const float bit = ((float)((1 << 7) - 1))/((float)((1 << 12) - 1));
  uint16_t velocity = result * bit * 1.5;

  const float conversion_factor = 3.3f / (1 << 12);
  double volt = result * conversion_factor;
  
  if (volt >= 0.2) {
    if (velocity < 30) {
        velocity = 30;
    } else if (velocity > 115) {
        velocity = 115;
    }
    // Send Note On for current position at full velocity (127) on channel 1.
    uint8_t note_on[3] = { 0x90 | channel, SNARE, velocity };
    tud_midi_stream_write(cable_num, note_on, 3);

    // Send Note Off for previous note.
    uint8_t note_off[3] = { 0x80 | channel, SNARE, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
    sleep_ms(500);
  }

}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
