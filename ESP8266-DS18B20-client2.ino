// ** deska Node MCU 0.9(ESP-12 module)
// kompilace z vice programu
// zapisuje data na WEBINEC
// V01 - na 1 wifi 
// V02 - pridany vsechny domaci site

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h> // must be included here!!
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClient.h>
ESP8266WiFiMulti WiFiMulti;
#define countof(a) (sizeof(a) / sizeof(a[0]))
#define REDLED D7
#define VSTUP  D0

// -------------  ESP DALLAS DS18B20 devices ------------------------------
// Data wire is plugged into port D2 on the ESP8266
#define ONE_WIRE_BUS D2
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

uint8_t sensor[5][8] = { 
                         {0x28, 0x4C, 0x1A, 0x80, 0xE3, 0xE1, 0x3D, 0xDF},   // T0
                         {0x28, 0x40, 0x7F, 0x03, 0x00, 0x00, 0x80, 0x6D},   // T1
                         {0x28, 0x8E, 0xC6, 0x80, 0xE3, 0xE1, 0x3D, 0x51},   // T2
                         {0x28, 0xBD, 0x76, 0x03, 0x00, 0x00, 0x80, 0xA6},   // T3
                         {0x28, 0xD6, 0xE5, 0x80, 0xE3, 0xE1, 0x3D, 0x68}    // T4
                       } ;
// ------------ time & NTP variables --------------------------------------
// NTP Servers and tie zone
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 1;     // Central European Time
// variables and functions
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
char currtime[]   = "hh:mm:ss dd.mm.yyyy";
int  hh,mm,ss,DD,MM,YYYY; 
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
// --------------- Wifi variables ----------------------------------------
const char *ssid[4]     = { "BezDratu4",      "BezDratu3",      "jirigovo",    "TP-Link_C0AB" };
const char *password[4] = { "Nevim598neznam!", "Nevim598neznaM", "Neser34bohY", "93847742" };
#define MAXP 30
int maxpok = MAXP;      // pocet pokusu o pripojeni
int apmax=3;            // 0..3 pripojeni
int network = 1;        // cislo AP (zresetuje se na 0)
float tempSensor[5];    // merene teploty
float tempcal[5] = { 0,	-0.02,	-0.25,	-0.0925,	0.2125 }; // kalibrace teploty


