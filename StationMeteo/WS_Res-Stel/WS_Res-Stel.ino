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

//#include "Adafruit_Sensor.h"
#include "Wire.h"

#include <Ticker.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <JsonListener.h>

#include "settings.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"

#include "DSEG7Classic-BoldFont.h" //Pour affichage frame heure



// Initialize the temperature/ humidity sensor /vcc
DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;
float temperature = 0.0;
char FormattedTemperature[10];
char FormattedHumidity[10];
#ifdef READVCC
ADC_MODE(ADC_VCC);
#endif
float vcc = 0;

//ThingspeakClient thingspeak;


/*Ticker tickerWeather;
Ticker tickerDHT;
#ifdef READVCC
Ticker tickerVCC;
#endif
*/

// initiate the client
OpenWeatherMapCurrent client;
OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

Ticker ticker;
// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;
// flag changed in the ticker function every 1 minute
bool readyForDHTUpdate = false;
bool readyForCTempUpdate = false;
// flag changed in the ticker function every 10sec
bool readyForVCCUpdate = false;
String lastUpdate = "--";
long timeSinceLastWUpdate = 0;



//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawOtaProgress(unsigned int, unsigned int);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void drawSleep(OLEDDisplay *display);
void setReadyForWeatherUpdate();
//void setReadyForDHTUpdate();
//void updateDHT(OLEDDisplay *display);
//void updateCTempUpdate();
int8_t getWifiQuality();

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawIndoor, drawCurrentWeather, drawForecast};
// a rajouter pour 2ème prévision drawForecast2, 
int numberOfFrames = 4;
FrameCallback framesOffline[] = {drawIndoor, drawDateTime};
int numberOfFramesOffline = 2; 

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

/*
 *  SETUP
 */
void setup() {
  Serial.begin(115200);
  Serial.println();
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
  display.drawXbm(0, 5, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.drawString(88, 18, "StationMétéo7 \nPar Squix78\nmods par Foxies");
  display.display();
  

//--Parametre WIFI
 // Mode manuel (decommenter)
 //WiFi.begin(WIFI_SSID, WIFI_PWD);

  //Mode auto par WiFiManager
  
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  String hostname(HOSTNAME);
  //hostname += String(ESP.getChipId(), HEX);
  
  WiFi.hostname(hostname);
  //or use this for auto generated name ESP + ChipID
  wifiManager.setTimeout(30);
  wifiManager.autoConnect(hostname.c_str());

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 30 * 2)
  {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connection au WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    display.display();

    counter++;
  }
  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, NTP_SERVERS);

  ui.setTargetFPS(30);
  ui.setTimePerFrame(4*1000); // Setup frame display time to 10 sec
  
  //Hack until disableIndicator works:
  //Set an empty symbol 
  ui.setActiveSymbol(emptySymbol);
  ui.setInactiveSymbol(emptySymbol);
  // Activation symbol transition
  //ui.setActiveSymbol(activeSymbol);
  //ui.setInactiveSymbol(inactiveSymbol);

  ui.disableIndicator();
  
  //  You can change the transition that is used
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_DOWN);
  // Add frames
  if (WiFi.status() == WL_CONNECTED)
  {
    ui.setFrames(frames, numberOfFrames);
  }
  else
  {
    ui.setFrames(framesOffline, numberOfFramesOffline);
  }
    ui.setOverlays(overlays, numberOfOverlays);

  // Setup OTA
  Serial.println("Hostname: " + hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onProgress(drawOtaProgress);
  ArduinoOTA.begin();
  
  // Inital UI takes care of initalising the display too.
  ui.init();
  Serial.println("UI config done");
  updateData(&display);
  Serial.println("Setup done!");
  
#ifdef READVCC
  updateVCC();
#endif

//---Capteur choc--
 pinMode(shakerPin, INPUT_PULLUP);             // Vibration sensor input 
  attachInterrupt(shakerPin, shaker, FALLING);  // Set an inturrupt that will toggle the city location when the vibration switch is shaken
  
//-----Init RGB Led
//set the RGB pins as outputs 
  pinMode(RGB_Red, OUTPUT);
  pinMode(RGB_Green, OUTPUT);
  pinMode(RGB_Blue, OUTPUT);

// updateData(&display);
  ticker.attach(UPDATE_INTERVAL_MIN, setReadyForWeatherUpdate); 
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForCTempUpdate);
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForDHTUpdate);
  ticker.attach(UPDATE_INTERVAL_SEC, setReadyForVCCUpdate);
}
/*
 * ---------------------Fin Setup----------
 */
