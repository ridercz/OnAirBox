/********************************************************************************************************************
 * On-Air Indicator Box                                                                                             *
 * ---------------------------------------------------------------------------------------------------------------- *
 * Copyright (c) Michal Altair Valasek, 2024 | www.rider.cz | github.com/ridercz                                    *
 * Licensed under terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.     *
 * ---------------------------------------------------------------------------------------------------------------- *
 * This sketch demonstrates how to create an "ON-AIR" box that can be used to signalize that a person is currently  *
 * streaming or recording using a LED.                                                                              *
 * This firmware is for both MASTER and SLAVE devices:                                                              *
 * - SLAVE devices receive messages from MQTT server and turn their LED on or off accordingly.                      *
 * - MASTER has a locking button in addition. It signalizes the ON-AIR or OFF-AIR status by sending MQTT messages.  *
 * ---------------------------------------------------------------------------------------------------------------- *
 * Wiring:                                                                                                          *
 * - Connect button between pin IO33 and GND (master only).                                                         *
 * - Connect LED between pin IO2 (built-in LED) and GND.                                                            *
 ********************************************************************************************************************/

#define VERSION "OnAirBox/2.0.0"

/* Libraries *******************************************************************************************************/

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

/* Configuration - change to fit your needs *************************************************************************/

// WiFi connection options
#define WIFI_SSID "boxlab.lazyhorse.net"
#define WIFI_PASS "IHaveHorsePower!"

// MQTT connection options
#define MQTT_SERVER "mqtt.boxlab.lazyhorse.net"
#define MQTT_PORT 8883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_SERVER_TLS
#define MQTT_TOPIC_STATUS "onair/status"
#define MQTT_TOPIC_ARRIVE "onair/arrive"
#define MQTT_TOPIC_DEPART "onair/depart"
#define MQTT_RECONNECT_DELAY 1000

/* Internal configuration - do not change unless you know what you are doing *****************************************/

// Operational parameters
#define LED_PIN LED_BUILTIN      // LED pin - use built-in LED
#define LED_INTERVAL 1000        // ms; LED blink interval
#define LED_TTL 30000            // ms; repeat interval for "on air" messages
#define LED_TIMEOUT 70000        // ms; timeout after which the LED is turned off, if no message is received, must be greater than LED_TTL
#define BUTTON_PIN 33            // Button pin
#define BUTTON_DEBOUNCE 50       // ms; button debounce time
#define REBOOT_INTERVAL 97200000 // ms; preventive reboot interval (27 hours)
#define WIFI_TIMEOUT 60000       // ms; device will reboot when it cannot connect to WiFi for this time

/* Global variables *************************************************************************************************/
#ifdef MQTT_SERVER_TLS
WiFiClientSecure wifiClient; // WiFi client with TLS
#else
WiFiClient wifiClient; // WiFi client without TLS
#endif
PubSubClient mqttClient(wifiClient);   // MQTT client instance
unsigned long lastMessageReceived = 0; // Last message received millis (used to detect mqtt timeout)
unsigned long lastLedToggle = 0;       // Last LED toggle millis (used to blink LED)
unsigned long lastMessageSent = 0;     // Last message sent millis (used to prevent mqtt timeout)
unsigned long lastWifiConnection = 0;  // Last WiFi connection start millis (used to detect connection timeout)
bool isOnAir = false;                  // On-Air status
bool lastLedState = false;             // Last LED blink state (used to toggle LED)
bool lastButtonState = false;          // Last button press state (used to toggle state)
bool firstWiFiConnection = true;       // First WiFi connection flag
bool firstMqttConnection = true;       // First MQTT connection flag

/* Helper methods ***************************************************************************************************/

// This method ensures that the device is connected to WiFi
void ensureWifiConnected()
{
  // If we are already connected, do nothing
  if (WiFi.status() == WL_CONNECTED)
    return;

  // Turn LED ON
  digitalWrite(LED_PIN, HIGH);

  // Connect to WiFi
  if (firstWiFiConnection)
  {
    Serial.println("Connecting to " WIFI_SSID "...");
  }
  else
  {
    Serial.println("Reconnecting to " WIFI_SSID "...");
  }
  firstWiFiConnection = false;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiConnection = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    // If we cannot connect to WiFi for a long time, reboot
    if (millis() - lastWifiConnection > WIFI_TIMEOUT)
    {
      Serial.println("\nWiFi connection timeout, rebooting...");
      ESP.restart();
    }
  }
  Serial.println("OK");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP().toString());
}

