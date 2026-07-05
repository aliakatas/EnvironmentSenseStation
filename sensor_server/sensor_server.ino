/**********************************************************************
  Filename    : Sensor Server
  Description : Use ESP32's WiFi server feature to wait for other WiFi devices to connect.
                Respond with sensor data.
  Author      : Aristotelis Liakatas
**********************************************************************/

//This Macro definition decide whether you use I2C or SPI
//When USEIIC is 1 means use I2C interface, When it is 0,use SPI interface
#define USEIIC 0

#include "secrets.h"
#include "i2c_bus_recovery.h"

#include "sd_read_write.h"
#include "SD_MMC.h"

#define SD_MMC_CMD 15 //Please do not modify it.
#define SD_MMC_CLK 14 //Please do not modify it. 
#define SD_MMC_D0  2  //Please do not modify it.

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>

#if(!USEIIC)
#include <SPI.h>
#endif 

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


#if(USEIIC)
	Adafruit_BME280 bme;
	const uint8_t BME280_I2C_ADDR = 0x76;   // change to 0x77 if that's your wiring
	const uint8_t BME280_CHIPID_REG = 0xD0; // datasheet-fixed register, always returns 0x60 when healthy
	const uint8_t BME280_EXPECTED_CHIPID = 0x60;
#else
	#define SPI_SCK  13
	#define SPI_MISO 0 // as 12 is a strapping pin
	#define SPI_MOSI 32
	#define SPI_CS   33
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

const char* LOG_FILE_ON_SD = "/run-log.log";

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
        appendFile(SD_MMC, LOG_FILE_ON_SD, "WiFi reconnect failed\n");
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
    appendFile(SD_MMC, LOG_FILE_ON_SD, "sensor initialisation: OK\n");
    return true;
}

bool checkSensorHealth()
{
    float temperature = bme.readTemperature();
    float humidity = bme.readHumidity();
    float pressure = bme.readPressure();

    return !(isnan(temperature) || isnan(humidity) || isnan(pressure));
}

void initializeSDCard()
{
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
      Serial.println("Card Mount Failed");
      return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return;
    }

    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

    // Best for preserving previous logs!
    appendFile(SD_MMC, LOG_FILE_ON_SD, "***********\n");
    // otherwise:
    // writeFile(SD_MMC, LOG_FILE_ON_SD, "***********\n");
}

void monitorHealth()
{
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED && now - lastWifiCheck >= WIFI_CHECK_INTERVAL_MS) {
        lastWifiCheck = now;
        Serial.println("WiFi disconnected, attempting reconnect...");
        appendFile(SD_MMC, LOG_FILE_ON_SD, "WiFi disconnected, attempting reconnect...\n");
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
            appendFile(SD_MMC, LOG_FILE_ON_SD, "BME280 health check failed, reinitializing...\n");
            if (initializeSensor()) {
                sensorReinitFailures = 0;
                return;
            }

            sensorReinitFailures++;
            if (sensorReinitFailures >= MAX_SENSOR_REINIT_FAILURES) {
                Serial.println("BME280 reinit failed repeatedly, restarting...");
                appendFile(SD_MMC, LOG_FILE_ON_SD, "BME280 reinit failed repeatedly, restarting...\n");
                ESP.restart();
            }
        }
    }
}

// --- Sensor health tracking -------------------------------------------------
//
// Rationale: Adafruit_BME280::readTemperature()/readHumidity()/readPressure()
// only return NAN when the *corresponding oversampling field is set to
// SAMPLING_NONE* in the ctrl_meas/ctrl_hum registers (i.e. that channel is
// deliberately disabled). They do NOT return NAN when the I2C bus is wedged,
// the sensor has crashed, or a read returns stale/garbage data — in those
// cases you get a normal-looking float that is simply wrong. So NAN-checking
// alone (and equally, range-checking against numbers that merely look
// "implausible") cannot be the detection mechanism.
//
// Instead we use two independent, library-agnostic signals:
//
//   1. Chip ID register (0xD0) — this is a fixed, documented, read-only
//      register that always returns 0x60 on a functioning BME280. If the
//      I2C bus is wedged, addressed wrong, or the sensor has browned out,
//      this read will return 0x00, 0xFF, or some other wrong value. This
//      is a direct hardware-truth check, not an inference from output data.
//
//   2. Read staleness — if a wedged I2C transaction silently returns the
//      previous buffer contents instead of failing, the chip ID check
//      above can still pass (cached correctly) while the data registers
//      are frozen. We catch this by comparing the latest reading against
//      the previous N readings: bit-for-bit identical floats across
//      several consecutive samples, on a sensor with no filtering enabled,
//      is itself the anomaly signal — real sensor noise basically never
//      produces exact repeats over multiple samples.
//
// Neither check relies on guessing what a "reasonable" temperature is.

static float lastTemp = NAN, lastHum = NAN, lastPres = NAN;
static uint8_t repeatCount = 0;
const uint8_t STALE_REPEAT_THRESHOLD = 4; // consecutive identical reads before we call it stale

