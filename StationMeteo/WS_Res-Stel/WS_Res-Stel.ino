/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

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

See more at https://thingpulse.com
*/

#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>

// time
#include <time.h>                       // Time Client: simple class which uses the header date and time to set the clock
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <Ticker.h>
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
//#include "Adafruit_Sensor.h"
#include "DHT.h"
#include "DSEG7Classic-BoldFont.h" //Pour affichage frame heure


/***************************
 * Begin Settings
 **************************/

// WIFI
const char* WIFI_SSID = "xxxxx";
const char* WIFI_PWD = "xxxxxx";

#define TZ              1       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

// Setup
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D1;//D3;//D1   Jaune ex:D3
const int SDC_PIN = D2;//D4;//D2   Blanc ex:D4
#else
const int SDA_PIN = 5; //D7 ex 5; 
const int SDC_PIN = 4; //D8 ex 4;
#endif

// DHT Settings
// Uncomment whatever type you're using!
// #define DHTPIN D2 // NodeMCU
#define DHTPIN 0//4 // Wemos D1R2 Mini
//const int DHTPIN = D3;
// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#if DHTTYPE == DHT11
#define DHTTEXT "DHT11"
#elif DHTTYPE == DHT11
#define DHTTEXT "DHT21"
#elif DHTTYPE == DHT22
#define DHTTEXT "DHT22"
#endif
char FormattedTemperature[10];
char FormattedHumidity[10];

//RGB setting
#define RGB_Red D5 //14
#define RGB_Green D6 // 12
#define RGB_Blue D7 //13
//const int RGB_Red = 14;//14;D5
//const int RGB_Green = 12;//12;D6
//const int RGB_Blue = 13;//13;D7
float tempC;
int blueTemp= 0; int greenTemp= 0; int redTemp= 0;


// TimeClient settings
const float UTC_OFFSET = 0;
float utcoh = 0;

// OpenWeatherMap Settings
// initiate the client
//OpenWeatherMapForecast client;
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/

String OPEN_WEATHER_MAP_APP_ID = "xxxxxxxxxxxxxxxxxxxxxxxx";
String OPEN_WEATHER_MAP_LOCATION = "xxxxx";
// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.

String OPEN_WEATHER_MAP_LANGUAGE = "fr";
const boolean IS_METRIC = true;
const uint8_t MAX_FORECASTS = 4;

// Adjust according to your language
const String WDAY_NAMES[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
const String MONTH_NAMES[] = {"JAN", "FEV", "MAR", "AVR", "MAI", "JUN", "JUL", "AOU", "SEP", "OCT", "NOV", "DEC"};

// declare city 
// OPEN_WEATHER_MAP_LOCATION_ID = "6454979" 
// OPEN_WEATHER_MAP_LOCATION_ID = "2992771" 
//const String city1 = "Leeds_Bradford";
//const String country1 = "UK";

//const String city2 = "Sydney";
//const String country2 = "AU";


//-----------digital pin for shaker
#define shakerPin D8 //5         // what digital pin the shaker switch is connect to
volatile int state = LOW;   // Volatile to toggls low to high and back when vibration sensor is shaken
int currState = state;      // variable to compare to see if the state has changed
volatile long lastVibration = millis(); // required to debounce the shaker.

unsigned long timerSleep = 0; // timer for esp8266 shutdown

//-------------Temperature Color

/***************************
 * End Settings
 **************************/
 // Initialize the oled display for address 0x3c
 // sda-pin=14 and sdc-pin=12
 SH1106Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );
 
// Initialize the temperature/ humidity sensor
DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;
float temperature = 0.0;

Ticker ticker;

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;
// flag changed in the ticker function every 1 minute
bool readyForDHTUpdate = true;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;

