/**
  ESP8266 Tower Controller firmware for the Open eXtensible Rack System
  
  See ********** for documentation.
  Compile options:
    ESP8266
  External dependencies. Install using the Arduino library manager:
    "PubSubClient" by Nick O'Leary
    "OXRS-IO-MQTT-ESP32-LIB" by OXRS Core Team
    "OXRS-IO-API-ESP32-LIB" by OXRS Core Team
    "OXRS-IO-SENSORS-ESP-LIB" by Austin's Creations
    "Adafruit_NeoPixel.h" by Adafruit
    
    
  GitHub repository:
    ***********
    
  Bugs/Features:
    See GitHub issues list
    
  Copyright 2021 Austins Creations
*/

/*----------------------- Connection Type --------------------------------*/
// select connection mode here - comment / uncomment the one needed
#define ethMode    // tower uses ethernet
//#define wifiMode   // tower uses wifi

/*--------------------------- Version ------------------------------------*/
#define FW_NAME       "OXRS-AC-TowerController-ESP8266-FW"
#define FW_SHORT_NAME "Tower Controller"
#define FW_MAKER      "Austin's Creations"
#define FW_VERSION    "1.0.2"

/*--------------------------- Configuration ------------------------------*/
// Should be no user configuration in this file, everything should be in;
#include "config.h"

/*--------------------------- Libraries ----------------------------------*/
#if defined(ethMode)
  #include <SPI.h>                      // for ethernet
  #include <Ethernet.h>               // for ethernet
#endif
#include <Wire.h>                     // For I2C
#include <ESP8266WiFi.h>              // For networking
#include <PubSubClient.h>             // For MQTT
#include <OXRS_MQTT.h>                // For MQTT
#include <OXRS_API.h>                 // For REST API
#include <Adafruit_NeoPixel.h>        // for the LED drivers
#include <OXRS_SENSORS.h>             // for Qwiic i2c sensors


/*--------------------------- Constants ----------------------------------*/
// Serial
#define       SERIAL_BAUD_RATE          115200

// MQTT
#define       MQTT_MAX_MESSAGE_SIZE     4096

// REST API
#define       REST_API_PORT             80

// Supported LED modes
#define       LED_MODE_NONE             0
#define       LED_MODE_COLOUR           1
#define       LED_MODE_FLASH            3
#define       LED_MODE_BLINK            4

// Supported Tower modes
#define       TOWER_MODE_SINGLE         0
#define       TOWER_MODE_MULTI          1

// For the 3ch WS2811 LED driver IC
#define       LED_PIN                   0
#define       LED_COUNT                 2

#define       DEFAULT_BLINK_RATE        1000

/*-------------------------- Internal datatypes --------------------------*/

/*--------------------------- Global Variables ---------------------------*/
// Flashing state for any strips in flash mode
uint8_t g_flash_state = false;

// Blink flash control - internal flashing control
uint8_t g_blink_state = false;
uint32_t blinkMs = DEFAULT_BLINK_RATE;
unsigned long lastBlink;

// LED varibles
uint8_t towermode = TOWER_MODE_SINGLE;   // set default tower mode to single - can be changed via mqtt config
uint8_t ledmode[6] = {0,0,0,0,0,0};        // mode for each led channel
uint8_t ledbrightness[6] = {0,0,0,0,0,0};  // brightness for led channel

// tower varible
uint8_t output[6] = {0,0,0,0,0,0};       // output value for tower 
uint8_t sleepModes[6] = {0,1,1,1,1,0};   // if a light is on / off during sleep mode
uint8_t sleepState = false;              // is the device supposed to be asleep

/*--------------------------- Instantiate Global Objects -----------------*/
// client && server
#if defined(ethMode)
  EthernetClient client;
  EthernetServer server(80);
#elif defined(wifiMode)
  WiFiClient client;
  WiFiServer server(REST_API_PORT);
#endif

// MQTT client
PubSubClient mqttClient(client);
OXRS_MQTT mqtt(mqttClient);

OXRS_API api(mqtt);

