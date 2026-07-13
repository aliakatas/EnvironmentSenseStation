// Minimal Mongoose-based HTTP server exposing on-demand BME280 readings.

#include "sensor_utilities.h"

#include "bme280.h"
#include "json.hpp"
#include "mongoose.h"

#include <linux/spi/spidev.h>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <chrono>

struct bme280_dev dev;

static volatile std::sig_atomic_t g_stop = 0;

static void on_signal(int) {
    g_stop = 1;
}

static bool read_board_temperature_c(float &temperature_c) {
    // Raspberry Pi exposes SoC temperature in millidegrees C via thermal zone 0.
    std::ifstream thermal_file("/sys/class/thermal/thermal_zone0/temp");
    if (!thermal_file.is_open()) {
        return false;
    }

    long temperature_milli_c = 0;
    thermal_file >> temperature_milli_c;
    if (!thermal_file.good() && !thermal_file.eof()) {
        return false;
    }

    temperature_c = static_cast<float>(temperature_milli_c) / 1000.0f;
    return true;
}

// ---------------------------------------------------------------------
// Fill this in with your existing BME280 read code + nlohmann::json.
//
// Expected contract:
//   - Perform the SPI read.
//   - Return a JSON string
//   - On sensor failure, return an empty string; the caller will respond 503.
// ---------------------------------------------------------------------
static std::string read_sensor_payload() {
    // TODO: replace with real sensor read + nlohmann::json serialization
    dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    dev.settings.osr_p = BME280_OVERSAMPLING_16X;
    dev.settings.osr_t = BME280_OVERSAMPLING_2X;
    dev.settings.filter = BME280_FILTER_COEFF_16;
    dev.settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

    uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL |
                            BME280_STANDBY_SEL | BME280_FILTER_SEL;
    int8_t rslt = bme280_set_sensor_settings(settings_sel, &dev);
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);
    if (rslt != BME280_OK) {
        return "";
    }

    struct bme280_data comp_data;
    dev.delay_ms(70);
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    if (rslt != BME280_OK) {
        return "";
    }

    float board_temperature_c = 0.0f;
    const bool has_board_temperature = read_board_temperature_c(board_temperature_c);

    nlohmann::json payload = {
        {"board_temperature", {{"value", has_board_temperature ? nlohmann::json(board_temperature_c) : nlohmann::json(nullptr)}, {"unit", "C"}}},
        {"temperature", {{"value", comp_data.temperature}, {"unit", "C"}}},
        {"humidity", {{"value", comp_data.humidity}, {"unit", "%"}}},
        {"pressure", {{"value", comp_data.pressure / 100.0f}, {"unit", "hPa"}}},
        {"health", {{"sensor", "ok"}, {"board_temperature", has_board_temperature ? "ok" : "unavailable"}}},
        {"status", "ok"}
    };

    return payload.dump();
}

static void handle_sensor_request(struct mg_connection *c) {
    std::string body = read_sensor_payload();

    if (body.empty()) {
        mg_http_reply(c, 503, "Content-Type: application/json\r\n",
                      "{\"error\":\"sensor read failed\"}\n");
        return;
    }

    mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                  "%s\n", body.c_str());
}

static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev != MG_EV_HTTP_MSG) {
        return;
    }

    auto *hm = static_cast<struct mg_http_message *>(ev_data);

    if (mg_match(hm->uri, mg_str("/sensors/bme280"), nullptr)) {
        handle_sensor_request(c);
    } else if (mg_match(hm->uri, mg_str("/healthz"), nullptr)) {
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "ok\n");
    } else {
        mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                      "{\"error\":\"not found\"}\n");
    }
}

int main(int argc, char *argv[]) {
    // Bind address: tailscale0 interface IP, or "0.0.0.0" if you're firewalling
    // at the OS/systemd level instead. Override via argv for flexibility.
    const char *listen_url = (argc > 1) ? argv[1] : "http://0.0.0.0:23001";

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::atexit(sensor_utilities::close_transport);

    // Give the sensor some time to warm up
    const int warmup_time_seconds = 2;
    std::cout << "Warming up the sensor for " << warmup_time_seconds << " seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(warmup_time_seconds));

    if (!sensor_utilities::initialize_transport()) {
        return EXIT_FAILURE;
    }

    dev.dev_id = 0;
    dev.intf = BME280_SPI_INTF;
    dev.read = sensor_utilities::user_spi_read;
    dev.write = sensor_utilities::user_spi_write;
    dev.delay_ms = sensor_utilities::user_delay_ms;

    int8_t rslt = bme280_init(&dev);
    std::printf("\r\nBME280 Init Result is: %d\r\n", rslt);
    if (rslt != BME280_OK) 
    {
        return EXIT_FAILURE;
    }

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    struct mg_connection *c = mg_http_listen(&mgr, listen_url, event_handler, nullptr);
    if (c == nullptr) {
        std::fprintf(stderr, "Failed to listen on %s\n", listen_url);
        mg_mgr_free(&mgr);
        return 1;
    }

    std::fprintf(stderr, "Listening on %s\n", listen_url);

    while (!g_stop) {
        mg_mgr_poll(&mgr, 1000);
    }

    std::fprintf(stderr, "Shutting down\n");
    mg_mgr_free(&mgr);
    return 0;
}
