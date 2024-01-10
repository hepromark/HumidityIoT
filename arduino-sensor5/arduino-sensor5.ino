#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Secrets.h"
#include "DHT.h"

#define AWS_IOT_PUBLISH_TOPIC "sensor/soil_humidity/plant5"
#define AWS_IOT_SENSOR_ID 5

// Sensor constants
const int DRY = 657;
const int WET = 296;
const int ANALOG_IN = A0;
const int SECONDS_PER_DETECTION = 300;
const int DETECTIONS_PER_SEND = 12;

unsigned long lastMillis = 0;
unsigned long previousMillis = 0;

// Certificates from Secrets.h
BearSSL::X509List cert(ca_cert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(priv_key);

WiFiClientSecure net;
PubSubClient client(net);

void connectAWS()
{
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  Serial.println("Connecting to AWS IOT");
 
  // Loading bar 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout Error :( !");
    return;
  }
  Serial.println("AWS IoT Connected!");
}

void publishMessage(float humidity, int raw_signal)
{
  StaticJsonDocument<200> doc;
  doc["sensor_id"] = AWS_IOT_SENSOR_ID;
  doc["soil_humidity"] = humidity;
  doc["soil_analog_raw"] = raw_signal;
  doc["soil_analog_max"] = DRY;
  doc["soil_analog_min"] = WET;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup()
{
  Serial.begin(9600);
  connectAWS();
}

// Store sensor data for sending
int current_detection = 0;
int previous_humidity = 0;
int previous_analog = 0;

void loop()
{
  int analog_val = 0;
  analog_val = analogRead(ANALOG_IN);
  Serial.println(analog_val);

  float soil_humidity= map(analog_val, DRY, WET, 0, 100);
  
  // Bound humidity levels
  if (soil_humidity> 100) {
    soil_humidity= 100;
  } else if (soil_humidity< 0) {
    soil_humidity= 0;
  }

  // Add to running total
  previous_humidity += soil_humidity;
  previous_analog += analog_val;

  // Print to console
  Serial.print(soil_humidity);
  Serial.println("%");
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    // Send data if threshold reached
    if (current_detection > DETECTIONS_PER_SEND) 
    {
      client.loop();

      Serial.print("Sending data to AWS IoT");
      lastMillis = millis();
      float temp = previous_humidity / current_detection;
      int temp2 = previous_analog / current_detection;
      publishMessage(temp, temp2);

      // Reset counters
      previous_humidity = 0;
      previous_analog = 0;
      current_detection = 0;
    }
  }

  // Detects once per SECONDS_PER_DETECTION
  delay(1000 * SECONDS_PER_DETECTION);
  Serial.println(current_detection);
  current_detection++;
}
