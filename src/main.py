# Import libraries
from wifi_connector import WiFiConnector
from board_temp_sensor import BoardTempSensor, celsius_to_farenheit
from http_stuff import handle_request
from machine import Pin, I2C, WDT
from bme280 import BME280
import time

# Set up the sensors
# This is the on-board temperature sensor
board_temp = BoardTempSensor()

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) 

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)   # by default, the address should have been 0x76, however, my sensor is using the alternate

def run_server(sock, wdt=None):
    """Run the HTTP server to serve sensor data"""

    while True:
        # Feed watchdog if provided
        if wdt:
            wdt.feed()
        client = None

        try:
            client, remote_address = sock.accept()
            client.settimeout(5.0)  # Timeout for client operations
            print('Client connected from', remote_address)

            if wdt:
                wdt.feed()

            request = client.recv(1024).decode('utf-8')
            request = str(request)
            # print('Request:', request.split('\n')[0])  # Print first line
        
            response = handle_request(request, bme, board_temp, wdt=wdt)

            if wdt:
                wdt.feed()
        
            client.send(response.encode('utf-8'))
            client.close()

        except OSError as e:
            if e.args[0] != 110:  # 110 is ETIMEDOUT, which is expected
                print('Connection error:', e)
        except Exception as e:
            print('Unexpected error:', e)
        finally:
            # Always close client if it was created
            if client:
                try:
                    client.close()
                except:
                    pass

if __name__ == "__main__":
    # Initialize watchdog (8 seconds timeout)
    wdt = WDT(timeout=8000)

    try:
        # Connect to WiFi first
        wificonnector = WiFiConnector()

        if wificonnector.connected:
            # Small delay to ensure connection is stable
            time.sleep(2)

        sock = wificonnector.open_socket()
        sock.settimeout(2.0)
        
        # Start the server
        try:
            run_server(sock=sock, wdt=wdt)
        except Exception as e:
            print(f"Server error: {e}")
    except Exception as e:
        print(f"Fatal error: {e}")
        # restart after delay
        import machine
        time.sleep(10)
        machine.reset()

