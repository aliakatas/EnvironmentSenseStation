# Import libraries
from wifi_connector import WiFiConnector
from board_temp_sensor import BoardTempSensor, celsius_to_farenheit
from http_stuff import handle_request
from machine import Pin, I2C, ADC
from bme280 import BME280
import time
import json

# Set up the sensors
# This is the on-board temperature sensor
board_temp = BoardTempSensor()

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) 

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)   # by default, the address should have been 0x76, however, my sensor is using the alternate

# Initialize ADC on GP26 
soil_sensor = ADC(Pin(26))

def run_server(sock):
    """Run the HTTP server to serve sensor data"""

    while True:
        try:
            client, remote_address = sock.accept()
            print('Client connected from', remote_address)

            request = client.recv(1024).decode('utf-8')
            request = str(request)
            # print('Request:', request.split('\n')[0])  # Print first line
        
            response = handle_request(request, bme, board_temp, soil_sensor)
        
            client.send(response.encode('utf-8'))
            client.close()

        except OSError as e:
            print('Connection error:', e)
            client.close()

if __name__ == "__main__":
    try:
        # Connect to WiFi first
        wificonnector = WiFiConnector()

        if wificonnector.connected:
            # Small delay to ensure connection is stable
            time.sleep(2)

        sock = wificonnector.open_socket()
        
        # Start the server
        try:
            run_server(sock=sock)
        except Exception as e:
            print(f"Server error: {e}")
    except Exception as e:
        print(f"Fatal error: {e}")
        # restart after delay
        import machine
        time.sleep(10)
        machine.reset()

