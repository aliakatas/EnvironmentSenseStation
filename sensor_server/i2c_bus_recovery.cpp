#include "i2c_bus_recovery.h"

#include <Wire.h>
#include "driver/gpio.h"

#define I2C_SDA_PIN 21   // adjust to your wiring
#define I2C_SCL_PIN 22   // adjust to your wiring


void i2c_bus_recover(void) {
    Wire.end();  // release the pins from the Wire peripheral first

    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, INPUT); // let SDA float so we can see if a device is holding it low

    // Toggle SCL up to 9 times to unstick a device holding SDA low
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        if (digitalRead(I2C_SDA_PIN) == HIGH) break; // bus freed early
    }

    // Generate a manual STOP condition
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SDA_PIN, HIGH);

    // Re-initialise Wire and your sensor
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(10);
}