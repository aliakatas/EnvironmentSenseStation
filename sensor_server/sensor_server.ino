/**********************************************************************
  Filename    : Sensor Server
  Description : Use ESP32's WiFi server feature to wait for other WiFi devices to connect.
                Respond with sensor data.
  Author      : Aristotelis Liakatas
**********************************************************************/
#include "secrets.h"

#include <WiFi.h>
#include <ArduinoJson.h>

#define port 80
const char *ssid_Router     = ROUTER_SSID;
const char *password_Router = SSID_PASSWORD;
WiFiServer  server(port);

// --- Your data fields ---
String  field_name    = "ESP32-Device";
String  field_status  = "online";
float   field_value   = 42.7;

static int clicks = 0;

void setup()
{
    Serial.begin(115200);
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
}

String buildJson()
{
    StaticJsonDocument<256> doc;
    doc["name"]   = field_name;
    doc["status"] = field_status;
    doc["value"]  = field_value;
    doc["clicks"] = clicks++;

    String output;
    serializeJson(doc, output);
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
