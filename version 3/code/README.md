## New V1.0.1 code

Compatible with the latest work from OXRS and the HTML adminUI

this one peice of code can either work with wifi or ethernet. Wifi requires config.h to be edited with your credentials
```c++
/*----------------------- Connection Type --------------------------------*/
// select connection mode here - comment / uncomment the one needed
#define ethMode    // tower uses ethernet
//#define wifiMode   // tower uses wifi
```
top of the code has two defines, uncomment the one relevent for your operation to activate it. (only one should be used at a time)

an extra function has been added: blink - unlike flash this can run self contained. the rate of flashing can be set in the config (in milliseconds) default is 1000

## OXRS
the tower controller is compatible with the OXRS system: https://oxrs.io/

and works with the ongoing development of the HTML AdminUI https://github.com/OXRS-IO/OXRS-IO-AdminUI-WEB-APP

## Required Libraries
OXRS_API https://github.com/OXRS-IO/OXRS-IO-API-ESP32-LIB

OXRS_MQTT https://github.com/OXRS-IO/OXRS-IO-MQTT-ESP32-LIB

OXRS_Sensors https://github.com/austinscreations/OXRS-AC-I2CSensors-ESP-LIB

the sensor library also uses quite a few libraries that can all be installed from the arduino IDE library.
```c++
#include <Adafruit_MCP9808.h>         // For temp sensor
#include <Adafruit_SSD1306.h>         // for OLED display
#include <RTClib.h>                   // for PCF8523 RTC
#include <hp_BH1750.h>                // for bh1750 lux sensor
#include <Adafruit_SHT4x.h>           // for SHT40 Temp / humidity sensor
```
