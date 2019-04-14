#ifndef TUBE_MANAGER_HPP
#define TUBE_MANAGER_HPP

#include <stdint.h>

#include "tube-driver.hpp"

class TubeManager {
public:
  TubeManager(TubeDriver& td);

  void set_digits(uint8_t _one, uint8_t _two, uint8_t _three, uint8_t _four, uint8_t _five, uint8_t _six);
  void set_posion_prev_int(uint32_t new_poison_prev_int) { poison_prev_int = new_poison_prev_int; };
  void set_posion_prev_dur(uint32_t new_poison_prev_dur) { poison_prev_dur = new_poison_prev_dur; };
  void set_posion_prev_spd(uint8_t new_poison_prev_spd) { poison_prev_spd = new_poison_prev_spd; };
  void tick_10ms();

private:
  TubeDriver& td;

  uint32_t poison_prev_int;
  uint8_t poison_prev_dur;
  uint8_t poison_prev_spd;

  time_t poison_prev_start;
  bool poison_prev_active;
  uint8_t one_index;

  uint8_t one;
  uint8_t two;
  uint8_t three;
  uint8_t four;
  uint8_t five;
  uint8_t six;
};


#endif // TUBE_MANAGER_HPP
