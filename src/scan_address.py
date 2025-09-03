from machine import Pin, I2C

# Initialize I2C bus
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000)
print(f'The address of the sensor is {hex(i2c.scan()[0])}')
