#ifndef __RELAY_H__
#define __RELAY_H__

class Relay
{
  int _relay_pin;
  int _led_pin;

  bool _relay_status;

public:
  Relay(int relay_pin, int led_pin);
  bool status();
  bool change_status(bool new_status);
  bool on();
  bool off();
};

#endif