/**
  ESP8266 Tower Controller firmware for the Open eXtensible Rack System
  
  See ********** for documentation.
  Compile options:
    ESP8266
  External dependencies. Install using the Arduino library manager:
    "PubSubClient" by Nick O'Leary
    "OXRS-IO-MQTT-ESP32-LIB" by OXRS Core Team
    "OXRS-IO-API-ESP32-LIB" by OXRS Core Team
    "Adafruit_NeoPixel.h" by Adafruit
    "Adafruit_MCP9808.h" by Adafruit
    "RTClib.h"
    "hp_BH1750.h"
    
    
  GitHub repository:
    ***********
    
  Bugs/Features:
    See GitHub issues list
    
  Copyright 2021 Austins Creations
*/


/*--------------------------- Version ------------------------------------*/
#define FW_NAME       "OXRS-AC-TowerController-ESP8266-FW"
#define FW_SHORT_NAME "Tower Controller"
#define FW_MAKER      "Austin's Creations"
#define FW_VERSION    "0.9.1"

/*--------------------------- Configuration ------------------------------*/
// Should be no user configuration in this file, everything should be in;
#include "config.h"

/*--------------------------- Libraries ----------------------------------*/
#include <SPI.h>                      // for ethernet
#include <Ethernet.h>                 // for ethernet
#include <Wire.h>                     // For I2C
#include <ESP8266WiFi.h>              // For networking
#include <PubSubClient.h>             // For MQTT
#include <OXRS_MQTT.h>                // For MQTT
#include <OXRS_API.h>                 // For REST API
#include <Adafruit_NeoPixel.h>        // for the LED drivers
#include <Adafruit_MCP9808.h>         // For temp sensor
#include <Adafruit_SSD1306.h>         // for OLED display
#include <RTClib.h>                   // for PCF8523 RTC
#include <hp_BH1750.h>                // for bh1750 lux sensor
#include <Adafruit_SHT4x.h>           // for SHT40 Temp / humidity sensor


/*--------------------------- Constants ----------------------------------*/
// Serial
#define       SERIAL_BAUD_RATE          115200

// MQTT
#define       MQTT_MAX_MESSAGE_SIZE     4096

// REST API
#define       REST_API_PORT             80

// How often to update sensors
#define       DEFAULT_UPDATE_MS         60000

// MCP9808 temperature sensor
#define       MCP9808_I2C_ADDRESS       0x18
#define       MCP9808_MODE              0

//general temp monitoring varible
#define       TEMP_C                    1
#define       TEMP_F                    2

// SHT40 temperature and humidity sensor
#define       SHT40_I2C_ADDRESS         0x44

// BH1750 LUX sensor
#define       BH1750_I2C_ADDRESS        0x23 // or 0x5C

// PCF8523 RTC Module
#define       PCF8523_I2C_ADDRESS       0x68
#define       PCF8523_12                0
#define       PCF8523_24                1

// OLED Screen
#define       OLED_I2C_ADDRESS          0x3C
#define       OLED_RESET                -1
#define       SCREEN_WIDTH              128
#define       SCREEN_HEIGHT             32 
#define       OLED_INTERVAL_TEMP        10000L
#define       OLED_INTERVAL_LUX         2500L

// Supported LED modes
#define       LED_MODE_NONE             0
#define       LED_MODE_COLOUR           1
#define       LED_MODE_FLASH            3

// Supported Tower modes
#define       TOWER_MODE_SINGLE         0
#define       TOWER_MODE_MULTI          1

// For the 3ch WS2811 LED driver IC
#define       LED_PIN                   0
#define       LED_COUNT                 2

// Supported OLED modes
#define       OLED_MODE_OFF             0 // screen is off
#define       OLED_MODE_ONE             1 // screen shows IP and MAC address
#define       OLED_MODE_TWO             2 // screen shows Time and Temp
#define       OLED_MODE_THREE           3 // screen shows Temp and LUX
#define       OLED_MODE_FOUR            4 // screen shows Temp and Humidity
#define       OLED_MODE_FIVE            5 // screen shows Custom text per - line

/*-------------------------- Internal datatypes --------------------------*/

