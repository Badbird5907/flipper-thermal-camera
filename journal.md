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
---
I've finished placing components and routing the PCB. I'll spend the next few days refining the design and adding other things too 

![img](https://cdn.hackclub.com/019e10e4-ce4c-7e44-921d-279c907bfae1/image.png)

---
Just spent a stupid amount of time fixing the edge.cuts layer and adding rounded corners
![rounded corners](https://cdn.hackclub.com/019e1101-269d-7286-9ed5-5ee2d2b27972/image.png)

---

I finished up the PCB design and added some testpoints. Next, i'll work on sourcing the parts, and then i'll design the case.

![pcb render](https://cdn.hackclub.com/019e1318-7207-750a-850a-dd007c84f362/image.png)

# 05/18

I sourced all the parts and created a bom (BOM.csv). I also had to spend some time reworking the connector in the schematic, but we're good to go now :)

Next, i'm going to get started on the flipper app

Btw, i've decided against designing a case, as it kind of goes against the flipper's aesthetic (especially mine, which has a clear case mod). Most pcb mods on top have no case to showcase the PCB, so I think it's best if I just don't make one.

# 05/19

Okay, I think this is as far as I can get it until I get the PCB manufactured and built. There's some sample code that probably won't work in [flipper-app](/flipper-app)

# 05/24

Updated the silkscreen with a better/larger stylized "title" inspired by the video game module.
This took over 2-3 hours to do because I had first designed it in affinity designer.
After fiddling around with vectors, I finally got something that looks half-decent. Importing the vector into KiCad was the hardest part.
Importing it directly into KiCad made the black background take over the white text (kind of a skill issue on my part). The closest I got 
was to convert the SVG into a DXF and importing that. It gave me a outline but no background.
I then spent the next hour fiddling around with the DXF in autodesk trying to fill in the text, but nothing really looked good (DXF conversion
dropped the curve resolution too much).

I finally gave up and imported a SVG of the border only into KiCad, then used the Text tool to add the text (Knockout on), and then used the
polygon tool to fill in the border. Not the best solution, but it's the best I could come up with.

![affinity](https://cdn.badbird.dev/screenshots/2026/05/EuB7UVnwpR.png)

![KiCad](https://cdn.badbird.dev/screenshots/2026/05/zjmn41jP9w.png)
![Render](https://cdn.badbird.dev/screenshots/2026/05/WPVof91aNT.png)


# 06/07

Had to switch out the existing IMU for a BNO055 as it went out of stock. Using the BNO055 is a upgrade anyway, it can provide both raw and fused 9-axis sensor data over I2C as it has its own ARM processor.

Also added more capacitors for filtering and a diode to prevent frying the LDO by accident with a short, and a diode array to protect the data pins from ESD.
