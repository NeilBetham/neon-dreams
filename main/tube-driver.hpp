#ifndef TUBE_DRIVER_HPP
#define TUBE_DRIVER_HPP

#include <stdint.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

class TubeDriver {
public:
  TubeDriver(uint8_t mosi_pin, uint8_t sclk_pin,
             uint8_t _le_pin, uint8_t _pol_pin, uint8_t _blank_pin, uint8_t _hv_dis_pin);
  ~TubeDriver() {};

  void set_tubes(int8_t one, int8_t two, int8_t three, int8_t four, int8_t five, int8_t six);

  void disable_hv() { gpio_set_level((gpio_num_t)hv_dis_pin, 1); };
  void enable_hv() { gpio_set_level((gpio_num_t)hv_dis_pin, 0); };
  bool hv_enabled() { return (bool)gpio_get_level((gpio_num_t)hv_dis_pin); }

private:
  spi_device_handle_t spi;
  spi_bus_config_t bus_config;

  uint8_t le_pin;
  uint8_t pol_pin;
  uint8_t blank_pin;
  uint8_t hv_dis_pin;

  void send_cathodes(const uint8_t* data, int len);
};


#endif // TUBE_DRIVER_HPP
