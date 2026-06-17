/**********************************************************************
  Filename    : Sensor Server
  Description : Use ESP32's WiFi server feature to wait for other WiFi devices to connect.
                Respond with sensor data.
  Author      : Aristotelis Liakatas
**********************************************************************/
#include "secrets.h"

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//This Macro definition decide whether you use I2C or SPI
//When USEIIC is 1 means use I2C interface, When it is 0,use SPI interface
#define USEIIC 1

#if(USEIIC)
	Adafruit_BME280 bme;
#else
	#define SPI_SCK 13
	#define SPI_MISO 12
	#define SPI_MOSI 11
	#define SPI_CS 10
	Adafruit_BME280 bme(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCK);
#endif

const int port = SERVER_PORT;
const char *ssid_Router     = ROUTER_SSID;
const char *password_Router = SSID_PASSWORD;
WiFiServer  server(port);

const unsigned long WIFI_CHECK_INTERVAL_MS = 15000;
const unsigned long SENSOR_CHECK_INTERVAL_MS = 30000;
const unsigned long SENSOR_RETRY_INTERVAL_MS = 10000;
const int MAX_SENSOR_REINIT_FAILURES = 3;

unsigned long lastWifiCheck = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastSensorRetry = 0;
int sensorReinitFailures = 0;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

bool connectWifi()
{
    WiFi.disconnect();
    WiFi.begin(ssid_Router, password_Router);

    unsigned long timeout = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi reconnect failed.");
        return false;
    }

    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("IP port: %d\n", port);
    return true;
}

bool initializeSensor()
{
    Serial.println("Initialising sensor...");
    if (!bme.begin()) {
        Serial.println("Init Fail,Please Check your address or the wire you connected!");
        return false;
    }

    Serial.println("Init Success");
    Serial.println("Temperature           Pressure             Humidity");
    return true;
}

bool checkSensorHealth()
{
    float temperature = bme.readTemperature();
    float humidity = bme.readHumidity();
    float pressure = bme.readPressure();

    return !(isnan(temperature) || isnan(humidity) || isnan(pressure));
}

void monitorHealth()
{
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED && now - lastWifiCheck >= WIFI_CHECK_INTERVAL_MS) {
        lastWifiCheck = now;
        Serial.println("WiFi disconnected, attempting reconnect...");
        connectWifi();
    }

    if (now - lastSensorCheck >= SENSOR_CHECK_INTERVAL_MS) {
        lastSensorCheck = now;

        if (checkSensorHealth()) {
            sensorReinitFailures = 0;
            return;
        }

        if (now - lastSensorRetry >= SENSOR_RETRY_INTERVAL_MS) {
            lastSensorRetry = now;
            Serial.println("BME280 health check failed, reinitializing...");
            if (initializeSensor()) {
                sensorReinitFailures = 0;
                return;
            }

            sensorReinitFailures++;
            if (sensorReinitFailures >= MAX_SENSOR_REINIT_FAILURES) {
                Serial.println("BME280 reinit failed repeatedly, restarting...");
                ESP.restart();
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);

    // Start with WiFi
    Serial.printf("\nConnecting to ");
    Serial.println(ssid_Router);
    connectWifi();
    server.begin(port);								
    WiFi.setAutoReconnect(true);

    // Go on with the sensor
    for (int attempt = 0; attempt < 3; attempt++) {
        if (initializeSensor()) {
            return;
        }

        delay(2000);
    }

    Serial.println("BME280 init failed repeatedly, restarting...");
    ESP.restart();
}

String buildJson()
{
    // Read raw value
    uint8_t board_temp_raw = temprature_sens_read();
    // Convert to Celsius
    float board_temp_celsius = (board_temp_raw - 32) / 1.8F;

    float temperature_value = bme.readTemperature();
    float humidity_value = bme.readHumidity();
    float pressure_value = bme.readPressure();

    JsonDocument health;
    // the wifi bit, is it a bit ridiculous...?
    health["wifi"] = WiFi.status() == WL_CONNECTED ? "ok" : "disconnected";
    health["sensor"] = (isnan(temperature_value) || isnan(humidity_value) || isnan(pressure_value)) ? "degraded" : "ok";

    JsonDocument board_temperature;
    board_temperature["value"] = board_temp_celsius;
    board_temperature["unit"] = "C";

    JsonDocument temperature;
    temperature["value"] = temperature_value;
    temperature["unit"] = "C";

    JsonDocument humidity;
    humidity["value"] = humidity_value;
    humidity["unit"] = "%";

    JsonDocument pressure;
    pressure["value"] = pressure_value/100.0F;
    pressure["unit"] = "hPa";

    JsonDocument response;
    response["board_temperature"] = board_temperature;
    response["temperature"] = temperature;
    response["humidity"] = humidity;
    response["pressure"] = pressure;
    response["health"] = health;
    response["status"] = "ok";

    String output;
    serializeJson(response, output);
    return output;
}

void handleClient(WiFiClient &client)
{
    String requestLine = "";

    // Read the first line of the HTTP request (e.g. "GET / HTTP/1.1")
    if (client.available())
        requestLine = client.readStringUntil('\n');

    // Drain the remaining headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;   // blank line signals end of headers
    }

    // Serial.println("Request: " + requestLine);

    if (requestLine.startsWith("GET")) {
        String json = buildJson();
        String response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "Content-Length: " + String(json.length()) + "\r\n"
            "\r\n" +
            json;
        client.print(response);
        // Serial.println("Sent JSON: " + json);
    } else {
        // Return 405 for anything that isn't a GET
        client.print(
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Connection: close\r\n"
            "\r\n"
        );
    }
}

void loop()
{
    monitorHealth();

  WiFiClient client = server.accept();
  if (client) {
    //   Serial.println("Client connected.");
      unsigned long timeout = millis() + 2000;   // 2 s to send headers
      while (client.connected() && !client.available()) {
          if (millis() > timeout) break;
          delay(1);
      }
      handleClient(client);
      client.stop();
    //   Serial.println("Client disconnected.");
  }
}
