#include "BLE_Device.h"

/* ------------------------ Adding Required Libraries ----------------------- */
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Preferences.h>
#include "Timer.h"

// BLE Device Configuration
#define SERVICE_UUID "208fc8fc-64ed-4423-ba22-2230821ae406"
#define CHARACTERISTIC_UUID "e462c4e9-3704-4af8-9a20-446fa2eef1d0"
Preferences pref;

//--------------------Data Structure for Appliance Interface---------------------------------
typedef struct
{
    bool mData;          // Stores Manual Mode Value (0/1)
    unsigned int onMin;  // Stores ON time (in minutes) for Periodic Mode
    unsigned int offMin; // Stores OFF time (in minutes) for Periodic Mode
    uint8_t tON[4];      // Stores FROM-TO (in Hr:Min) time interval for Timeframe Mode
} Data;

typedef struct
{
    char ID[3];      // Stores Interface ID
    bool curr_state; // Stores Current state of interface
    bool prev_state; // Stores previous state of Interface
    Data val[4];
} Interface;
uint8_t nInterface = 2; // Number of available interfaces
Interface app[2];
char smode;      // Stores Interface Mode [Manual (M), Periodic (P), Timeframe (T)]
char delm = ','; // CSV format using comma as delimeter

//----------------------- BLE Communication protocol data structure----------------------------
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool devCon = true;
bool pFlag = false;

//---------------------------Commands from User through Mobile App-----------------------------
char cmdKey = 'X';
bool cmdF = false;   // Flag to indicate command received from mobile app
char App1onBuff[4];  // Buffer to store ON time for periodic/timeframe mode
char App1offBuff[4]; // Buffer to store OFF time for periodic/timeframe mode
char App2onBuff[4];
char App2offBuff[4];
char cmdBuff[4];
String sInfo = "";    // Stores device info such as no. of interfaces, its mode and state.
bool initFlg = false; // Flag variable for device on-start setup configuration
byte i = 0;

//---------------------- Callback function related to device connection------------------------
class MyServCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    }
    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};
