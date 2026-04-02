#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

// MQTT broker details
const char* mqtt_server = "BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_topic = "home/doorbell/ring";

// MQTT authentication
const char* mqtt_user = "USERNAME";
const char* mqtt_password = "PASSWORD";

// GPIO to check
const int gpioPin = 5;

WiFiClient espClient;
PubSubClient client(espClient);

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { // retry ~10 seconds
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return;
  }
  
  Serial.println("WiFi connection failed, will retry in loop");
}

void connectMQTT() {
  if (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    while (!client.connected()) {
      if (WiFi.status() == WL_CONNECTED) {
        // Use username and password
        if (client.connect("ESP32C6Client", mqtt_user, mqtt_password)) {
          Serial.println("MQTT connected");
        } else {
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println(", retrying in 2s");
          delay(2000);
        }
      } else {
        Serial.println("WiFi not connected, cannot connect MQTT");
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(gpioPin, INPUT);

  connectWiFi();
  client.setServer(mqtt_server, mqtt_port);
  connectMQTT();
}

void loop() {
  // Ensure WiFi and MQTT are connected
  connectWiFi();
  connectMQTT();

  client.loop();

  int gpioState = digitalRead(gpioPin);
  if (gpioState == HIGH) {
    Serial.println("GPIO is HIGH, sending MQTT message...");
    if (client.connected()) {
      // Don't retain messages!
      client.publish(mqtt_topic, "RING", false);
      delay(5000); // wait 5 seconds before checking again
    }
  }
}
