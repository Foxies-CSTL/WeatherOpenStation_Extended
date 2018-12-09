/**The MIT License (MIT)

Copyright (c) 2016 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

/* Customizations by Neptune (NeptuneEng on Twitter, Neptune2 on Github)
 *  
 *  Added Wifi Splash screen and credit to Squix78
 *  Modified progress bar to a thicker and symmetrical shape
 *  Replaced TimeClient with built-in lwip sntp client (no need for external ntp client library)
 *  Added Daylight Saving Time Auto adjuster with DST rules using simpleDSTadjust library
 *  https://github.com/neptune2/simpleDSTadjust
 *  Added Setting examples for Boston, Zurich and Sydney
  *  Selectable NTP servers for each locale
  *  DST rules and timezone settings customizable for each locale
   *  See https://www.timeanddate.com/time/change/ for DST rules
  *  Added AM/PM or 24-hour option for each locale
 *  Changed to 7-segment Clock font from http://www.keshikan.net/fonts-e.html
 *  Added Forecast screen for days 4-6 (requires 1.1.3 or later version of esp8266_Weather_Station library)
 *  Added support for DHT22, DHT21 and DHT11 Indoor Temperature and Humidity Sensors
 *  Fixed bug preventing display.flipScreenVertically() from working
 *  Slight adjustment to overlay
 */
#include <simpleDSTadjust.h>
#include "DHT.h"
/***************************
 * Begin Settings
 **************************/

//---Voltage Accu LiPo---
#define READVCC
#ifdef READVCC
char FormattedVcc[10];
#endif 

//-- WIFI Manuel-----
const char* WIFI_SSID = "Acces_Point";
const char* WIFI_PWD = "PassWord";
#define HOSTNAME "MeteoFun"

// Setup
const int UPDATE_INTERVAL_MIN = 10 * 60; // Update every 20 minutes
const int UPDATE_INTERVAL_SECS = 2 * 60; // Update every 60 secondes
const int UPDATE_INTERVAL_SEC = 1 * 10; // Update every 60 secondes
const int SLEEP_INTERVAL_SECS = 30;   // Going to Sleep after idle times, set 0 for dont sleep
/*
 * Choix ecran lcd
 */
//---Definition Ecran LCD 9 ou 1.3
// >>> Uncomment one of the following 2 lines to define which OLED display interface type you are using
//#define spiOLED
#define i2cOLED

#ifdef spiOLED
#include "SSD1306Spi.h"
// Pin definitions for SPI OLED
#define OLED_CS     D8  // Chip select
#define OLED_DC     D2  // Data/Command
#define OLED_RESET  D0  // RESET  - If you get an error on this line, either change Tools->Board to the board you are using or change "D0" to the appropriate pin number for your board.
//
SSD1306Spi display(OLED_RESET, OLED_DC, OLED_CS);  // SPI OLED
#endif

#ifdef i2cOLED
#include "SH1106Wire.h"
// Pin definitions for I2C OLED
#define I2C_DISPLAY_ADDRESS 0x3c
#define SDA_PIN 4 //D2
#define SDC_PIN 5 //D1
//
SH1106Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);  // I2C OLED
#endif

#include "OLEDDisplayUi.h"
OLEDDisplayUi   ui(&display);
/*
 *  Fin SETUP OLED *
 */

/*
 * Setting sonde Temp & Humidity *
 */
// DHT Settings
// Uncomment whatever type you're using!
// #define DHTPIN D2 // NodeMCU
#define DHTPIN 0 // Wemos D1R2 Mini
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#if DHTTYPE == DHT22
#define DHTTEXT "DHT22"
#elif DHTTYPE == DHT21
#define DHTTEXT "DHT21"
#elif DHTTYPE == DHT11
#define DHTTEXT "DHT11"
#endif

/*
 *  Setup RGB Led *
 */
//#define RGB_Red 12
//#define RGB_Green 14
//#define RGB_Blue 13
const int RGB_Red = 12; //D6
const int RGB_Green = 14; // D5
const int RGB_Blue = 13; //D7
//create constants for the three analog input pins
const int redPot = 0;
const int greenPot = 1;
const int bluePot = 2;
//create constants for the three RGB pulse width pins 
//const int redPin = 5;
//const int greenPin = 6;
//const int bluePin = 9;
//create variables to store the red, green and blue values 
int redVal;
int greenVal;
int blueVal;
int RGB[3] = {0, 0, 0};
//-------------Temperature Color---------
//float tempF;// = 0; //temperature to convert
//int TempRGB;// = 0; // int to convert fahtemp float into an int

// NIGHTLIGHT MODE
byte NightColor[3] = {0,0,0};

