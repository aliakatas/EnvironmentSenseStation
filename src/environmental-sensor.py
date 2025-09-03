from machine import Pin, I2C
from bme280 import BME280

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000) # Using GP16 and GP17

# Initialize the BME280 sensor
bme = BME280(i2c=i2c, address=0x77)

# Read and print sensor data
print("Reading environmental data from BME280 sensor...")
print(bme.values)
