# Barometer
Crowpanel 5" Barometer with garbage day reminder. Simple, two trick pony...
----

V1.7 update:
* Fixed weekday, month, day, year display line. Artifacts left over from single day digit.
* Added OTA (over the air update) so updated code can be uploaded via wifi
----
I originally used an Arduino MEGA with a ColdTears 5" display in 2012 as a barometer and it's a little long in the tooth now and three years prior to starting with scratch building my first delta 3D printer (that's still running with slightly less than 10,000hrs on it)...

<img width="350" height="216" alt="Baro_ORG" src="https://github.com/user-attachments/assets/6a9b94b5-e9a8-4044-b08c-88e8abb41906" />

Time to update it. In hunting around for a 5" TFT display I happened across the Crowpanel 5" TFT with the build in ESP32S3. So that was the start. After toying with LVGL and getting nowhere I decided to use libraries that I had worked with before and were at least familar with. All compiled using the Arduino IDE 2.3.8.

The single drawback to this code is that it negates the touch screen portion. In order to be able to read the I2C BMP280/ATH20 I had to "nop" out the touch portion of the gxf code. Hence thats why there is a special gfx_conf.h included with this project.

<img width="350" height="293" alt="FaceShot" src="https://github.com/user-attachments/assets/63798005-9f29-4dc6-ae83-51b087489e94" />

<h4>Wifi Setup</h4>

After getting the code uploaded, Wifi manager will take over and create an access point. Use your phone/tablet/etc to look for a wifi network called "Barometer". Connect to it and it should take you to the familar wifi setup where you select your own wifi network for it connect to.

<h4>Libraries Required</h4>

LovyanGFX (using 1.2.19 at this point in time)

The remaining libraries are standard time, SDCARD, and WIFI libaries for the ESP32. In the Arduino you're looking for the <b>ESP32S3 Dev Module</b> setting for the processor.

The Partition Scheme is HUGE APP setting (3MB, no OTA, 1MB spiffs)

<h4>Graphics</h4>

For the graphics, I used a microSD card, formatted, and placed the BMP graphics at the root level.

<h4>Garbage Reminder</h4>

Here Organic garbage is collected every week but recycling and regular garbage will alternate. Garbage day is wednesday so the reminder shows up on tuesdays on. Not exactly rocket science there...

<h4>Personalizing It</h4>

If you want to use the same code base, you'll need to change a few things. First is the timezone you're in. All the zones can be found here. There's a timezoneoffset variable in the code and it's strictly used for the sunrise/sunset times.

https://github.com/JChristensen/Timezone

Next up is your longitude and latitude. This is a good source to help with that:

https://www.latlong.net

Last is the trigger day for the garbage reminder (variable name triggerDAY). If you don't want to use it, set to a number like 8 or 9 since the code will only look for days between 1 and 7.

<h4>Custom Case</h4>

Custom STL files are provided if you want to print your own case. I used M3x20 with heat inset nuts for the face/back.

<img width="350" height="290" alt="face_inset" src="https://github.com/user-attachments/assets/932e6fed-74f5-47af-be5a-359a60ecb778" />

M2 x 6 for the BMP280/ATH20 mount.

<img width="350" height="327" alt="rearSesnor" src="https://github.com/user-attachments/assets/390bd8ef-9bf7-4b3c-8a31-693275e666cc" />

