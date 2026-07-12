// Minimal Mongoose-based HTTP server exposing on-demand BME280 readings.

#include "mongoose.h"

#include <csignal>
#include <cstdio>
#include <string>

static volatile std::sig_atomic_t g_stop = 0;

static void on_signal(int) {
    g_stop = 1;
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
    return R"({"board_temperature": {"value": 50,"unit": "C"},"temperature": {"value": 23,"unit": "C"},"humidity": {"value": 69,"unit": "%"},"pressure": {"value": 1023,"unit": "hPa"},"health": {"sensor":"ok"},"status": "ok"})";
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
