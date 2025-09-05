# EnvironmentSenseStation
Environmental parameters sensors driven by a Raspberry Pico 2 W.

## Features
- Measure ambient temperature, pressure, and humidity.
- Measure soil moisture.
- Measure board temperature.
- Serve data over LAN using the onboard WiFi.

## Hardware
- Raspberry Pi Pico 2 WH from [The Pi Hut](https://thepihut.com/products/raspberry-pi-pico-2-w?variant=54063378760065). Product [specs](https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf).
- BME280 sensor from [The Pi Hut](https://thepihut.com/products/bme280-environmental-sensor). Product [specs](https://www.waveshare.com/wiki/BME280_Environmental_Sensor).
- Capacitive soil moisture sensor (v2.0) from [The Pi Hut](https://thepihut.com/products/capacitive-soil-moisture-sensor).
- Breadboard
- Jumper cables
- Power supply (for independent operation)

### Dependencies
To be able to talk to the sensor, we need the following library:
- [Micropython BME280](https://pypi.org/project/micropython-bme280/)

The library script [bme280.py](./src/bme280.py) has been modified by adding the following functions/properties to the class to make value reading easier:
- environmental_parameters: function, returns the parameters below as a tuple
- temperature: property (C)
- humidity: property (%)
- pressure: property (hPa)

## Connect the sensors
The BME280 sensor from Waveshare has 6 pins and can be used with I2C or SPI. 
This project is using the I2C implementation.

| Function Pin | Controller Slot | Description |
| -------- | ------- | ------- |
| VCC | 3.3V / 5V | Power input |
| GND | GND | Ground |
| SDA | SDA | I2C data line |
| SCL | SCL | I2C clock line |
| ADDR | NC/GND | Address chip select (default is high): When the voltage is high, the address is 0 x 77. When the voltage is low, the address is: 0 x 76 |
| CS | NC | Used for SPI mode |

Based on the image below:
![](https://docs.micropython.org/en/latest/_images/pico_pinout.png)

We can use the following pin locations:
- #1 for SDA
- #2 for SCL
- #38 for GND
- #36 for VCC

Similarly, for the soil moisture sensor which has 3 pins, the pins used are the following:
- #31 for ADC
- #38 for GND
- #36 for VCC

### Calibrate soil moisture sensor
As this is an analogue sensor, you need to get some data to calibrate values for normal operation.
Using the [soil_sensor_test](./src/soil_sensor_test.py) script, you can collect data for the sensor in dry and fully wet conditions.
The test can be run in open air or by sticking the sensor in a pot.

In open air, the sensor reads:
- dry: 16%
- wet: 67%

In soil, the readings are below:
- dry: TBC
- wet: TBC

## Operation
Before starting, make sure to install the latest firmware for the controller using the [official site](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html).
Then, transfer the contents of the [src](./src/) folder to the Pico and test with Thonny to confirm there are no surprises.

Once the server is running on the controller, clients can request data by sending requests to "controller-IP/sensors".
The response is in json format as follows:
```json
{
   "timestamp": {
      "value": <seconds since reference>,
      "unit": "seconds",
      "reference": <array in the form [YYYY, MM, DD, hh, mm, ss]>
   },
   "board_temperature": {
      "value": <temperature>,
      "unit": "C"
   },
   "temperature": {
      "value": <temperature>,
      "unit": "C"
   },
   "humidity": {
      "value": <humidity>,
      "unit": "%"
   },
   "pressure": {
      "value": <pressure>,
      "unit": "hPa"
   },
   {
      "soil": {
         "value": <moisture>,
         "unit": "%"
      }
   },
   "status": "ok"
}
```

You may still contact the server through "controller-IP". However, a simple webpage will appear with a link directing to the sensors' endpoint.

----

## Tips
### Discover address of sensor
If after wiring the sensor the script fails mentioning it can't find the address, it is worth running the [scan script](./src/scan_address.py).

### Discover local IP address of controller
After the LED on the Pico turns on steady, go to your router's admin page and check for the connected devices - there should be an entry for "Pico2W".

