from machine import Pin, I2C, ADC
from bme280 import BME280
from capacitive_soil_sensor import get_soil_moisture

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) 

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)

# Initialize ADC on GP26 (you can use GP27 or GP28 instead)
soil_sensor = ADC(Pin(26))

# Read and print sensor data
print("Reading environmental data from BME280 sensor...")
print(bme.values)
print(f"Soil moisture: {get_soil_moisture(soil_sensor)}%")