/*--------------------------- Global Variables ---------------------------*/
// Flashing state for any strips in flash mode
uint8_t g_flash_state = false;

// How often to update Sensor mqtt telemetry
uint32_t lastUpdate;
uint32_t UpdateMs    = DEFAULT_UPDATE_MS;

// I2C devices found
bool    SensorFound = false;            // set to true if sensor requiring tele mqtt updated is needed
bool    temp_SensorFound = false;
bool    lux_SensorFound = false;
bool    hum_SensorFound = false;

// mqtt temperature reading
float temperature;
float tempSHT_C;
float tempSHT_F;
float humSHT;

// universal tmperature variable - for degrees c / f
uint8_t tempMode = TEMP_C;

// sensor varibles - using with OLED
float c; // temp reading variable
float f; // temp reading variable
float h; // hum  reading variable sht40
float l; // lux reading variable
unsigned long previous_OLED_temp = 0;
unsigned long previous_OLED_lux = 0;

// OLED varibles
bool    oled_Found = false;
uint8_t screenMode = OLED_MODE_ONE;
String  screenLineOne = "Hello";
String  screenLineTwo = "World";

// RTC varibles
bool    rtc_Found = false;
int     current_hour;
int     current_minute;
uint8_t clockMode = PCF8523_24;

// LED varibles
uint8_t towermode = TOWER_MODE_SINGLE;   // set default tower mode to single - can be changed via mqtt config
uint8_t ledmode[6] = {0,0,0,0,0,0};        // mode for each led channel
uint8_t ledbrightness[6] = {0,0,0,0,0,0};  // brightness for led channel

// tower varible
uint8_t output[6] = {0,0,0,0,0,0};       // output value for tower 
uint8_t sleepModes[7] = {0,1,1,1,1,0,1}; // if a light is on / off during sleep mode includes OLED as well
uint8_t sleepState = false;              // is the device supposed to be asleep

/*--------------------------- Instantiate Global Objects -----------------*/
// Ethernet client
EthernetClient client;

// MQTT client
PubSubClient mqttClient(client);
OXRS_MQTT mqtt(mqttClient);

// REST API
//WiFiServer server(REST_API_PORT);
EthernetServer server(80);
OXRS_API api(mqtt);