//declaring prototypes
//+void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
//+void drawOtaProgress(unsigned int, unsigned int);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
//void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();
void setReadyForDHTUpdate();
//+int8_t getWifiQuality();

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawIndoor };
// a rajouter pour 2ème prévision drawForecast2, 
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();

  // initialize display
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();  // Comment out to flip display 180deg
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);
  
 // Credit where credit is due
  display.drawXbm(-6, 5, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.drawString(88, 18, "StationMétéo \nPar Squix78\nmods par Foxies");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connection au WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }
  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, "fr.pool.ntp.org");

  ui.setTargetFPS(30);
  ui.setTimePerFrame(5*1000); // Setup frame display time to 10 sec
  
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_DOWN);
  // Add frames
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();
  Serial.println("");
  updateData(&display);
  
 ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
  
  pinMode(shakerPin, INPUT_PULLUP);             // Vibration sensor input 
  attachInterrupt(shakerPin, shaker, FALLING);  // Set an inturrupt that will toggle the city location when the vibration switch is shaken
//-----RGB Led
  // initialize serial communication at 9600 bits per second:
//    Serial.begin(9600);
//    dht.begin();
//    receiver.enableIRIn();
//    pinMode(Sw1, INPUT);  
//    pinMode(Sw2, INPUT);    
//    pinMode(IR_Pin, INPUT);   
    pinMode(RGB_Red, OUTPUT);
    pinMode(RGB_Green, OUTPUT);
    pinMode(RGB_Blue, OUTPUT);
   // pinMode(RGB_Red, OUTPUT);
   // pinMode(RGB_Green, OUTPUT);
   // pinMode(RGB_Blue, OUTPUT);
    analogWrite(RGB_Red, 600);     // RGB_Red 
    analogWrite(RGB_Green, 600);     // RGB_Green 
    analogWrite(RGB_Blue, 600);     // RGB_Blue     
}

void loop() {

  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }
  
  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }
  
  int remainingTimeBudget = ui.update();
  
  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    //read the pushbutton value into a variable
    
     delay(remainingTimeBudget);
  }
/*  if (state != currState) {
      Serial.println(state);
      currState = state;
      changecity();
  }*/
  // Shutdown and go to sleep function
  /*
  timerSleep = millis();
  if (timerSleep >= 10*60000 && ui.getUiState().frameState == FIXED){ // after 2 minutes go to sleep
    drawSleep(&display);
        // go to deepsleep for xx minutes or 0 = permanently
        ESP.deepSleep(0,  WAKE_RF_DEFAULT);                       // 0 delay = permanently to sleep
        delay(1000);                                              // delay to allow the ESP to go to sleep.
  }*/
  //---Module RGB---
  //tempC = dht.readTemperature(true);//read the value from the sensor
  tempC = dht.readTemperature(!IS_METRIC);
  //Serial.println(tempC);             //send the data to the computer
  if(tempC<0){
  blueTemp = 255;
  }
  else if(tempC>0&&tempC<=45){
  blueTemp= map(tempC, 0, 45, 255, 0);
  }
  else if(tempC>45){
  blueTemp = 0;
  }
  
  if(tempC<15){
  greenTemp = 0;
  }
  else if(tempC>15&&tempC<=35){
  greenTemp = map(tempC, 15, 35, 1, 254);
  }
  else if(tempC>35&&tempC<=85){
  greenTemp = map(tempC, 35, 85, 255, 0);
  }
  else if(tempC>85){
  greenTemp = 0;
  }
  
  if(tempC<65){
  redTemp = 0;
  }
  else if(tempC>=65){
  redTemp= map(tempC, 65, 90, 1, 255);
  }
  else if(tempC>90){
  redTemp = 255;
  }
  setColor(redTemp, greenTemp, blueTemp);
  delay (200);
