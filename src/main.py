# Import libraries
from wifi_connector import WiFiConnector
from board_temp_sensor import BoardTempSensor
from bme280 import BME280
from machine import Pin, I2C, WDT
import socket
import gc
import time
import machine
import network

# Set up the sensors
# This is the on-board temperature sensor
board_temp = BoardTempSensor()

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) 

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)   # by default, the address should have been 0x76, however, my sensor is using the alternate

# Fix the port for the socket 
PORT = 5005

# Something to allow entering REPL mode if needed
SAFE_MODE = False

# This will effectively disable the watchdog during development, but will be enabled in production
DEBUG = False

# Outline the valid requests the server will respond to, and the expected response format. This is important for the client to know how to parse the data.
# Request: "SENSORS"
# Response: "V=1,TS=<uptime_seconds>,BT=<board_temp_C>,T=<temperature_C>,H=<humidity_percent>,P=<pressure_hPa>,S=<status_ok_or_err>,E=<error_message_if_any>"
#
# Request: "STATUS"
# Response: "V=1,TS=<uptime_seconds>,MEM=<free_memory_bytes>,S=<status_ok_or_err>,E=<error_message_if_any>"
valid_requests = [b"SENSORS", b"STATUS"]

def serve_udp(bme, board_temp, wdt=None):
    wlan = network.WLAN(network.STA_IF)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))
    sock.settimeout(1.0)

    start_time = time.ticks_ms()

    while True:
        if wdt:
            wdt.feed()

        if not wlan.isconnected():
            print("WiFi lost — rebooting")
            machine.reset()

        try:
            try:
                data, addr = sock.recvfrom(128)
            except OSError as e:
                print(e)
                continue

            if data != b"SENSORS":
                continue

            try:
                temperature, pressure, humidity = bme.environmental_parameters()
                uptime_s = time.ticks_diff(time.ticks_ms(), start_time) // 1000

                payload = (
                    f"TS={uptime_s},"
                    f"BT={board_temp.temperatureC():.2f},"
                    f"T={temperature:.2f},"
                    f"H={humidity:.2f},"
                    f"P={pressure:.2f},"
                    f"S=ok"
                )

            except Exception as e:
                payload = f"S=err,E={type(e).__name__}"

            payload = "V=1," + payload
            sock.sendto(payload.encode(), addr)

        except Exception as e:
            print("UDP error:", e)

        finally:
            gc.collect()
            # if time.ticks_diff(time.ticks_ms(), start_time) > 86_400_000:
            #     print("Daily reboot")
            if time.ticks_diff(time.ticks_ms(), start_time) > 43_200_000:
                print("Half-day reboot")
                machine.reset()

if __name__ == "__main__":
    
    # Check for Ctrl+C to enter REPL
    print("Starting... Press Ctrl+C within 10 seconds to enter REPL")

    try:
        for _ in range(100):   # 10 seconds
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("REPL mode")
        SAFE_MODE = True

    if SAFE_MODE:
        print("Server NOT started")
        while True:
            time.sleep(1)
    
    # Initialize watchdog (8 seconds timeout)
    wdt = None if DEBUG else WDT(timeout=8000)

    try:

        if wdt is not None:
            wdt.feed()

        # Connect to WiFi first
        wificonnector = WiFiConnector()

        if wdt is not None:
            wdt.feed()

        if wificonnector.connected:
            # Small delay to ensure connection is stable
            time.sleep(2)
            if wdt is not None:
                wdt.feed()

        # Run the server to serve sensor data
        try:
            serve_udp(bme, board_temp, wdt=wdt)
        except Exception as e:
            print(f"Server error: {e}")
            raise e
    except Exception as e:
        print(f"Fatal error: {e}")
        
        # restart after delay
        time.sleep(10)
        machine.reset()
        

