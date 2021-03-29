#include "tube-manager.hpp"

#include <esp_log.h>
#include <sys/time.h>
#include <time.h>


TubeManager::TubeManager(TubeDriver& _td) : td(_td), poison_prev_int(300), poison_prev_dur(10) {};

void TubeManager::set_digits(uint8_t _one, uint8_t _two, uint8_t _three, uint8_t _four, uint8_t _five, uint8_t _six) {
  one = _one;
  two = _two;
  three = _three;
  four = _four;
  five = _five;
  six = _six;

  digits_set = true;
}


void TubeManager::tick_10ms() {
  if(!digits_set) {
    if(scan_count % 10 == 0) {
      if(scan_increment) {
        scan_pos++;
      } else {
        scan_pos--;
      }

      if(scan_pos >= 6) {
        scan_increment = false;
        scan_pos = 6;
      } else if(scan_pos <= 1) {
        scan_increment = true;
        scan_pos = 1;
      }

      // Update the tubes
      one = (0x02 >> scan_pos) & 0x01;
      two = (0x04 >> scan_pos) & 0x01;
      three = (0x08 >> scan_pos) & 0x01;
      four = (0x10 >> scan_pos) & 0x01;
      five = (0x20 >> scan_pos) & 0x01;
      six = (0x40 >> scan_pos) & 0x01;

      one = one > 0 ? one : -1;
      two = two > 0 ? two : -1;
      three = three > 0 ? three : -1;
      four = four > 0 ? four : -1;
      five = five > 0 ? five : -1;
      six = six > 0 ? six : -1;
    }

    scan_count++;

    td.set_tubes(one, two, three, four, five, six);
    return;
  }

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
