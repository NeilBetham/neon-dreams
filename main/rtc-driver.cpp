#include "rtc-driver.hpp"
#include <esp_log.h>
#include <esp_err.h>


RTCDriver::RTCDriver(uint8_t esp_i2c_port_num, uint8_t sda, uint8_t scl) {
  rtc_port = (i2c_port_t)esp_i2c_port_num;

  i2c_config_t rtc_comm_conf;
  rtc_comm_conf.mode = I2C_MODE_MASTER;
  rtc_comm_conf.sda_io_num = (gpio_num_t)sda;
  rtc_comm_conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
  rtc_comm_conf.scl_io_num = (gpio_num_t)scl;
  rtc_comm_conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
  rtc_comm_conf.master.clk_speed = 200000;  // 400kHz max

  i2c_param_config(rtc_port, &rtc_comm_conf);
  ESP_ERROR_CHECK(i2c_driver_install(rtc_port, I2C_MODE_MASTER, 0, 0, 0));
}

bool RTCDriver::sync() {
  uint8_t reg_status[7] = {0};

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, 0xD0 | 0x00, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, 0x00, (i2c_ack_type_t)true);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, 0xD0 | 0x01, (i2c_ack_type_t)true);
  i2c_master_read(cmd, (uint8_t*)&reg_status, 7, I2C_MASTER_LAST_NACK );
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(rtc_port, cmd, 50 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);

  if (ret != ESP_OK){
    ESP_LOGI("I2C", "Error message: %s", esp_err_to_name(ret));
    return false;
  }

  sec = (((reg_status[0] & 0x70) >> 4) * 10) + (0x0F & reg_status[0]);
  min = (((reg_status[1] & 0x70) >> 4) * 10) + (0x0F & reg_status[1]);
  hour = (((reg_status[2] & (twelve_hour_mode_enable ? 0x10 : 0x30)) >> 4) * 10) + (0x0F & reg_status[2]);
  dow = reg_status[3] & 0x07;
  date = (((reg_status[4] & 0x30) >> 4) * 10) + (0x0F & reg_status[4]);
  month = (((reg_status[5] & 0x10) >> 4) * 10) + (0x0F & reg_status[5]);
  year = ((((uint16_t)reg_status[6] & 0xF0) >> 4) * 10) + (0x0F & reg_status[6]);

  return true;
}


bool RTCDriver::set_clock(uint8_t new_sec, uint8_t new_min, uint8_t new_hour, uint8_t new_dow, uint8_t new_date, uint8_t new_month, uint16_t new_year) {
  uint8_t sec_reg   = 0x00 | ((new_sec / 10) << 4) | (new_sec % 10);
  uint8_t min_reg   = 0x00 | ((new_min / 10) << 4) | (new_min % 10);
  uint8_t hour_reg  = (twelve_hour_mode_enable & 0x01) << 6 | (new_hour / 10) << 4 | (new_hour % 10);
  uint8_t dow_reg   = new_dow & 0x03;
  uint8_t date_reg  = 0x00 | ((new_date / 10) << 4) | (new_date % 10);
  uint8_t month_reg = 0x00 | ((new_month / 10) << 4) | (new_month % 10);
  uint8_t year_reg  = 0x00 | ((new_year / 10) << 4) | (new_year % 10);

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, 0xD0 | 0x00, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, 0x00, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, sec_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, min_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, hour_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, dow_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, date_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, month_reg, (i2c_ack_type_t)true);
  i2c_master_write_byte(cmd, year_reg, (i2c_ack_type_t)true);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(rtc_port, cmd, 50 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);

  if (ret != ESP_OK){
    ESP_LOGI("I2C", "Error message: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}
