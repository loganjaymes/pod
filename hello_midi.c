#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#include "bsp/board_api.h"
#include "tusb.h"

// DEFINE ALL NOTE POS
// GPIO ADC separated by whitespace (since muxing, need to have drums grouped based on which instruments would be least likely to be played synchronously)
#define KICK 36
#define SNARE 38

#define TOM_1 0
#define HAT_CLOSE 60
#define HAT_OPEN 64

#define TOM_2 0
// below on same IO, have interrupt to change or change it based on song ig
#define CRASH 0 
#define RIDE 0

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
volatile int hat = HAT_CLOSE;  // global var for state change

void led_blinking_task(void);
void midi_task(int);

// ========= BUTTON INTERRUPT TO TOGGLE HAT STATE =========
// a little finnicky, might need to add a sleep somewhere
void toggle_hat_irq(uint gpio, uint32_t events) {
  if (gpio == 16 && (events & GPIO_IRQ_EDGE_FALL)) {
    if (hat == HAT_CLOSE) {
      hat = HAT_OPEN;
    } else {
      hat = HAT_CLOSE;
    }
  }
}


/*------------- MAIN -------------*/
int main(void) {
  board_init();
  stdio_init_all();
  adc_init();

  adc_gpio_init(26);  // adc 0
  adc_gpio_init(27);  // adc 1
  adc_gpio_init(28);  // adc 2
  gpio_init(16);      // hat state handler
  gpio_pull_up(16);

  gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_FALL, true, &toggle_hat_irq);
  
  
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
    midi_task(hat);
  }
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

void midi_task(int hat_state)
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

  const float conversion_factor = 3.3f / (1 << 12);
  const float bit = ((float)((1 << 7) - 1))/((float)((1 << 12) - 1));
  
  // Select ADC input 0 (GPIO26)
  adc_select_input(0);
  uint16_t kick_or_snare = adc_read();
  // calculate velocity & voltage (voltage is lowkey redundant but id need to recalc as a digital value, so thats a TODO)
  uint16_t kos_vel = kick_or_snare * bit * 1.5;
  double kos_volt = kick_or_snare * conversion_factor;

  // Select ADC input 1
  adc_select_input(1);
  uint16_t hat_or_tom = adc_read();
  uint16_t hot_vel = hat_or_tom * bit * 1.5;
  double hot_volt = hat_or_tom * conversion_factor;

  // Select ADC input 2
  adc_select_input(2);
  uint16_t cym_or_tom = adc_read();
  uint16_t cot_vel = cym_or_tom * bit * 1.5;
  double cot_volt = cym_or_tom * conversion_factor;
  
  // TODO: REFACTOR ALL OF TS INTO A FUNCTION OR SOMETHING SO THERES NOT A GIANT IF TREE
  // KICK NEEDS A LOTTTT HIGHER THRESH
  if (kos_volt >= 0.4) {
    if (kos_vel < 30) {
        kos_vel = 30;
    } else if (kos_vel > 115) {
        kos_vel = 115;
    }
    // ======= MUXER HERE =======
    // Send Note On for current position at full velocity (127) on channel 1.
    uint8_t note_on[3] = { 0x90 | channel, KICK, kos_vel };
    tud_midi_stream_write(cable_num, note_on, 3);

    // Send Note Off for previous note.
    uint8_t note_off[3] = { 0x80 | channel, KICK, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
    sleep_ms(150);

  } else if (hot_volt >= 0.2) {
    if (hot_vel < 30) {
        hot_vel = 30;
    } else if (hot_vel > 115) {
        hot_vel = 115;
    }
    // ======= MUXER HERE =======
    // Send Note On for current position at full velocity (127) on channel 1.
    uint8_t note_on[3] = { 0x90 | channel, hat_state, hot_vel };
    tud_midi_stream_write(cable_num, note_on, 3);

    // Send Note Off for previous note.
    uint8_t note_off[3] = { 0x80 | channel, hat_state, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
    sleep_ms(50);

  } else if (cot_volt >= 0.1) {
     if (cot_vel < 30) {
        cot_vel = 30;
    } else if (cot_vel > 115) {
        cot_vel = 115;
    }
    // ======= MUXER HERE =======
    // Send Note On for current position at full velocity (127) on channel 1.
    uint8_t note_on[3] = { 0x90 | channel, SNARE, cot_vel };
    tud_midi_stream_write(cable_num, note_on, 3);

    // Send Note Off for previous note.
    uint8_t note_off[3] = { 0x80 | channel, SNARE, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
    sleep_ms(50);
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
