#include "relay.h"

Relay::Relay(int relay_pin, int led_pin)
{
    _relay_pin = relay_pin;
    _led_pin = led_pin;
    _relay_status = false;
}

bool Relay::status()
{
    return _relay_status;
}

bool Relay::change_status(bool new_status)
{
    if (new_status == _relay_status)
      return false;

    if (new_status)
    {
      digitalWrite(_led_pin, LOW);
      digitalWrite(_relay_pin, HIGH);
    }
    else
    {
      digitalWrite(_led_pin, HIGH);
      digitalWrite(_relay_pin, LOW);
    }

    _relay_status = new_status;

    return true;
}

bool Relay::on()
{
    return change_status(true);
}

bool Relay::off()
{
    return change_status(false);
}