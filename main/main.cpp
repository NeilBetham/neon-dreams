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

#include "tube-driver.hpp"
#include "rtc-driver.hpp"


#define GPIO_OUTPUT_IO_LE        15
#define GPIO_OUTPUT_IO_POL       16
#define GPIO_OUTPUT_IO_BL        17
#define GPIO_OUTPUT_IO_HV_DIS    18

#define SPI_MOSI    13
#define SPI_SCLK    14

#define RTC_I2C_PORT I2C_NUM_0
#define RTC_I2C_SDA 27
#define RTC_I2C_SCL 26


RTCDriver rtc(RTC_I2C_PORT, RTC_I2C_SDA, RTC_I2C_SCL);
TubeDriver tubes(SPI_MOSI, SPI_SCLK, GPIO_OUTPUT_IO_LE, GPIO_OUTPUT_IO_POL, GPIO_OUTPUT_IO_BL, GPIO_OUTPUT_IO_HV_DIS);

void configure_clock() {
  rtc.set_clock(0, 0, 3, 1, 13, 4, 2019);
  rtc.sync();
}

void set_tubes() {
  rtc.sync();
  tubes.set_tubes(
    rtc.get_hour() / 10, rtc.get_hour() % 10,
    rtc.get_min() / 10, rtc.get_min() % 10,
    rtc.get_sec() / 10, rtc.get_sec() % 10
  );
}


extern "C" {
	void app_main(void);
}
void app_main(void) {
  configure_clock();
  tubes.enable_hv();
  while(1) {
    vTaskDelay(100);
    set_tubes();
  }
}
