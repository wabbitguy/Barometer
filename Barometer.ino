/**************************CrowPanel ESP32 HMI Display Example Code************************
Version     :	1.1
Suitable for:	CrowPanel ESP32 HMI Display but you have to kill the touch
orginally done in 2012 with a ColdTearsElectronics 5" 800x480 display on an arduino MEGA
CrowPanel 5" is 800 x 480
**************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>  // Include the WiFiManager library
#include <WebServer.h>
#include <ESPmDNS.h>
#include <TimeLib.h>   //https://github.com/PaulStoffregen/Time
#include <Timezone.h>  //https://github.com/JChristensen/Timezone
#include <TimeLord.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <SD.h>          // For microSD card access
#include "gfx_conf.h"    // this is the display panel config
#include <ArduinoOTA.h>  // added OTA updating

//Modify the corresponding pin according to the circuit diagram.
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10

SPIClass SD_SPI;
File rootSDcard;
//
#define HOSTNAME "Barometer"
#define VERSION "1.8"
//
Adafruit_AHTX0 aht;

Adafruit_BMP280 bmp;  // I2C
int alt1, alt2;
int bar1, bar2;
float YVRinHg = 101769.70;
float meters2FT = 3.28084;
float pressure, kPa, mBars, inches, tempC, tempF, myHumidity;
//
//Edit These Lines According To Your Timezone and Daylight Saving Time
//TimeZone Settings Details https://github.com/JChristensen/Timezone
//US Pacific Time Zone (British Columia changes to forever bcPDT as of March 8, 2026)
TimeChangeRule bcPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };  // 7 hour offset
TimeChangeRule bcPST = { "PST", First, dowSunday, Nov, 2, -480 };   // 8 hour offset
String fallTZLabel = "PST";// this is the fall time change if you have one
Timezone timeZoneRule(bcPDT, bcPDT);

//Pointer To The Time Change Rule, Use to Get The TZ Abbrev
TimeChangeRule *tcr;
time_t utc;
int8_t timeZoneOffset = -7;  // Pacific Daylight Time
//
// west longitude and north latitude - approximate
// Note TimeLord is only accurate to within 5 minutes
float LONGITUDE = -122.76604;
float LATITUDE = 49.14924;
//
int sunRiseMinute = 0;
int sunRiseHour = 0;
int sunSetHour = 0;
int sunSetMinute = 0;
int address_OFF_Minute, address_ON_Minute;  // number of minutes into the day for sunrise, sunset. leave these alone
char sunRiseTime[30];
char sunSetTime[30];
char clockTime[30];
char calendar[30];
const String shortDOW[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const String shortMON[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
uint16_t baroHeadingsY = 220;
//
uint8_t triggerDAY = 3;   // Pick the day to aleart you...Sun = 1, Mon = 2, etc
bool runOnce = false;     // this sets up the check for trigger days
bool showingCan = false;  // used to know when we are showing the garbage/recycle cans
//
//NTP Server http://tf.nist.gov/tf-cgi/servers.cgi
static const char ntpServerName[] = "pool.ntp.org";
WiFiUDP Udp;
// Local Port to Listen For UDP Packets
uint16_t localPort;
uint8_t ntpUpdateFrequency = 121;  // update the time every x minutes
//
uint8_t lastSecond = 99;
uint8_t lastMinute;
uint8_t lastHour;
uint8_t lastDay;
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myWeekDay, myMonth;
uint16_t myYear;
bool display24HR = false;  // we don't want to show 24hr time by default
//
float hourlyReading[24];  // for the hourly readings
float hourlyMb[24];       // millibars for the pressure change readings
int xMargin = 110;        // inset of graph
int masterScale = 0;
//
void handleClock();                                                                                      // handles all the clock, baromemter/garbage day stuff
void handle_BarReadings();                                                                               // saves all the barometer graph
void showBarometerHeadings();                                                                            // shows the current pressure etc
void readATH20();                                                                                        // read the humditiy and temperature
void bmpRead();                                                                                          // from BMP280
void getSunTimes(uint8_t theDay, uint8_t theMonth, int theYear, uint8_t theDOW, uint8_t dstCorrection);  // sunrise/set
void drawWiFiQuality();                                                                                  // graph for wifi
int8_t getWifiQuality();                                                                                 // signal strength
time_t getNtpTime();                                                                                     // get from time server
void sendNTPpacket(IPAddress &address);                                                                  // ask for time
void showGraphLabels();                                                                                  // sunrise/set etc
void showPressureGraph();                                                                                // shows the graph of pressure (L->R)
void drawChangeGraphic();                                                                                // graphic bmp to show steady/rise/fall in pressure 3hr window
void printDirectory(File dir, int numTabs);                                                              // just to display filenames on the SD card
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y);                                    // actual drawing routine
uint16_t read16(fs::File &f);                                                                            // routines to read bytes etc
uint32_t read32(fs::File &f);
bool newCanTest();  // look at the day to see what can goes out
//
void setup() {
  Serial.begin(115200);
  //
  Wire.begin(19, 20);  // we need these pins for I2C
  //
  aht.begin();  // start up the sensor
  bmp.begin();  // start up the second sensor
  delay(50);
  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
                                                    //
  SD.begin();
  rootSDcard = SD.open("/");

  delay(2000);

  //Display Prepare
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  //
  //WiFiManager
  //Local intialization.
  WiFiManager wifiManager;
  //AP Configuration
  wifiManager.setHostname("Barometer");  // sets the custom DNS name that appears in your router
  wifiManager.setAPCallback(configModeCallback);
  //Exit After Config Instead of connecting
  wifiManager.setBreakAfterConfig(true);

  if (!wifiManager.autoConnect("Barometer")) {
    delay(3000);
    ESP.restart();
    delay(5000);
  } else {
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    // Seed Random With values Unique To This Device
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    String ipaddress = WiFi.localIP().toString();
    Udp.begin(localPort);  // port to use
    setSyncProvider(getNtpTime);
    //Set Sync Intervals
    setSyncInterval(ntpUpdateFrequency * 60);
  }
  tft.fillScreen(TFT_BLACK);
  //
  // OTA Setup
  // String hostname(HOSTNAME);
  // WiFi.hostname(hostname);
  ArduinoOTA.setHostname("Barometer");
  // ArduinoOTA.setPassword((const char *)"12345");
  ArduinoOTA.begin();
}
//
void loop() {
  static time_t prevDisplay = 0;
  timeStatus_t ts = timeStatus();
  utc = now();
  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      //update the schedule checking only if time has changed
      if (now() != prevDisplay) {
        prevDisplay = now();
        handleClock();  // go handle the schedule
        tmElements_t tm;
        breakTime(now(), tm);
      }
      break;
    case timeNotSet:
      now();
      delay(3000);
      ESP.restart();
  }
  // Handle OTA update requests, so you can update the firmware via Wifi
  ArduinoOTA.handle();
}
//To Display <Setup> if not connected to AP
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.setCursor(100, 200);
  tft.setTextSize(2);                  // sets the text size
  tft.setFont(&fonts::FreeMono9pt7b);  // Set custom font
  tft.print("WiFi Access Point: ");
  tft.setTextColor(TFT_RED, TFT_BLACK);  // character colour and background
  tft.print("Barometer");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // character colour and background
  delay(2000);
}
//
void handleClock() {
  tmElements_t tm;
  breakTime(now(), tm);
  char buffer[24];            // holds the formated time/day/date
  uint8_t dstCorrection = 0;  // default to off
  uint8_t currentWeek;
  uint16_t dayOfTheYear;

  // --- Current local time ---
  time_t utc = now();
  time_t localTime = timeZoneRule.toLocal(utc, &tcr);

  if (String(tcr->abbrev) == fallTZLabel) {// handles a fall time change
    dstCorrection = 1;
  }

  myWeekDay = weekday(localTime);
  myHour = hourFormat12(localTime);
  myDay = day(localTime);
  my24Hour = hour(localTime);    // 24-hour clock
  myMinute = minute(localTime);  // use localTime
  mySecond = second(localTime);  // use localTime
  myYear = year(localTime);
  myMonth = month(localTime);  // 1 to 12

  struct tm *timeinfo;
  timeinfo = localtime(&localTime);

  dayOfTheYear = timeinfo->tm_yday + 1;  // tm_yday is 0-365
  currentWeek = ((timeinfo->tm_yday + 7 - timeinfo->tm_wday) / 7) + 1;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);  // character colour and background
  // add code here to print the date info
  if (display24HR == true) {
    sprintf(buffer, "%02u:%02u:%02u", my24Hour, myMinute, mySecond);  // kill the leading 0 on the hours
  } else {
    sprintf(buffer, "%02u:%02u:%02u", myHour, myMinute, mySecond);  // kill the leading 0 on the hours
  }
  tft.setTextSize(2);                            // see what size it is, yep right
  tft.drawString(buffer, 180, 24, 7);            // Overwrite the text to clear it
  tft.setTextColor(TFT_WHITE, TFT_BLACK);        // character colour and background
  if (lastDay != myDay) {                        // if the day has changed or has been updated, update date info
    tft.fillRect(250, 140, 320, 30, TFT_BLACK);  // erase anything there (font overlap)
    tft.setTextSize(1);
    tft.setFont(&fonts::FreeSerif18pt7b);  // custom font
    if (myDay < 10) {                      // format slightly different for date less than 10...
      tft.setCursor(276, 140);             // roughly middle
      sprintf(buffer, "%s %s %1u, %4u", shortDOW[myWeekDay - 1], shortMON[myMonth - 1], myDay, myYear);
    } else {
      tft.setCursor(266, 140);  // roughly middle
      sprintf(buffer, "%s %s %2u, %4u", shortDOW[myWeekDay - 1], shortMON[myMonth - 1], myDay, myYear);
    }
    tft.printf(buffer);  // display the calendar info
  }
  //
  if (myWeekDay == triggerDAY) {              // we are on a tuesday
    if (showingCan == false) {                // we ONLY need to check once on a Tuesday
      if (newCanTest(currentWeek) == true) {  // true is it's a recycle day (even week), false is garbage
        drawBmp(SD, "/trash.bmp", 54, 26);    //display the image 96 x 96, 24bit
        //Serial.println("trash");
      } else {                                // garbage day
        drawBmp(SD, "/recycle.bmp", 54, 26);  //display the image 96 x 96, 24bit
        //Serial.println("recycle");
      }
      showingCan = true;  // dont do it once on trigger day, then check following day
    }
  } else {                                      // we are any day of the week BUT tuesday
    if (showingCan == true) {                   // if this is true we have a different day of the week
      tft.fillRect(54, 26, 96, 96, TFT_BLACK);  // this clears the whole graphic
      showingCan = false;                       // reset the flag so we run it next trigger day in a week
    }
  }
  //
  if (runOnce == false) {
    getSunTimes(myDay, myMonth, myYear, myWeekDay, dstCorrection);  // checks only at a specific time
    runOnce = true;                                                 // dont do it again until 2am
  } else {
    if (my24Hour == 2 && myMinute == 00 && mySecond < 2) {
      getSunTimes(myDay, myMonth, myYear, myWeekDay, dstCorrection);  // checks only at 2AM and less than 2 seconds)
    }
  }
  //
  if (myMinute != lastMinute) {  // go read the BMP and AHT every minute
    bmpRead();                   // read the pressure
    readATH20();                 // read the temp and humidity (indoors)
  }
  //
  if (lastHour != my24Hour) {
    handle_BarReadings();  // go update the bargraph array
  }
  //
  if (mySecond == 15) {
    drawWiFiQuality();  // go update the wifi strength graphic once a minute
  }
  lastSecond = mySecond;  // save it for next time
  lastMinute = myMinute;  // save this for the next pass
  lastHour = my24Hour;    // save for the next pass
  lastDay = myDay;        // save the day...
}
//
void handle_BarReadings() {
  bool didFind = false;                                                // a flag to control placing
  for (uint8_t findBlank = 0; findBlank < 24; findBlank++) {           // we look for the first empty spot
    if (hourlyReading[findBlank] == 0) {                               // did we find one?
      if (didFind == false) {                                          // make sure we don't add it twice
        hourlyReading[findBlank] = hourlyReading[findBlank] = inches;  // save result
        hourlyMb[findBlank] = mBars;                                   // and for the pressure
        didFind = true;                                                // so we only add one to the array
      }
    }
  }  // at this point we've either added to the array or not, the didFind flag will tell us
  //
  if (didFind == false) {  // if we didn't find a spot to add the number, shift the array, add it to the end
    for (int theMove = 0; theMove < 23; theMove++) {
      hourlyReading[theMove] = hourlyReading[theMove + 1];  // copy the whole array down
      hourlyMb[theMove] = hourlyMb[theMove + 1];            // copy the whole array down
    }
    hourlyReading[23] = inches;  // save the reading in inches for this hour
    hourlyMb[23] = mBars;        // save the millibars to end of array
    //printDirectory(rootSDcard,0);// works perfect
  }
  showPressureGraph();  // update the graph
}
//
void showBarometerHeadings() {
  char displayUT[20];
  // if (showCalendar==false) {
  tft.setTextSize(1);                  // sets the text size
  tft.setFont(&fonts::FreeMono9pt7b);  // Set custom font
  tft.setCursor(110, baroHeadingsY);   //set cursor
  sprintf(displayUT, "inHG:%2u.%02u", bar1, bar2);
  tft.print(displayUT);
  tft.setCursor(360, baroHeadingsY);
  tft.print("m:" + String(mBars));  // mBars
  tft.setCursor(592, baroHeadingsY);
  tft.print("kPa:" + String(kPa));
  //  tft.Put_Text("inHG:", 110, 230, BVS_22);
  // tft.Put_Text("m:", 360, 230, BVS_22);
  //  tft.Put_Text("kPa:", 592, 230, BVS_22);
  //  tft.Set_character_spacing(10);
  // }
}
//
void readATH20() {  // ATH20 is more accurate for temperature
  char displayUT[20];
  sensors_event_t humidity, tempCel;
  aht.getEvent(&humidity, &tempCel);  // populate temp and humidity objects with fresh data
  tempF = (tempCel.temperature * 9.0 / 5.0) + 32.0;
  myHumidity = humidity.relative_humidity;  // get the current humdity indoors
  tft.setTextSize(1);                       // sets the text size
  tft.setFont(&fonts::FreeMono9pt7b);       // Set custom font
  tft.setCursor(230, 450);
  dtostrf(tempF, 4, 1, displayUT);  // the variable, width (numb characters), decimals, buffer
  tft.print("Temp:");               // temperature
  tft.print(displayUT);
  tft.print("F");
  tft.setCursor(470, 450);
  tft.print("Humidity:");
  dtostrf(myHumidity, 2, 0, displayUT);
  tft.print(displayUT);
  tft.print("%");
}
//
//----------------------------Taking Readings from BMP280-------------------------------
void bmpRead() {
  //Remove the " * 3.280839895" for units in meters
  float alt = bmp.readAltitude(YVRinHg) * meters2FT;  //Read Altitude with correction
  alt1 = (int)alt;                                    //Read Altitude digits on the left of the decimal place
  alt2 = (int)((alt - alt1) * 100.0);                 //Read Altitude digits on the right of the decimal place

  pressure = bmp.readPressure();  //Read Barometric Pressure
  float bar = pressure * 0.0002952998751;
  kPa = pressure / 1000;
  mBars = pressure / 100;
  bar1 = (int)bar;                     //Read Altitude digits on the left of the decimal place
  bar2 = (int)((bar - bar1) * 100.0);  //Read Altitude digits on the right of the decimal place
  inches = bar1 + (bar2 * .01);        // this the current pressure reading 29.56 for example
  showBarometerHeadings();             // go show them on the display now
}
//
void getSunTimes(uint8_t theDay, uint8_t theMonth, int theYear, uint8_t theDOW, uint8_t dstCorrection) {
  TimeLord tardis;
  tardis.Position(LATITUDE, LONGITUDE);  // figure out where we are
  tardis.TimeZone(timeZoneOffset * 60);  // your time zone offset
  //
  uint8_t today[] = { 0, 0, 2, theDay, theMonth, theYear };  // 2am
                                                             //
  tft.setTextSize(1);
  tft.setFont(&fonts::FreeSerif12pt7b);  // custom font
  if (tardis.SunRise(today)) {
    //    Serial.println(today[tl_minute], DEC);
    sunRiseHour = today[tl_hour] + dstCorrection;
    sunRiseMinute = today[tl_minute];
    sprintf(sunRiseTime, "Sunrise:%2u:%02u", sunRiseHour, sunRiseMinute);
    tft.setCursor(180, 186);  // left side
    tft.print(sunRiseTime);   // show the sunrise time
  }
  if (tardis.SunSet(today)) {
    //    Serial.println(today[tl_minute], DEC);
    sunSetHour = today[tl_hour] + dstCorrection;
    sunSetMinute = today[tl_minute];
    sprintf(sunSetTime, "Sunset:%2u:%02u", sunSetHour - 12, sunSetMinute);  //the sunSetHour will be in 12hr format
    tft.setCursor(492, 186);                                                //; column, row
    tft.print(sunSetTime);                                                  // show the sunset time
  }
}
//
void drawWiFiQuality() {
  const byte numBars = 5;            // set the number of total bars to display
  const byte barWidth = 3;           // set bar width, height in pixels
  const byte barHeight = 20;         // should be multiple of numBars, or to indicate zero value
  const byte barSpace = 1;           // set number of pixels between bars
  const uint16_t barXPosBase = 775;  // set the baseline X-pos for drawing the bars
  const byte barYPosBase = 20;       // set the baseline Y-pos for drawing the bars
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {  // current bar loop
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {  // draw bar height loop
      for (byte ii = 0; ii < barWidth; ii++) {    // draw bar width loop
        byte nextBarThreshold = (i + 1) * (100 / numBars);
        byte currentBarThreshold = i * (100 / numBars);
        byte currentBarIncrements = (barHeight / numBars) * (i + 1);
        float rangePerBar = (100 / numBars);
        float currentBarStrength;
        if ((quality > currentBarThreshold) && (quality < nextBarThreshold)) {
          currentBarStrength = ((quality - currentBarThreshold) / rangePerBar) * currentBarIncrements;
        } else if (quality >= nextBarThreshold) {
          currentBarStrength = currentBarIncrements;
        } else {
          currentBarStrength = 0;
        }
        if (j < currentBarStrength) {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barColor);
        } else {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barBackColor);
        }
      }
    }
  }
}
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}
//
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress timeServerIP;  // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  //  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0;  // return 0 if unable to get the time
}
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
//
void showGraphLabels() {
  char theNumb[12];
  int x, y;
  tft.setTextSize(1);                               // sets the text size
  tft.setFont(&fonts::FreeMono9pt7b);               // Set custom font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);           // character colour and background
  tft.fillRect(xMargin, 240, 602, 200, TFT_BLACK);  // this clears the whole graph
  //
  for (int i = 0; i < 11; i++) {  // draw horizontal lines
    y = 240 + (i * 20);
    tft.drawLine(xMargin, y, 600 + xMargin, y, TFT_WHITE);
  }
  //Section to draw the vertical lines
  for (int i = 0; i < 25; i++) {
    x = xMargin + (i * 25);
    tft.drawLine(x, 240, x, 440, TFT_WHITE);
  }
  // now show the pressure numbers
  float theVal = 31.00;
  for (y = 0; y < 11; y++) {
    dtostrf(theVal, 5, 2, theNumb);  // the variable, width (numb characters), decimals, buffer
    tft.setCursor(40, 233 + (y * 20));
    tft.print(theNumb);
    theVal -= .20;
  }
}
//
void showPressureGraph() {
  uint16_t totalHeightPixels, theX;
  //
  showGraphLabels();  // show the grid  and scale first
  //((y2 - y1) / y1)*100 = your percentage change. 31.00 to 29.00 is -6.45% change
  //
  float lowVal = 33, highValue = 0;
  int theRange = 0;
  for (int floop = 0; floop < 24; floop++) {
    if (hourlyReading[floop] > highValue) highValue = hourlyReading[floop];  // find the highest number
    if (hourlyReading[floop] < lowVal) lowVal = hourlyReading[floop];        // find the lowest value
  }
  theRange = (highValue - lowVal) * 100;  // 30.11 - 29.52 = .57, * 100= 57
  int myScale = 190 / theRange;           // this will give X pixels PER point (3 for example)
  if (masterScale != 0) {                 // we change the master scale in programming via BT
    myScale = masterScale;                // new variable
  } else {
    if (myScale > 5) myScale = 5;  // limit the maximum spread
  }
  //
  theX = xMargin + 1;                                             // starting X left side
  for (uint8_t myStep = 0; myStep < 24; myStep++) {               // we walk across the display with this
    if (hourlyReading[myStep] != 0) {                             // we check to make sure there is a reading here
      totalHeightPixels = (hourlyReading[myStep] - 29.00) * 100;  // number of vertical pixels to draw
      // Serial.print("myStep:");
      // Serial.print(myStep);
      // Serial.print(" -- ");
      // Serial.print(" HourlyReading:" + String(hourlyReading[myStep]) + " -- ");
      // Serial.println("totalHeightPixels:" + String(totalHeightPixels));
      if (totalHeightPixels > 0 && totalHeightPixels < 200) {                                   // we need something to draw, otherwise, forget it
        tft.fillRect(theX, 241 + (200 - totalHeightPixels), 24, totalHeightPixels, TFT_GREEN);  // works perfectly
      }
    }
    //tft.fillRect(theX, 241, 24, 200, TFT_GREEN);  // works perfectly
    theX += 25;  // head to the next grid
  }
  drawChangeGraphic();
}
//
//
void drawChangeGraphic() {
  uint8_t dataSpot = 0;  // the point we found the last array entry
  uint8_t whichGraphic;
  bool dropping = false;
  //
  //
  for (uint8_t findData = 0; findData < 24; findData++) {
    if (hourlyMb[findData] != 0) {
      dataSpot = findData;  // save the spot (it just counts up)
                            //      Serial.print("Data at Spot:");
                            //      Serial.println(dataSpot);
    }
  }
  // at this point dataSpot will be the highest point in the array
  if (dataSpot < 3) {  // less than a 3 hour cycle
                       //   Serial.println("Not enough data yet");
    return;            // bail out of the rest of the routine
  }
  //Serial.println("Using DataSpot and - 3");
  float changeRate = hourlyMb[dataSpot] - hourlyMb[dataSpot - 3];  // check the three hour trend
  if (changeRate < 0) {                                            // we have a falling system
    changeRate = abs(changeRate);                                  // change to a positive number
    dropping = true;
  }
  if (changeRate < .1) whichGraphic = 0;
  if (changeRate >= .1 && changeRate <= 1.59) whichGraphic = 1;
  if (changeRate >= 1.6 && changeRate < 3.59) whichGraphic = 2;
  if (changeRate >= 3.6) whichGraphic = 3;
  if (dropping == true && whichGraphic != 0) whichGraphic += 3;  // add 3 to the result for a dropping barometer
  //
  //tft.fillRect(630, 4, 128, 128, TFT_GREEN);// default area for the graphic
  //drawBmp(SD, "/steady.bmp", 630, 4);  //display the image 128 x 128, 24bit
  //Serial.println(whichGraphic);
  switch (whichGraphic) {
    case 0:                                   // steady!
      drawBmp(SD, "/0_steady.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 1:                                      // rising slow
      drawBmp(SD, "/1_risingSlo.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 2:                                     // rising FAST
      drawBmp(SD, "/2_riseFast.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 3:                                     // rising super fast
      drawBmp(SD, "/3_risingSF.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 4:                                     // falling slow
      drawBmp(SD, "/4_fallSlow.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 5:                                     // falling  fast
      drawBmp(SD, "/5_fallFast.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
    case 6:                                   // falling super fast
      drawBmp(SD, "/6_fallSF.bmp", 650, 26);  //display the image 96 x 96, 24bit
      break;
  }
}
//
// ---------------- BMP STUFF ----------------

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
//
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y) {

  if ((x >= tft.width()) || (y >= tft.height())) return;

  // Open requested file on SD card
  File bmpFS = fs.open(filename, "r");

  if (!bmpFS) {
    Serial.print("File not found");
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;  //, col;
  uint8_t r, g, b;

  // uint32_t startTime = millis();

  if (read16(bmpFS) == 0x4D42) {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
      y += h - 1;

      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {

        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t *bptr = lineBuffer;
        uint16_t *tptr = (uint16_t *)lineBuffer;
        // Convert 24 to 16-bit colours
        for (uint16_t col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);
      // Serial.print("Loaded in ");
      // Serial.print(millis() - startTime);
      // Serial.println(" ms");
    } else Serial.println("BMP format not recognized.");
  }
  bmpFS.close();
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}
//
bool newCanTest(int week_number) {
  Serial.print("Week Number:");
  Serial.println(week_number);
  // Determine if the week number is odd or even
  if (week_number % 2 == 0) {
    //printf("The current week (%d) is even.\n", week_number);
    Serial.println("EVEN WEEK");
    return true;  // even week is garbage
  } else {
    //printf("The current week (%d) is odd.\n", week_number);
    Serial.println("ODD Numbered WEEK");
    return false;  // an odd week is a recycle
  }
}