/*
 * -----------------------Boucle----------
 */
void loop() {
     
    if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_MIN)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
    }    
    if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    updateCTemp();
    updateDHT();
    timeSinceLastWUpdate = millis();
    }
 
    if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
    }
    
    if (readyForVCCUpdate && ui.getUiState()->frameState == FIXED) {
    updateVCC();
    }
    
    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    //read the pushbutton value into a variable
    ArduinoOTA.handle();
    delay(remainingTimeBudget);
    }
  //if (state != currState) {
  //    Serial.println(state);
  //    currState = state;
  //    changecity();
  //}
  // Shutdown and go to sleep function
    timerSleep = millis();
    if (timerSleep >= 20*60000 && ui.getUiState()->frameState == FIXED){ // after 2 minutes go to sleep
    drawSleep(&display);
    // go to deepsleep for xx minutes or 0 = permanently
     ESP.deepSleep(5,  WAKE_RF_DEFAULT);    // 0 delay = permanently to sleep ESP.deepSleep([microseconds], [mode])  
     delay(1000);                            // delay to allow the ESP to go to sleep.
     }
}
/*
 * -------------------Fin boucle------
 */
/*
 * ------------------ Wifi manager---
 */
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entrez votre config");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "-= WIFI MANAGER =-");
  display.drawString(64, 20, "SVP connectez au Spot:");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "Pour la config. du module.");
  display.display();
}
/*
 *--------------- Ecran de progression
 */
void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 12, percentage);
  display->display();
}

void drawOtaProgress(unsigned int progress, unsigned int total) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "OTA Update");
  display.drawProgressBar(2, 28, 124, 12, progress / (total / 100));
  display.display();
}
/*
 * -------------------Update-------------
 */
void updateData(OLEDDisplay *display) {
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  drawProgress(display, 10, "Mise à jour Horloge...");
  if (wifiConnected){
   configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  }

  drawProgress(display, 20, "Mise à jour Météo...");
  if (wifiConnected) {
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  }

  drawProgress(display, 30, "Mise à jour Prévisions...");
  if (wifiConnected) {
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
  }
    
  drawProgress(display, 40, "Mesure Capteur Humidité");
  updateDHT();
  humidity = dht.readHumidity();
  drawProgress(display, 50, "Mesure Capteur Temp ");
  temperature = dht.readTemperature(!IS_METRIC);// Read temperature as Fahrenheit (isFahrenheit = true)
  delay(500);
  
  #ifdef READVCC
  drawProgress(display, 70, "Mesure tension Accu.");
  updateVCC();
  #endif
  
  drawProgress(display, 90, "Test des couleurs");
  testRGB();
  updateCTemp();
  
  readyForDHTUpdate = false;
  readyForWeatherUpdate = false;  
  readyForCTempUpdate = false;
  drawProgress(display, 100, "Succés...");
  delay(1000);
}

//------------- Called every 1 minute
void updateDHT() {
  Serial.println("Lecture Sonde...");
  temperature = dht.readTemperature(!IS_METRIC);
  dtostrf(temperature, 4, 1, FormattedTemperature);
  humidity = dht.readHumidity();
  dtostrf(humidity, 4, 1, FormattedHumidity);
  Serial.println("DHT: T"+ String(FormattedTemperature) + (IS_METRIC ? "°C": "°F") + " H" + String(FormattedHumidity) + "%");
  
  readyForDHTUpdate = false;
  }
  
//-------------- Called every 10 secondes
void updateVCC() {
  Serial.println("Lecture batt...");
  vcc = (ESP.getVcc() / 1024.00f);
  dtostrf(vcc, 4, 2, FormattedVcc);
  Serial.println("Vcc: " + String(FormattedVcc) + "V");
  
  readyForVCCUpdate = false;
  }
  
