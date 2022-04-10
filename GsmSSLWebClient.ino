/*
  Web client

 This sketch connects to a website using SSL through a MKR GSM 1400 board. Specifically,
 this example downloads the URL "http://www.arduino.cc/asciilogo.txt" and
 prints it to the Serial monitor.

 Circuit:
 * MKR GSM 1400 board
 * Antenna
 * SIM card with a data plan

 created 8 Mar 2012
 by Tom Igoe
*/

// libraries
#include <MKRGSM.h>
#include <ArduinoLowPower.h>
#include "arduino_secrets.h"
// Sensitive data in arduino_secret.h file
const char PINNUMBER[]     = SECRET_PINNUMBER;
const char GPRS_APN[]      = SECRET_GPRS_APN;
const char GPRS_LOGIN[]    = SECRET_GPRS_LOGIN;
const char GPRS_PASSWORD[] = SECRET_GPRS_PASSWORD;
const char phoneNumber[20] = "0597097607"; 

#define samplingInterval 1250
#define sendInterval 50000
#define pHPin A2            //pH meter Analog output to MKR GSM 1400 A2
#define orpPin A1           //ORP meter Analog output to MKR GSM 1400 A1
static float pHOffset = 1.86;            //deviation compensate
#define orpOffset +13         // zero drift voltage offset
#define orpVoltage 5.00     //system voltage
#define pHArrayLength 40    //times of collection pH
#define orpArrayLength 40   //times of collection ORP

// initialize the instances
GSMSSLClient client;
GPRS gprs;
GSM gsmAccess;
GSM_SMS sms;

// URL server to connect to, ID google script, 
char server[] = "script.google.com";
const String scriptWebAppID = "AKfycbyqCCVK5p9D1qKBIae_dR7CYuvZqXMkhweTg7GAWXliflIKaQ";
int port = 443; // port 443 is the default for HTTPS

int pHArray[pHArrayLength];   //Store the average value of the pH sensor measured values
int pHArrayIndex = 0;
int orpArray[orpArrayLength];
int orpArrayIndex=0;
static float pHValue, voltage, orpValue;
String faulthReport;
bool pHAlert = false;
bool orpAlert = false;

void setup() {

  //MODEM.debug();

  Serial.begin(9600);
 /*  while (!Serial) {
  } */

  Serial.println("Starting Waterquality Measurement ...");
  // connection state
  bool connected = false;
  bool serverConected = false;
  // After starting the modem with GSM.begin()
  // attach the shield to the GPRS network with the APN, login and password
  while (!connected) {
    uint8_t connectionFails = 0;
    uint8_t maximumConnectionFails = 5;
    bool sleepMode = false;
    uint8_t connectionRetries = 0;
    uint8_t maximumConnectionRetries = 2;
    if (connectionFails == maximumConnectionFails)
    {
      // reset processor after 5 failed connections
      connectionRetries++;
      if (connectionRetries == maximumConnectionRetries)
      {
        LowPower.deepSleep(1800000);
      }
      else
      {
        connectionRetries = connectionRetries;
      }
      

    }
    else {
      if ((gsmAccess.begin(PINNUMBER) == GSM_READY) &&
        (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY)) {
        connected = true;
        Serial.println("Connected");
      } 
      else {
        Serial.println("Not connected");
        delay(1000);
        connectionFails++;
        connected = false;
      }
    }
  }
}

double Averaging(int* arr, int number) {
  int total;
  int avg;
    for(int i=0;i<number;i++) {
      total+=arr[i];
      orpArrayIndex = 0;
  }
    avg = total/number;
    return avg;
}
bool ControlpHValue(float value) {
  float maximumpH = 14.0;
  float minimumpH = 0.0;
  if (value > maximumpH)
  {
    faulthReport = "unknown pH value, above" + String(maximumpH);
    return false;
  }
  else if (value < minimumpH)
  {
    faulthReport =  "unknown pH value, under" + String(minimumpH);
    return false;
  }
  else {
    return true;
  }
}

bool ControlorpValue(float value) {
  float maximumorp = 600.0;
  float minimumorp = 300.0;
  if (value > maximumorp)
  {
    faulthReport = "orp value too high, above" + String(maximumorp);
    return false;
  }
  else if (value < minimumorp)
  {
    faulthReport = "orp value too low, under" + String(minimumorp);
    return false;
  }
  else {
    return true;
  }
}

bool IsAlert() {
  if(ControlpHValue(pHValue) == false) {
    pHAlert = true;
  }
  else if(ControlorpValue(orpValue) == false)
  {
    orpAlert = true;
  }
}

void AlertSMS() {
  sms.beginSMS(phoneNumber);
  sms.print("Faulth: " + faulthReport);
  sms.endSMS();

}


double avergearray(int* arr, int number) {
  int i;
  int max, min;
  double averageValue;
  long amount = 0;
  if (number <= 0) {
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if (number < 5) { //less than 5, calculated directly statistics
    for (i = 0; i < number; i++) {
      amount += arr[i];
    }
    averageValue = amount / number;
    return averageValue;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0]; max = arr[1];
    }
    else {
      min = arr[1]; max = arr[0];
    }
    for (i = 2; i < number; i++) {
      if (arr[i] < min) {
        amount += min;      //arr<min
        min = arr[i];
      } else {
        if (arr[i] > max) {
          amount += max;  //arr>max
          max = arr[i];
        } else {
          amount += arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    averageValue = (double)amount / (number - 2);
  }//if
  return averageValue;
}

void sendData(int value1, int value2)  {
  Serial.print("Connecting to ");
  Serial.println(server);
  if (!client.connect(server, port)) {
    Serial.println("Connection failed");
    return;
  }
  String string_ORP =  String(orpValue); 
  String string_pH=  String(pHValue);
  String url = "/macros/s/" +  scriptWebAppID+ "/exec?pH=" + string_pH + "&ORP=" + string_ORP;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + "script.google.com" + "\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  String line = client.readStringUntil('\n');
  Serial.println(line);
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("successfull!");
  } else {
    Serial.println("failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
} 

void loop() {
  static unsigned long samplingTime = millis();
  static unsigned long sendTime = millis();
  if(millis() >= samplingTime) {
    samplingTime = millis() + samplingInterval;
    pHArray[pHArrayIndex++] = analogRead(pHPin);
    orpArray[orpArrayIndex++]=analogRead(orpPin);
    if (pHArrayIndex == pHArrayLength && orpArrayIndex == orpArrayLength) {
      pHArrayIndex = 0;
      orpArrayIndex = 0;
      orpValue = Averaging(orpArray, orpArrayLength);
      voltage = avergearray(pHArray, pHArrayLength) * 5.0 / 1024;
      pHValue = avergearray(pHArray,pHArrayLength) * 14 / 1024 - pHOffset;
/*       if(ControlpHValue(pHValue) == false) {
        AlertSMS();
        LowPower.deepSleep(1800000);
      }
      else if(ControlorpValue(orpValue) == false) {
        AlertSMS();
        LowPower.deepSleep(1800000);
        
        
      } */
    }
  }
  if (millis() >= sendTime ) {
    sendTime = millis() + sendInterval;
    sendData(pHValue, orpValue);
  }
}
