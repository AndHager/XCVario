# OpenIVario
Software for ESP32 based lean Variometer system with OpenVario Interface running on XC Vario hardware

![Vario]( https://raw.githubusercontent.com/iltis42/OpenIVario/master/images/Vario3D/vario-perspectiv.png )
![Vario Back]( https://github.com/iltis42/OpenIVario/blob/master/images/Vario3D/vario-perspectiv-back.png )

The project is dedicated to my open source and open hardware flight computer development using ESP32 WROOM development board plus modern sensors such as BMP280, DS1820b, MP5004DP. 

* Evolution
The history of the project is is available within branches and is listed below. 

1. [DM-R1](https://github.com/iltis42/OpenIVario/tree/DM-R1) branch
The inital 'Dot Matrix' release 1 features a 128x64 pixel monocrome LCD display. It delivers already all data in the well known and documented OperVario format to any flight computer running XC-Soar, but had limitation's in grafical resolution. Beside vario, speed to fly command arrow and altitude, further informations like battery voltage, temperature and MC value were mapped to a single field that changes every 3 seconds. A handful of prototypes has been build and tested in air with several flight computers. The sensor board with 47x102 mm hosts several modules and is mounted horizontally in a cyclindric housing through the I-Panel, while the display keeps in front. Also the first revision had a responsive kalman filter, that delivers a smooth response without the typical delay of several seconds in simple low pass filters. The vario is optimized to display exactly what you feel when entering lift.

2. [LV-R2](https://github.com/iltis42/OpenIVario/tree/LV-R2) branch
The 'Long Vario' release 2 features a new technology 320x240 pixel IPS LCS display, readable from almost any angle (80/80/80) and with 500 nits brightness it performs well in sunlight. The sensor board and mostly the housing are the same as in the DM-R1 type, but as the display has higher resolution and features color, the arrangement of data on the display has been reworked, and more things were added like a moving gauge for airspeed (IAS), a permanant display of MC value, temperature and battery charge state and bluetooth connection indication. The color is used to visualize positive and netative values. A low number of prototypes has been made by use of a 3D printed ABS housing, shielded by copper foil and mounted from front of I-Panel. 

3. master branch (default, what you see here)
The third generation 'XC Vario' uses the same display as the long vario, but comes with a new sensor board 61x64 mm, vertically mounted behind the display board. The new sensor board has a RJ45 connector for electrical connections, digital potentiomenter, and some more features like software update over the air, serial output and serial to bluetooth bridge and connector for external audio. It still has an and efficient loudspeaker on board, and uses a just 34 mm long CNC fabricated aluminium housing that is mounted from back of the I-Panel as any other standard instrument. Prototypes under test performed well, and three digit number of boards have been fabricated and can be purchased now via http://xcvario.de webshop that is selling the hardware for an interresting price.

[List of Software Releases] (https://github.com/iltis42/OpenIVario/releases/), latest release on the top
[Ongoing Development] (https://github.com/iltis42/OpenIVario/tree/master/images), latest beta software is the one with the highest revision number or newest date

At the begin i want to show a short Demo of the variometer, including sensitivity and full sunlight test, followed by an explanation of various setup options and how to use. Its now year four of this project, demo videos with prototypes based on the hardware from LV-R2 branch see here below:


* [Quick Demo on YouTube](https://www.youtube.com/watch?v=Piu5SiNPaRg)
* [Vario Sensitivity Test](https://www.youtube.com/watch?v=RqFLOQ9wvgY)
* [Basic Function Demo](https://www.youtube.com/watch?v=zGldyS57ZgQ)
* [Full Sunlight test](https://www.youtube.com/watch?v=TFL9i2DBNpA)
* [Audio Demo in Flight](https://www.youtube.com/watch?v=6Vc6OHcO_T4)  ("Single Tone" Modus)
* [Basic Vario Setup MC, QNH, Ballast](https://www.youtube.com/watch?v=DvqhuaVlfEI)
* [Airfield Elevation and Vario Setup](https://www.youtube.com/watch?v=x3UIpL9qGec)
* [Bluetooth, Audio and Polar Setup](https://www.youtube.com/watch?v=9HcsfyLX-wE)
* [Flap (WK) Indicator Setup](https://www.youtube.com/watch?v=tP2a2aDoOsg)
* [Vario System Setup](https://www.youtube.com/watch?v=BCR16WUTwJY)
* [Setup XCSoar to connect with Vario](https://www.youtube.com/watch?v=LDgnvLoTekU&t=95s)



The design supports the follwing sensors:
* TE Variometer
* Airspeed
* Barometric Altitude
* Outside Temperature
* Battery Voltage

The ESP32 module contains a bluetooth sender module, so we are able to transmit all data to XCSoar in [OpenVario format](https://github.com/iltis42/OpenIVario/blob/master/putty.log), so XCSoar can operate as full glide computer with TE-Vario, Barometric Altitude, Speed and more.

* [Handbuch Deutsch](https://github.com/iltis42/OpenIVario/blob/master/handbook/Handbuch-D.pdf)

* [User Manual English](https://github.com/iltis42/OpenIVario/blob/master/handbook/Usermanual.pdf)

The Vario Prototype with 500 nits 2.4 inch IPS Technology LCD Display features a low power consumation of less that 1W, that equals to 70mA at 12.5V operating voltage. The Size is 68(H)x64(B)x35(T) mm, for a standard 57mm Instrument Panel. Weight is below 300 gramm.

The Soft- and Hardware features:

- Easy QNH and McCready Adjustment
- Audio-Generator with adjustable Volume and Deadband plus setup option for frequency tone style and range
- Integrated Loudspeaker 
- Flap Position Indicator Option
- S2F (Speed2Fly) Indicator with configurable MC Ballast and Bugs based on customizable Polar
- More that 80 Polars are already included ( list in https://github.com/iltis42/OpenIVario/blob/master/main/Polars.cpp )
- IAS (Indicated Airspeed) Indicator
- Temperatur Indicator
- Battery Charge Indicator
- Variometer display with adjustable range (1 m/s - 30 m/s), and damping ( 1s - 10s )
- Bluetooth Interface for Android devices running XC Soar (OpenVario or Borgelt protocol to sync from or to the device)
- High precision barometric Altimeter 1 hPa (8 meter) absolute accuracy
- Sunlight readable 500 nits 2.4 inch IPS Technology Display
- Battery Voltage Indicator
- Audio Switch for Vario/S2F Audio
- Detailed Setup Menu
- Serial to Bluetooth Bridge e.g. for FLARM
- Serial Output of OpenVarion Format NMEA data
- Software Update OverTheAir (OTA) via WiFi Access Point

