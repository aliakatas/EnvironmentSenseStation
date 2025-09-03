from machine import ADC
from utilities import celsius_to_farenheit

class BoardTempSensor:
    
    def __init__(self):
        self.sensor_temp = ADC(4)
        
    def temperatureC(self):
        temp_conversion_factor = 3.3 / (65535)
        reading = self.sensor_temp.read_u16() * temp_conversion_factor
        
        temperature = 27 - (reading - 0.706) / 0.001721
        
        return temperature
        
    def temperatureF(self):
        return celsius_to_farenheit(self.temperatureC())