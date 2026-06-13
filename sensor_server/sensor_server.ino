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

#define port 80
const char *ssid_Router     = ROUTER_SSID;
const char *password_Router = SSID_PASSWORD;
WiFiServer  server(port);

void setup()
{
    Serial.begin(115200);

    // Start with WiFi
    Serial.printf("\nConnecting to ");
    Serial.println(ssid_Router);
    WiFi.disconnect();
    WiFi.begin(ssid_Router, password_Router);
    delay(1000);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());			
    Serial.printf("IP port: %d\n",port);			
    server.begin(port);								
    WiFi.setAutoReconnect(true);

    // Go on with the sensor
    bool rslt;
    rslt = bme.begin();  
    if (!rslt) {
        Serial.println("Init Fail,Please Check your address or the wire you connected!!!");
        while (1);
    }
	
    Serial.println("Init Success");
    Serial.println("Temperature           Pressure             Humidity");
}

String buildJson()
{
    JsonDocument temperature;
    temperature["value"] = bme.readTemperature();
    temperature["unit"] = "C";

    JsonDocument humidity;
    humidity["value"] = bme.readHumidity();
    humidity["unit"] = "%";

    JsonDocument pressure;
    pressure["value"] = bme.readPressure()/100.0F;
    pressure["unit"] = "hPa";

    JsonDocument response;
    response["temperature"] = temperature;
    response["humidity"] = humidity;
    response["pressure"] = pressure;
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

    Serial.println("Request: " + requestLine);

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
        Serial.println("Sent JSON: " + json);
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
  WiFiClient client = server.accept();
  if (client) {
      Serial.println("Client connected.");
      unsigned long timeout = millis() + 2000;   // 2 s to send headers
      while (client.connected() && !client.available()) {
          if (millis() > timeout) break;
          delay(1);
      }
      handleClient(client);
      client.stop();
      Serial.println("Client disconnected.");
  }
}
