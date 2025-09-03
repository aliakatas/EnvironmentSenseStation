# Import libraries
from wifi_connector import WiFiConnector
from board_temp_sensor import BoardTempSensor, celsius_to_farenheit
from machine import Pin, I2C
from bme280 import BME280
import time
import json

# Set up the sensors
# This is the on-board temperature sensor
board_temp = BoardTempSensor()

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) # Using GP16 and GP17

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)   # by default, the address should have been 0x76, however, my sensor is using the alternate

def read_sensors():
    """Read your sensor data and return as dictionary"""
    temperature, pressure, humidity = bme.environmental_parameters()
    
    sensor_data = {
        "timestamp": time.time(),
        "board_temperature_C": board_temp.temperatureC(),
        "board_temperature_F": board_temp.temperatureF(),
        "temperature_C": temperature,
        "temperature_F": celsius_to_farenheit(temperature),
        "humidity": humidity,
        "pressure": pressure,
        "status": "ok"
    }
    return sensor_data


def create_http_response(data):
    """Create HTTP response with JSON data"""
    json_data = json.dumps(data)
    
    response = f"""HTTP/1.1 200 OK
        Content-Type: application/json
        Content-Length: {len(json_data)}
        Connection: close

        {json_data}"""
    return response


def handle_request(request):
    """Parse request and determine response"""
    lines = request.split('\n')
    if len(lines) > 0:
        method_line = lines[0]
        if 'GET /sensors' in method_line:
            sensor_data = read_sensors()
            return create_http_response(sensor_data)
        elif 'GET /' in method_line:
            # Simple index page
            html = """HTTP/1.1 200 OK
                Content-Type: text/html

                <html><body>
                <h1>Pico 2 W Sensor Server</h1>
                <p><a href="/sensors">Get Sensor Data (JSON)</a></p>
                </body></html>"""
            return html
    
    # 404 response
    return """HTTP/1.1 404 Not Found
        Content-Type: text/plain

        Not Found"""


try:
    wificonnector = WiFiConnector()

    connection = wificonnector.open_socket()
    
    while True:
        client = connection.accept()[0]
        request = client.recv(1024)
        request = str(request)
        #print(request)
        
        response = handle_request(request)
        
        client.send(response.encode('utf-8'))
        client.close()
    
except Exception as e:
    print(e)
