#include <Arduino.h>
#include <nvs_flash.h>

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#define PIN_SCL 22
#define PIN_SDA 21
//-----------------Flags to control state of interfaces for various switching mode-------------
bool app1Flg = true;
bool app2Flg = true;
bool sosFlag = false;
bool tFlag = false; // Flag to indicate Switch Mode command
bool prev_tFlag = false;
bool app1State;
bool app2State;
bool autoMode;
//---------------------------Data Point Storage Variables--------------------------------------
const char *path = "/switchData.txt";
char timeStamp[17];
char msg[23];
int tBytes = 0;
int uBytes = 0;
bool wFlag = false;
unsigned int dCount = 0;
//-----------------------------Variables for Switch Mode---------------------------------------
unsigned int min1 = 0; // Count minutes needed to periodically switch interface 1
unsigned int min2 = 0; // Count minutes needed to periodically switch interface 2
uint8_t rmin = 0;      // Minute variable needed for Timeframe switch mode operation
uint8_t rhr = 0;       // Hour variable needed for Timeframe switch mode operation

bool week[7] = {false, false, false, false, false, false, false};
byte cur_day = 7;
byte curr_zone = 5, prev_zone = 0;
bool zoneChg = false;
unsigned int app1ON, app1OFF, app2ON, app2OFF;
char sch_buff[3];
byte sch;
byte frame_App1[18][4];
byte frame_App2[18][4];
char rtcDay[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
byte App1Sch;
byte App2Sch;
byte b = 0;
byte diff = 0;

void defaultState()
{
  // Configure the Prescaler at 80 the quarter of the ESP32 is cadence at 80Mhz
  // 80000000 / 80 = 1000000 tics / seconde
  timer = timerBegin(0, 80, true); // true - count up; false - count down
  timerAttachInterrupt(timer, &onTime, true);
  timerAlarmWrite(timer, 60 * 1000000, true);
  timerAlarmDisable(timer);

  tFlag = false;
  pref.putBool("prev_tFlag", false);

  smode = 'M';
  initFlg = false;

  app[0].ID[0] = '0';
  app[0].ID[1] = '1';
  app[0].ID[2] = NULL;
  app[1].ID[0] = '0';
  app[1].ID[1] = '2';
  app[1].ID[2] = NULL;

  app[0].prev_state = 0;
  app[0].curr_state = 0;
  app[1].prev_state = 0;
  app[1].curr_state = 0;

  app[0].val[0].mData = 0;
  app[1].val[0].mData = 0;

  for (int k = 0; k < 4; k++)
  {
    app[0].val[k].onMin = 0;
    app[0].val[k].offMin = 0;
    app[1].val[k].onMin = 0;
    app[1].val[k].offMin = 0;
  }

  for (i = 0; i < 4; i++)
  {
    app[0].val[0].tON[i] = 0;
    app[1].val[0].tON[i] = 0;
  }

  for (i = 0; i < 18; i++)
  {
    for (int k = 0; k < 4; k++)
    {
      frame_App1[i][k] = 0;
      frame_App2[i][k] = 0;
    }
  }
  sch = 0;
  App1Sch = 0;
  App2Sch = 0;
  prev_zone = 0;
  autoMode = 0;
  zoneChg = false;

  // Timeframe
  for (i = 0; i < 7; i++)
    week[i] = 0;
  App1Sch = 0;
  App2Sch = 0;
  for (i = 0; i < 18; i++)
  {
    for (int j = 0; j < 4; j++)
      frame_App1[i][j] = 0;
  }

  for (i = 0; i < 18; i++)
  {
    for (int j = 0; j < 4; j++)
      frame_App2[i][j] = 0;
  }
  writeFile("Dy/M/Y,H:M,App1,App2\r\n");
}

void savedData()
{
  // Configure the Prescaler at 80 the quarter of the ESP32 is cadence at 80Mhz
  // 80000000 / 80 = 1000000 tics / seconde
  timer = timerBegin(0, 80, true); // true - count up; false - count down
  timerAttachInterrupt(timer, &onTime, true);
  timerAlarmWrite(timer, 60 * 1000000, true); // 1 minute Timer
  timerAlarmDisable(timer);

  smode = pref.getChar("mode", 0);

  app[0].ID[0] = '0';
  app[0].ID[1] = '1';
  app[0].ID[2] = NULL;
  app[1].ID[0] = '0';
  app[1].ID[1] = '2';
  app[1].ID[2] = NULL;

  app[0].prev_state = pref.getBool("pState1", 0);
  app[0].curr_state = 0;
  app[1].prev_state = pref.getBool("pState2", 0);
  app[1].curr_state = 0;
  // Manual Mode
  app[0].val[0].mData = pref.getBool("man1", 0);
  app[1].val[0].mData = pref.getBool("man2", 0);
  // Period Mode
  app[0].val[0].onMin = pref.getInt("z1A1ON", 0);
  app[0].val[0].offMin = pref.getInt("z1A1OFF", 0);
  app[0].val[1].onMin = pref.getInt("z2A1ON", 0);
  app[0].val[1].offMin = pref.getInt("z2A1OFF", 0);
  app[0].val[2].onMin = pref.getInt("z3A1ON", 0);
  app[0].val[2].offMin = pref.getInt("z3A1OFF", 0);
  app[0].val[3].onMin = pref.getInt("z4A1ON", 0);
  app[0].val[3].offMin = pref.getInt("z4A1OFF", 0);

  app[1].val[0].onMin = pref.getInt("z1A2ON", 0);
  app[1].val[0].offMin = pref.getInt("z1A2OFF", 0);
  app[1].val[1].onMin = pref.getInt("z2A2ON", 0);
  app[1].val[1].offMin = pref.getInt("z2A2OFF", 0);
  app[1].val[2].onMin = pref.getInt("z3A2ON", 0);
  app[1].val[2].offMin = pref.getInt("z3A2OFF", 0);
  app[1].val[3].onMin = pref.getInt("z4A2ON", 0);
  app[1].val[3].offMin = pref.getInt("z4A2OFF", 0);

  min1 = pref.getUInt("m1", 0);
  min2 = pref.getUInt("m2", 0);

  // Timeframe
  pref.getBytes("Week", week, 7);
  App1Sch = pref.getInt("A1Sch", 0);
  App2Sch = pref.getInt("A2Sch", 0);
  pref.getBytes("TFA1", frame_App1, 72);
  pref.getBytes("TFA2", frame_App1, 72);

  autoMode = pref.getBool("auto", 0);
  if (autoMode == 1)
  {
    tFlag = true;
    Serial.println("[AUTO MODE] ON");
    pCharacteristic->setValue("[AUTO MODE] ON");
    pCharacteristic->notify();
    if ((smode == 'P') || (smode == 'T'))
      timerAlarmEnable(timer);
  }
  else
  {
    tFlag = false;
    Serial.println("[AUTO MODE] OFF");
    pCharacteristic->setValue("[AUTO MODE] OFF");
    pCharacteristic->notify();
  }
}

void setup()
{
  Serial.begin(115200);
  // BLE Configuration
  bleConf();
  initFlg = pref.getBool("svFlg", 0);
  if (initFlg)
  {
    Serial.println("Loading saved configuration...");
    savedData();
  }
  else
  {
    Serial.println("Loading default configuration...");
    defaultState();
  }

  pref.end();

  pinMode(AP1_pin, OUTPUT);
  pinMode(AP2_pin, OUTPUT);
  pinMode(SOS_Button, INPUT);
  digitalWrite(AP1_pin, LOW);
  digitalWrite(AP2_pin, LOW);

  while ((!rtc.begin()) && i < 5)
  {
    Serial.println("Couldn't find RTC");
    pCharacteristic->setValue("[Error] RTC not found.");
    pCharacteristic->notify();
    i++;
  }
  rtc.adjust(DateTime(__DATE__, __TIME__));
}

void bleConf()
{
  Serial.println("Starting BLE Switch!");

  BLEDevice::init("BLE_Switch");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00); // functions that help with iPhone connections issue 0x06
  // pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void stateChg()
{
  rtcFunc();
  strcpy(msg, timeStamp);
  strcat(msg, ",");
  if (app[0].curr_state)
    strcat(msg, "1");
  else
    strcat(msg, "0");
  strcat(msg, ",");
  if (app[1].curr_state)
    strcat(msg, "1");
  else
    strcat(msg, "0");
  strcat(msg, "\r\n");
  appendFile(msg);
  /*pref.begin("store",false);
  pref.putBool("pState1", app[0].prev_state);
  //pref.putBool("cState1", app[0].curr_state);
  pref.putBool("pState2", app[1].prev_state);
  //pref.putBool("cState2", app[1].curr_state);
  pref.end();*/
}

void feedWDT()
{
  TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
  TIMERG0.wdt_feed = 1;
  TIMERG0.wdt_wprotect = 0;
}

void periodMode()
{
  if (timerFlg)
  {
    // Comment out enter / exit to deactivate the critical section
    portENTER_CRITICAL(&timerMux);
    timerFlg = false;
    portEXIT_CRITICAL(&timerMux);
    rtcFunc();

    // Get ON and OFF time for respective timezone
    if (zoneChg)
    {
      switch (curr_zone)
      {
      case 1:
        if (app[0].val[0].onMin > 0 || app[0].val[0].offMin > 0 || app[1].val[0].onMin > 0 || app[1].val[0].offMin > 0)
        {
          min1 = 0;
          min2 = 0;
          app1ON = app[0].val[0].onMin;
          app1OFF = app[0].val[0].offMin;
          app2ON = app[1].val[0].onMin;
          app2OFF = app[1].val[0].offMin;
          Serial.println("TimeZone 1 Periodic Mode Set");
          pFlag = true;
        }
        else
          pFlag = false;
        break;
      case 2:
        if (app[0].val[1].onMin > 0 || app[0].val[1].offMin > 0 || app[1].val[1].onMin > 0 || app[1].val[1].offMin > 0)
        {
          min1 = 0;
          min2 = 0;
          app1ON = app[0].val[1].onMin;
          app1OFF = app[0].val[1].offMin;
          app2ON = app[1].val[1].onMin;
          app2OFF = app[1].val[1].offMin;
          pFlag = true;
          Serial.println("TimeZone 2 Periodic Mode Set");
        }
        else
          pFlag = false;
        break;
      case 3:
        if (app[0].val[2].onMin > 0 || app[0].val[2].offMin > 0 || app[1].val[2].onMin > 0 || app[1].val[2].offMin > 0)
        {
          min1 = 0;
          min2 = 0;
          app1ON = app[0].val[2].onMin;
          app1OFF = app[0].val[2].offMin;
          app2ON = app[1].val[2].onMin;
          app2OFF = app[1].val[2].offMin;
          pFlag = true;
          Serial.println("TimeZone 3 Periodic Mode Set");
        }
        else
          pFlag = false;
        break;
      case 4:
        if (app[0].val[3].onMin > 0 || app[0].val[3].offMin > 0 || app[1].val[3].onMin > 0 || app[1].val[3].offMin > 0)
        {
          min1 = 0;
          min2 = 0;
          app1ON = app[0].val[3].onMin;
          app1OFF = app[0].val[3].offMin;
          app2ON = app[1].val[3].onMin;
          app2OFF = app[1].val[3].offMin;
          pFlag = true;
          Serial.println("TimeZone 4 Periodic Mode Set");
        }
        else
          pFlag = false;
        break;
      }
      if (!pFlag)
      {
        Serial.print("No time setting for Timezone: ");
        Serial.println(curr_zone);
        app1ON = 0;
        app1OFF = 0;
        app2ON = 0;
        app2OFF = 0;
      }

      Serial.print("Appliance 1 ON TIME: ");
      Serial.println(app1ON);
      Serial.print("Appliance 1 OFF TIME: ");
      Serial.println(app1OFF);
      Serial.print("Appliance 2 ON TIME: ");
      Serial.println(app2ON);
      Serial.print("Appliance 2 OFF TIME: ");
      Serial.println(app2OFF);
    }
    min1++;
    min2++;

    pref.begin("store", false);
    pref.putUInt("m1", min1);
    pref.putUInt("m2", min2);
    pref.end();

    Serial.print("Period 1: ");
    Serial.println(min1);
    Serial.print("Period 2: ");
    Serial.println(min2);

    if (week[cur_day] && pFlag)
    {
      // Setting for Appliance 1
      if (min1 <= app1ON)
      {
        digitalWrite(AP1_pin, HIGH);
        Serial.println("Interface 1: HIGH");
        app[0].curr_state = 1;
      }
      else if (min1 > app1ON && min1 <= (app1ON + app1OFF))
      {
        digitalWrite(AP1_pin, LOW);
        Serial.println("Interface 1: LOW");
        app[0].curr_state = 0;
      }
      else if (app1ON != 0)
      {
        digitalWrite(AP1_pin, HIGH);
        Serial.println("Interface 1: HIGH");
        app[0].curr_state = 1;
        min1 = 1;
      }

      // Setting for Appliance 2
      if (min2 <= app2ON)
      {
        digitalWrite(AP2_pin, HIGH);
        Serial.println("Interface 2: HIGH");
        app[1].curr_state = 1;
      }
      else if (min2 > app2ON && min2 <= (app2ON + app2OFF))
      {
        digitalWrite(AP2_pin, LOW);
        Serial.println("Interface 2: LOW");
        app[1].curr_state = 0;
      }
      else if (app2OFF != 0)
      {
        digitalWrite(AP2_pin, HIGH);
        Serial.println("Interface 2: HIGH");
        app[1].curr_state = 1;
        min2 = 1;
      }
    }
    else
    {
      digitalWrite(AP1_pin, LOW);
      digitalWrite(AP2_pin, LOW);
      Serial.println("Interface 1: LOW");
      Serial.println("Interface 2: LOW");
    }
  }
}

void timeFrameMode()
{
  if (timerFlg)
  {
    // Comment out enter / exit to deactivate the critical section
    portENTER_CRITICAL(&timerMux);
    timerFlg = false;
    portEXIT_CRITICAL(&timerMux);
    rtcFunc();
    // Check Appliance 1 schedules
    if (week[cur_day] && App1Sch > 0)
    {
      for (int b = 0; b < App1Sch; b++)
      {
        diff = frame_App1[b][2] - frame_App1[b][0];
        if (diff > 0)
        {
          if (rhr == frame_App1[b][0])
          {
            if ((rhr >= frame_App1[b][0] && rmin >= frame_App1[b][1]) && (rhr <= frame_App1[b][0] && rmin <= 59))
            {
              app[0].curr_state = 1;
              break;
            }
            else
              app[0].curr_state = 0;
          }
          else if (rhr == frame_App1[b][2])
          {
            if ((rhr >= frame_App1[b][2] && rmin >= 0) && (rhr <= frame_App1[b][2] && rmin <= frame_App1[b][3]))
            {
              app[0].curr_state = 1;
              break;
            }
            else
              app[0].curr_state = 1;
          }
          else
          {
            for (int l = 1; l < diff; l++)
            {
              if ((rhr >= (frame_App1[b][0] + l) && rmin >= 0) && (rhr <= (frame_App1[b][0] + l) && rmin <= 59))
              {
                app[0].curr_state = 1;
                break;
              }
              else
                app[0].curr_state = 0;
            }
          }
        }
        else
        {
          if ((rhr >= frame_App1[b][0] && rmin >= frame_App1[b][1]) && (rhr <= frame_App1[b][2] && rmin < frame_App1[b][3]))
          {
            app[0].curr_state = 1;
            break;
          }
          else
            app[0].curr_state = 0;
        }
      }
    }
    else
    {
      app[0].curr_state = 0;
    }
    // Check Appliance 2 schedules
    if (week[cur_day] && App2Sch > 0)
    {
      for (int b = 0; b < App2Sch; b++)
      {
        diff = frame_App2[b][2] - frame_App2[b][0];
        if (diff > 0)
        {
          if (rhr == frame_App2[b][0])
          {
            if ((rhr >= frame_App2[b][0] && rmin >= frame_App2[b][1]) && (rhr <= frame_App2[b][0] && rmin <= 59))
            {
              app[1].curr_state = 1;
              break;
            }
            else
              app[1].curr_state = 0;
          }
          else if (rhr == frame_App2[b][2])
          {
            if ((rhr >= frame_App2[b][2] && rmin >= 0) && (rhr <= frame_App2[b][2] && rmin < frame_App2[b][3]))
            {
              app[1].curr_state = 1;
              break;
            }
            else
              app[1].curr_state = 0;
          }
          else
          {
            for (int l = 1; l < diff; l++)
            {
              if ((rhr >= (frame_App2[b][0] + l) && rmin >= 0) && (rhr <= (frame_App2[b][0] + l) && rmin <= 59))
              {
                app[1].curr_state = 1;
                break;
              }
              else
                app[1].curr_state = 0;
            }
          }
        }
        else
        {
          if ((rhr >= frame_App2[b][0] && rmin >= frame_App2[b][1]) && (rhr <= frame_App2[b][2] && rmin < frame_App2[b][3]))
          {
            app[1].curr_state = 1;
            break;
          }
          else
            app[1].curr_state = 0;
        }
      }
    }
    else
    {
      app[1].curr_state = 0;
    }

    if (app[0].curr_state == 1)
    {
      Serial.println("Appliance 1: HIGH");
      digitalWrite(AP1_pin, HIGH);
    }
    else
    {
      Serial.println("Appliance 1: LOW");
      digitalWrite(AP1_pin, LOW);
    }

    if (app[1].curr_state == 1)
    {
      Serial.println("Appliance 2: HIGH");
      digitalWrite(AP2_pin, HIGH);
    }
    else
    {
      Serial.println("Appliance 2: LOW");
      digitalWrite(AP2_pin, LOW);
    }
  }
}

void manualMode()
{
  if (app1Flg)
  {
    if (app[0].val[0].mData == 1)
    {
      Serial.println("Appliance 1: HIGH");
      digitalWrite(AP1_pin, HIGH);
    }
    else
    {
      Serial.println("Appliance 1: LOW");
      digitalWrite(AP1_pin, LOW);
    }
    app[0].curr_state = app[0].val[0].mData;
    app1Flg = false;
  }
  if (app2Flg)
  {
    if (app[1].val[0].mData == 1)
    {
      Serial.println("Appliance 2: HIGH");
      digitalWrite(AP2_pin, HIGH);
    }
    else
    {
      Serial.println("Appliance 2: LOW");
      digitalWrite(AP2_pin, LOW);
    }
    app[1].curr_state = app[1].val[0].mData;
    app2Flg = false;
  }
}

void loop()
{
  bleDevice();
  feedWDT();
  vTaskDelay(5 / portTICK_PERIOD_MS);
  // emergencyCheck();
  if (!tFlag)
    timerAlarmDisable(timer);
  else if (!cmdF && tFlag)
  {
    if (smode == 'M')
      manualMode();
    else if (smode == 'P')
      periodMode();
    else if (smode == 'T')
      timeFrameMode();
  }

  if ((app[0].prev_state != app[0].curr_state) || (app[1].prev_state != app[1].curr_state))
    stateChg();

  pref.begin("store", false);
  pref.putBool("pState1", app[0].curr_state);
  pref.putBool("pState2", app[1].curr_state);
  app[0].prev_state = pref.getBool("pState1", 0);
  app[1].prev_state = pref.getBool("pState2", 0);
  pref.end();

  delay(500);
}

void swApp()
{
  app1Flg = true;
  app2Flg = true;
  min1 = 0;
  min2 = 0;
  switch (smode)
  {
  case 'M':
    Serial.println("------------");
    Serial.println("MANUAL MODE:");
    Serial.println("------------");
    pCharacteristic->setValue("[MODE] Manual");
    pCharacteristic->notify();
    break;
  case 'P':
    Serial.println("------------");
    Serial.println("PERIOD MODE:");
    Serial.println("------------");
    pCharacteristic->setValue("[MODE] Periodic");
    pCharacteristic->notify();

    app[0].curr_state = 0;
    app[1].curr_state = 0;

    // Interface 1
    digitalWrite(AP1_pin, LOW);
    Serial.println("Interface 1: LOW");
    // Interface 2
    digitalWrite(AP2_pin, LOW);
    Serial.println("Interface 2: LOW");
    timerAlarmEnable(timer);
    break;
  case 'T':
    Serial.println("------------");
    Serial.println("TIMEFRAME MODE:");
    Serial.println("------------");
    pCharacteristic->setValue("[MODE] Timeframe");
    pCharacteristic->notify();
    app[0].curr_state = 0;
    app[1].curr_state = 0;
    // Interface 1
    digitalWrite(AP1_pin, LOW);
    // Serial.println("Interface 1: LOW");
    // Interface 2
    digitalWrite(AP2_pin, LOW);
    // Serial.println("Interface 2: LOW");
    timerAlarmEnable(timer);
    break;
  }
}

void rtcFunc()
{
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  cur_day = now.dayOfTheWeek();
  Serial.println(cur_day);
  rhr = now.hour();
  Serial.print(rhr);
  Serial.print(':');
  rmin = now.minute();
  Serial.println(rmin);
  sprintf(timeStamp, "%02d/%02d/%04d,%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute());
  getTimeZone();
}

void getTimeZone()
{
  /*TimeZone 1: 06:00 AM to 11:59 AM  (06:00 - 11:59)
    TimeZone 2: 12:00 PM to 05:59 PM  (12:00 - 17:59)
    TimeZone 3: 06:00 PM to 11:59 PM  (18:00 - 23:59)
    TimeZone 4: 12:00 AM to 05:59 AM  (00:00 - 05:59)
  */
  if (rhr >= 6 && (rhr <= 11 && rmin <= 59))
  {
    curr_zone = 1;
  }
  else if (rhr >= 12 && (rhr <= 17 && rmin <= 59))
  {
    curr_zone = 2;
  }
  else if (rhr >= 18 && (rhr <= 23 && rmin <= 59))
  {
    curr_zone = 3;
  }
  else if (rhr >= 00 && (rhr <= 5 && rmin <= 59))
  {
    curr_zone = 4;
  }
  if (prev_zone != curr_zone)
    zoneChg = true;
  else
    zoneChg = false;
  prev_zone = curr_zone;
}

void bleDevice()
{
  if (deviceConnected)
  {
    if (devCon)
    {
      swInfo();
      pCharacteristic->setValue("[BLE Switch] Ready");
      pCharacteristic->notify();
      devCon = false;
    }
  }
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);
    // BLEDevice::startAdvertising();
    pServer->startAdvertising();
    Serial.println("Start Advertising again...");
    oldDeviceConnected = deviceConnected;
    devCon = true;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}