// This method ensures that the device is connected to WiFi and MQTT server
void ensureMqttConnected()
{
  while (mqttClient.state() != MQTT_CONNECTED)
  {
    // First, ensure we are connected to WiFi
    ensureWifiConnected();

    // Turn LED on
    digitalWrite(LED_PIN, HIGH);

    // If it's not first connection attempt, print the current state and wait for a while
    if (!firstMqttConnection)
    {
      switch (mqttClient.state())
      {
      case MQTT_CONNECTION_TIMEOUT:
        Serial.println("MQTT connection state: timeout (server didn't respond)");
        break;
      case MQTT_CONNECTION_LOST:
        Serial.println("MQTT connection state: connection lost (server disconnected)");
        break;
      case MQTT_CONNECT_FAILED:
        Serial.println("MQTT connection state: connection failed (server didn't accept the connection)");
        break;
      case MQTT_DISCONNECTED:
        Serial.println("MQTT connection state: disconnected");
        break;
      case MQTT_CONNECT_BAD_PROTOCOL:
        Serial.println("MQTT connection state: bad protocol (unsupoorted version)");
        break;
      case MQTT_CONNECT_BAD_CLIENT_ID:
        Serial.println("MQTT connection state: bad client ID (server rejected client ID)");
        break;
      case MQTT_CONNECT_UNAVAILABLE:
        Serial.println("MQTT connection state: unavailable (server was unable to accept connection)");
        break;
      case MQTT_CONNECT_BAD_CREDENTIALS:
        Serial.println("MQTT connection state: bad credentials");
        break;
      case MQTT_CONNECT_UNAUTHORIZED:
        Serial.println("MQTT connection state: unauthorized");
        break;
      default:
        Serial.printf("MQTT connection state: %d (unknown)\n", mqttClient.state());
        break;
      }
      Serial.printf("Waiting %d ms before next connection attempt...\n", MQTT_RECONNECT_DELAY);
      delay(MQTT_RECONNECT_DELAY);
    }
    firstMqttConnection = false;

    // Connect to MQTT server
    Serial.printf("Connecting to %s:%d...", MQTT_SERVER, MQTT_PORT);
    auto clientId = WiFi.macAddress();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_DEPART, 0, false, clientId.c_str()))
    {
      Serial.println("OK");

      // Send a message that we have arrived
      Serial.printf("Publishing to topic %s...", MQTT_TOPIC_ARRIVE);
      if (mqttClient.publish(MQTT_TOPIC_ARRIVE, clientId.c_str()))
      {
        Serial.println("OK");
      }
      else
      {
        Serial.println("Failed!");
      }

      // Subscribe to chat topic
      Serial.printf("Subscribing to topic %s...", MQTT_TOPIC_STATUS);
      if (mqttClient.subscribe(MQTT_TOPIC_STATUS))
      {
        Serial.println("OK");
      }
      else
      {
        Serial.println("Failed!");
      }
    }
    else
    {
      // Connection failed
      Serial.println("Failed!");
    }
  }
}

// This method is called when a message is received from MQTT
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // Process message
  if (length == 1 && payload[0] == '1')
  {
    isOnAir = true;
    lastMessageReceived = millis();
    Serial.printf("On-Air status set to ON for %i ms\n", LED_TIMEOUT);
  }
  else if (length == 1 && payload[0] == '0')
  {
    isOnAir = false;
    lastMessageReceived = millis();
    Serial.println("On-Air status set to OFF");
  }
  else
  {
    // Print received message
    Serial.printf("Unrecognized message arrived to topic %s, lenght %i bytes: ", topic, length);
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
}

/* Main program *****************************************************************************************************/

// This method is called once at the beginning of the program
void setup()
{
  // Initialize serial port
  Serial.begin(9600);
  delay(500);
  Serial.println();
  Serial.println();
#ifdef BUTTON_PIN
  Serial.println(VERSION " (master configuration)");
#else
  Serial.println(VERSION " (slave configuration)");
#endif

  // Initialize LED pin and turn LED ON
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

#ifdef BUTTON_PIN
  // Initialize button pin, if defined
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Set last button state to inverse of current state to force initial state change
  lastButtonState = !digitalRead(BUTTON_PIN);
#endif

  // Disable TLS server certificate verification
  wifiClient.setInsecure();

  // Set MQTT client callback
  mqttClient.setCallback(mqttCallback);
}

// This method is called repeatedly in an endless loop
void loop()
{
#ifdef REBOOT_INTERVAL
  // Reboot every REBOOT_INTERVAL ms if defined, unless currently on air
  if (!isOnAir && millis() > REBOOT_INTERVAL)
  {
    Serial.println("Rebooting...");
    ESP.restart();
  }
#endif

  // Ensure MQTT connection
  ensureMqttConnected();

#ifdef BUTTON_PIN
  // Check for button state, if defined
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState != lastButtonState)
  {
    // This is very crude debouncing, but it works for our purposes
    delay(BUTTON_DEBOUNCE);
    if (currentButtonState == digitalRead(BUTTON_PIN))
    {
      lastButtonState = currentButtonState;
      if (currentButtonState == LOW)
      {
        Serial.print("Button pressed, enabling ON AIR mode...");
        bool result = mqttClient.publish(MQTT_TOPIC_STATUS, "1");
        lastMessageSent = millis();
        Serial.println(result ? "OK" : "Failed!");
      }
      else
      {
        Serial.print("Button released, disabling ON AIR mode...");
        bool result = mqttClient.publish(MQTT_TOPIC_STATUS, "0");
        Serial.println(result ? "OK" : "Failed!");
      }
    }
  }

  // Send message every LED_TTL ms
  if (isOnAir && millis() - lastMessageSent > LED_TTL)
  {
    Serial.print("Sending ON AIR message before TTL...");
    bool result = mqttClient.publish(MQTT_TOPIC_STATUS, "1");
    lastMessageSent = millis();
    Serial.println(result ? "OK" : "Failed!");
  }

#endif

  // Handle MQTT messages
  mqttClient.loop();

  if (isOnAir && millis() - lastMessageReceived > LED_TIMEOUT)
  {
    isOnAir = false;
    Serial.println("On-Air status set to OFF (timeout)");
  }

  // Check for on-air status
  if (isOnAir)
  {
    if (LED_INTERVAL == 0)
    {
      // Turn on LED
      digitalWrite(LED_PIN, HIGH);
    }
    else if (millis() - lastLedToggle > LED_INTERVAL)
    {
      // Blink LED
      lastLedToggle = millis();
      lastLedState = !lastLedState;
      digitalWrite(LED_PIN, lastLedState ? HIGH : LOW);
    }
  }
  else
  {
    // Turn off LED
    digitalWrite(LED_PIN, LOW);
  }
}