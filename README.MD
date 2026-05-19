# Flipper Zero Thermal Camera

This is a thermal camera module for the Flipper Zero!

It uses the MLX90640 thermal sensor, with a *whopping* resolution of 32x24, and a suite of other auxiliary sensors, including:
- VEML7700 Light Sensor
- ICM-20948 IMU
- SHT31 Temperature & Humidity Sensor

All 4 sensors communicate to the flipper via it's I2C bus.

### The PCB
This is a simple two-layer PCB that mounts the MLX90640 and VEML7700 sensors in the front, and the ICM-20948 and SHT31 on the back.
It uses two 90-degree header pins to connect to the flipper zero's GPIO.

![render](https://i.imgur.com/VbunBRB.png)
![pcb](https://i.imgur.com/DS2qZjv.png)
![schematic](https://i.imgur.com/972ToW5.png)

**Why no case?** Most flipper PCB mods don't use a case to show off the PCB. I think it especially goes better with my flipper as it has a clear case mod.

### The App

I have the app bootstrapped in [flipper-app](/flipper-app). It's not complete yet as I cannot test without the PCB.
