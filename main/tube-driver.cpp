#include "tube-driver.hpp"
#include <esp_log.h>
#include <esp_err.h>
#include <string.h>
#include <freertos/task.h>

TubeDriver::TubeDriver(uint8_t mosi_pin, uint8_t sclk_pin, uint8_t _le_pin, uint8_t _pol_pin, uint8_t _blank_pin, uint8_t _hv_dis_pin) :
                       le_pin(_le_pin), pol_pin(_pol_pin), blank_pin(_blank_pin), hv_dis_pin(_hv_dis_pin) {
  gpio_config_t io_conf;

  // Configure GPIO
  io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = ((1ULL << le_pin) | (1ULL << pol_pin) | (1ULL << blank_pin) | (1ULL << hv_dis_pin));
  io_conf.pull_down_en = (gpio_pulldown_t)0;
  io_conf.pull_up_en = (gpio_pullup_t)1;

  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Put GPIOs into known states
  gpio_set_level((gpio_num_t)pol_pin, 1);
  gpio_set_level((gpio_num_t)blank_pin, 1);
  gpio_set_level((gpio_num_t)le_pin, 0);
  gpio_set_level((gpio_num_t)hv_dis_pin, 1);

  // Configure SPI
  spi_bus_config_t buscfg = {
    mosi_io_num: mosi_pin,
    miso_io_num: -1,
    sclk_io_num: sclk_pin,
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

    mode: 1,                                //SPI mode 1 Polarity 0 Phase 1

    duty_cycle_pos: 0,
    cs_ena_pretrans: 0,
    cs_ena_posttrans: 0,

    clock_speed_hz: 15000 * 1000,           // Clock out at 1 MHz; this is max clock for rev 1.0 of board

    input_delay_ns: 0,

    spics_io_num: -1,               //CS pin

    flags: SPI_DEVICE_BIT_LSBFIRST,

    queue_size: 7,                          //We want to be able to queue 7 transactions at a time

    pre_cb: NULL,
    post_cb: NULL
  };

  ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
  ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
}


void TubeDriver::set_tubes(uint8_t one, uint8_t two, uint8_t three, uint8_t four, uint8_t five, uint8_t six){
  typedef struct {
    uint8_t nc1:2;
    uint16_t six:10;
    uint16_t five:10;
    uint16_t four:10;

    uint8_t nc2:2;
    uint16_t three:10;
    uint16_t two:10;
    uint16_t one:10;
  } __attribute__((packed))cathode_enables_t;

  cathode_enables_t cathode_enables;

  memset(&cathode_enables, 0x00, sizeof(cathode_enables_t));

  cathode_enables.one = (0x0200 >> one);
  cathode_enables.two = (0x0200 >> two);
  cathode_enables.three = (0x0200 >> three);
  cathode_enables.four = (0x0200 >> four);
  cathode_enables.five = (0x0200 >> five);
  cathode_enables.six = (0x0200 >> six);

  cathode_enables.nc1 = 0;
  cathode_enables.nc2 = 0;

  send_cathodes((const uint8_t*)&cathode_enables, sizeof(cathode_enables_t));

  gpio_set_level((gpio_num_t)le_pin, 1);
  vTaskDelay(1);
  gpio_set_level((gpio_num_t)le_pin, 0);
};


void TubeDriver::send_cathodes(const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if(len == 0) return;                //no need to send anything
    memset(&t, 0, sizeof(t));           //Zero out the transaction
    t.length = len * 8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;                 //Data
    t.user = (void*)1;                  //D/C needs to be set to 1
    ret = spi_device_transmit(spi, &t); //Transmit!
    if( ret != ESP_OK){
      ESP_LOGI("I2C", "Error message: %s", esp_err_to_name(ret));
    }
}