bool chipIdIsHealthy()
{
#if(USEIIC)
    Wire.beginTransmission(BME280_I2C_ADDR);
    Wire.write(BME280_CHIPID_REG);
    if (Wire.endTransmission(false) != 0) {
        // NACK or bus error on the address/register phase — bus is unhappy
        return false;
    }
    if (Wire.requestFrom(BME280_I2C_ADDR, (uint8_t)1) != 1) {
        return false; // didn't even get a byte back
    }
    uint8_t chipId = Wire.read();
    return chipId == BME280_EXPECTED_CHIPID;
#else
    // SPI chip-ID check would go through bme's own SPI device; for now,
    // staleness detection (below) is the primary signal in SPI mode.
    return true;
#endif
}

bool readingIsStale(float temp, float hum, float pres)
{
    bool identicalToLast =
        (temp == lastTemp) && (hum == lastHum) && (pres == lastPres);

    if (identicalToLast) {
        repeatCount++;
    } else {
        repeatCount = 0;
    }

    lastTemp = temp;
    lastHum  = hum;
    lastPres = pres;

    return repeatCount >= STALE_REPEAT_THRESHOLD;
}

void recoverSensor()
{
    Serial.println("BME280 fault detected (bad chip ID and/or stale readings) - recovering...");
    appendFile(SD_MMC, LOG_FILE_ON_SD, "fault detected (bad chip ID and/or stale readings) - recovering...\n");

    // i2c_bus_recover();          // bit-bang SCL/SDA to free a wedged bus, then Wire.begin() again
    // bool ok = bme.begin(BME280_I2C_ADDR, &Wire);  // full re-init incl. re-reading calibration
    // if (!ok) 
    // {
    //     Serial.println("Re-init failed - will retry on next request.");
    //     appendFile(SD_MMC, LOG_FILE_ON_SD, "Re-init failed - will retry on next request.\n");
    // } else 
    // {
    //     Serial.println("Sensor re-initialised.");
    //     appendFile(SD_MMC, LOG_FILE_ON_SD, "Sensor re-initialised.\n");
    // }
    // repeatCount = 0;
    // lastTemp = lastHum = lastPres = NAN;
    delay(100);
}

void setup()
{
    Serial.begin(115200);

    // Start with the card where the log goes!
    initializeSDCard();

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
    appendFile(SD_MMC, LOG_FILE_ON_SD, "BME280 init failed repeatedly, restarting...\n");
    ESP.restart();
}

String buildJson()
{
    // // Step 1: hardware-truth check, independent of any reading's value
    // if (!chipIdIsHealthy()) {
    //     recoverSensor();
    // }

    // // Step 2: take the actual reading
    // float temp = bme.readTemperature();
    // float hum  = bme.readHumidity();
    // float pres = bme.readPressure() / 100.0F;

    // // Step 3: staleness check using this reading; if it trips, recover and
    // // re-read once so the client gets a fresh value rather than the one
    // // that triggered the recovery.
    // if (readingIsStale(temp, hum, pres)) {
    //     recoverSensor();
    //     temp = bme.readTemperature();
    //     hum  = bme.readHumidity();
    //     pres = bme.readPressure() / 100.0F;
    //     // reset tracking with the fresh values so we don't immediately
    //     // re-trigger on the next call
    //     lastTemp = temp; lastHum = hum; lastPres = pres; repeatCount = 0;
    // }

    // Read raw value
    uint8_t board_temp_raw = temprature_sens_read();
    // Convert to Celsius
    float board_temp_celsius = (board_temp_raw - 32) / 1.8F;

    float temperature_value = bme.readTemperature();
    float humidity_value = bme.readHumidity();
    float pressure_value = bme.readPressure();

    JsonDocument health;
    // the wifi bit, is it a bit ridiculous...?
    // health["wifi"] = WiFi.status() == WL_CONNECTED ? "ok" : "disconnected";
    bool degraded_state = isnan(temperature_value) || isnan(humidity_value) || isnan(pressure_value) || temperature_value < -50;
    health["sensor"] = degraded_state ? "degraded" : "ok";

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
    String requestPath = "/";

    // Read the first line of the HTTP request (e.g. "GET / HTTP/1.1")
    if (client.available())
        requestLine = client.readStringUntil('\n');

    int firstSpace = requestLine.indexOf(' ');
    int secondSpace = requestLine.indexOf(' ', firstSpace + 1);
    if (firstSpace >= 0 && secondSpace > firstSpace) {
        requestPath = requestLine.substring(firstSpace + 1, secondSpace);
    }

    // Drain the remaining headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;   // blank line signals end of headers
    }

    // Serial.println("Request: " + requestLine);

    if (requestLine.startsWith("GET")) {
        if (requestPath == "/reset") {
            client.print(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "\r\n"
                "{\"status\":\"resetting\"}"
            );
            client.flush();
            appendFile(SD_MMC, LOG_FILE_ON_SD, "Device restart requested...\n");
            delay(100);
            ESP.restart();
            return;
        }

        if (requestPath == "/log") {
            String logContents = getFileContents(SD_MMC, LOG_FILE_ON_SD);
            JsonDocument loglog;
            loglog["text"] = logContents;
            loglog["size"] = SD_MMC.usedBytes();
            loglog["capacity"] = SD_MMC.totalBytes();

            String output;
            serializeJson(loglog, output);

            String response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "Content-Length: " + String(output.length()) + "\r\n"
                "\r\n" +
                output;
            client.print(response);

        }

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
    // monitorHealth();

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

