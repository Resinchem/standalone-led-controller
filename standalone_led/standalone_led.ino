/*
 * Standalone LED Controller with Motion Detection
 * Includes captive portal and OTA Updates
 * This provides standalone motion-activated control for WS2812b LED strips
 * Version: 0.46
 * Last Updated: 2/14/2024
 * ResinChem Tech
 * See Github release for 0.46 notes
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
#include <WiFiClient.h>
#include <ESP8266HTTPUpdateServer.h>

#ifdef ESP32
  #include <SPIFFS.h>
#endif
#define VERSION "v0.46 (ESP8266)"

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

// ===============================
//  Effects array 
// ===============================
//  Effects are defined in defineEffects() - called in Setup
//  Effects must be handled in the lights on call
//  To add an effect:
//    - Increase array below (if adding)
//    - Add element and add name in defineEffects()
//    - Update case statement in toggleLights()
//    - Add function to implement effect (called by toggleLights)
int numberOfEffects = 5;
String Effects[5]; 

// ===========================
//  Device and Global Vars
// ===========================
String deviceName = "MotionLED";
String wifiHostName = "MotionLED";
String otaHostName = "MotionLED";

bool bTesting = false;
uint16_t ota_time_elapsed = 0;           // Counter when OTA active
bool ota_flag = true;
bool lightsOn = false;
unsigned long onTime = 0;

//Chars for JSON/Config
char device_name[25];
char wifi_hostname[25];
char ota_hostname[25];
char pir_count[3];
char led_count[5];
char led_on_time[4];
char led_brightness[4];
char led_red[4];
char led_green[4];
char led_blue[4];
char led_red_m2[4];
char led_green_m2[4];
char led_blue_m2[4];
char led_effect[16];
char led_effect_m2[16];

String baseIP;


WiFiClient espClient;
ESP8266WebServer server;
ESP8266HTTPUpdateServer httpUpdater;
WiFiManager wifiManager;
CRGB LEDs[NUM_LEDS_MAX];           

//---- Captive Portal -------
//flag for saving data in captive portal
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1) 
    Serial.println("Should save config");
  #endif
  shouldSaveConfig = true;
}
//---------------------------

//==========================
// LED Setup
//==========================
// defaults - will be set/overwritten by portal or last vals on reboot
byte numPIRs = 2;
int numLEDs = 30;        
int ledOnTime = 15;
byte ledBrightness = 128;
byte ledRed_m1 = 255;
byte ledGreen_m1 = 255;
byte ledBlue_m1 = 255;
byte ledRed_m2 = 255;
byte ledGreen_m2 = 255;
byte ledBlue_m2 = 255;
String ledEffect_m1 = "Solid";
String ledEffect_m2 = "Solid";
CRGB ledColorOn_m1 = CRGB::White;
CRGB ledColorOn_m2 = CRGB::White;
CRGB ledColorOff = CRGB::Black;

//===============================
// Web pages and handlers
//===============================
// Main Settings page
// Root / Main Settings page handler
void handleRoot() {
  String mainPage = "<html>\
  <head>\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
    <title>VAR_DEVICE_NAME Controller</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>VAR_DEVICE_NAME Controller Settings</h1><br>\
    Changes made here will be used <b><i>until the controller is restarted</i></b>, unless the box to save the settings as new boot defaults is checked.<br><br>\
    To test settings, leave the box unchecked and click 'Update'. Once you have settings you'd like to keep, check the box and click 'Update' to write the settings as the new boot defaults.<br><br>\
    If you need to change wifi settings, you must use the 'Reset All' command.<br><br>\
    <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">\
      <table>\
      <tr>\
      <td><label for=\"pirs\">Number of Motion PIRs (1-2):</label></td>\
      <td><input type=\"number\" min=\"1\" max=\"2\" name=\"pirs\" value=\"";
   
  mainPage += String(numPIRs);
  mainPage += "\"></td></tr>\
      <tr>\
      <td><label for=\"leds\">Number of Pixels (1-600):</label></td>\
      <td><input type=\"number\" min=\"1\" max=\"600\" name=\"leds\" value=\"";
  mainPage += String(numLEDs);    
  mainPage += "\"></td></tr>\
      <tr>\
      <td><label for=\"brightness\">LED Brightness (0-255):</label></td>\
      <td><input type=\"number\" min=\"0\" max=\"255\" name=\"brightness\" value=\"";
  mainPage += String(ledBrightness);
  mainPage += "\"></td>\
      </tr>\
      <tr>\
      <td><label for=\"ledtime\">LED On Time (seconds):</label></td>\
      <td><input type=\"number\" min=\"0\" max=\"999\" name=\"ontime\" value=\"";
  mainPage += String(ledOnTime);
  mainPage += "\"></td>\
      </tr>\
      </table><br>\
      <b><u>LED Colors and Effects</b></u>:<br>\
      If two motion detectors are defined, each may have its own color and effect.  When a motion detector is activated, it will apply its own color and effect.<br>\
      For effects that use two colors (e.g. 2-Segment), the other motion detector's color setting will be used as the secondary color.<br><br>\
      <table>\
      <tr>\
      <td>&nbsp;</td><td><b><u>Motion 1</b></u></td><td><b><u>Motion 2</b></u></td>\
      <tr>\
      <td><label for=\"red\">Red (0-255):</label></td>\
      <td><input type=\"number\" min=\"0\" max=\"255\" name=\"red1\"  value=\"";
  mainPage += String(ledRed_m1);
  mainPage += "\"></td><td><input type=\"number\" min=\"0\" max=\"255\" name=\"red2\"  value=\"";
  mainPage += String(ledRed_m2);
  mainPage += "\"></td>\
      </tr>\
      <tr>\
      <td><label for=\"green\">Green (0-255):</label></td>\
      <td><input type=\"number\" min=\"0\" max=\"255\" name=\"green1\" value=\"";
  mainPage += String(ledGreen_m1);
  mainPage += "\"></td><td><input type=\"number\" min=\"0\" max=\"255\" name=\"green2\" value=\"";
  mainPage += String(ledGreen_m2);
  mainPage += "\"></td>\
      </tr>\
      <tr>\
      <td><label for=\"blue\">Blue (0-255):</label></td>\
      <td><input type=\"number\" min=\"0\" max=\"255\" name=\"blue1\" value=\"";
  mainPage += String(ledBlue_m1);
  mainPage += "\"></td><td><input type=\"number\" min=\"0\" max=\"255\" name=\"blue2\" value=\"";
  mainPage += String(ledBlue_m2);
  mainPage += "\"></td>\
      </tr>\
      <tr>\
      <td><label for=\"effect\">Effect:</label></td>\
      <td><select name=\"effect1\">";
 // Dropdown Effects boxes
 for (byte i = 0; i < numberOfEffects; i++) {
   mainPage += "<option value=\"" + Effects[i] + "\"";
   if (Effects[i] == ledEffect_m1) {
     mainPage += " selected";
   }
   mainPage += ">" + Effects[i] + "</option>";
 }
 mainPage += "</select></td><td><select name=\"effect2\">";
 for (byte i = 0; i < numberOfEffects; i++) {
   mainPage += "<option value=\"" + Effects[i] + "\"";
   if (Effects[i] == ledEffect_m2) {
     mainPage += " selected";
   }
   mainPage += ">" + Effects[i] + "</option>";
 }

 mainPage += "</td></tr>\
      </table><br>\
      <input type=\"checkbox\" name=\"chksave\" value=\"save\">Save all settings as new boot defaults (controller will reboot)<br><br>\
      <input type=\"submit\" value=\"Update\">\
    </form>\
    <br>\
    <h2>Controller Commands</h2>\
    Caution: Restart and Reset are executed immediately when the button is clicked.<br>\
    <table border=\"1\" cellpadding=\"10\">\
    <tr>\
    <td><button id=\"btnrestart\" onclick=\"location.href = './restart';\">Restart</button></td><td>This will reboot controller and reload default boot values.</td>\
    </tr><tr>\
    <td><button id=\"btnreset\" style=\"background-color:#FAADB7\" onclick=\"location.href = './reset';\">RESET ALL</button></td><td><b>WARNING</b>: This will clear all settings, including WiFi! You must complete initial setup again.</td>\
    </tr><tr>\
    <td><button id=\"btnupdate\" onclick=\"location.href = './update';\">Firmware Upgrade</button></td><td>Upload and apply new firmware from local file.</td>\
    </tr></table><br>\
    Current version: VAR_CURRRENT_VER\
  </body>\
</html>";
  mainPage.replace("VAR_DEVICE_NAME", deviceName);
  mainPage.replace("VAR_CURRRENT_VER", VERSION);
  server.send(200, "text/html", mainPage);
}

// Settings submit handler - Settings results
void handleForm() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    String saveSettings;
    numPIRs = server.arg("pirs").toInt();
    numLEDs = server.arg("leds").toInt();
    ledBrightness = server.arg("brightness").toInt();
    ledOnTime = server.arg("ontime").toInt();
    ledRed_m1 = server.arg("red1").toInt();
    ledGreen_m1 = server.arg("green1").toInt();
    ledBlue_m1 = server.arg("blue1").toInt();
    ledRed_m2 = server.arg("red2").toInt();
    ledGreen_m2 = server.arg("green2").toInt();
    ledBlue_m2 = server.arg("blue2").toInt();
    ledEffect_m1 = server.arg("effect1");
    ledEffect_m2 = server.arg("effect2");
    saveSettings = server.arg("chksave");
    String message = "<html>\
      </head>\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
        <title>VAR_DEVICE_NAME Controller Settings</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
      <H1>Settings updated!</H1><br>\
      <H3>Current values are:</H3>";
    message += "Num PIRs: " + server.arg("pirs") + "<br>";
    message += "Num LEDs: " + server.arg("leds") + "<br>";
    message += "Brightness: " + server.arg("brightness") + "<br>";
    message += "LED On Time:" + server.arg("ontime") + "<br><br>";
    message += "<b>LED Color/Effect</b>:<br><br>";
    message += "<table border=\"1\" cellpadding=\"7\">";
    message += "<tr><td>&nbsp;</td><td><u>Motion 1</u></td><td><u>Motion 2</u></td></tr>";
    message += "<tr><td>Red:</td><td>" + server.arg("red1") + "</td><td>" + server.arg("red2") + "</td></tr>";  
    message += "<tr><td>Green:</td><td>" + server.arg("green1") + "</td><td>" +  server.arg("green2") + "</td></tr>";  
    message += "<tr><td>Blue:</td><td>" + server.arg("blue1") + "</td><td>" + server.arg("blue2") + "</td></tr>"; 
    message += "<tr><td>Effect:</td><td>" + server.arg("effect1") + "</td><td>" + server.arg("effect2") + "</td></tr>";
    message += "</table>";
    if (saveSettings == "save") {
      message += "<br>";
      message += "<b>New settings saved as boot defaults.</b> Controller will now reboot.<br>";
      message += "You can return to the settings page after boot completes (lights will briefly turn blue to indicate completed boot).<br>";    
    } 
    message += "<br><a href=\"http://";
    message += baseIP;
    message += "\">Return to settings</a><br>";
    message += "</body></html>";
    message.replace("VAR_DEVICE_NAME", deviceName);
    server.send(200, "text/html", message);
    delay(1000);
    if (saveSettings == "save") {
      updateSettings(true);
    } else {
      updateSettings(false);
    } 
  }
}

// Firmware update handler
void handleUpdate() {
  String updFirmware = "<html>\
      </head>\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
        <title>VAR_DEVICE_NAME Controller Settings</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
      <H1>Firmware Update</H1>\
      <H3>Current firmware version: ";
  updFirmware += VERSION;
  updFirmware += "</H3><br>";
  updFirmware += "Notes:<br>";
  updFirmware += "<ul>";
  updFirmware += "<li>The firmware update will begin as soon as the Update Firmware button is clicked.</li><br>";
  updFirmware += "<li>Your current settings will be retained.</li><br>";
  updFirmware += "<li><b>Please be patient!</b> The update will take a few minutes.  Do not refresh the page or navigate away.</li><br>";
  updFirmware += "<li>If the upload is successful, a brief message will appear and the controller will reboot.</li><br>";
  updFirmware += "<li>After rebooting, you'll automatically be taken back to the main settings page and the update will be complete.</li><br>";
  updFirmware += "</ul><br>";
  updFirmware += "</body></html>";    
  updFirmware += "<form method='POST' action='/update2' enctype='multipart/form-data'>";
  updFirmware += "<input type='file' accept='.bin,.bin.gz' name='Select file' style='width: 300px'><br><br>";
  updFirmware += "<input type='submit' value='Update Firmware'>";
  updFirmware += "</form><br>";
  updFirmware += "<br><a href=\"http://";
  updFirmware += baseIP;
  updFirmware += "\">Return to settings</a><br>";
  updFirmware += "</body></html>";
  updFirmware.replace("VAR_DEVICE_NAME", deviceName);
  server.send(200, "text/html", updFirmware); 
}

void updateSettings(bool saveBoot) {
  // This updates the current local settings for current session only.  
  // Will be overwritten with reboot/reset/OTAUpdate
  if (saveBoot) {
    updateBootSettings();
  }
  //Update FastLED with new brightness and ON color settings
  FastLED.setBrightness(ledBrightness);
  ledColorOn_m1 = CRGB(ledRed_m1, ledGreen_m1, ledBlue_m1);
  ledColorOn_m2 = CRGB(ledRed_m2, ledGreen_m2, ledBlue_m2);
  toggleLights(true, 1);
  delay(2000);
  toggleLights(false, 0);
}

void updateBootSettings() {
  // Writes new settings to SPIFFS (new boot defaults)
  char t_device_name[25];
  char t_pir_count[3];
  char t_led_count[5];
  char t_led_on_time[4];
  char t_led_brightness[4];
  char t_led_red_m1[4];
  char t_led_green_m1[4];
  char t_led_blue_m1[4];
  char t_led_red_m2[4];
  char t_led_green_m2[4];
  char t_led_blue_m2[4];
  char t_led_effect[16];
  char t_led_effect_m2[16];
  int eff_len = 16;
  int dev_name_len = 25;
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("Attempting to update boot settings");
  #endif

  //Convert values into char arrays
  deviceName.toCharArray(t_device_name, dev_name_len);
  sprintf(t_pir_count, "%u", numPIRs);
  sprintf(t_led_count, "%u", numLEDs);
  sprintf(t_led_on_time, "%u", ledOnTime);
  sprintf(t_led_brightness, "%u", ledBrightness);
  sprintf(t_led_red_m1, "%u", ledRed_m1);
  sprintf(t_led_green_m1, "%u", ledGreen_m1);
  sprintf(t_led_blue_m1, "%u", ledBlue_m1);
  sprintf(t_led_red_m2, "%u", ledRed_m2);
  sprintf(t_led_green_m2, "%u", ledGreen_m2);
  sprintf(t_led_blue_m2, "%u", ledBlue_m2);
  ledEffect_m1.toCharArray(t_led_effect, eff_len);
  ledEffect_m2.toCharArray(t_led_effect_m2, eff_len);

    DynamicJsonDocument json(1024);
    json["device_name"] = t_device_name;
    json["pir_count"] = t_pir_count;
    json["led_count"] = t_led_count;
    json["led_on_time"] = t_led_on_time;
    json["led_brightness"] = t_led_brightness;
    json["led_red"] = t_led_red_m1;
    json["led_green"] = t_led_green_m1;
    json["led_blue"] = t_led_blue_m1;
    json["led_red_m2"] = t_led_red_m2;
    json["led_green_m2"] = t_led_green_m2;
    json["led_blue_m2"] = t_led_blue_m2;
    json["led_effect"] = t_led_effect;
    json["led_effect_m2"] = t_led_effect_m2;

    if (SPIFFS.begin()) {
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("failed to open config file for writing");
        #endif
      }
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          serializeJson(json, Serial);
        #endif
        serializeJson(json, configFile);
        configFile.close();
        //end save
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Boot settings saved. Rebooting controller.");
      #endif
    } else {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("failed to mount FS");
      #endif
    }
  SPIFFS.end();
  ESP.restart();
}

void handleReset() {
    String resetMsg = "<HTML>\
      </head>\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
        <title>VAR_DEVICE_NAME Controller Reset</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
      <H1>Controller Resetting...</H1><br>\
      <H3>After this process is complete, you must setup your controller again:</H3>\
      <ul>\
      <li>Connect a device to the controller's local access point: MotionLED_AP</li>\
      <li>Open a browser and go to: 192.168.4.1</li>\
      <li>Enter your WiFi information, device name and other default values</li>\
      <li>Click Save. The controller will reboot and rejoin your WiFi</li>\
      </ul><br>\
      Once the above process is complete, you can return to the main settings page by rejoining your WiFi and entering the IP address assigned by your router in a browser.<br><br>\
      <b>This page will NOT automatically reload or refresh</b>\
      </body></html>";
    resetMsg.replace("VAR_DEVICE_NAME", deviceName);
    server.send(200, "text/html", resetMsg);
    delay(1000);
    wifiManager.resetSettings();
    SPIFFS.format();
    delay(1000);
    ESP.restart();
}

void handleRestart() {
    String restartMsg = "<HTML>\
      </head>\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
        <title>VAR_DEVICE_NAME Controller Restart</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
      <H1>Controller restarting...</H1><br>\
      <H3>Please wait</H3><br>\
      After the controller completes the boot process (lights will flash blue followed by red/green for approximately 2-3 seconds each), you may click the following link to return to the main page:<br><br>\
      <a href=\"http://";      
    restartMsg += baseIP;
    restartMsg += "\">Return to settings</a><br>";
    restartMsg += "</body></html>";
    restartMsg.replace("VAR_DEVICE_NAME", deviceName);
    server.send(200, "text/html", restartMsg);
    delay(1000);
    ESP.restart();
}

// Not found or invalid page handler
void handleNotFound() {
  String message = "File Not Found or invalid command.\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(404, "text/plain", message);
}
// ==============================
//  Define Effects
// ==============================
//  Increase array size above if adding new
//  Effect name must not exceed 15 characters and must be a String
void defineEffects() {
  Effects[0] = "Solid";
  Effects[1] = "Chase";
  Effects[2] = "Chase-Reverse";
  Effects[3] = "In-Out";
  Effects[4] = "2-Segment";
}

// ==================================
//  Main Setup
// ==================================
void setup() {
  bool spiffsOpen = false;
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.begin(115200);
    Serial.println();
  #endif

  // -----------------------------------------
  //  Captive Portal and Wifi Onboarding Setup
  // -----------------------------------------
  //clean FS, for testing - uncomment next line ONLY if you wish to wipe current FS
  //SPIFFS.format();

  // *******************************
  // read configuration from FS json
  // *******************************
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("mounting FS...");
  #endif
  
  if (SPIFFS.begin()) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("mounted file system");
    #endif
    spiffsOpen = true;
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

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          serializeJson(json, Serial);
        #endif
        if ( ! deserializeError ) {
          #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
            Serial.println("\nparsed json");
          #endif
          // Read values here
          strcpy(pir_count, json["pir_count"]);
          strcpy(led_count, json["led_count"]);
          strcpy(led_on_time, json["led_on_time"]);
          strcpy(led_brightness, json["led_brightness"]);
          strcpy(led_red, json["led_red"]);
          strcpy(led_green, json["led_green"]);
          strcpy(led_blue, json["led_blue"]);
          strcpy(led_red_m2, json["led_red_m2"]|"255");
          strcpy(led_green_m2, json["led_green_m2"]|"255");
          strcpy(led_blue_m2, json["led_blue_m2"]|"255");
          strcpy(led_effect, json["led_effect"]|"Solid");
          strcpy(led_effect_m2, json["led_effect_m2"]|"Solid");
          strcpy(device_name, json["device_name"]|"MotionLED");
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
  
  WiFi.mode(WIFI_STA);
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_text("<p>Each controller must have a unique name. a-z, A-Z, 0-9, underscore and hyphen only.  No spaces, 24 chars max.  See the wiki for more information.</p>");
  WiFiManagerParameter custom_dev_name("devName", "Device Name", device_name, 24, "Unique Device Name (24 chars max)");
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
  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_dev_name);
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
  //here  "MyApName".  If not supplied, will use ESP + last 7 digits of MAC
  //and goes into a blocking loop awaiting configuration. If a password
  //is desired for the AP, add it after the AP name (e.g. autoConnect("MyApName", "12345678")
  if (!wifiManager.autoConnect("MotionLED_AP")) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("failed to connect and hit timeout");
    #endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  if (shouldSaveConfig) {
    strcpy(device_name, custom_dev_name.getValue());
  }

  //Set Host Names 
  deviceName = String(device_name);
  wifiHostName = deviceName;
  otaHostName = deviceName + "OTA";
  
  //if you get here you have connected to the WiFi
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(wifiHostName.c_str());
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("connected to your wifi...yay!");
  #endif
  //read updated parameters
  strcpy(device_name, custom_dev_name.getValue());
  strcpy(pir_count, custom_pir_num.getValue());
  strcpy(led_count, custom_led_num.getValue());
  strcpy(led_on_time, custom_led_time.getValue());
  strcpy(led_brightness, custom_led_brightness.getValue());
  strcpy(led_red, custom_led_red.getValue());
  strcpy(led_green, custom_led_green.getValue());
  strcpy(led_blue, custom_led_blue.getValue());

  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("The values in the file are: ");
    Serial.println("\tdevice_name : " + String(device_name));
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

    DynamicJsonDocument json(1024);
    json["device_name"] = device_name;
    json["pir_count"] = pir_count;
    json["led_count"] = led_count;
    json["led_on_time"] = led_on_time;
    json["led_brightness"] = led_brightness;
    json["led_red"] = led_red;
    json["led_green"] = led_green;
    json["led_blue"] = led_blue;
    // New values
    json["led_red_m2"] = led_red;
    json["led_green_m2"] = led_green;
    json["led_blue_m2"] = led_blue;
    json["led_effect"] = "Solid";
    json["led_effect_m2"] = "Solid";

    //Provide defaults for first time values that aren't part of onboarding (Issue #2)
    ledEffect_m1.toCharArray(led_effect, 16);
    ledEffect_m2.toCharArray(led_effect_m2, 16);
    strcpy(led_red_m2, led_red);
    strcpy(led_green_m2, led_green);
    strcpy(led_blue_m2, led_blue);
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("failed to open config file for writing");
      #endif
    }
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      serializeJson(json, Serial);
    #endif
    serializeJson(json, configFile);
    configFile.close();
    SPIFFS.end();
    //end save
  }
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  #endif
  baseIP = WiFi.localIP().toString();
  // ----------------------------
  

  //Convert config values to local values
  numPIRs = (String(pir_count)).toInt();
  numLEDs = (String(led_count)).toInt();
  ledOnTime = (String(led_on_time)).toInt();
  ledBrightness = (String(led_brightness)).toInt();
  ledRed_m1 = (String(led_red)).toInt();
  ledGreen_m1 = (String(led_green)).toInt();
  ledBlue_m1 = (String(led_blue)).toInt();
  ledRed_m2 = (String(led_red_m2)).toInt();
  ledGreen_m2 = (String(led_green_m2)).toInt();
  ledBlue_m2 = (String(led_blue_m2)).toInt();
  ledEffect_m1 = String(led_effect);
  ledEffect_m2 = String(led_effect_m2);

  // SETUP FASTLED  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS_MAX);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(ledBrightness);
  ledColorOn_m1 = CRGB(ledRed_m1, ledGreen_m1, ledBlue_m1);
  ledColorOn_m2 = CRGB(ledRed_m2, ledGreen_m2, ledBlue_m2);

  // Setup motion pins
  pinMode(MOTION_PIN_1, INPUT);
  pinMode(MOTION_PIN_2, INPUT);

  // Define Effects
  defineEffects();
  
  //-----------------------------
  // Setup OTA Updates
  //-----------------------------
  ArduinoOTA.setHostname(otaHostName.c_str());
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
  
  //------------------------------
  // Setup handlers for web calls
  //------------------------------
  server.on("/", handleRoot);

  server.on("/postform/", handleForm);

  server.onNotFound(handleNotFound);

  server.on("/update", handleUpdate);

  server.on("/restart", handleRestart);

  server.on("/reset", handleReset);

  server.on("/otaupdate",[]() {
    //Called directly from browser address (//ip_address/otaupdate) to put controller in ota mode for uploadling from Arduino IDE
    server.send(200, "text/html", "<h1>Ready for upload...<h1><h3>Start upload from IDE now</h3>");
    ota_flag = true;
    ota_time = ota_time_window;
    ota_time_elapsed = 0;
  });
  //OTA Update Handler
  httpUpdater.setup(&server, "/update2");
  httpUpdater.setup(&server);
  server.begin();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("Setup complete - starting main loop");
  #endif

  // ---------------------------------------------------------
  // Flash LEDs blue for 2 seconds to indicate successful boot 
  // ---------------------------------------------------------
  fill_solid(LEDs, numLEDs, CRGB::Blue);
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("LEDs Blue - FASTLED ok");
  #endif
  delay(2000);
  fill_solid(LEDs, numLEDs, CRGB::Black);
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("LEDs Reset to off");
  #endif
}

//==================================
// Main Loop
//==================================
void loop() {
  //Handle OTA updates when OTA flag set via HTML call to http://ip_address/otaupdate
  if (ota_flag) {
    showOTA();
    uint16_t ota_time_start = millis();
    while (ota_time_elapsed < ota_time) {
      ArduinoOTA.handle();  
      ota_time_elapsed = millis()-ota_time_start;   
      delay(10); 
    }
    allLEDsOff();
    ota_flag = false;
  }
  //Handle any web calls
  server.handleClient();
  //Main loop
  unsigned long curTime = millis();

  if ((bTesting) && (!lightsOn)) {
    toggleLights(true, 1);
  } else if (numPIRs > 1) {
    if (digitalRead(MOTION_PIN_1) == HIGH) {
      if (!lightsOn) {
        toggleLights(true, 1); 
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("Motion 1 Lights on");
        #endif 
      }
      onTime = millis();    
    } else if  (digitalRead(MOTION_PIN_2) == HIGH) {
      if (!lightsOn) {
        toggleLights(true, 2);
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("Motion 2 Lights on");
        #endif 
      }
      onTime = millis();    
    } else if ((digitalRead(MOTION_PIN_1) == LOW) && (digitalRead(MOTION_PIN_2) == LOW) && (lightsOn) && ((millis() - onTime) > (ledOnTime * 1000))) {
      toggleLights(false, 0);
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Lights off");
      #endif
    }

  } else {
    if ((digitalRead(MOTION_PIN_1) == HIGH)) {
      if (!lightsOn) {
        toggleLights(true, 1);
      }
      onTime = millis();
    } else if ((digitalRead(MOTION_PIN_1) == LOW) && (lightsOn) && ((millis() - onTime) > (ledOnTime * 1000))) {
      toggleLights(false, 0);
    }
  }
}
// =========================
//  Other functions
// =========================
void toggleLights(bool turnOn, byte whichMotion) {
  if (turnOn) {
    lightsOn = true;
    if (whichMotion == 1) {
      if (ledEffect_m1 == "Solid") {
        effectSolid(ledColorOn_m1);
      } else if (ledEffect_m1 == "Chase") {
        effectChase(ledColorOn_m1);
      } else if (ledEffect_m1 == "Chase-Reverse") {
        effectChaseReverse(ledColorOn_m1);
      } else if (ledEffect_m1 == "In-Out") {
        effectInOut(ledColorOn_m1);
      } else if (ledEffect_m1 == "2-Segment") {
        effect2Segment(ledColorOn_m1);
      } else {
        //Default to solid (Issue #2)
        effectSolid(ledColorOn_m1);
      }
    } else {
      if (ledEffect_m2 == "Solid") {
        effectSolid(ledColorOn_m2);
      } else if (ledEffect_m2 == "Chase") {
        effectChase(ledColorOn_m2);
      } else if (ledEffect_m2 == "Chase-Reverse") {
        effectChaseReverse(ledColorOn_m2);
      } else if (ledEffect_m2 == "In-Out") {
        effectInOut(ledColorOn_m2);
      } else if (ledEffect_m2 == "2-Segment") {
        effect2Segment(ledColorOn_m2);
      } else {
        //Default to solid (Issue #2)
        effectSolid(ledColorOn_m2);
      }
      
    }
  } else {
    fill_solid(LEDs, numLEDs, ledColorOff);   
    lightsOn = false;
    FastLED.show();
  }
}
// =====================
//  Effects
// =====================
void effectSolid(CRGB ledColor) {
  fill_solid(LEDs, numLEDs, ledColor);
  FastLED.show();
}

void effectChase(CRGB ledColor) {
  for (int i=0; i < (numLEDs); i++) {
    LEDs[i] = ledColor;
    FastLED.show();
    delay(25);
  }
}

void effectChaseReverse(CRGB ledColor) {
  for (int r=(numLEDs-1); r>=0; r--) {
    LEDs[r] = ledColor;
    FastLED.show();
    delay(25);
  }
}

void effect2Segment(CRGB ledColor) {
  for (int i=0; i < (numLEDs); i++) {
    LEDs[i] = ledColorOn_m1;
    LEDs[(numLEDs - 1) - i] = ledColorOn_m2;
    FastLED.show();
    delay(25);
  }  
}

void effectInOut(CRGB ledColor) {
  bool isOdd;
  int startPixel = numLEDs/2;
  int mod = numLEDs % 2;
  int upCounter;
  //adjust for 0 based array
  startPixel = startPixel - 1;
  if (mod != 0) {
    startPixel = startPixel + 1;
    upCounter = startPixel;
  } else {
    upCounter = startPixel + 1;
  }
  for (int i=(startPixel); i >= 0; i--) {
     LEDs[i] = ledColor;
     LEDs[upCounter] = ledColor;
     upCounter = upCounter + 1;
     FastLED.show();
     delay(25);
  }
}

void showOTA() {
  fill_solid(LEDs, numLEDs, CRGB::Black);
  //Alternate LED colors using red and green
  FastLED.setBrightness(ledBrightness);
  for (int i=0; i < (numLEDs-1); i = i + 2) {
    LEDs[i] = CRGB::Red;
    LEDs[i+1] = CRGB::Green;
  }
  FastLED.show();  
}

void allLEDsOff() {
  fill_solid(LEDs, numLEDs, ledColorOff);   
  lightsOn = false;
  FastLED.show();

}
