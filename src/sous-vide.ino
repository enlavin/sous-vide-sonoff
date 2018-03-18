#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <stdio.h>
#include <strings.h>
#include <math.h>
#include "secrets.h"
#include "relay.h"

#define RELAY_PIN 12
#define LED_PIN 13

void mqtt_callback(char* topic, byte* payload, unsigned int length);

#define CHECK_WIFI_INTERVAL 1000
#define SAFETY_INTERVAL 120000

#define MQTT_SENT_TEMP_INTERVAL 1000
#define MQTT_CLIENT_NAME "sous-vide-sonoff"
#define MQTT_TOPIC_TEMP "sous-vide/current_temp"
#define MQTT_TOPIC_RELAY_ACTIVE "sous-vide/output_relay"
#define MQTT_TOPIC_OUTPUT "sous-vide/output"
#define MQTT_TOPIC_SETPOINT "sous-vide/setpoint_temp"
#define MQTT_TOPIC_WINDOW_SIZE "sous-vide/window_size"
#define MQTT_TOPIC_PROBE_ERROR "sous-vide/probe_error"
#define MQTT_TOPIC_KP "sous-vide/kp"
#define MQTT_TOPIC_KD "sous-vide/kd"
#define MQTT_TOPIC_KI "sous-vide/ki"
#define MQTT_TOPIC_TUNING "sous-vide/autotune"
#define MQTT_TOPIC_AUTOTUNE_KP "sous-vide/autotune_kp"
#define MQTT_TOPIC_AUTOTUNE_KD "sous-vide/autotune_kd"
#define MQTT_TOPIC_AUTOTUNE_KI "sous-vide/autotune_ki"

#define PID_WINDOW_SIZE 20000

#define ONE_WIRE_BUS_PIN 14
#define TEMP_PROBE_ERROR_THRESHOLD 20
#define PROBE_INIT_TEMP -2000
#define PROBE_TEMP_INTERVAL 2000

WiFiClient wifi_client;
PubSubClient mqtt_client(MQTT_SERVER, 1883, mqtt_callback, wifi_client);
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress temp_sensor;

double pid_setpoint = 62, pid_input = PROBE_INIT_TEMP, pid_output;
double pid_kp = 2500, pid_ki = 3, pid_kd = 0;
int window_size = PID_WINDOW_SIZE;
unsigned long window_start_time;
PID cooker_pid(&pid_input, &pid_output, &pid_setpoint, pid_kp, pid_ki, pid_kd, P_ON_M, DIRECT);

// autotune support
PID_ATune cooker_pid_autotune(&pid_input, &pid_output);
bool pid_tuning = false;
byte autotune_mode_remember = 2;
double autotune_step = 500, autotune_noise = 1, autotune_startvalue = 100;
unsigned int autotune_lookback = 20;

bool probe_error = false;
double last_temp = PROBE_INIT_TEMP;
unsigned long last_temp_sent;
double temp_temp;
unsigned int probe_errors = 0;
unsigned int last_probed_temp = 0;


char *float_to_str(double f)
{
  // Not thread safe!
  static char buff[10];
  snprintf (buff, sizeof(buff), "%3.4f", f);
  return buff;
}

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  static char msg[32];
  if (!strcmp(topic, MQTT_TOPIC_SETPOINT))
  {
    strncpy(msg, (char *)payload, length);
    msg[length] = 0;

    float new_setpoint;
    new_setpoint = atof(msg);
    pid_setpoint = new_setpoint;
    Serial.print("New setpoint: ");
    Serial.println(msg);
  }
  else if (!strcmp(topic, MQTT_TOPIC_KP))
  {
    strncpy(msg, (char *)payload, length);
    msg[length] = 0;

    float new_kp = atof(msg);
    pid_kp = new_kp;
    cooker_pid.SetTunings(pid_kp, pid_ki, pid_kd);
    Serial.print("New Kp: ");
    Serial.println(msg);
  }
  else if (!strcmp(topic, MQTT_TOPIC_KD))
  {
    strncpy(msg, (char *)payload, length);
    msg[length] = 0;

    float new_kd = atof(msg);
    pid_kd = new_kd;
    cooker_pid.SetTunings(pid_kp, pid_ki, pid_kd);
    Serial.print("New Kd: ");
    Serial.println(msg);
  }
  else if (!strcmp(topic, MQTT_TOPIC_KI))
  {
    strncpy(msg, (char *)payload, length);
    msg[length] = 0;

    float new_ki = atof(msg);
    pid_ki = new_ki;
    cooker_pid.SetTunings(pid_kp, pid_ki, pid_kd);
    Serial.print("New Ki: ");
    Serial.println(msg);
  }
  else if (!strcmp(topic, MQTT_TOPIC_TUNING))
  {
    if (!strncmp((char*)payload, "ON", 2) || !strncmp((char*)payload, "1", 1))
    {
      Serial.print("Start tuning");
      Serial.println(msg);

      pid_tuning = false;
      change_autotune();
      pid_tuning = true;
    }
    else
    {
      Serial.print("Stop tuning");
      Serial.println(msg);
      autotune_helper(false);
    }
  }
  else if (!strcmp(topic, MQTT_TOPIC_WINDOW_SIZE))
  {
    strncpy(msg, (char *)payload, length);
    msg[length] = 0;

    float new_window_size = atof(msg);
    window_size = new_window_size;
    window_start_time = millis();
    cooker_pid.SetOutputLimits(0, window_size);
    Serial.print("New window size: ");
    Serial.println(msg);
  }
}