// LED Driver IC
Adafruit_NeoPixel driver(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

// OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PCF8523 RTC
RTC_PCF8523 rtc;

// MCP9808 temp sensor
Adafruit_MCP9808 tempSensor = Adafruit_MCP9808();

// BH1750 lux sensor
hp_BH1750 luxSensor;

// SHT40 Temperature / Humitity Sensor
Adafruit_SHT4x sht4 = Adafruit_SHT4x();


#include "OLED.h"                     // emdedded routine for OLED display

/*--------------------------- Program ------------------------------------*/
void mqttCallback(char * topic, uint8_t * payload, unsigned int length) 
{
  // Pass this message down to our MQTT handler
  mqtt.receive(topic, payload, length);
}

/**
  Setup
*/
void setup() {
  Ethernet.init(15);
  // Startup logging to serial
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();
  Serial.println(F("==============================="));
  Serial.println(FW_NAME);
  Serial.print  (F("            v"));
  Serial.println(FW_VERSION);
  Serial.println(F("==============================="));

  // Start the I2C bus
  Wire.begin();

  // Scan the I2C bus and set up I/O buffers
  scanI2CBus();

  // Set up LED Driver
  driver.begin();          // INITIALIZE NeoPixel strip object (REQUIRED)
  driver_off();            // Turn all drivers off
  driver_off();            // Turn all drivers off

  OLED_loop();             // show default screen of the MAC address

  // Set up network and obtain an IP address
  byte mac[6];
  initialiseEthernet(mac);
  

  OLED_loop();             // show default screen + add the IP address

  // Set up MQTT (don't attempt to connect yet)
  initialiseMqtt(mac);

  // Set up the REST API
  initialiseRestApi();

}

/**
  Main processing loop
*/
void loop() {
  ethernet_loop();
  mqtt.loop();

  EthernetClient client = server.available();
  api.checkEthernet(&client);

  driver_loop();  // update driver channels

  OLED_loop();    // update OLED screen if available

  updateSensor(); // updates the tele sensor data

}

void ethernet_loop()
{
  switch (Ethernet.maintain()) {
    case 1:
      //renewed fail
      Serial.println("Error: renewed fail");
      break;

    case 2:
      //renewed success
      Serial.println("Renewed success");
      //print your local IP address:
      Serial.print("My IP address: ");
      Serial.println(Ethernet.localIP());
      break;

    case 3:
      //rebind fail
      Serial.println("Error: rebind fail");
      break;

    case 4:
      //rebind success
      Serial.println("Rebind success");
      //print your local IP address:
      Serial.print("My IP address: ");
      Serial.println(Ethernet.localIP());
      break;

    default:
      //nothing happened
      break;
  }
}

void driver_loop()
{
  for (uint8_t chan; chan < 6; chan++)
  {
/*
 * need to know what each channel has to do
 * save each channels value to combine at the end and send to drivers
 * update values based on flashing mode
 * buzzer to be added later
 * 
 */
    if (ledmode[chan] == LED_MODE_COLOUR) // static color selected
    {
      output[chan] = ledbrightness[chan];
    }
    else if (ledmode[chan] == LED_MODE_FLASH) // flash specified channel
    {
      if (g_flash_state == HIGH)
      {
        output[chan] = ledbrightness[chan];
      }
      else
      {
        output[chan] = 0;
      }
    }
    else
    {
      output[chan] = 0;
    }
    // device should be asleep instead
    if (sleepState == true)
    {
      if (sleepModes[chan] == 1) // this channel is confirmed to be off in sleepstate mode
      {
        output[chan] = 0;
      }
    }
  }
    driver.setPixelColor(0,output[0],output[1],output[2]);         //  Set pixel's color (in RAM)
    driver.setPixelColor(1,output[3],output[4],output[5]);         //  Set pixel's color (in RAM)
    driver.show();                                                 //  Update drivers to match
  
}



/**
  I2C
*/
void scanI2CBus()
{
//  BH1750_I2C_ADDRESS  varible with address in it
//  MCP9808_I2C_ADDRESS varible with address in it
//  PCF8523_I2C_ADDRESS varible with address in it
//  OLED_I2C_ADDRESS    varible with address in it
//  SensorFound       / true false
//  temp_SensorFound  / true false
//  lux_SensorFound   / true false
//  hum_SensorFound   / true false
//  rtc_Found         / true false
//  oled_Found        / true false

  Serial.println(F("[TLC] scanning for i2c devices..."));

    Serial.print(F(" - 0x"));
    Serial.print(BH1750_I2C_ADDRESS, HEX);
    Serial.print(F("..."));
  // Check if there is anything responding on this address
  Wire.beginTransmission(BH1750_I2C_ADDRESS);
  if (Wire.endTransmission() == 0)
  {
    lux_SensorFound = luxSensor.begin(BH1750_TO_GROUND);
      if (!lux_SensorFound)
      {
        Serial.print(F("[TLC] no BH1750 lux sensor found at 0x"));
        Serial.println(BH1750_I2C_ADDRESS, HEX);
        return;
      }
      Serial.println(F("BH1750"));
      luxSensor.start(); // start getting value (for non-blocking use)
      SensorFound = true;
    }
    else
    {
      Serial.println(F("empty"));
    }

    Serial.print(F(" - 0x"));
    Serial.print(SHT40_I2C_ADDRESS, HEX);
    Serial.print(F("..."));
  // Check if there is anything responding on this address
  Wire.beginTransmission(SHT40_I2C_ADDRESS);
  if (Wire.endTransmission() == 0)
  {
    hum_SensorFound = sht4.begin();
      if (!hum_SensorFound)
      {
        Serial.print(F("[TLC] no SHT40 sensor found at 0x"));
        Serial.println(SHT40_I2C_ADDRESS, HEX);
        return;
      }
      Serial.println(F("SHT40"));
      sht4.setPrecision(SHT4X_MED_PRECISION);
      sht4.setHeater(SHT4X_NO_HEATER);
      SensorFound = true;
    }
    else
    {
      Serial.println(F("empty"));
    }
    

    Serial.print(F(" - 0x"));
    Serial.print(MCP9808_I2C_ADDRESS, HEX);
    Serial.print(F("..."));
    Wire.beginTransmission(MCP9808_I2C_ADDRESS);
  if (Wire.endTransmission() == 0)
  {
      temp_SensorFound = tempSensor.begin(MCP9808_I2C_ADDRESS);
      if (!temp_SensorFound)
      {
        Serial.print(F("[TLC] no MCP9808 temp sensor found at 0x"));
        Serial.println(MCP9808_I2C_ADDRESS, HEX);
        return;
      }
      // Set the temp sensor resolution (higher res takes longer for reading)
      tempSensor.setResolution(MCP9808_MODE);

      Serial.println(F("MCP9808"));
      SensorFound = true;
    }
    else
    {
      Serial.println(F("empty"));
    }

    Serial.print(F(" - 0x"));
    Serial.print(OLED_I2C_ADDRESS, HEX);
    Serial.print(F("..."));
  // Check if there is anything responding on this address
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  if (Wire.endTransmission() == 0)
  {
    oled_Found = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
      if (!oled_Found)
      {
        Serial.print(F("[TLC] no OLED found at 0x"));
        Serial.println(OLED_I2C_ADDRESS, HEX);
        return;
      }
      display.clearDisplay();
      display.display();
      Serial.println(F("OLED"));
    }
    else
    {
      Serial.println(F("empty"));
    }

    Serial.print(F(" - 0x"));
    Serial.print(PCF8523_I2C_ADDRESS, HEX);
    Serial.print(F("..."));
  // Check if there is anything responding on this address
  Wire.beginTransmission(PCF8523_I2C_ADDRESS);
  if (Wire.endTransmission() == 0)
  {
    Serial.println(F("RTC"));
    rtc_Found = rtc.begin();
      if (!rtc_Found)
      {
        Serial.print(F("[TLC] no RTC found at 0x"));
        Serial.println(PCF8523_I2C_ADDRESS, HEX);
        return;
      }

    if (! rtc.initialized() || rtc.lostPower()) 
    {
    Serial.println(F("[TLC] RTC is NOT initialized!"));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println(F("[TLC] RTC Time set with compile time"));
    }
      rtc.start();
    }
    else
    {
      Serial.println(F("empty"));
    }
}

/**
  MQTT
*/
void getFirmwareJson(JsonVariant json)
{
  JsonObject firmware = json.createNestedObject("firmware");

  firmware["name"] = FW_NAME;
  firmware["shortName"] = FW_SHORT_NAME;
  firmware["maker"] = FW_MAKER;
  firmware["version"] = FW_VERSION;
}

void getNetworkJson(JsonVariant json)
{
  byte mac[6];
  WiFi.macAddress(mac);
  
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  JsonObject network = json.createNestedObject("network");

  network["ip"] = WiFi.localIP();
  network["mac"] = mac_display;
}

void getConfigSchemaJson(JsonVariant json)
{
  JsonObject configSchema = json.createNestedObject("configSchema");
  
  // Config schema metadata
  configSchema["$schema"] = "http://json-schema.org/draft-04/schema#";
  configSchema["title"] = FW_NAME;
  configSchema["type"] = "object";
}

void getCommandSchemaJson(JsonVariant json)
{
  JsonObject commandSchema = json.createNestedObject("commandSchema");
  
  // Command schema metadata
  commandSchema["$schema"] = "http://json-schema.org/draft-04/schema#";
  commandSchema["title"] = FW_NAME;
  commandSchema["type"] = "object";
}

void mqttConnected() 
{
  // Build device adoption info
  DynamicJsonDocument json(4096);

  JsonVariant adopt = json.as<JsonVariant>();
  
  getFirmwareJson(adopt);
  getNetworkJson(adopt);
  getConfigSchemaJson(adopt);
  getCommandSchemaJson(adopt);

  // Publish device adoption info
  mqtt.publishAdopt(adopt);

  // Log the fact we are now connected
  Serial.println("[TLC] mqtt connected");
}

void mqttDisconnected(int state) 
{
  // Log the disconnect reason
  // See https://github.com/knolleary/pubsubclient/blob/2d228f2f862a95846c65a8518c79f48dfc8f188c/src/PubSubClient.h#L44
  switch (state)
  {
    case MQTT_CONNECTION_TIMEOUT:
      Serial.println(F("[TLC] mqtt connection timeout"));
      break;
    case MQTT_CONNECTION_LOST:
      Serial.println(F("[TLC] mqtt connection lost"));
      break;
    case MQTT_CONNECT_FAILED:
      Serial.println(F("[TLC] mqtt connect failed"));
      break;
    case MQTT_DISCONNECTED:
      Serial.println(F("[TLC] mqtt disconnected"));
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      Serial.println(F("[TLC] mqtt bad protocol"));
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      Serial.println(F("[TLC] mqtt bad client id"));
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      Serial.println(F("[TLC] mqtt unavailable"));
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      Serial.println(F("[TLC] mqtt bad credentials"));
      break;      
    case MQTT_CONNECT_UNAUTHORIZED:
      Serial.println(F("[TLC] mqtt unauthorised"));
      break;      
  }
}

void jsonConfig(JsonVariant json) // config payload
{
  if (json.containsKey("ScreenMode")) // for what mode the OLED is in
  {
    if (strcmp(json["ScreenMode"], "off") == 0){screenMode = OLED_MODE_OFF;}
    if (strcmp(json["ScreenMode"], "one") == 0){screenMode = OLED_MODE_ONE;}
    if (strcmp(json["ScreenMode"], "two") == 0){screenMode = OLED_MODE_TWO;}
    if (strcmp(json["ScreenMode"], "three") == 0){screenMode = OLED_MODE_THREE;}
    if (strcmp(json["ScreenMode"], "four") == 0){screenMode = OLED_MODE_FOUR;} 
    if (strcmp(json["ScreenMode"], "five") == 0){screenMode = OLED_MODE_FIVE;}
  }

  if (json.containsKey("ClockMode")) // for what mode the tower lights are in
  {
    if (strcmp(json["ClockMode"], "12") == 0)
    {
      clockMode = PCF8523_12;
    }
    else if (strcmp(json["ClockMode"], "24") == 0)
    {
      clockMode = PCF8523_24;
    }
    else 
    {
      Serial.println(F("[TLC] invalid Temp Mode Config"));
    }
  }


  if (json.containsKey("TempMode")) // for what mode the tower lights are in
  {
    if (strcmp(json["TempMode"], "c") == 0)
    {
      tempMode = TEMP_C;
    }
    else if (strcmp(json["TempMode"], "f") == 0)
    {
      tempMode = TEMP_F;
    }
    else 
    {
      Serial.println(F("[TLC] invalid Temp Mode Config"));
    }
  }
  
  if (json.containsKey("TowerMode")) // for what mode the tower lights are in
  {
    if (strcmp(json["TowerMode"], "single") == 0)
    {
      towermode = TOWER_MODE_SINGLE;
      for (uint8_t x = 0; x < 6; x++){ledbrightness[x] = 0;} // reset tower outputs
    }
    else if (strcmp(json["TowerMode"], "multi") == 0)
    {
      towermode = TOWER_MODE_MULTI;
      for (uint8_t x = 0; x < 6; x++){ledbrightness[x] = 0;} // reset tower outputs
    }
    else 
    {
      Serial.println(F("[TLC] invalid Tower Mode Config"));
    }
  }

   if (json.containsKey("sleepModes"))
  {
    JsonArray array = json["sleepModes"].as<JsonArray>();
    uint8_t sleepnum = 0;
    
    for (JsonVariant v : array)
    {
      sleepModes[sleepnum++] = v.as<uint8_t>();
    }
  }

  if (json.containsKey("year")) // set RTC time
  {
    if (rtc_Found == true)// we have an RTC to update
    {
      if (!json.containsKey("month"))
      {
        Serial.println(F("[TLC] mising month value"));
        return;
      }
      if (!json.containsKey("day"))
      {
        Serial.println(F("[TLC] mising day value"));
        return;
      }
      if (!json.containsKey("hour"))
      {
        Serial.println(F("[TLC] mising hour value"));
        return;
      }
      if (!json.containsKey("minute"))
      {
        Serial.println(F("[TLC] mising minute value"));
        return;
      }
      if (!json.containsKey("seconds"))
      {
        Serial.println(F("[TLC] mising seconds value"));
        return;
      }
//      January 21, 2014 at 3am you would call:
//      rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
      rtc.adjust(DateTime(json["year"].as<uint16_t>(),json["month"].as<uint8_t>(),json["day"].as<uint8_t>(), json["hour"].as<uint8_t>(), json["minute"].as<uint8_t>(), json["seconds"].as<uint8_t>()));
    } else{Serial.println(F("[TLC] no RTC detected to update"));}
  }

   if (json.containsKey("UpdateMillis")) // for updating i2c sensors
   {
     UpdateMs = json["UpdateMillis"].as<uint32_t>();
   }
}

void jsonCommand(JsonVariant json) // do something payloads
{  
  if (json.containsKey("ScreenMode")) // for what mode the OLED is in
  {
    if (strcmp(json["ScreenMode"], "off") == 0){screenMode = OLED_MODE_OFF;}
    if (strcmp(json["ScreenMode"], "one") == 0){screenMode = OLED_MODE_ONE;}
    if (strcmp(json["ScreenMode"], "two") == 0){screenMode = OLED_MODE_TWO;}
    if (strcmp(json["ScreenMode"], "three") == 0){screenMode = OLED_MODE_THREE;}
    if (strcmp(json["ScreenMode"], "four") == 0){screenMode = OLED_MODE_FOUR;}
    if (strcmp(json["ScreenMode"], "five") == 0){screenMode = OLED_MODE_FIVE;} 
  }

  if (json.containsKey("OLEDone")) // custom text for OLED
  {
    screenLineOne = json["OLEDone"].as<String>();
  }

  if (json.containsKey("OLEDtwo")) // custom text for OLED
  {
    screenLineTwo = json["OLEDtwo"].as<String>();
  }

  if (json.containsKey("channel"))
  {
      jsonChannelCommand(json);
  }

  if (json.containsKey("flash"))
  {
    g_flash_state = json["flash"].as<bool>() ? HIGH : LOW;
  }

  if (json.containsKey("sleep"))
  {
    sleepState = json["sleep"].as<bool>() ? HIGH : LOW;
  }

  if (json.containsKey("restart") && json["restart"].as<bool>())
  {
    ESP.restart();
  }
}

void jsonChannelCommand(JsonVariant json) //sets led channel mode and brightness during normal operation
{
  uint8_t channels = getChannel(json);
  if (channels == 0) return;

  uint8_t lightchan = (channels - 1);

if (json.containsKey("mode"))
  {
    if (strcmp(json["mode"], "colour") == 0)
    { 
      ledmode[lightchan] = LED_MODE_COLOUR;
    }
    else if (strcmp(json["mode"], "flash") == 0)
    {
      ledmode[lightchan] = LED_MODE_FLASH;
    }
    else 
    {
      Serial.println(F("[TLC] invalid mode"));
      return;
    }
  }

  if (json.containsKey("brightness"))
  {
    if (lightchan == 5)
    {
      ledbrightness[lightchan] = json["brightness"].as<uint8_t>();
    }
    else
    {
      if (towermode == TOWER_MODE_MULTI)
      {
        ledbrightness[lightchan] = json["brightness"].as<uint8_t>();
      }
      else
      {
        for (uint8_t x = 0; x < 5; x++)
        {
          ledbrightness[x] = 0;
        }
        ledbrightness[lightchan] = json["brightness"].as<uint8_t>();
      }
    }
  }
}

uint8_t getChannel(JsonVariant json)
{
  if (!json.containsKey("channel"))
  {
    Serial.println(F("[TLC] missing channel"));
    return 0;
  }
  
  uint8_t channel = json["channel"].as<uint8_t>();

  // Check the controller is valid for this device
  if (channel <= 0 || channel > 6)
  {
    Serial.println(F("[TLC] invalid channel"));
    return 0;
  }

  return channel;
}



/*
 WIFI
 */
void initialiseEthernet(byte * mac)
{
  // Determine MAC address
  WiFi.macAddress(mac);

  // Display MAC address on serial
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(F("[TLC] mac address: "));
  Serial.println(mac_display);  

  Serial.println("[TLC] Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("[TLC] Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("[TLC] Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("[TLC] Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  }
  Serial.println();
  // print your local IP address:
  Serial.print(F("[TLC] IP address: "));
  Serial.println(Ethernet.localIP());
}

/*
 MQTT
 */
void initialiseMqtt(byte * mac)
{
  // Set the default client id to the last 3 bytes of the MAC address
  char clientId[32];
  sprintf_P(clientId, PSTR("%02x%02x%02x"), mac[3], mac[4], mac[5]);  
  mqtt.setClientId(clientId);
  
  // Register our callbacks
  mqtt.onConnected(mqttConnected);
  mqtt.onDisconnected(mqttDisconnected);
  mqtt.onConfig(jsonConfig);
  mqtt.onCommand(jsonCommand);  

  // Start listening for MQTT messages
  mqttClient.setCallback(mqttCallback);  
}


void initialiseRestApi(void)
{
  // NOTE: this must be called *after* initialising MQTT since that sets
  //       the default client id, which has lower precendence than MQTT
  //       settings stored in file and loaded by the API

  // Set up the REST API
  api.begin();
  api.setFirmware(FW_NAME, FW_SHORT_NAME, FW_MAKER, FW_VERSION);

  server.begin();
}


// LED Driver extra
void driver_off() {
    driver.setPixelColor(0,0,0,0);         //  Set pixel's color (in RAM)
    driver.setPixelColor(1,0,0,0);         //  Set pixel's color (in RAM)
    driver.show();                         //  Update drivers to match
}


boolean publishTelemetry(JsonVariant json)
{
  // Exit early if no network connection
  if (Ethernet.linkStatus() == LinkON){
    boolean success = mqtt.publishTelemetry(json);
    return success;
  }else{
    return false;
  }
}

void updateSensor()
{
  // Ignore if it has been disabled
  if (UpdateMs == 0) { return; }
  
  // Check if we need to get a new temp reading and publish
  if ((millis() - lastUpdate) > UpdateMs)
  {    
    StaticJsonDocument<64> json;
    
    if (temp_SensorFound == true)
    {
      if (tempMode == TEMP_C){temperature = tempSensor.readTempC();}
      if (tempMode == TEMP_F){temperature = tempSensor.readTempF();}
      if (temperature != NAN)
      {
      // Publish temp to mqtt
      char payload[8];
      sprintf(payload, "%2.1f", temperature);
      json["MCP9808-temp"] = payload;
     }
    }

    if (hum_SensorFound == true)
    {
      sensors_event_t humidity, temp;
      sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

      tempSHT_C = temp.temperature;
      tempSHT_F = (temp.temperature*1.8)+32;
      humSHT = humidity.relative_humidity;
      char payload[8];
      char payload2[8];
      if (tempMode == TEMP_C){sprintf(payload, "%2.1f", tempSHT_C);}
      if (tempMode == TEMP_F){sprintf(payload, "%2.1f", tempSHT_F);}
      // Publish temp to mqtt
      json["SHT40-temp"] = payload;
      sprintf(payload2, "%2.1f", humSHT);
      json["SHT40-hum"] = payload2;
     }

    if (lux_SensorFound == true)
    {
      if (luxSensor.hasValue() == true) // non blocking reading
      {
        float lux = luxSensor.getLux();
        luxSensor.start();
        char payload[8];
        sprintf(payload, "%2.1f", lux);
        json["lux"] = payload;
      }
    }
    
      char cstr1[1];
      itoa(sleepState, cstr1, 10);
      json["sleepState"] = cstr1;
    
    publishTelemetry(json.as<JsonVariant>());
    // Reset our timer
    lastUpdate = millis();
  }
}
