# 05/09
Initialized this project and started designing the schematic.

I set up just the basic things needed to drive the MLX90640, which is the sensor I decided to go with.
It's really easy to use (over I2C), and has a (low) resolution of 32x24 pixels.

I added a XC6206P332, which is a 3.3v LDO regulator, which steps down the voltage from the 5v Flipper Zero bus to 3.3v.
The reason why we don't use the Flipper's 3.3v line is because the flipper's SD card shares that line, and it's best we avoid using it if possible.

I also added 2.2k pull-up resistors for the I2C lines, and some LEDs to indicate the power status.
Next i'm going to add some other sensors, like a ambient light sensor.

# 05/10

I finished the schematic. I added a light sensor (SHT31), an ambient light sensor (VEML7700), and a IMU (ICM-20948) which alos includes a 3-axis compass.

I'm now working on designing and laying out the PCB.
