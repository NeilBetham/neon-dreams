#include "tube-manager.hpp"

#include <esp_log.h>
#include <sys/time.h>
#include <time.h>


TubeManager::TubeManager(TubeDriver& _td) : td(_td), poison_prev_int(30), poison_prev_dur(10) {};

void TubeManager::set_digits(uint8_t _one, uint8_t _two, uint8_t _three, uint8_t _four, uint8_t _five, uint8_t _six) {
  one = _one;
  two = _two;
  three = _three;
  four = _four;
  five = _five;
  six = _six;
}


void TubeManager::tick_10ms() {
  time_t now = 0;
  time(&now);

  if(poison_prev_active && ((now - poison_prev_start) < poison_prev_dur)) {
    td.set_tubes(
      one_index,
      (one_index + 1) % 10,
      (one_index + 2) % 10,
      (one_index + 3) % 10,
      (one_index + 4) % 10,
      (one_index + 5) % 10
    );
    one_index++;
    one_index %= 10;
  } else {
    td.set_tubes(one, two, three, four, five, six);
    poison_prev_active = false;
  }

  // Don't run prevention on startup
  if(now - 1200 > poison_prev_start){
    poison_prev_start = now;
    return;
  }

  // Posion prevention is needed, run now
  if(now > poison_prev_start + poison_prev_int){
    poison_prev_start = now;
    poison_prev_active = true;
    one_index = 0;
  }
}
