# Sous vide controller firmware for Sonoff Basic

I wanted a simple temperature controller to try sous vide cooking without breaking the bank. On Christmas I got some Sonoff Basic switches to automate a couple of lights at home and I decided they would be a great platform to run a temperature controller:

* Arduino compatible esp8266 (WiFi and all)
* Access to a GPIO pin for the temperature probe (after some [hardware modifications](https://github.com/arendst/Sonoff-Tasmota/wiki/Sonoff-Basic))
* Included relay to switch a heating element on/off
* Really cheap

## BOM

* Sonoff Basic (I got mine from Aliexpress)
* DS18B20 sumersible probe
* Rice cooker for the heating element
* USB 3.3V TTL serial converter to reprogram the Sonoff

The DS18B20 can work with 3.3V and it's heat resistant in the range of temperatures at which sous vide cooking usually take place. The accuracy of the probe is 0.5 and the resolution at 12 bits can be as much as 0.0625 degrees Celsius, enough for this project.

Hardware wise this project is quite simple: just connect the rice cooker to the Sonoff and set the cooker settings to max, if any.

To connect the temperature probe to the Sonoff I made a small hole to the plastic case and got a 5 cable header out for the 5 pins in the Sonoff motherboard. The probe goes to the GPIO14 pin.

## Software Requirements

The project has been generated using the [PlatformIO](https://platformio.org/) environment with Visual Studio Code. Building the project with this ide is a matter of opening the folder and hitting "Build". You probably can use a different IDE to generate the binaries if you want to, but this seemed easy enough for me.

The projects depend on a couple of libraries for Arduino, but PlatformIO takes care of retrieving them during the compilation:

* OneWire, DallasTemperature
* PID
* PubSubClient

## WiFi and MQTT

I did not want to spend too much time on the interface with the controller. So instead of using a LCD and some buttons, or even creating a web interface, I decided it would be more simple to just use MQTT to interact with the device. I already had a mosquitto server configured for some home automation experiments.

To configure the WiFi password and the MQTT credentials you need to copy the file `secrets.h.sample` to `secrets.h` and fill in the blanks.

The controller publishes the following topics on reboot, just after it connects to the WiFi:

* Temperature
* PID output
* Relay status
* PID Kp
* PID Ki
* PID Kd

And the these every couple of second:

* Temperature
* PID output
* Relay status

The controller also subscribes to some topics to configure the PID tuning parameters, that you can modify them using any MQTT client. I have been using [MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash) for Android, but you can use any other MQTT client.

## PID controller

Sous vide cooking requires very precise temperatures that need to be maintained for long periods of time (sometimes more than 24h). [PID control](https://en.wikipedia.org/wiki/PID_controller) is the way to go and I used a very well known [Arduino PID Library](https://github.com/br3ttb/Arduino-PID-Library) for this.

After some manual tuning of the PID parameters the temperature of the rice cooker is kept at the setpoint +/- 0.1 degrees Celsius. You will need to adjust yours to your environment (heating element, external temperature, ...). There are a lot of PID tuning resources on the internet. 
