#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <sodium.h>
#include <driver/gpio.h>
#include <esp_int_wdt.h>
#include <driver/spi_master.h>

#include "config.hpp"

#include "mongoose.h"
#include "sdkconfig.h"


#define GPIO_OUTPUT_IO_LE        15
#define GPIO_OUTPUT_IO_POL       16
#define GPIO_OUTPUT_IO_BL        17
#define GPIO_OUTPUT_IO_HV_DIS    18

#define GPIO_OUTPUT_PIN_SEL  ((1ULL << GPIO_OUTPUT_IO_LE) | (1ULL << GPIO_OUTPUT_IO_POL) | (1ULL << GPIO_OUTPUT_IO_BL) | (1ULL << GPIO_OUTPUT_IO_HV_DIS))

#define SPI_MOSI    13
#define SPI_SCLK    14


void send_cathodes(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if(len == 0) return;                //no need to send anything
    memset(&t, 0, sizeof(t));           //Zero out the transaction
    t.length = len * 8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;                 //Data
    t.user = (void*)1;                  //D/C needs to be set to 1
    ret = spi_device_transmit(spi, &t); //Transmit!
    assert(ret == ESP_OK);              //Should have had no issues.
}


void init_gpio(){
  gpio_config_t io_conf;

  io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  io_conf.pull_down_en = (gpio_pulldown_t)0;
  io_conf.pull_up_en = (gpio_pullup_t)1;

  gpio_config(&io_conf);


  // Setup HV Serial to Parallel Converters
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_LE, 1);
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_POL, 0);
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_BL, 1);
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_HV_DIS, 0);
}