//-------------------Callback function for read/write charactersitic value---------------------
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        /*
         * Check for commands received from Mobile App
         *  Info command             : I swInfo()
         *  Switch command           : S swApp()
         *    Periodic               : P
         *    Timeframe              : T
         *    Manual                 : M             :
         *  Factory Reset command    : F
         *  Restart command          : R
         *  Data Sync command        : D
         *  Switch Resume command    : B
         *  Pause command            : P
         *  Auto mode                : A
         *  RTC Adjust command       : C
         */
        if (value.length() > 0)
        {
            pref.begin("store", false);
            cmdF = true;
            if ((smode == 'P') || (smode == 'T'))
                timerAlarmDisable(timer);
            char cmd[(value.length()) + 1];
            uint8_t cdelm = 0;
            i = 0;
            while (i <= value.length())
            {
                cmd[i] = value[i];
                i++;
            }
            cmd[i] = '\0';
            Serial.println("-----------");
            i = 0;
            //------------------------------------------------------//
            // Parsing CSV command data to extract useful information
            //------------------------------------------------------//
            if (cmd[i] == 'F' && cmd[i + 1] == '\0')
            { //[Factory Reset Command]
                cmdKey = 'F';
                Serial.println("Factory Reset Command"); // Command Format: {'F','\0'}
                nvs_flash_erase();                       // Erase Non-Voltage Storage (NVS) which stores state variables
                nvs_flash_init();
                deleteFile(); // Delete data point file stored in SPIFFS FileSystem
                ESP.restart();
            }
            else if (cmd[i] == 'I' && cmd[i + 1] == '\0') //[Switch Info Command]
            {                                             // Command Format: {'I', '\0'}
                cmdKey = 'I';
                Serial.println("Info Command");
                swInfo();
            }
            else if (cmd[i] == 'D' && cmd[i + 1] == '\0') //[Data Sync Command]
            {
                cmdKey = 'D';
                Serial.println("Data Sync Command");
                readFile();
            }
            else if (cmd[i] == 'T' && cmd[i + 1] == '\0')
            { //[Time Command]
                cmdKey = 'T';
                Serial.println("RTC Command"); // Command Format: {'T','\0'}
                rtcFunc();
                pCharacteristic->setValue(timeStamp);
                pCharacteristic->notify();
            }
            else if (cmd[i] == 'R' && cmd[i + 1] == '\0')
            { //[Restart Command]
                cmdKey = 'R';
                Serial.println("Restart Command"); // Command Format: {'R','\0'}
                ESP.restart();
            }
            else if (cmd[i] == 'A' && cmd[i + 3] == '\0')
            {
                Serial.println("Auto Mode Command");
                if (cmd[i + 1] == delm)
                {
                    if (cmd[i + 2] == '1')
                    {
                        autoMode = 1;
                        pref.putBool("auto", autoMode);
                        pCharacteristic->setValue("[AUTO MODE] ON");
                        pCharacteristic->notify();
                    }
                    else if (cmd[i + 2] == '0')
                    {
                        autoMode = 0;
                        pref.putBool("auto", autoMode);
                        pCharacteristic->setValue("[AUTO MODE] OFF");
                        pCharacteristic->notify();
                    }
                    else
                    {
                        Serial.println("Invalid Auto Mode");
                        pCharacteristic->setValue("[Error]Invalid Auto Mode");
                        pCharacteristic->notify();
                    }
                }
                else
                {
                    Serial.println("Invalid Command");
                    pCharacteristic->setValue("[Error]Invalid Command");
                    pCharacteristic->notify();
                }
            }
            else if (cmd[i] == 'B' && cmd[i + 1] == '\0') //[Switch Resume Command]
            {                                             // Command Format: {'B', '\0'}
                cmdKey = 'B';
                Serial.println("Resume Command");
                tFlag = true;
                if (smode == 'M')
                {
                    pCharacteristic->setValue("[Resume] M ");
                    pCharacteristic->notify();
                }
                else if (smode == 'P')
                {
                    char sendStr[22];
                    sprintf(sendStr, "%s:%d,%d", "[Resume] P", min1, min2);
                    Serial.println(min1);
                    Serial.println(min2);
                    pCharacteristic->setValue(sendStr);
                    pCharacteristic->notify();
                    timerAlarmEnable(timer);
                }
                else if (smode == 'T')
                {
                    pCharacteristic->setValue("[Resume] T");
                    pCharacteristic->notify();
                    timerAlarmEnable(timer);
                }
                if (app[0].curr_state)
                    digitalWrite(AP1_pin, HIGH);
                else
                    digitalWrite(AP1_pin, LOW);

                if (app[1].curr_state)
                    digitalWrite(AP2_pin, HIGH);
                else
                    digitalWrite(AP2_pin, LOW);
            }
            else if (cmd[i] == 'P' && cmd[i + 1] == '\0') //[Switch Pause Command]
            {                                             // Command Format: {'P', '\0'}
                cmdKey = 'P';
                Serial.println("Pause Command");
                tFlag = false;
                if (smode == 'M')
                {
                    pCharacteristic->setValue("[Paused] M ");
                    pCharacteristic->notify();
                }
                else if (smode == 'P')
                {
                    pCharacteristic->setValue("[Paused] P");
                    pCharacteristic->notify();
                }
                else if (smode == 'T')
                {
                    pCharacteristic->setValue("[Paused] T");
                    pCharacteristic->notify();
                }
            }
            else if (cmd[i] == 'S' && cmd[i + 1] != '\0') //[Switch Mode Command]
            {
                cmdKey = 'S';
                Serial.println("Switch Command");
                /*
                 * Format:{'S','Mode', 'Value', '\0'}
                 * If Mode = 'M'  (Manual),     then Value = {'0/1')
                 * If Mode = 'P'  (Periodic),   then Value = {'Day, Zone, ONTIME, OFFTIME' (in MINUTES)}
                 * If Mode = 'T'  (Timeframe),  then Value = {'Day, Zone, ONTIME' (in HR:MIN)}
                 */
                int j = 0;
                i++;
                while (cmd[i] != NULL)
                {
                    if (cmd[i] == delm)
                    {
                        cdelm++;
                        j = 0;
                        i++;
                        continue;
                    }
                    if (cdelm == 1)
                    {                                 // Mode
                        pref.putBool("svFlg", 1);     // svFlag indicates stored data for setup configuration
                        pref.putChar("mode", cmd[i]); // Mode variable is saved in NVS flash area
                        // smode = pref.getChar("mode", 0);
                        smode = cmd[i];
                        i++;
                    }
                    else if (cdelm == 2)
                    { //---------------Value------------------
                        if (smode == 'M')
                        {                                                //[Manual]
                            pref.putBool("man1", (cmd[i] - 48));         // Appliance 1 manual value stored in NVS
                            pref.putBool("man2", (cmd[i = i + 2] - 48)); // Appliance 2 manual value stored in NVS
                            app[0].val[0].mData = pref.getBool("man1", 0);
                            app[1].val[0].mData = pref.getBool("man2", 0);
                            Serial.println(app[0].val[0].mData);
                            Serial.println(app[1].val[0].mData);
                            i++;
                        }
                        else if (smode == 'P')
                        { //[Periodic]
                            // S,P,SMTWTFS,[Zone1(ONOFFONOFF)],[Zone2(ONOFFONOFF)],[Zone3(ONOFFONOFF)],[Zone4(ONOFFONOFF)]
                            int k = 0;

                            // Get Days
                            while (cmd[i] != delm)
                            {
                                week[k] = cmd[i++] - 48;
                                k++;
                            }
                            k = 0;

                            // Save week data to memory
                            pref.putBytes("Week", week, 7);
                            i++;
                            Serial.print("Week: ");

                            for (int l = 0; l < 7; l++)
                            {
                                Serial.print(week[l]);
                                Serial.print(" ");
                            }
                            Serial.println();

                            // Setting for TimeZone
                            for (int m = 0; m < 4; m++)
                            {
                                // Appliance 1 ON Time
                                while (cmd[i] != delm)
                                    App1onBuff[k++] = cmd[i++];
                                App1onBuff[k] = NULL;
                                i++;
                                k = 0;

                                // Appliance 1 OFF Time
                                while (cmd[i] != delm)
                                    App1offBuff[k++] = cmd[i++];
                                App1offBuff[k] = NULL;
                                i++;
                                k = 0;

                                // Appliance 2 ON Time
                                while (cmd[i] != delm)
                                    App2onBuff[k++] = cmd[i++];
                                App2onBuff[k] = NULL;
                                i++;
                                k = 0;

                                // Appliance 2 OFF Time
                                while (cmd[i] != delm && cmd[i] != NULL)
                                    App2offBuff[k++] = cmd[i++];
                                App2offBuff[k] = NULL;
                                i++;
                                k = 0;

                                app[0].val[m].onMin = atoi(App1onBuff);
                                app[0].val[m].offMin = atoi(App1offBuff);
                                app[1].val[m].onMin = atoi(App2onBuff);
                                app[1].val[m].offMin = atoi(App2offBuff);
                                Serial.print("Timezone: ");
                                Serial.println(m);
                                Serial.println("--------------------");
                                Serial.print("Appliance 1 ON Time: ");
                                Serial.println(app[0].val[m].onMin);
                                Serial.print("Appliance 1 OFF Time: ");
                                Serial.println(app[0].val[m].offMin);
                                Serial.print("Appliance 2 ON Time: ");
                                Serial.println(app[1].val[m].onMin);
                                Serial.print("Appliance 2 OFF Time: ");
                                Serial.println(app[1].val[m].offMin);
                                Serial.println("--------------------");
                            }
                            prev_zone = 0;
                            // Appliance 1 Zone 1
                            pref.putInt("z1A1ON", app[0].val[0].onMin);
                            pref.putInt("z1A1OFF", app[0].val[0].offMin);
                            // Appliance 1 Zone 2
                            pref.putInt("z2A1ON", app[0].val[1].onMin);
                            pref.putInt("z2A1OFF", app[0].val[1].offMin);
                            // Appliance 1 Zone 3
                            pref.putInt("z3A1ON", app[0].val[2].onMin);
                            pref.putInt("z3A1OFF", app[0].val[2].offMin);
                            // Appliance 1 Zone 4
                            pref.putInt("z4A1ON", app[0].val[3].onMin);
                            pref.putInt("z4A1OFF", app[0].val[3].offMin);

                            // Appliance 2 Zone 1
                            pref.putInt("z1A2ON", app[1].val[0].onMin);
                            pref.putInt("z1A2OFF", app[1].val[0].offMin);
                            // Appliance 2 Zone 2
                            pref.putInt("z2A2ON", app[1].val[1].onMin);
                            pref.putInt("z2A2OFF", app[1].val[1].offMin);
                            // Appliance 2 Zone 3
                            pref.putInt("z3A2ON", app[1].val[2].onMin);
                            pref.putInt("z3A2OFF", app[1].val[2].offMin);
                            // Appliance 2 Zone 4
                            pref.putInt("z4A2ON", app[1].val[3].onMin);
                            pref.putInt("z4A2OFF", app[1].val[3].offMin);
                        }
                        else if (smode == 'T')
                        { //[Timeframe]
                            // S,T,[SMTWTFS],[App1Schedule],{[FROM,TO] App1Schedule times},[App2Schedule],{[FROM,TO] App2Schedule times}
                            int k = 0, l = 0, m = 0;
                            // Get Days
                            while (cmd[i] != delm)
                                week[k++] = cmd[i++] - 48;

                            // Save week data to memory
                            pref.putBytes("Week", week, 7);
                            Serial.print("Week: ");
                            for (l = 0; l < 7; l++)
                            {
                                Serial.print(week[l]);
                                Serial.print(" ");
                            }
                            Serial.println();
                            l = 0;
                            i++;

                            // Get Number of schedule
                            while (cmd[i] != delm)
                                sch_buff[l++] = cmd[i++];
                            sch_buff[l] = NULL;
                            App1Sch = atoi(sch_buff);
                            Serial.print("[Appliance 1] Schedules: ");
                            Serial.println(App1Sch);
                            pref.putInt("A1Sch", App1Sch);
                            pref.putInt("A1Sch", App1Sch);
                            i++;

                            // Get Appliance 1 time data
                            for (m = 0; m < App1Sch; m++)
                            {
                                k = 0;
                                l = 0;
                                // Appliance 1
                                while (k < 4) // FROM
                                    cmdBuff[k++] = cmd[i++] - 48;
                                frame_App1[m][l++] = cmdBuff[0] * 10 + cmdBuff[1];
                                frame_App1[m][l++] = cmdBuff[2] * 10 + cmdBuff[3];

                                k = 0;
                                while (k < 4) // TO
                                    cmdBuff[k++] = cmd[i++] - 48;
                                frame_App1[m][l++] = cmdBuff[0] * 10 + cmdBuff[1];
                                frame_App1[m][l++] = cmdBuff[2] * 10 + cmdBuff[3];
                                i++;
                            }
                            l = 0;

                            // Get Number of schedule
                            while (cmd[i] != delm)
                                sch_buff[l++] = cmd[i++];
                            sch_buff[l] = NULL;
                            App2Sch = atoi(sch_buff);
                            Serial.print("[Appliance 2] Schedules: ");
                            Serial.println(App2Sch);
                            pref.putInt("A2Sch", App2Sch);
                            i++;

                            // Get Appliance 2 time data
                            for (m = 0; m < App2Sch; m++)
                            {
                                k = 0;
                                l = 0;
                                while (k < 4) // FROM
                                    cmdBuff[k++] = cmd[i++] - 48;
                                frame_App2[m][l++] = cmdBuff[0] * 10 + cmdBuff[1];
                                frame_App2[m][l++] = cmdBuff[2] * 10 + cmdBuff[3];
                                k = 0;
                                while (k < 4) // TO
                                    cmdBuff[k++] = cmd[i++] - 48;
                                frame_App2[m][l++] = cmdBuff[0] * 10 + cmdBuff[1];
                                frame_App2[m][l++] = cmdBuff[2] * 10 + cmdBuff[3];
                                i++;
                            }
                            pref.putBytes("TFA1", frame_App1, 72);
                            // Appliance 1 TimeFrame Data
                            for (m = 0; m < App1Sch; m++)
                            {
                                Serial.print("Appliance 1 Schedule ");
                                Serial.println(m + 1);
                                Serial.print(frame_App1[m][0]);
                                Serial.print(":");
                                Serial.print(frame_App1[m][1]);
                                Serial.print(" - ");
                                Serial.print(frame_App1[m][2]);
                                Serial.print(":");
                                Serial.println(frame_App1[m][3]);
                            }
                            pref.putBytes("TFA2", frame_App2, 72);
                            // Appliance 2 TimeFrame Data
                            for (m = 0; m < App2Sch; m++)
                            {
                                Serial.print("Appliance 2 Schedule ");
                                Serial.println(m + 1);
                                Serial.print(frame_App2[m][0]);
                                Serial.print(":");
                                Serial.print(frame_App2[m][1]);
                                Serial.print(" - ");
                                Serial.print(frame_App2[m][2]);
                                Serial.print(":");
                                Serial.println(frame_App2[m][3]);
                            }
                            b = 0;
                        }
                        else
                        {
                            Serial.println("Invalid mode");
                            pCharacteristic->setValue("[Error] Invalid Mode");
                            pCharacteristic->notify();
                        }
                    }
                    else
                    {
                        Serial.println("Invalid command");
                        pCharacteristic->setValue("[Error] Invalid Command");
                        pCharacteristic->notify();
                    }
                }
                tFlag = true;
                pref.putBool("prev_tFlag", tFlag);
                swApp();
            }
            else if (cmd[i] == 'C') //[RTC Time Adjust Command]
            {                       // Command Format: {'C', '\0'}
                cmdKey = 'C';
                Serial.println("RTC Time Adjust Command");
                i++;
                unsigned int yrr, mnth, dy, hrr, mnn, n;
                char buf[5];
                if (cmd[i] == delm)
                {
                    i++;
                    for (n = 0; n < 4; n++) // Year
                        buf[n] = cmd[i++];
                    buf[n] = NULL;
                    yrr = atoi(buf);
                    i++;
                    for (n = 0; n < 2; n++) // Month
                        buf[n] = cmd[i++];
                    buf[n] = NULL;
                    mnth = atoi(buf);
                    i++;
                    for (n = 0; n < 2; n++) // Day
                        buf[n] = cmd[i++];
                    buf[n] = NULL;
                    dy = atoi(buf);
                    i++;
                    for (n = 0; n < 2; n++) // Hr
                        buf[n] = cmd[i++];
                    buf[n] = NULL;
                    hrr = atoi(buf);
                    i++;
                    for (n = 0; n < 2; n++) // Min
                        buf[n] = cmd[i++];
                    buf[n] = NULL;
                    mnn = atoi(buf);

                    rtc.adjust(DateTime(yrr, mnth, dy, hrr, mnn, 0));
                    Serial.println("RTC Time Adjusted.");
                    pCharacteristic->setValue("RTC Time Adjusted.");
                    pCharacteristic->notify();
                }
                else
                {
                    Serial.println("[Error]Invalid RTC Command");
                    pCharacteristic->setValue("[Error] Invalid RTC Command");
                    pCharacteristic->notify();
                }
            }
            else
            {
                Serial.println("[Error]Invalid Command");
                pCharacteristic->setValue("[Error] Invalid Command");
                pCharacteristic->notify();
            }
            cmdF = false;
            if ((smode == 'P') || (smode == 'T'))
                timerAlarmEnable(timer);
            pref.end();
        }
    }
};