//-----------digital pin for shaker
//int Led=13;//define LED port
//int buttonpin=3; //define switch port
#define shakerPin D8         // what digital pin the shaker switch is connect to
volatile int state = LOW;   // Volatile to toggls low to high and back when vibration sensor is shaken
int currState = state;      // variable to compare to see if the state has changed
volatile long lastVibration = millis(); // required to debounce the shaker.
unsigned long timerSleep = 0; // timer for esp8266 shutdown 

//--------NTP----------
//#define UTC_OFFSET       1       // (utc+) TZ in hours//6
#define DST_MN          60      // use 60min for summer time in some countries
#define TZ_MN           ((UTC_OFFSET)*60)
#define TZ_SEC          ((UTC_OFFSET)*3600)
#define DST_SEC         ((DST_MN)*60)

time_t now;

//-------------- OpenWeatherMap Settings
String OPEN_WEATHER_MAP_APP_ID = "e7...........................";
const boolean IS_METRIC = true;
String OPEN_WEATHER_MAP_LANGUAGE = "fr";

// Adjust according to your language
const String WDAY_NAMES[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
const String MONTH_NAMES[] = {"JAN", "FEV", "MAR", "AVR", "MAI", "JUN", "JUL", "AOU", "SEP", "OCT", "NOV", "DEC"};
const uint8_t MAX_FORECASTS = 4;
// -----------------------------------
// Example Locales (uncomment only 1)
#define city1   // 
//#if VILLE == city2
//#define city2
//#elif VILLE == city3
//#define city3
//#elif VILLE == city1
//#define city3
//#endif
//const float VILLE = city1;
//#define CITY Mont-de-marsan
//#define Bourges
//#define Sydney
// declare city 
// OPEN_WEATHER_MAP_LOCATION_ID = "6454979" //Bourges
// OPEN_WEATHER_MAP_LOCATION_ID = "2992771" //Mont-de-marsan

//--------------------------
#ifdef city1
//DST rules for Central European Time Zone
#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
// Uncomment for 24 Hour style clock
#define STYLE_24HR
#define NTP_SERVERS "0.fr.pool.ntp.org", "1.fr.pool.ntp.org", "2.fr.pool.ntp.org"
// OpenWeatherMap City Settings
String OPEN_WEATHER_MAP_LOCATION = "Bourges";//bourges
String OPEN_WEATHER_MAP_LOCATION_ID = "6454979";//Bourges
String DISPLAYED_CITY_NAME = "Bourges";
#endif

#ifdef city2
//DST rules for Central European Time Zone
#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
// Uncomment for 24 Hour style clock
#define STYLE_24HR
#define NTP_SERVERS_1 "0.fr.pool.ntp.org", "1.fr.pool.ntp.org", "2.fr.pool.ntp.org"
// OpenWeatherMap City Settings
String OPEN_WEATHER_MAP_LOCATION_1 = "Bourges";//bourges
String OPEN_WEATHER_MAP_LOCATION_ID_1 = "6454979";//Bourges
String DISPLAYED_CITY_NAME_1 = "Bourges";
#endif

//---------------------------------
#ifdef city3
#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
// Uncomment for 24 Hour style clock
#define STYLE_24HR
#define NTP_SERVERS "0.fr.pool.ntp.org", "1.fr.pool.ntp.org", "2.fr.pool.ntp.org"
// OpenWeatherMap Settings
String OPEN_WEATHER_MAP_LOCATION= "Mont-de-marsan";
String OPEN_WEATHER_MAP_LOCATION_ID = "2992771";
String DISPLAYED_CITY_NAME = "Mont-de-marsan";
#endif

#ifdef city4
//DST Rules for Australia Eastern Time Zone (Sydney)
#define UTC_OFFSET +10
struct dstRule StartRule = {"AEDT", First, Sun, Oct, 2, 3600}; // Australia Eastern Daylight time = UTC/GMT +11 hours
struct dstRule EndRule = {"AEST", First, Sun, Apr, 2, 0};      // Australia Eastern Standard time = UTC/GMT +10 hour
// Uncomment for 24 Hour style clock
//#define STYLE_24HR
#define NTP_SERVERS_3 "0.au.pool.ntp.org", "1.au.pool.ntp.org", "2.au.pool.ntp.org"
// OpenWeatherMap Settings
String OPEN_WEATHER_MAP_LOCATION_3 = "Sydney";
String OPEN_WEATHER_MAP_LOCATION_ID_3 = "2992771";
String DISPLAYED_CITY_NAME_3 = "Sydney";
#endif

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

/***************************
 * End Settings
 **************************/
 
