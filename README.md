# Sous vide controller firmware for Sonoff Basic

I wanted a simple temperature controller to try sous vide cooking without breaking the bank. There are some DIY projects around using commercial temperature controllers which end up quite cheap in the end, but I wanted to learn something new in the process.

Last Christmas I got a couple of Sonoff Basic switches to automate some lights at home and I decided they would be a great platform to run a custom temperature controller:

* Arduino compatible esp8266 (WiFi and all)
* Access to a GPIO pin for the temperature probe (after some [hardware modifications](https://github.com/arendst/Sonoff-Tasmota/wiki/Sonoff-Basic))
* Included relay to switch a heating element on/off
* Case included
* Really cheap

## BOM

* Sonoff Basic (I got mine from Aliexpress, about 5 euro)
* DS18B20 sumersible probe (3 euro)
* Rice cooker for the heating element (I had one already, 20 euro)
* USB 3.3V TTL serial converter to reprogram the Sonoff (1 euro)
* Electric supplies like cable for 10A, sockets (6 euro)

![rice cooker](https://i.imgur.com/kyidZef.jpg)

Total is about 30 euro. And considering that I already had most the components except for the Sonoff it ended up very cheap indeed. The electric supplies were more expensive than the Sonoff! Go figure...

The DS18B20 can work with 3.3V and it's heat resistant in the range of temperatures at which sous vide cooking usually take place. The accuracy of the probe is 0.5 and the resolution at 12 bits can be as much as 0.0625 degrees Celsius, enough for this project.

Hardware wise this project is quite simple: just connect the rice cooker to the Sonoff and set the cooker settings to max, if any. To connect the temperature probe to the Sonoff I made a small hole to the plastic case and got a 5 cable header out for the 5 pins in the Sonoff motherboard. The probe goes to the GPIO14 pin.

## Software Requirements

The project has been generated using the [PlatformIO](https://platformio.org/) environment with Visual Studio Code. Building the project with this ide is a matter of opening the folder and hitting "Build". You probably can use a different IDE to generate the binaries if you want to, but this seemed easy enough for me.

The project depends on a couple of libraries for Arduino, but PlatformIO takes care of retrieving them during the compilation:

* OneWire, DallasTemperature (temperature probe)
* PID (PID controller)
* PubSubClient (MQTT client)

## WiFi and MQTT

I did not want to spend too much time on the interface with the controller. So instead of using a LCD and some buttons, or even creating a web interface, I decided it would be more simple to just use MQTT to interact with the device. I already had a mosquitto server configured for some home automation experiments.

To configure the WiFi password and the MQTT credentials you need to copy the file `secrets.h.sample` to `secrets.h` and fill in the blanks.

The controller publishes the following topics on reboot, just after it connects to the WiFi:

| Topic  | Description ||
|-------|--------------|-----|
|sous-vide/current_temp| Temperature | Numeric |
|sous-vide/output| PID output |Numeric |
|sous-vide/output_relay| Relay status | ON/OFF|
|sous-vide/kp| PID Kp |Numeric|
|sous-vide/ki| PID Ki |Numeric|
|sous-vide/kd| PID Kd |Numeric|

These are sent every couple of second so you can capture them and use the data to monitor the process and generate charts:

| Topic  | Description ||
|-------|--------------|--|
|sous-vide/current_temp| Temperature |Numeric|
|sous-vide/output| PID output |Numeric|
|sous-vide/output_relay| Relay status | ON/OFF|

The controller also subscribes to some topics to configure the PID tuning parameters, that you can modify them using any MQTT client. I have been using [MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash) for Android, but you can use any other MQTT client. Take a look at the source code for the topic names.

![MQTT Dashboard](https://i.imgur.com/zJeSKcp.png)

If there is a probe error for a significant amount of time the controller switches off and it will send a notification

| Topic  | Description ||
|-------|--------------|--|
|sous-vide/probe_error| Probe error |ON/OFF|


## PID controller

Sous vide cooking requires very precise temperatures that need to be maintained for long periods of time (sometimes more than 24h). [PID control](https://en.wikipedia.org/wiki/PID_controller) is the way to go and I used a very well known [Arduino PID Library](https://github.com/br3ttb/Arduino-PID-Library) for this.

After some manual tuning of the PID parameters the temperature of the rice cooker is kept at the setpoint +/- 0.1 degrees Celsius. You will need to adjust yours to your environment (heating element, external temperature, ...). There are a lot of PID tuning resources on the internet. 

![Temperature curve](https://i.imgur.com/KihC6kN.png)
