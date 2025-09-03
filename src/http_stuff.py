import json
import time
from utilities import celsius_to_farenheit

def read_sensors(bme_sensor, board_sensor):
    """Read your sensor data and return as dictionary"""
    temperature, pressure, humidity = bme_sensor.environmental_parameters()
    
    sensor_data = {
        "timestamp": time.time(),
        "board_temperature_C": board_sensor.temperatureC(),
        "board_temperature_F": board_sensor.temperatureF(),
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


def handle_request(request, bme_sensor, board_sensor):
    """Parse request and determine response"""
    lines = request.split('\n')
    if len(lines) > 0:
        method_line = lines[0]
        if 'GET /sensors' in method_line:
            sensor_data = read_sensors(bme_sensor, board_sensor)
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
