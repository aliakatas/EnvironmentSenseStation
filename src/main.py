# Import libraries
from wifi_connector import WiFiConnector
from board_temp_sensor import BoardTempSensor
from http_stuff import handle_request
from machine import Pin, I2C, WDT
from bme280 import BME280
import time
import gc

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
            client.settimeout(3.0)  # Timeout for client operations
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
            print('Response sent to', remote_address)
            client.close()
            print('Client connection closed')

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
            
            # # Run garbage collection to free memory
            # print("\nAllocated memory: {} KB\nFree memory: {} KB".format(gc.mem_alloc() / 1024, gc.mem_free() / 1024))
            # gc.collect()


if __name__ == "__main__":
    
    # Check for Ctrl+C to enter REPL
    print("Starting... Press Ctrl+C within 5 seconds to enter REPL")
    try:
        for _ in range(50):
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("Entering REPL")
        raise
    
    # Initialize watchdog (8 seconds timeout)
    wdt = WDT(timeout=8000)

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
        # print("Restarting machine...")