char msg[100];
// ----------------debuggigng --------------------------------------------x
boolean line = true;    // print on serial
boolean debug = true;   // debug prints
int     ledstate=0;     // led state
// ---------------------SETUP --------------------------- 
void setup() 
{ int i;
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VSTUP,INPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(REDLED, OUTPUT);
  digitalWrite(REDLED, LOW);
  if (line) Serial.begin(115200);
  if (line) 
  { for(i=1;i<8;i++)
    delay(1000);
    Serial.println(i);
    Serial.println("ESP WIFI logger with 3xDS18B20 by jirig V2.3 2023/11/03");
  }
  
  // connect to wifi AP
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    for(i=0;i<5;i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }  
    
    if (line) Serial.print(".");
    maxpok--;
    if (maxpok<=0)
    { network++;
      if (network>apmax) network=0;
      if (line) 
      { Serial.println();
        Serial.print("Connecting to ");
        Serial.print(network);
        Serial.print(" ");
        Serial.print(ssid[network]);
        Serial.print(": ");
      }   
      WiFi.begin(ssid[network], password[network]);
      maxpok = MAXP;
    }
  }
  digitalWrite(LED_BUILTIN, LOW);  // says 'connected'
  if (line) 
  { Serial.println("");
    Serial.print("WiFi connected to ");
    Serial.println(ssid[network]);  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Netmask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
  }

  i=0;
  // connecting to NTP server
  if (line) Serial.println("Starting UDP");
  Udp.begin(localPort);
  if (line) 
  { Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("waiting for sync");
  }
  setSyncProvider(getNtpTime);    // Set the time using NTP
  //setSyncInterval(300);         // default is 1800 (s)
  if (line) digitalClockDisplay();
 
  discoverOneWireDevices();
  digitalWrite(REDLED, HIGH);
  Serial.println("-- setup end --"); 
}
// ------------------ PROGRAM LOOP ---------------------- 
void loop() 
{ float d,h,t,f;
  int min, sec, i;
  char stringurl[100];

  if (second() != 0)
  { if (line) Serial.print("");
    delay(5);
    return;
  }  
  if (line) Serial.println("");

  sensors.requestTemperatures();
  for(i=0;i<5;i++)
  { tempSensor[i] = sensors.getTempC(sensor[i]) + tempcal[i]; // temp and calibration
  }

  stringClockDisplay();
  sprintf(msg,"T%d = %.4f, T%d = %.4f, T%d = %.4f, T%d = %.4f, T%d = %.4f at %s",0,tempSensor[0],1,tempSensor[1],2,tempSensor[2],3,tempSensor[3],4,tempSensor[4],currtime); 
  Serial.println(msg);
  while (second() == 0);
  if (minute()%5 != 0)
  { if (line) Serial.println("minute%5<>0");
    return;
  }  

  sprintf(stringurl,"http://www.webinec.cz/mycka/test7.php?val1=%.3f&val2=%.3f&val3=%.3f&val4=%.3f&val5=%.3f",
                     tempSensor[0],tempSensor[1],tempSensor[2],tempSensor[3],tempSensor[4]);
  Serial.println(stringurl);
  if ((WiFiMulti.run() == WL_CONNECTED)) 
  { WiFiClient client;
    HTTPClient http;
    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, stringurl))   // HTTP
    { if (line) Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) 
      { // HTTP header has been send and Server response header has been handled
        if (line) Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
        { String payload = http.getString();
          if (line) Serial.println(payload);
        }
      } else 
      { if (line) Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else 
    { if (line) Serial.printf("[HTTP} Unable to connect\n");
      digitalWrite(LED_BUILTIN, HIGH);
      wifi_reconnect();
    }
  }

}
// ----------------- FUNCTIONS -------------------------  
void stringClockDisplay()
{ char tt[] = "xx:";
  int  i    = 0;
  // digital clock display of the time
  hh   = hour();     digit2(hh,0);
  mm   = minute();   digit2(mm,3);
  ss   = second();   digit2(ss,6);
  DD   = day();      digit2(DD,9);
  MM   = month();    digit2(MM,12);
  YYYY = year();     digit2(YYYY/100,15); digit2(YYYY%100,17); 
}

void digit2(int val, int poz)
{ int  i = poz;
  byte h = val/10;
  byte d = val%10;
  currtime[i++]= '0' + h; 
  currtime[i++]= '0' + d;   
}


//**//**//**//

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println(); 
}
 
void printDigits(int digits)
{ // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10) Serial.print('0');
  Serial.print(digits);
}
 
/*-------- NTP code ----------*/
 
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
 
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address
 
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  if (line) Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  if (line) 
  { Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
  }  
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) 
  { int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) 
    {if (line) Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  if (line) Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
  digitalWrite(LED_BUILTIN, HIGH);
  wifi_reconnect();
}
 
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
 
void discoverOneWireDevices(void) 
{ byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];

  if (line) Serial.print("Looking for 1-Wire devices...\n\r");// "\n\r" is NewLine 
  while(oneWire.search(addr)) {
    if (line) Serial.print("\n\r\n\rFound \'1-Wire\' device with address:\n\r");
    for( i = 0; i < 8; i++) {
      if (line) Serial.print("0x");
      if (addr[i] < 16) {
        if (line) Serial.print('0');
      }
      if (line) Serial.print(addr[i], HEX);
      if (i < 7) {
        if (line) Serial.print(", ");
      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
      if (line) Serial.print("CRC is not valid!\n\r");
      return;
    }
  }
  if (line) Serial.println();
  if (line) Serial.print("Devices done");
  oneWire.reset_search();
  return;
}

void wifi_reconnect(void)
{ int i=0;
  while (WiFi.status() != WL_CONNECTED) {
    for(i=0;i<5;i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }  
    
    if (line) Serial.print(".");
    //maxpok=maxpok-1;
    if (--maxpok<=0)
    { network++;
      maxpok = MAXP;
      if (network>apmax) network=0;
      { Serial.println();
        Serial.print("Connecting to ");
        Serial.print(network);
        Serial.print(" ");
        Serial.print(ssid[network]);
        Serial.print(": ");
      }   
      WiFi.begin(ssid[network], password[network]);
    }
  }
  digitalWrite(LED_BUILTIN, LOW);  // parmanent says 'connected'
  if (line) 
  { Serial.println("");
    Serial.print("WiFi connected to ");
    Serial.println(ssid[network]);  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Netmask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP()); 
  }
}
