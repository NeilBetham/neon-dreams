#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_int_wdt.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/apps/sntp.h>
#include <nvs_flash.h>
#include <sodium.h>
#include <sys/time.h>
#include <time.h>


#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <esp_event_loop.h>

#include "config.hpp"

#include "mongoose.h"
#include "sdkconfig.h"

#include "tube-driver.hpp"
#include "rtc-driver.hpp"
#include "tube-manager.hpp"


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
TubeManager tm(tubes);

EventGroupHandle_t wifi_event_group;


esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      xEventGroupSetBits(wifi_event_group, BIT0);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      /* This is a workaround as ESP32 WiFi libs don't currently
         auto-reassociate. */
      esp_wifi_connect();
      xEventGroupClearBits(wifi_event_group, BIT0);
      break;
    default:
      break;
  }
  return ESP_OK;
}


void config_wifi() {
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
  strcpy((char*)wifi_config.sta.password, WIFI_PASS);
  ESP_LOGI("WiFi", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}


void configure_clock(struct tm& time_info) {
  rtc.set_clock(
    time_info.tm_sec,
    time_info.tm_min,
    time_info.tm_hour,
    time_info.tm_wday,
    time_info.tm_mday,
    time_info.tm_mon,
    time_info.tm_year
  );
  rtc.sync();
}

void set_tubes() {
  rtc.sync();
  tm.set_digits(
    rtc.get_hour() / 10, rtc.get_hour() % 10,
    rtc.get_min() / 10, rtc.get_min() % 10,
    rtc.get_sec() / 10, rtc.get_sec() % 10
  );
}


struct tm get_ntp_time() {
  ESP_LOGI("NTP", "Initializing SNTP");
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char*)"pool.ntp.org");
  sntp_init();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {};
  int retry = 0;
  const int retry_count = 10;
  while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
      vTaskDelay(100);
      time(&now);
      localtime_r(&now, &timeinfo);
  }
  ESP_LOGI("NTP", "Clock synced");

  return timeinfo;
}


extern "C" {
	void app_main(void);
}
void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  config_wifi();
  xEventGroupWaitBits(wifi_event_group, BIT0, false, true, portMAX_DELAY);

  struct tm current_time = get_ntp_time();

  configure_clock(current_time);
  tubes.enable_hv();
  uint8_t loop_count = 0;
  while(1) {
    vTaskDelay(10);
    tm.tick_10ms();
    if(loop_count % 10 == 0){
      set_tubes();
      loop_count = 0;
    }
  }
}