bool connect_wifi()
{
  if (WiFi.status() == WL_CONNECTED)
    return true;

  while (WL_CONNECTED != WiFi.status())
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  return true;
}

void start_wifi()
{
  WiFi.disconnect();
  WiFi.setAutoConnect(true);
  WiFi.hostname("sous-vide-sonoff");
  //WiFi.config(ip, gw, net, dns);  // Set static IP
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool connect_mqtt_client()
{
  Serial.write("Connecting to MQTT broker...");
  bool result = mqtt_client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD);
  Serial.write(" Done.");
  return result;
}

bool connect_all_the_things()
{
  if (connect_wifi())
  {
    if (!mqtt_client.connected())
    {
      bool connected = connect_mqtt_client();
      if (connected)
      {
        subscribe_mqtt_topics();
      }

      return connected;
    }
    return true;
  }
  return false;
}

void autotune_helper(boolean start)
{
  if (start)
    autotune_mode_remember = cooker_pid.GetMode();
  else
    cooker_pid.SetMode(autotune_mode_remember);
}

void change_autotune()
{
  if (!pid_tuning)
  {
    //Set the output to the desired starting frequency.
    pid_output = autotune_startvalue;
    cooker_pid_autotune.SetNoiseBand(autotune_noise);
    cooker_pid_autotune.SetOutputStep(autotune_step);
    cooker_pid_autotune.SetLookbackSec((int)autotune_lookback);
    autotune_helper(true);
    pid_tuning = true;

    //mqtt_client.publish(MQTT_TOPIC_TUNING, "ON");
  }
  else
  {
    cooker_pid_autotune.Cancel();
    pid_tuning = false;
    autotune_helper(false);
    mqtt_client.publish(MQTT_TOPIC_TUNING, "OFF");
  }
}

void subscribe_mqtt_topics()
{
  mqtt_client.subscribe(MQTT_TOPIC_SETPOINT);
  mqtt_client.subscribe(MQTT_TOPIC_KP);
  mqtt_client.subscribe(MQTT_TOPIC_KI);
  mqtt_client.subscribe(MQTT_TOPIC_KD);
  mqtt_client.subscribe(MQTT_TOPIC_TUNING);
}

unsigned long last_autotune_sent;
unsigned long last_check_connectivity;

Relay relay(RELAY_PIN, LED_PIN);

// the setup function runs once when you press reset or power the board
void setup() 
{
  // initialize digital pin LED_PIN as an output.
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  relay.off();

  // temp probe settings (only 1 ds18b20 probe supported)
  sensors.begin();
  //sensors.getAddress(temp_sensor, 0);
  //sensors.setResolution(temp_sensor, 12);
  //sensors.setWaitForConversion(true);

  // PID settings
  pid_tuning = false;
  window_start_time = millis();
  cooker_pid.SetSampleTime(1000);
  cooker_pid.SetOutputLimits(0, window_size);
  cooker_pid.SetMode(AUTOMATIC);

  Serial.begin(115200);
  start_wifi();
  connect_all_the_things();

  // publish set points
  mqtt_client.publish(MQTT_TOPIC_SETPOINT, float_to_str(pid_setpoint));
  mqtt_client.publish(MQTT_TOPIC_KP, float_to_str(pid_kp));
  mqtt_client.publish(MQTT_TOPIC_KI, float_to_str(pid_ki));
  mqtt_client.publish(MQTT_TOPIC_KD, float_to_str(pid_kd));
  mqtt_client.publish(MQTT_TOPIC_PROBE_ERROR, "OFF");

  relay.off();

  last_autotune_sent = millis();
  last_check_connectivity = millis();

  subscribe_mqtt_topics();
}

