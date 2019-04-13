#ifndef RTC_DRIVER_HPP
#define RTC_DRIVER_HPP

#include <stdint.h>
#include <driver/i2c.h>


class RTCDriver {
public:
  RTCDriver(uint8_t esp_i2c_port_num, uint8_t sda_pin, uint8_t scl_pin);
  ~RTCDriver() {};

  bool set_clock(uint8_t new_sec, uint8_t new_min, uint8_t new_hour,
                 uint8_t new_dow, uint8_t new_date, uint8_t new_month,
                 uint16_t new_year);
  bool sync();

  uint8_t get_sec() { return sec; };
  uint8_t get_min() { return min; };
  uint8_t get_hour() { return hour; };
  uint8_t get_dow() { return dow; };
  uint8_t get_date() { return date; };
  uint8_t get_month() { return month; };
  uint16_t get_year() { return year; };

private:
  i2c_port_t rtc_port;

  bool twelve_hour_mode_enable;

  uint8_t sec;
  uint8_t min;
  uint8_t hour;
  uint8_t dow;
  uint8_t date;
  uint8_t month;
  uint8_t year;
};

#endif // RTC_DRIVER_HPP