void updateCTemp() {
  Serial.println("Couleur du temp...");
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (wifiConnected) {
    String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
    Serial.println(temp);
    float tempF = (currentWeather.temp * 1.8) +32; //convert *C to *F
    int TempRGB = (int(tempF)); //converting the *F temp into an int  
    ColorTempF(TempRGB);//21
    }
    else
    {
    temperature = dht.readTemperature(!IS_METRIC);//read temp in *C
    dtostrf(temperature,4, 1, FormattedTemperature);
    Serial.println("T" + String(FormattedTemperature) + (IS_METRIC ? "°C": "°F"));
    int TempRGB = (int(temperature)); //converting the temp into an int
    ColorTempC(TempRGB);//21
  }
  readyForCTempUpdate = false;
}

/*
 * ---------------Partie affichage frame
 */
//------------frame1 heure
//
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) 
{
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  sprintf_P(buff, PSTR("%s %02d.%02d.%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 2 + y, String(buff));
  
  display->setFont(DSEG7_Classic_Bold_21);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 20 + y, String(buff));
}

//------------frame2 Météo du jour
//
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String city = String(DISPLAYED_CITY_NAME);
  display->drawString(55 + x, 0 + y, city);
  
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 13 + y, temp);
  
  display->setFont(Meteocons_Plain_42);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(30 + x, 0 + y, currentWeather.iconMeteoCon);
}

//------------frame 3 partie Prévisions 3jours
//
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

//---------------Détails prévisions frame 3
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(x + 10, y + 34, temp);
 
}

//--------------frame 4 infos capteur
//
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, " Informations Sonde " );
  display->setFont(ArialMT_Plain_16);
  dtostrf(temperature,4, 1, FormattedTemperature);
  display->drawString(64 + x, 12 + y, "Intérieur: " + String(FormattedTemperature) + (IS_METRIC ? "°C": "°F"));
  dtostrf(humidity,4, 1, FormattedHumidity);
  display->drawString(64 + x, 30 + y, "Humidité: " + String(FormattedHumidity) + "%");
}

//-----------------Pied de page
//
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state) {
  #ifdef READVCC
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 52, "Vcc: " + String(FormattedVcc) + "V");
  #else
  char time_str[11];
  time_t now = dstAdjusted.time(nullptr);
  struct tm * timeinfo = localtime (&now);

  display->setFont(ArialMT_Plain_10);

   #ifdef STYLE_24HR
   sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
   #else
    int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d%s\n",hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_hour>=12?"pm":"am");
   #endif

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(5, 52, time_str);

  #endif

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Meteocons_Plain_10);
  display->drawString(75, 52, currentWeather.iconMeteoCon);
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(103, 52, temp);

  int8_t quality = getWifiQuality();
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        display->setPixel(120 + 2 * i, 61 - j);
      }
    }
  }
  display->drawHorizontalLine(0, 50, 128);
}

//-----------Partie pour mise en sommeil 
//
void drawSleep(OLEDDisplay *display) {

        display->clear();
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        display->drawString(64, 0, "Mise en sommeil!");
        display->display();
        delay(5000);
        display->displayOff();
        Serial.println();
        Serial.println("Arrêt connection. mise en veille...");
        delay(1000);
        timerSleep = 0;
}
//
//-------- getWifiQuality
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality()
{
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100)
  {
    return 0;
  }
  else if (dbm >= -50)
  {
    return 100;
  }
  else
  {
    return 2 * (dbm + 100);
  }
}

//--Partie vibration
void shaker(){
  long timeNow = millis();

  if (timeNow - lastVibration > 10000){
  state=!state;
}
  lastVibration = timeNow;
 }

//-Partie gestion couleur RGB
void testRGB() { // fade in and out of Red, Green, Blue
    analogWrite(RGB_Red, 0);     // R off
    analogWrite(RGB_Green, 0);     // G off
    analogWrite(RGB_Blue, 0);     // B off
    fade(RGB_Red); // R
    fade(RGB_Green); // G
    fade(RGB_Blue); // B 
}