void set_tubes(spi_device_handle_t spi_bus, uint8_t one, uint8_t two, uint8_t three, uint8_t four, uint8_t five, uint8_t six){
  typedef struct {
    uint8_t nc1:2;

    uint8_t six_zero:1;
    uint8_t six_nine:1;
    uint8_t six_eight:1;
    uint8_t six_seven:1;
    uint8_t six_six:1;
    uint8_t six_five:1;
    uint8_t six_four:1;
    uint8_t six_three:1;
    uint8_t six_two:1;
    uint8_t six_one:1;

    uint8_t five_zero:1;
    uint8_t five_nine:1;
    uint8_t five_eight:1;
    uint8_t five_seven:1;
    uint8_t five_six:1;
    uint8_t five_five:1;
    uint8_t five_four:1;
    uint8_t five_three:1;
    uint8_t five_two:1;
    uint8_t five_one:1;

    uint8_t four_zero:1;
    uint8_t four_nine:1;
    uint8_t four_eight:1;
    uint8_t four_seven:1;
    uint8_t four_six:1;
    uint8_t four_five:1;
    uint8_t four_four:1;
    uint8_t four_three:1;
    uint8_t four_two:1;
    uint8_t four_one:1;

    uint8_t nc2:2;

    uint8_t three_zero:1;
    uint8_t three_nine:1;
    uint8_t three_eight:1;
    uint8_t three_seven:1;
    uint8_t three_six:1;
    uint8_t three_five:1;
    uint8_t three_four:1;
    uint8_t three_three:1;
    uint8_t three_two:1;
    uint8_t three_one:1;

    uint8_t two_zero:1;
    uint8_t two_nine:1;
    uint8_t two_eight:1;
    uint8_t two_seven:1;
    uint8_t two_six:1;
    uint8_t two_five:1;
    uint8_t two_four:1;
    uint8_t two_three:1;
    uint8_t two_two:1;
    uint8_t two_one:1;

    uint8_t one_zero:1;
    uint8_t one_nine:1;
    uint8_t one_eight:1;
    uint8_t one_seven:1;
    uint8_t one_six:1;
    uint8_t one_five:1;
    uint8_t one_four:1;
    uint8_t one_three:1;
    uint8_t one_two:1;
    uint8_t one_one:1;
  } __attribute__((packed))cathode_enables_t;

  cathode_enables_t cathode_enables;

  memset(&cathode_enables, 0xFF, sizeof(cathode_enables_t));

  switch(one) {
    case 0:
      cathode_enables.one_zero = 0;
      break;
    case 1:
      cathode_enables.one_one = 0;
      break;
    case 2:
      cathode_enables.one_two = 0;
      break;
    case 3:
      cathode_enables.one_three = 0;
      break;
    case 4:
      cathode_enables.one_four = 0;
      break;
    case 5:
      cathode_enables.one_five = 0;
      break;
    case 6:
      cathode_enables.one_six = 0;
      break;
    case 7:
      cathode_enables.one_seven = 0;
      break;
    case 8:
      cathode_enables.one_eight = 0;
      break;
    case 9:
      cathode_enables.one_nine = 0;
      break;
    default:
      break;
  }

  switch(two) {
    case 0:
      cathode_enables.two_zero = 0;
      break;
    case 1:
      cathode_enables.two_one = 0;
      break;
    case 2:
      cathode_enables.two_two = 0;
      break;
    case 3:
      cathode_enables.two_three = 0;
      break;
    case 4:
      cathode_enables.two_four = 0;
      break;
    case 5:
      cathode_enables.two_five = 0;
      break;
    case 6:
      cathode_enables.two_six = 0;
      break;
    case 7:
      cathode_enables.two_seven = 0;
      break;
    case 8:
      cathode_enables.two_eight = 0;
      break;
    case 9:
      cathode_enables.two_nine = 0;
      break;
    default:
      break;
  }

  switch(three) {
    case 0:
      cathode_enables.three_zero = 0;
      break;
    case 1:
      cathode_enables.three_one = 0;
      break;
    case 2:
      cathode_enables.three_two = 0;
      break;
    case 3:
      cathode_enables.three_three = 0;
      break;
    case 4:
      cathode_enables.three_four = 0;
      break;
    case 5:
      cathode_enables.three_five = 0;
      break;
    case 6:
      cathode_enables.three_six = 0;
      break;
    case 7:
      cathode_enables.three_seven = 0;
      break;
    case 8:
      cathode_enables.three_eight = 0;
      break;
    case 9:
      cathode_enables.three_nine = 0;
      break;
    default:
      break;
  }

  switch(four) {
    case 0:
      cathode_enables.four_zero = 0;
      break;
    case 1:
      cathode_enables.four_one = 0;
      break;
    case 2:
      cathode_enables.four_two = 0;
      break;
    case 3:
      cathode_enables.four_three = 0;
      break;
    case 4:
      cathode_enables.four_four = 0;
      break;
    case 5:
      cathode_enables.four_five = 0;
      break;
    case 6:
      cathode_enables.four_six = 0;
      break;
    case 7:
      cathode_enables.four_seven = 0;
      break;
    case 8:
      cathode_enables.four_eight = 0;
      break;
    case 9:
      cathode_enables.four_nine = 0;
      break;
    default:
      break;
  }

  switch(five) {
    case 0:
      cathode_enables.five_zero = 0;
      break;
    case 1:
      cathode_enables.five_one = 0;
      break;
    case 2:
      cathode_enables.five_two = 0;
      break;
    case 3:
      cathode_enables.five_three = 0;
      break;
    case 4:
      cathode_enables.five_four = 0;
      break;
    case 5:
      cathode_enables.five_five = 0;
      break;
    case 6:
      cathode_enables.five_six = 0;
      break;
    case 7:
      cathode_enables.five_seven = 0;
      break;
    case 8:
      cathode_enables.five_eight = 0;
      break;
    case 9:
      cathode_enables.five_nine = 0;
      break;
    default:
      break;
  }

  switch(six) {
    case 0:
      cathode_enables.six_zero = 0;
      break;
    case 1:
      cathode_enables.six_one = 0;
      break;
    case 2:
      cathode_enables.six_two = 0;
      break;
    case 3:
      cathode_enables.six_three = 0;
      break;
    case 4:
      cathode_enables.six_four = 0;
      break;
    case 5:
      cathode_enables.six_five = 0;
      break;
    case 6:
      cathode_enables.six_six = 0;
      break;
    case 7:
      cathode_enables.six_seven = 0;
      break;
    case 8:
      cathode_enables.six_eight = 0;
      break;
    case 9:
      cathode_enables.six_nine = 0;
      break;
    default:
      break;
  }

  cathode_enables.nc1 = 0;
  cathode_enables.nc2 = 0;

  send_cathodes(spi_bus, (const uint8_t*)&cathode_enables, sizeof(cathode_enables_t));

  // gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_LE, 0);
  // for(int i = 0; i < 100;) { i++; }
  // gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_LE, 1);
};

extern "C" {
	void app_main(void);
}
void app_main(void) {
  init_gpio();

  esp_err_t ret;
  spi_device_handle_t spi;
  spi_bus_config_t buscfg = {
    mosi_io_num: SPI_MOSI,
    miso_io_num: -1,
    sclk_io_num: SPI_SCLK,
    quadwp_io_num: -1,
    quadhd_io_num: -1,
    max_transfer_sz: 128,
    flags: 0,
    intr_flags: 0
  };
  spi_device_interface_config_t devcfg = {
    command_bits: 0,
    address_bits: 0,
    dummy_bits: 0,

    mode: 0,                                //SPI mode 1 Polarity 0 Phase 1

    duty_cycle_pos: 0,
    cs_ena_pretrans: 0,
    cs_ena_posttrans: 0,

    clock_speed_hz: 1000 * 1000,           //Clock out at 1 MHz

    input_delay_ns: 0,

    spics_io_num: -1,               //CS pin

    flags: SPI_DEVICE_BIT_LSBFIRST,

    queue_size: 7,                          //We want to be able to queue 7 transactions at a time

    pre_cb: NULL,
    post_cb: NULL
  };

  ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  ESP_ERROR_CHECK(ret);

  ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);

  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_POL, 0);
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_BL, 0);
  gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_LE, 0);

  uint8_t counter = 0;

  while(1){
    vTaskDelay(100);
    ESP_LOGI("Loop", "Sending commands to converters...");
    if(counter > 9) { counter = 0; }
    set_tubes(spi, counter, counter, counter, counter, counter, counter);
    counter++;
  }
}