void swInfo()
{
    /*Information related to node:
     * Number of Interfaces
     * Interface ID
     * Interface State
     * Interface Mode: Periodic (P) or Manual (M)
     * Interface value: seconds or ON/OFF(1/0)
     */
    char b[200];

    sInfo = String(nInterface) + ',' + String(app[0].prev_state) + ',' + String(app[1].prev_state) + ',' + String(app[0].ID) + ',' + String(app[0].curr_state) + ',' + String(app[1].ID) + ',' + String(app[1].curr_state) + ',' + String(smode) + ',';
    if (smode == 'M')
    {
        sInfo += String(app[0].val[0].mData) + ',' + String(app[1].val[0].mData);
    }
    else if (smode == 'P')
    {
        for (int i = 0; i < 7; i++)
            sInfo += String(week[i]);
        sInfo += ',' + String(app[0].val[0].onMin) + ',' + String(app[0].val[0].offMin) + ',' + String(app[1].val[0].onMin) + ',' + String(app[1].val[0].offMin) + ',' + String(app[0].val[1].onMin) + ',' + String(app[0].val[1].offMin) + ',' + String(app[1].val[1].onMin) + ',' + String(app[1].val[1].offMin) + ',' + String(app[0].val[2].onMin) + ',' + String(app[0].val[2].offMin) + ',' + String(app[1].val[2].onMin) + ',' + String(app[1].val[2].offMin) + ',' + String(app[0].val[3].onMin) + ',' + String(app[0].val[3].offMin) + ',' + String(app[1].val[3].onMin) + ',' + String(app[1].val[3].offMin);
    }
    else if (smode == 'T')
    {
        for (int i = 0; i < 7; i++)
            sInfo += String(week[i]);
        sInfo += ',' + String(App1Sch) + ',';
        for (int m = 0; m < App1Sch; m++)
        {
            for (int l = 0; l < 4; l++)
            {
                if (frame_App1[m][l] < 10)
                    sInfo += '0' + String(frame_App1[m][l]);
                else
                    sInfo += String(frame_App1[m][l]);
            }
            sInfo += ',';
        }
        sInfo += String(App2Sch) + ',';
        for (int m = 0; m < App2Sch; m++)
        {
            for (int l = 0; l < 4; l++)
            {
                if (frame_App2[m][l] < 10)
                    sInfo += '0' + String(frame_App2[m][l]);
                else
                    sInfo += String(frame_App2[m][l]);
            }
            if (m != (App2Sch - 1))
                sInfo += ',';
        }
    }
    sInfo.toCharArray(b, 200);
    // dtostrf("12", 1, 2, data);
    Serial.println(b);
    pCharacteristic->setValue(b);
    pCharacteristic->notify();
    // delay(1000);
}
