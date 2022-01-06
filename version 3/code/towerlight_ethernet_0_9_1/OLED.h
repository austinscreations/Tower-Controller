#ifndef OLED_H
#define OLED_H

//single line limit with font 2 is 10 charactors

void off_screen() // custom text on line one
{
  display.clearDisplay();
  display.display();
}

void one_screen() // custom text on line one
{
  display.setCursor(0,0);
  display.print(screenLineOne);
}

void two_screen() // custom text on line two
{
  display.setCursor(0,16);
  display.print(screenLineTwo);
}

void time_screen() // show time on line one
{
  if (rtc_Found == true) // show the time if time is availble
  {
    display.setCursor(0,0);
    if (clockMode == PCF8523_12) // 12 hour clock requested
    {
      if(current_hour >= 13)
      {
       uint8_t mod_hour = current_hour/12;
       display.print(mod_hour);
      }
      else if (current_hour == 0) {display.print("12");}
      else 
      {
        if(current_hour<10){display.print(" ");}
        display.print(current_hour);
      }
      
      display.print(":");
      if(current_minute<10){display.print("0");}
      display.print(current_minute);

      if(current_hour >= 13){display.print(" PM");}
      else{display.print(" AM");}
    }
  
    if (clockMode == PCF8523_24) // 24 hour clock requested
    {
      if(current_hour<10){display.print('0');}
      display.print(current_hour);
      display.print(":");
      if(current_minute<10){display.print('0');}
      display.print(current_minute);
      display.print("h");
    }
  }
}

void temp_screen() // line two showing temp
{
  
  if (hum_SensorFound == true)
  {
    display.setCursor(0,16); // line 2
    if (tempMode == TEMP_C){display.print(c,1);display.print("\tC");}
    if (tempMode == TEMP_F){display.print(f,1); display.print("\tF");}
  }
  if (temp_SensorFound == true && hum_SensorFound == false)
  {
    display.setCursor(0,16); // line 2
    if (tempMode == TEMP_C){display.print(c,1);display.print("\tC");}
    if (tempMode == TEMP_F){display.print(f,1); display.print("\tF");}
  }
}

void hum_screen()
{
  if (hum_SensorFound == true)
  {
    display.setCursor(0,0); // line 1
    display.print(h,1);display.print("% rH"); 
  }
  
}

void lux_screen() // line one showing lux
{
  if (lux_SensorFound == true)
  {
   display.setCursor(0,0); // line 1
   display.print(l,1); display.print(" LUX");
  }
}

void IP_screen() // line one showing IP address
{
  if (Ethernet.linkStatus() == LinkON)
  {
   display.setTextSize(1);
   display.setCursor(0,0); // line 1
   display.print(Ethernet.localIP());
  }
}

void MAC_screen() // line two showing mac address
{
  display.setTextSize(1);
  byte mac[6];
  WiFi.macAddress(mac);
  char mac_display[18];
  sprintf_P(mac_display, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  display.setCursor(0,16); // line 2
  display.print(mac_display);
}







void OLED_loop() // control the OLED screen if availble
{
  if (oled_Found == true)
  {
    if ((millis() - previous_OLED_temp) > OLED_INTERVAL_TEMP)
    { 
      if (hum_SensorFound == true)
      {
        sensors_event_t humidity, temp;
        sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

        c = temp.temperature;
        f = (temp.temperature*1.8)+32;
        h = humidity.relative_humidity;
      }
      if (temp_SensorFound == true && hum_SensorFound == false)
      {
        c = tempSensor.readTempC();
        f = tempSensor.readTempF();
      }
      // Reset our timer
      previous_OLED_temp = millis();
    }

    if ((millis() - previous_OLED_lux) > OLED_INTERVAL_LUX)
    {
      if (lux_SensorFound == true && luxSensor.hasValue() == true)
      {
        l = luxSensor.getLux();
        luxSensor.start(); // start the next reading
        previous_OLED_lux = millis();
      }
    }
    

    if (rtc_Found == true)
    {
      // Time Keeping
      DateTime now = rtc.now();
      current_hour = now.hour();
      current_minute = now.minute();
    }
    
    if (sleepState == true && sleepModes[6] == true) // screen goes to sleep
    {
      off_screen();
    }
    else
    {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        
        if (screenMode == OLED_MODE_OFF){off_screen();}
        if (screenMode == OLED_MODE_ONE){IP_screen();MAC_screen();}
        if (screenMode == OLED_MODE_TWO){time_screen();temp_screen();}
        if (screenMode == OLED_MODE_THREE){lux_screen();temp_screen();}
        if (screenMode == OLED_MODE_FOUR){hum_screen();temp_screen();}
        if (screenMode == OLED_MODE_FIVE){one_screen();two_screen();}

        
        display.display();
    }
  }
}






#endif