void loop() 
{
  // Don't probe temp too fast or it will return errors  
  if (millis() - last_probed_temp > PROBE_TEMP_INTERVAL)
  {
    sensors.requestTemperatures();
    temp_temp = (double)sensors.getTempCByIndex(0);

    // Temp does not change very fast. Sudden changes usually are caused by a probe error
    if (temp_temp <= -127.0 
    || (pid_input != PROBE_INIT_TEMP && fabs(pid_input - temp_temp) > TEMP_PROBE_ERROR_THRESHOLD))
    {
        probe_errors++;
        Serial.write("Probe error\r\n");

        // switch off the relay if the probe measurements are unreliable for too long
        if (millis() - last_temp_sent > SAFETY_INTERVAL)
        {
          if (relay.off())
            mqtt_client.publish(MQTT_TOPIC_RELAY_ACTIVE, "OFF");
          probe_error = true;
          mqtt_client.publish(MQTT_TOPIC_PROBE_ERROR, "ON");
          Serial.write("Safety measures triggered. Output OFF\r\n");
        }

        delay(100);
        
        return;
    }

    last_probed_temp = millis();
  }


  // temp probe is working again
  if (probe_error)
  {
    probe_error = false;
    mqtt_client.publish(MQTT_TOPIC_PROBE_ERROR, "OFF");
  }

  pid_input = temp_temp; 

  if (!pid_tuning)
  {
    cooker_pid.Compute();
  }
  else
  {
    if (millis() - last_autotune_sent > 30000)
    {
      last_autotune_sent = millis();
      mqtt_client.publish(MQTT_TOPIC_AUTOTUNE_KP, float_to_str(cooker_pid_autotune.GetKp()));
      mqtt_client.publish(MQTT_TOPIC_AUTOTUNE_KI, float_to_str(cooker_pid_autotune.GetKi()));
      mqtt_client.publish(MQTT_TOPIC_AUTOTUNE_KD, float_to_str(cooker_pid_autotune.GetKd()));
    }

    if (cooker_pid_autotune.Runtime())
    {
      pid_tuning = false;
      mqtt_client.publish(MQTT_TOPIC_TUNING, "OFF");
    }

    if (!pid_tuning)
    { //we're done, set the tuning parameters
      pid_kp = cooker_pid_autotune.GetKp();
      pid_ki = cooker_pid_autotune.GetKi();
      pid_kd = cooker_pid_autotune.GetKd();
      mqtt_client.publish(MQTT_TOPIC_KP, float_to_str(pid_kp));
      mqtt_client.publish(MQTT_TOPIC_KI, float_to_str(pid_ki));
      mqtt_client.publish(MQTT_TOPIC_KD, float_to_str(pid_kd));

      cooker_pid.SetTunings(pid_kp, pid_ki, pid_kd);
      cooker_pid.SetMode(autotune_mode_remember);
      autotune_helper(false);
    }
  }

  // reconnect if the connection to the MQTT broker is lost
  if (!mqtt_client.loop())
    connect_all_the_things();

  // publish the temp every couple of seconds
  if (millis() - last_temp_sent > MQTT_SENT_TEMP_INTERVAL)
  {
    Serial.write("\r\n");
    Serial.write("Interval: ");
    Serial.println(millis() - last_temp_sent);

    Serial.write("Millis: ");
    Serial.println(millis());

    last_temp = pid_input;
    
    char *temp_fmt = float_to_str(pid_input);
    Serial.write("Temp: ");
    Serial.println(temp_fmt);
    mqtt_client.publish(MQTT_TOPIC_TEMP, temp_fmt);

    Serial.write("Output: ");
    Serial.println(pid_output);
    mqtt_client.publish(MQTT_TOPIC_OUTPUT, float_to_str(pid_output));

    last_temp_sent = millis();
  }

  // PID time proportional output
  if (millis() - window_start_time > window_size)
  {
    window_start_time += window_size;
  }

  if (pid_output > millis() - window_start_time)
  {
    if (relay.on())
    {
      Serial.write("Output ");
      mqtt_client.publish(MQTT_TOPIC_RELAY_ACTIVE, "ON");
      Serial.write("ON\r\n");
    }
  }
  else
  {
    if (relay.off())
    {
      Serial.write("Output ");
      mqtt_client.publish(MQTT_TOPIC_RELAY_ACTIVE, "OFF");
      Serial.write("OFF\r\n");
    }
  }
}
