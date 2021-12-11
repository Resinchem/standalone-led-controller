/*
 * Standalone LED Controller with Motion Detection
 * Includes captive portal and OTA Updates
 * This provides standalone motion-activated control for WS2812b LED strips
 * Version: 0.20
 * Last Updated: 12/6/2021
 * ResinChem Tech
 */
#include <FS.h>                   
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#ifdef ESP32
  #include <SPIFFS.h>
#endif


// ================================
//  User Defined values and options
// ================================
//  define your default values here, if there are different values in config.json, they are overwritten.

#define OTA_HOSTNAME "standaloneOTA"     //Port name to be broadcast in Arduino IDE for OTA Updates
uint16_t ota_time = 2500;                // minimum time on boot for IP address to show in IDE ports, in millisecs
uint16_t ota_time_window = 20000;        // time to start file upload when ota_flag set to true (after initial boot), in millsecs
#define DATA_PIN D4                      
#define MOTION_PIN_1 D5
#define MOTION_PIN_2 D6
#define NUM_LEDS_MAX 600
#define SERIAL_DEBUG 0                   // Enable(1) or disable (0) serial montior output - disable for compiled .bin
// ================================

uint16_t ota_time_elapsed = 0;           // Counter when OTA active
bool ota_flag = true;
bool lightsOn = false;
unsigned long onTime = 0;

char pir_count[3];
char led_count[5];
char led_on_time[4];
char led_brightness[4];
char led_red[4];
char led_green[4];
char led_blue[4];

//==================================
// CAPTIVE PORTAL SETUP
//==================================
//flag for saving data
bool shouldSaveConfig = false;


WiFiClient espClient;
ESP8266WebServer server;
WiFiManager wifiManager;
CRGB LEDs[NUM_LEDS_MAX];           

//callback notifying us of the need to save config
void saveConfigCallback () {
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1) 
    Serial.println("Should save config");
  #endif
  shouldSaveConfig = true;
}

//==========================
// LED Setup
//==========================
// defaults - will be set/overwritten by portal or last vals on reboot
byte numPIRs = 2;
int numLEDs = 30;        
int ledOnTime = 15;
byte ledBrightness = 128;
byte ledRed = 255;
byte ledGreen = 255;
byte ledBlue = 255;
CRGB ledColorOn = CRGB::White;
CRGB ledColorOff = CRGB::Black;

