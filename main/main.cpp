#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_event.h>
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

#include "config.hpp"

#include "mongoose.h"
#include "sdkconfig.h"

#include "tube-driver.hpp"
#include "rtc-driver.hpp"
#include "tube-manager.hpp"

#include "rollkit.hpp"


#define GPIO_OUTPUT_IO_LE        15
#define GPIO_OUTPUT_IO_POL       16
#define GPIO_OUTPUT_IO_BL        17
#define GPIO_OUTPUT_IO_HV_DIS    18

#define SPI_MOSI    13
#define SPI_SCLK    14

#define RTC_I2C_PORT I2C_NUM_0
#define RTC_I2C_SDA 27
#define RTC_I2C_SCL 26

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


RTCDriver rtc(RTC_I2C_PORT, RTC_I2C_SDA, RTC_I2C_SCL);
TubeDriver tubes(SPI_MOSI, SPI_SCLK, GPIO_OUTPUT_IO_LE, GPIO_OUTPUT_IO_POL, GPIO_OUTPUT_IO_BL, GPIO_OUTPUT_IO_HV_DIS);
TubeManager tm(tubes);

rollkit::App rollkit_app;
rollkit::Accessory acc;
rollkit::Service acc_switch;
rollkit::Characteristic acc_switch_on_char;
rollkit::Characteristic acc_switch_name_char;

EventGroupHandle_t wifi_event_group;

void init_rollkit(const std::string& mac) {
  rollkit_app.init(ACC_NAME, ACC_MODEL, ACC_MANUFACTURER, ACC_FIRMWARE_REVISION, ACC_SETUP_CODE, mac);

  acc_switch_on_char = rollkit::Characteristic(
    APPL_CHAR_UUID_ON,
    [](nlohmann::json v){ if((bool)v.get<int>()) { tubes.enable_hv(); } else { tubes.disable_hv(); }},
    []() -> nlohmann::json { return tubes.hv_enabled(); },
    "bool",
    {"pr", "pw", "ev"}
  );
  acc_switch_name_char = rollkit::Characteristic(
    APPL_CHAR_UUID_NAME,
    [](nlohmann::json v){},
    []() -> nlohmann::json { ESP_LOGD("acc", "Switch Name Read"); return "Tubes"; },
    "string",
    {"pr"}
  );
  acc_switch = rollkit::Service(
    APPL_SRVC_UUID_SWITCH,
    false,
    true
  );
  acc_switch.register_characteristic(acc_switch_on_char);
  acc_switch.register_characteristic(acc_switch_name_char);
  acc.register_service(acc_switch);
  rollkit_app.register_accessory(acc);
  rollkit_app.start();
}


void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  static uint32_t s_retry_num = 0;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 1000) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI("main", "Retrying WiFi connection");
    } else {
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI("main","Failed to connect to AP");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI("main", "Recevied IP:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}


void config_wifi() {
  ESP_ERROR_CHECK(esp_netif_init());
  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  auto net_if = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_netif_set_hostname(net_if, "NixieClock"));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
  strcpy((char*)wifi_config.sta.password, WIFI_PASS);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_LOGI("WiFi", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI("WiFi", "Waiting for connection....");
  EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY
  );
  ESP_LOGI("WiFi", "Connected");
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


void init_ntp() {
  ESP_LOGI("NTP", "Initializing SNTP");
  setenv("TZ", "EST+5EDT,M3.2.0,M11.1.0", 1);
  tzset();
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
  ESP_LOGI("TIME", "Current Time: %i:%i:%i", (int)timeinfo.tm_hour, (int)timeinfo.tm_min, (int)timeinfo.tm_sec);
  ESP_LOGI("NTP", "Clock synced");
}


struct tm get_ntp_time() {
  time_t now = 0;
  struct tm timeinfo = {};
  time(&now);
  localtime_r(&now, &timeinfo);
  return timeinfo;
}

void update_rtc() {
  auto time = get_ntp_time();
  ESP_LOGI("NTP", "Time Delta Pre Update: HH:MM:SS - %2.2i:%2.2i:%2.2i", rtc.get_hour() - time.tm_hour, rtc.get_min() - time.tm_min, rtc.get_sec() - time.tm_sec);
  configure_clock(time);
}


extern "C" {
	void app_main(void);
}
void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  config_wifi();
  init_ntp();
  rtc.init();
  update_rtc();

  std::string mac_address(17, 0);
  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
  sprintf(&mac_address[0], "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  set_tubes();
  tubes.enable_hv();
  init_rollkit(mac_address);
  while(1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    tm.tick_10ms();

    static uint32_t set_tube_timer = 0;
    if(set_tube_timer % 10 == 0) {
      set_tubes();
      set_tube_timer = 0;
    }
    set_tube_timer++;


    static uint32_t sync_timer = 0;
    if(sync_timer % 3600000 == 0) {
      ESP_LOGI("NTP", "Updating RTC Clock");
      update_rtc();
      sync_timer = 0;
    }
    sync_timer++;
  }
}