void fade(int pin) {
    for (int u = 0; u < 1024; u++) {
      analogWrite(pin, u);
      delay(1);
    }
    for (int u = 0; u < 1024; u++) {
      analogWrite(pin, 1023 - u);
      delay(1);
    }
}

//-Partie pour la couleur dome

void ColorTempC(int tempC) {
  if (tempC < 16) {
    // Temps froid -- bleu
    digitalWrite (RGB_Red, LOW);
    digitalWrite (RGB_Green, LOW);
    digitalWrite (RGB_Blue, HIGH);
  } else if (tempC <20){
    // Temps frais -- violet
    digitalWrite (RGB_Red, LOW);
    digitalWrite (RGB_Blue, HIGH);
    digitalWrite (RGB_Green, HIGH); 
  } else if (tempC <24){
    // Temps ideal -- vert
    digitalWrite (RGB_Red, LOW);
    digitalWrite (RGB_Green, HIGH);
    digitalWrite (RGB_Blue, LOW);
   } else if (tempC <28){
    // Temps chaud -- jaune
    digitalWrite (RGB_Red, HIGH);
    digitalWrite (RGB_Green, HIGH);
    digitalWrite (RGB_Blue, LOW);
   } else if (tempC <31){
    // Temps tres chaud -- magenta
    digitalWrite (RGB_Blue, HIGH);
    digitalWrite (RGB_Green, LOW);
    digitalWrite (RGB_Red, HIGH);
   } else {
    // Temps canicule -- Rouge
    digitalWrite (RGB_Red, HIGH);
    digitalWrite (RGB_Green, LOW);
    digitalWrite (RGB_Blue, LOW);
   }
}
// Set the rgb value based on the temperature value.
void ColorTempF(int tempF) {
  if (tempF >= 95) { //35
    RGB[0] = 172;
    RGB[1] = 36;
    RGB[2] = 48;
  }
  if (tempF >= 90 && tempF <=94) { //32 - 34
    RGB[0] = 207;
    RGB[1] = 46;
    RGB[2] = 49;
  }
  if (tempF >= 80 && tempF <=89) { //26 - 31
    RGB[0] = 238;
    RGB[1] = 113;
    RGB[2] = 25;
  }
  if (tempF >= 70 && tempF <=79) { //21 - 26
    RGB[0] = 255;
    RGB[1] = 200;
    RGB[2] = 8;
  }
  if (tempF >= 60 && tempF <=69) { // 15 - 20
    RGB[0] = 170;
    RGB[1] = 198;
    RGB[2] = 27;
  }
  if (tempF >= 50 && tempF <=59) { 
    RGB[0] = 23;
    RGB[1] = 106;
    RGB[2] = 43;
  }
  if (tempF >= 40 && tempF <=49) { 
    RGB[0] = 3;
    RGB[1] = 30;
    RGB[2] = 122;
  }
  if (tempF >= 30 && tempF <=39) { 
    RGB[0] = 0;
    RGB[1] = 147;
    RGB[2] = 159;
  }
  if (tempF >= 20 && tempF <=29) { //-6 - -1.6
    RGB[0] = 44;
    RGB[1] = 77;
    RGB[2] = 143;
  }
  if (tempF <=19) { // -7
    RGB[0] = 126;
    RGB[1] = 188;
    RGB[2] = 209;
  }
  analogWrite(RGB_Red, RGB[0]);
  analogWrite(RGB_Green, RGB[1]); 
  analogWrite(RGB_Blue, RGB[2]); 
}
//-------------------------------------/
//---Uptate pour ticker
void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
void setReadyForDHTUpdate() {
  Serial.println("Setting readyForDHTUpdate to true");
  readyForDHTUpdate = true;//false desactive capteur
}
void setReadyForCTempUpdate() {
  Serial.println("Setting readyForColorTemp to true");
  readyForCTempUpdate = true;
}
void setReadyForVCCUpdate() {
  Serial.println("Setting readyForVCCUpdate to true");
  readyForVCCUpdate = true;
}