//colorWipe(strip.Color(redTemp, greenTemp, blueTemp));
  // delay(2000);      //wait 200 ms before sending new data

  //showRGB(500); //Cycle de couleur RGB
}
//---------------------------//
//---------Update------------//
//---------------------------//
void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Mise à jour Horloge...");

  drawProgress(display, 30, "Mise à jour Météo...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrent(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION);

  drawProgress(display, 50, "Mise à jour Prévisions...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION, MAX_FORECASTS);
  
  drawProgress(display, 60, "Mise à jour Capteur");
  humidity = dht.readHumidity();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  temperature = dht.readTemperature(!IS_METRIC);
  
  drawProgress(display, 90, "Test des couleurs");
  testRGB();
  
  readyForDHTUpdate = false;
  readyForWeatherUpdate = false;  
  drawProgress(display, 100, "Succés...");
  delay(1000);
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

// Called every 1 minute
/*void updateDHT() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature(!IS_METRIC);
  readyForDHTUpdate = false;
}*/
//-----------------------------------------//
//----------declaration des fonctions-----//
//----------------------------------------//
//---------Partie affichage frame--------
//frame1 heure
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];

  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
//  display->setFont(ArialMT_Plain_24);
  display->setFont(DSEG7_Classic_Bold_21);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}
//frame2 Météo du jour
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}
//frame 3 partie Prévisions 3jours
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}
//Détails prévisions frame 3
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}
//frame 4 infos capteur
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, " Ambiance2 par " DHTTEXT );
  display->setFont(ArialMT_Plain_16);
  dtostrf(temperature,4, 1, FormattedTemperature);
  display->drawString(64 + x, 12 + y, "Intérieur: " + String(FormattedTemperature) + (IS_METRIC ? "°C": "°F"));
  dtostrf(humidity,4, 1, FormattedHumidity);
  display->drawString(64 + x, 30 + y, "Humidité: " + String(FormattedHumidity) + "%");
}

//-----Frame pour seconde prévision (drawForecast2)
/*//frame x partie Prévisions 3jours
  void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}
  void drawForecast2Details(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}
*/
//-------Pied de page
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}
//-Partie pour mise en sommeil 
void drawSleep(OLEDDisplay *display, OLEDDisplayUiState* state) {

        display->clear();
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        display->drawString(64, 0, "Going to Sleep!");
        display->display();
        delay(5000);
        display->displayOff();
        Serial.println();
        Serial.println("closing connection. going to sleep...");
        delay(1000);
        timerSleep = 0;
}
//-Partie couleur RGB
//-----------------------
void setColor(int red, int green, int blue) {
    analogWrite(RGB_Red, red);
    analogWrite(RGB_Green, green);
    analogWrite(RGB_Blue, blue);  
}

void showRGB(int mil) {
    setColor(1, 0, 0);  // red lowest brightness
    delay(mil);
    setColor(50, 0, 0);  // red
    delay(mil);
    setColor(140, 0, 0);  // red
    delay(mil);
    setColor(255, 0, 0);  // red
    delay(mil);
    setColor(0, 255, 0);  // green
    delay(mil);
    setColor(0, 0, 255);  // blue
    delay(mil);
    setColor(255, 255, 0);  // yellow
    delay(mil);  
    setColor(80, 0, 80);  // purple
    delay(mil);
    setColor(255, 50, 0);  // Orange
    delay(mil);
} 

void testRGB() { // fade in and out of Red, Green, Blue
    analogWrite(RGB_Red, 255);     // R off
    analogWrite(RGB_Green, 255);     // G off
    analogWrite(RGB_Blue, 255);     // B off 
    fade(RGB_Red); // R
    fade(RGB_Green); // G
    fade(RGB_Blue); // B 
    analogWrite(RGB_Red, 0);     // R off
    analogWrite(RGB_Green, 0);     // G off
    analogWrite(RGB_Blue, 0);     // B off
}

void fade(int pin) {
    for (int u = 0; u < 256; u++) {
      analogWrite(pin,  256 - u);
      delay(5);
    }
    for (int u = 0; u < 256; u++) {
      analogWrite(pin, u);
      delay(5);
    }
}    
//-------------------------------------/
void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
//---------Capteur
void setReadyForDHTUpdate() {
  Serial.println("Setting readyForDHTUpdate to true");
  readyForDHTUpdate = true;//desactive capteur
}
//--Partie vibration
void shaker(){
  long timeNow = millis();
  if (timeNow - lastVibration > 10000){
  state=!state;
}
  lastVibration = timeNow;
 }