// LED Driver IC
Adafruit_NeoPixel driver(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

OXRS_SENSORS sensor(mqtt);

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
  #if defined(ethMode)
    Ethernet.init(15);
  #endif
  
  // Startup logging to serial
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();
  Serial.println(F("==============================="));
  Serial.println(FW_NAME);
  Serial.print  (F("            v"));
  Serial.println(FW_VERSION);
  Serial.println(F("==============================="));

  // Set up LED Driver
  driver.begin();        // INITIALIZE NeoPixel strip object (REQUIRED)
  driver_off();          // Turn all drivers off
  driver_off();          // Turn all drivers off

  Wire.begin();
  sensor.begin();      // standard i2c GPIO

  byte mac[6];

  sensor.oled(WiFi.macAddress(mac)); // start screen - starts with MAC address showing

  #if defined(ethMode)
    initialiseEthernet(mac);
    sensor.oled(Ethernet.localIP()); // update screen - should show IP address
  #elif defined(wifiMode)
    initialiseWifi(mac);
    sensor.oled(WiFi.localIP()); // update screen - should show IP address
  #endif

  initialiseMqtt(mac);   // Set up MQTT (don't attempt to connect yet)

  initialiseRestApi();   // Set up the REST API

}

/**
  Main processing loop
*/
void loop() {
  #if defined(ethMode)
   ethernet_loop();
  #endif

    mqtt.loop();
  
  #if defined(ethMode)
    EthernetClient client = server.available();
    api.checkEthernet(&client);
  #elif defined(wifiMode)
    WiFiClient client = server.available();
    api.checkWifi(&client);
  #endif

  driver_loop();  // update driver channels
  sensor.oled();  // update OLED
  sensor.tele();  // update mqtt sensors

  if ((millis() - lastBlink) > blinkMs)
  { 
    if (g_blink_state == LOW)
    {
      g_blink_state = HIGH;
    }
    else
    {
      g_blink_state = LOW;
    }
    lastBlink = millis();
    driver_loop();  // update driver channels
  }
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
    else if (ledmode[chan] == LED_MODE_BLINK) // blink specified channel
    {
      if (g_blink_state == HIGH)
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
//  Serial.println("driver called");
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

  #if defined(ethMode)
    network["ip"] = Ethernet.localIP();
  #elif defined(wifiMode)
    network["ip"] = WiFi.localIP();
  #endif
  network["mac"] = mac_display;
}

void getConfigSchemaJson(JsonVariant json)
{
  JsonObject configSchema = json.createNestedObject("configSchema");
  
  // Config schema metadata
  configSchema["$schema"] = "http://json-schema.org/draft-07/schema#";
  configSchema["title"] = FW_NAME;
  configSchema["type"] = "object";

  JsonObject properties = configSchema.createNestedObject("properties");

  sensor.setConfigSchema(properties);

  JsonObject towerMode = properties.createNestedObject("towerMode");
  towerMode["type"] = "string";
  JsonArray towerEnum = towerMode.createNestedArray("enum");
  towerEnum.add("single");
  towerEnum.add("multi");

  JsonObject blinkMillis = properties.createNestedObject("blinkMs");
  blinkMillis["type"] = "integer";
  blinkMillis["minimum"] = 0;
  blinkMillis["maximum"] = 10000;
  blinkMillis["description"] = "Rate to blink a light in milliseconds";

  JsonObject towerSleep = properties.createNestedObject("towerSleep");
  towerSleep["type"] = "array";

  JsonObject towerItems = towerSleep.createNestedObject("items");
  towerItems["type"] = "object";

  JsonObject towerSleepProperties = towerItems.createNestedObject("properties");
  
  JsonObject sleepModes = towerSleepProperties.createNestedObject("sleepModes");
  sleepModes["type"] = "array";
  sleepModes["minItems"] = 6;
  sleepModes["maxItems"] = 6;
  JsonObject sleepItems = sleepModes.createNestedObject("items");
  sleepItems["type"] = "number";
  sleepItems["minimum"] = 0;
  sleepItems["maximum"] = 1;

}

void getCommandSchemaJson(JsonVariant json)
{
  JsonObject commandSchema = json.createNestedObject("commandSchema");
  
  // Command schema metadata
  commandSchema["$schema"] = "http://json-schema.org/draft-07/schema#";
  commandSchema["title"] = FW_NAME;
  commandSchema["type"] = "object";

  JsonObject properties = commandSchema.createNestedObject("properties");

  sensor.setCommandSchema(properties);

  JsonObject tower = properties.createNestedObject("tower");
  tower["type"] = "array";

  JsonObject towerItems = tower.createNestedObject("items");
  towerItems["type"] = "object";

  JsonObject towerProperties = towerItems.createNestedObject("properties");

  JsonObject channeling = towerProperties.createNestedObject("channel");
  channeling["type"] = "integer";
  channeling["minimum"] = 1;
  channeling["maximum"] = 5;

  JsonObject modes = towerProperties.createNestedObject("mode");
  modes["type"] = "string";
  JsonArray modesEnum = modes.createNestedArray("enum");
  modesEnum.add("colour");
  modesEnum.add("flash");
  modesEnum.add("blink");

  JsonObject brightnesss = towerProperties.createNestedObject("brightness");
  brightnesss["type"] = "integer";
  brightnesss["maximum"] = 255;

  JsonArray required = towerItems.createNestedArray("required");
  required.add("channel");
  required.add("mode");
  required.add("brightness");

  JsonObject sleeping = properties.createNestedObject("sleep");
  sleeping["type"] = "boolean";
  
  JsonObject flashing = properties.createNestedObject("flash");
  flashing["type"] = "boolean";

  JsonObject restarting = properties.createNestedObject("restart");
  restarting["type"] = "boolean";
}


/**
  API callbacks
*/
void apiAdopt(JsonVariant json)
{
  // Build device adoption info
  getFirmwareJson(json);
  getNetworkJson(json);
  getConfigSchemaJson(json);
  getCommandSchemaJson(json);
}

/**
  MQTT callbacks
*/
void mqttConnected() 
{
  // Build device adoption info
  // Build device adoption info
  DynamicJsonDocument json(JSON_ADOPT_MAX_SIZE);
  mqtt.publishAdopt(api.getAdopt(json.as<JsonVariant>()));

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
  sensor.conf(json); // check if we have new config
  
  if (json.containsKey("towerMode")) // for what mode the tower lights are in
  {
    if (strcmp(json["towerMode"], "single") == 0)
    {
      towermode = TOWER_MODE_SINGLE;
      for (uint8_t x = 0; x < 6; x++){ledbrightness[x] = 0;} // reset tower outputs
    }
    else if (strcmp(json["towerMode"], "multi") == 0)
    {
      towermode = TOWER_MODE_MULTI;
      for (uint8_t x = 0; x < 6; x++){ledbrightness[x] = 0;} // reset tower outputs
    }
    else 
    {
      Serial.println(F("[TLC] invalid Tower Mode Config"));
    }
  }

  if (json.containsKey("blinkMs")) // for updating blink speed
  {
    blinkMs = json["blinkMs"].as<uint32_t>();
  }

  if (json.containsKey("towerSleep"))
  {
    for (JsonVariant towerSleep : json["towerSleep"].as<JsonArray>())
    {
      if (json.containsKey("sleepModes"))
      {
        JsonArray array = json["sleepModes"].as<JsonArray>();
        uint8_t sleepnum = 0;
    
        for (JsonVariant v : array)
        {
          sleepModes[sleepnum++] = v.as<uint8_t>();
        }
      }
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

}

void jsonCommand(JsonVariant json) // do something payloads
{  
  sensor.cmnd(json); // check if we have new command
  
  if (json.containsKey("tower"))
  {
    for (JsonVariant tower : json["tower"].as<JsonArray>())
    {
      jsonTowerCommand(tower);
    }
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

void jsonTowerCommand(JsonVariant json) //sets led channel mode and brightness during normal operation
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
    else if (strcmp(json["mode"], "blink") == 0)
    {
      ledmode[lightchan] = LED_MODE_BLINK;
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
void initialiseWifi(byte * mac)
{
  // Determine MAC address
  WiFi.macAddress(mac);

  // Display MAC address on serial
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(F("[TLC] mac address: "));
  Serial.println(mac_display);  

  // Connect to WiFi
  Serial.print(F("[TLC] connecting to "));
  Serial.print(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  Serial.print(F("[TLC] ip address: "));
  Serial.println(WiFi.localIP());
}

/*
 ETHERNET
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


/*
 REST API
 */
void initialiseRestApi(void)
{
  // NOTE: this must be called *after* initialising MQTT since that sets
  //       the default client id, which has lower precendence than MQTT
  //       settings stored in file and loaded by the API

  // Set up the REST API
  api.begin();

  // Register our callbacks
  api.onAdopt(apiAdopt);

  server.begin();
}


// LED Driver extra
void driver_off() {
    driver.setPixelColor(0,0,0,0);         //  Set pixel's color (in RAM)
    driver.setPixelColor(1,0,0,0);         //  Set pixel's color (in RAM)
    driver.show();                         //  Update drivers to match
}