// ==================================
//  Main Setup
// ==================================
void setup() {
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.begin(115200);
    Serial.println();
  #endif
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("mounting FS...");
  #endif
  
  if (SPIFFS.begin()) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("mounted file system");
    #endif
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("reading config file");
      #endif
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("opened config file");
        #endif
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
            Serial.println("\nparsed json");
          #endif
          strcpy(pir_count, json["pir_count"]);
          strcpy(led_count, json["led_count"]);
          strcpy(led_on_time, json["led_on_time"]);
          strcpy(led_brightness, json["led_brightness"]);
          strcpy(led_red, json["led_red"]);
          strcpy(led_green, json["led_green"]);
          strcpy(led_blue, json["led_blue"]);
          
        } else {
          #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
            Serial.println("failed to load json config");
          #endif
        }
        configFile.close();
      }
    }
  } else {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("failed to mount FS");
    #endif
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_pir_num("pirCount", "Number of PIRs", pir_count, 3, "Number of Motion PIRs (1 or 2)");
  WiFiManagerParameter custom_led_num("ledCount", "Number of LEDs", led_count, 5, "Number of LEDs (max 600)");
  WiFiManagerParameter custom_led_time("ledTime", "LED on time", led_on_time, 4, "LED on time (seconds)");
  WiFiManagerParameter custom_led_brightness("ledBrightness", "LED Brightness:", led_brightness, 4, "LED Brightness (0-255)");
  WiFiManagerParameter custom_led_red("ledRed", "LED Red", led_red, 4, "LED Red (0-255)");
  WiFiManagerParameter custom_led_green("ledGreen", "LED Green", led_green, 4, "LED Green (0-255)");
  WiFiManagerParameter custom_led_blue("ledBlue", "LED Blue", led_blue, 4, "LED Blue (0-255)");

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  wifiManager.addParameter(&custom_pir_num);
  wifiManager.addParameter(&custom_led_num);
  wifiManager.addParameter(&custom_led_time);
  wifiManager.addParameter(&custom_led_brightness);
  wifiManager.addParameter(&custom_led_red);
  wifiManager.addParameter(&custom_led_green);
  wifiManager.addParameter(&custom_led_blue);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "MyApName".  If not supplied, will use ESP + last 5 digits of MAC
  //and goes into a blocking loop awaiting configuration. If a password
  //is desired for the AP, add it after the AP name (e.g. autoConnect("MyApName", "12345678")
  if (!wifiManager.autoConnect()) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("failed to connect and hit timeout");
    #endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("connected to your wifi...yay!");
  #endif
  //read updated parameters
  strcpy(pir_count, custom_pir_num.getValue());
  strcpy(led_count, custom_led_num.getValue());
  strcpy(led_on_time, custom_led_time.getValue());
  strcpy(led_brightness, custom_led_brightness.getValue());
  strcpy(led_red, custom_led_red.getValue());
  strcpy(led_green, custom_led_green.getValue());
  strcpy(led_blue, custom_led_blue.getValue());

  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("The values in the file are: ");
    Serial.println("\tpir_count : " + String(pir_count));
    Serial.println("\tled_count : " + String(led_count));
    Serial.println("\tdata_on_time : " + String(led_on_time));
    Serial.println("\tled_brightness : " + String(led_brightness));
    Serial.println("\tled_red : " + String(led_red));
    Serial.println("\tled_green : " + String(led_green));
    Serial.println("\tled_blue : " + String(led_blue));
  #endif
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("saving config");
    #endif
#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["pir_count"] = pir_count;
    json["led_count"] = led_count;
    json["led_on_time"] = led_on_time;
    json["led_brightness"] = led_brightness;
    json["led_red"] = led_red;
    json["led_green"] = led_green;
    json["led_blue"] = led_blue;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("failed to open config file for writing");
      #endif
    }

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }

  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  #endif
  
  // SETUP FASTLED
  //Convert config values
  numPIRs = (String(pir_count)).toInt();
  numLEDs = (String(led_count)).toInt();
  ledOnTime = (String(led_on_time)).toInt();
  ledBrightness = (String(led_brightness)).toInt();
  ledRed = (String(led_red)).toInt();
  ledGreen = (String(led_green)).toInt();
  ledBlue = (String(led_blue)).toInt();
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS_MAX);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(ledBrightness);
  fill_solid(LEDs, NUM_LEDS_MAX, CRGB::Blue);
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("LEDs Blue - FASTLED ok");
  #endif
  delay(1000);
  fill_solid(LEDs, NUM_LEDS_MAX, CRGB::Black);
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("LEDs Reset to off");
  #endif
  ledColorOn = CRGB(ledRed, ledGreen, ledBlue);

  // Setup motion pins
  pinMode(MOTION_PIN_1, INPUT);
  pinMode(MOTION_PIN_2, INPUT);
  
  //-----------------------------
  // Setup OTA Updates
  //-----------------------------
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  });
  ArduinoOTA.begin();
  // Setup handlers for web calls for OTAUpdate and Restart
  server.on("/restart",[](){
    server.send(200, "text/html", "<h1>Restarting...</h1>");
    delay(1000);
    ESP.restart();
  });
  server.on("/reset",[](){
    server.send(200, "text/html", "<h1>Resetting all values and restarting...<h1><h3>Reconnect to device AP and go to 192.168.4.1<h3>");
    delay(1000);
    wifiManager.resetSettings();
    delay(1000);
    ESP.restart();
  });
  server.on("/otaupdate",[]() {
    server.send(200, "text/html", "<h1>Ready for upload...<h1><h3>Start upload from IDE now</h3>");
    ota_flag = true;
    ota_time = ota_time_window;
    ota_time_elapsed = 0;
  });
  server.begin();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("Setup complete - starting main loop");
  #endif
}

void loop() {
  //Handle OTA updates when OTA flag set via HTML call to http://ip_address/otaupdate
  if (ota_flag) {
    uint16_t ota_time_start = millis();
    while (ota_time_elapsed < ota_time) {
      ArduinoOTA.handle();  
      ota_time_elapsed = millis()-ota_time_start;   
      delay(10); 
    }
    ota_flag = false;
  }
  //Handle any web calls
  server.handleClient();
  //Main loop
  unsigned long curTime = millis();

  if (numPIRs > 1) {
    if ((digitalRead(MOTION_PIN_1) == HIGH) || (digitalRead(MOTION_PIN_2) == HIGH)) {
      if (!lightsOn) {
        toggleLights(true); 
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("Lights on");
        #endif 
      }
      onTime = millis();    
    } else if ((digitalRead(MOTION_PIN_1) == LOW) && (digitalRead(MOTION_PIN_2) == LOW) && (lightsOn) && ((millis() - onTime) > (ledOnTime * 1000))) {
      toggleLights(false);
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Lights off");
      #endif
    }

  } else {
    if ((digitalRead(MOTION_PIN_1) == HIGH)) {
      if (!lightsOn) {
        toggleLights(true);
      }
      onTime = millis();
    } else if ((digitalRead(MOTION_PIN_1) == LOW) && (lightsOn) && ((millis() - onTime) > (ledOnTime * 1000))) {
      toggleLights(false);
    }
  }
}

void toggleLights(bool turnOn) {
  if (turnOn) {
    fill_solid(LEDs, NUM_LEDS_MAX, ledColorOn);
    lightsOn = true;
    FastLED.show();
  } else {
    fill_solid(LEDs, NUM_LEDS_MAX, ledColorOff);   
    lightsOn = false;
    FastLED.show();
  }
}
