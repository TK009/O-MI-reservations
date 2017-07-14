// vim settings:
// vim: et ts=2 sw=2

#include "Config.h"
#include "MicroUtil.h"
#include "types.h"
#include "OMIProcessing.h"
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <stdlib.h>
#include <string.h>
#include "SoftwareSerial.h"

extern "C" {
#include "user_interface.h"
}


SoftwareSerial Son_off(PIN_SONOFF_RX,PIN_SONOFF_TX);


//byte data[5]={START,RELAY_ON,0x0D,0x0D,STOP};
//int flag=1;
volatile unsigned int beat_index=0,sonoff_index=0;
    

//using namespace std;



void reconnect();
bool modemConnect();
void sonoff(void);
void send_current_or_power(int value,char* whichdata);
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
DB database;

WebSocketsClient webSocket;

#define MESSAGE_INTERVAL 35000
#define HEARTBEAT_INTERVAL 28000
#define RESPONSE_BUFFER 5120

#define LidStatusName FString("LidStatus")

#define GetValueOfItem(name, object, value) \
                tmp = 0; tmpb = 0; \
                if (!(findInfoItem((object), (name), item, tmp) \
                            && findValue(item, (value), tmpb))) { \
                    D("[OMIreserve] Error: Cannot parse "); DS(name); \
                    continue; \
                }


class StringBuffer : public String {
public:
  StringBuffer() : String() {}
  inline StringBuffer & copy_cstr(const char *cstr, unsigned int length) {
    this->copy(cstr,length);
    return *this;
  }
};
const char* PoleSubscriptionRequest = "<omiEnvelope xmlns=\"http://www.opengroup.org/xsd/omi/1.0/\" version=\"1.0\" ttl=\"-1\"><read msgformat=\"odf\" interval=\"-1\" callback=\"0\"><msg><Objects xmlns=\"http://www.opengroup.org/xsd/odf/1.0/\"><Object><id>HWTEST</id><InfoItem name=\"LidStatus\"/></Object></Objects></msg></read></omiEnvelope>";



/*const char* PoleSubscriptionRequest = 
  "<omiEnvelope xmlns=\"http://www.opengroup.org/xsd/omi/1.0/\" version=\"1.0\" ttl=\"-1\">"
   "<read msgformat=\"odf\" interval=\"-1\" callback=\"0\">"
     "<msg>"
       "<Objects xmlns=\"http://www.opengroup.org/xsd/odf/1.0/\">"
         "<Object>"
           "<id>ParkingService</id>"
           "<Object>"
             "<id>ParkingFacilities</id>"
             "<Object>"
               "<id>DipoliParkingLot</id>"
               "<Object>"
                 "<id>ParkingSpaceTypes</id>"
                 "<Object>"
                   "<id>ElectricVehicleParkingSpace</id>"
                   "<Object>"
                     "<id>Spaces</id>"
                     "<Object>"
                       "<id>EVSpace2</id>"
                       "<InfoItem name=\"Available\"/>"
                       "<Object>"
                         "<id>Charger</id>"
                         "<InfoItem name=\"LidStatus\"/>"
                       "</Object>"
                     "</Object>"
                   "</Object>"
                 "</Object>"
               "</Object>"
             "</Object>"
           "</Object>"
         "</Object>"
       "</Objects>"
     "</msg>"
   "</read>"
  "</omiEnvelope>" ;*/

uint64_t messageTimestamp = 0;
uint64_t heartbeatTimestamp = 0;
bool isConnected = false;
bool isSubscribed = false;
bool isXml = false;
int a=20,b=10;
//WStype_t state;
char adress;
//son off variables
boolean stringComplete = false;
int  serIn;             // var that will hold the bytes-in read from the serialBuffer
char *p;
//send power or current to server function.
void send_current_or_power(int value,char* whichdata){
String  str;
String const currentdata2="</value></InfoItem></Object></Objects></msg></write></omiEnvelope>,";
String  currentdata;
char currentdata1[]="15";
itoa(value, currentdata1, 10);
	
if(whichdata=="CURRENT"){

currentdata="<omiEnvelope xmlns=\"http://www.opengroup.org/xsd/omi/1.0/\" version=\"1.0\" ttl=\"-1\"><write msgformat=\"odf\"><msg><Objects xmlns=\"http://www.opengroup.org/xsd/odf/1.0/\"><Object><id>HWTEST</id><InfoItem name=\"Current\"> <value>";


}
else if(whichdata=="VOLTAGE"){

currentdata="<omiEnvelope xmlns=\"http://www.opengroup.org/xsd/omi/1.0/\" version=\"1.0\" ttl=\"-1\"><write msgformat=\"odf\"><msg><Objects xmlns=\"http://www.opengroup.org/xsd/odf/1.0/\"><Object><id>HWTEST</id><InfoItem name=\"Voltage\"> <value>";


}



    
str =currentdata+currentdata1+currentdata2;


	p = strtok(&str[0], ",");

webSocket.sendTXT(p);
//yield();
}


StringBuffer responsePayload;


os_timer_t myTimer;

bool tickOccured=true;

// start of timerCallback
void timerCallback(void *pArg) {
	ESP.wdtDisable();

if(isConnected&&isSubscribed){
	beat_index++;
	sonoff_index++;
/*	if(beat_index==28){
    digitalWrite(D0,!digitalRead(D0)); 
	DLN("Sending heartbeat");
    webSocket.sendTXT("");
    beat_index=0;
	}*/
	DLN("Sending power_data");
			a=Son_off.get_data(1);
			b=Son_off.get_data(0);
			
		
if(tickOccured==true){
send_current_or_power(a,"CURRENT");	
	tickOccured=false;

}
else{
	send_current_or_power(b,"VOLTAGE");
	tickOccured=true;

}



	
}
       
 ESP.wdtEnable(WDTO_8S);   
      
      

} // End of timerCallback

void user_init(void) {


      os_timer_setfn(&myTimer, timerCallback, NULL);


      os_timer_arm(&myTimer, 5000, true);
} // End of user_init

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {


    switch(type) {
        case WStype_DISCONNECTED:
            DFORMAT("[WSc] Disconnected!\n\r");
            isConnected = false;
           
           reconnect();
       
            break;
        case WStype_CONNECTED:
            {
                DFORMAT("[WSc] Connected to url: %s\n\r",  payload);
                isConnected = true;
            
                DLN("Sending Subscription");
                isSubscribed = false;
               user_init();
			    
            }
            break;
        case WStype_TEXT:
            if (length > 0) {
              DFORMAT("[WSc] get text: %s\n\r", payload);
              isXml = true;
              responsePayload.copy_cstr((const char *) payload, length);
            
            }
            break;
        case WStype_BIN:
            DFORMAT("[WSc] get binary length: %u\n\r", length);
            hexdump(payload, length);

            // send data to server
            // webSocket.sendBIN(payload, length);
            break;
    }

}



void setup() {

  responsePayload.reserve(RESPONSE_BUFFER);
noInterrupts();

WiFi.mode(WIFI_OFF);//wifi off for saving power

  // Set console baud rate
  DEBUG_PORT.begin(115200);
  //SerialD;
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(115200);
  SerialAT.swap();
  delay(3000);
//sonooff serial 
  Son_off.begin(115200);
  Son_off.enableRx(true);
   delay(10);
  // PINS
 pinMode(PIN_LOCK, OUTPUT);
  digitalWrite(PIN_LOCK, LOW);
  pinMode(PIN_GSM_PWR, OUTPUT);
  digitalWrite(PIN_GSM_PWR, LOW);

pinMode(D0, OUTPUT);
digitalWrite(D0, LOW);
  // BOOT GSM
  delay(50);
  D("PWR");
  digitalWrite(PIN_GSM_PWR, HIGH);
  D(".HIGH");
  delay(3200);
  D("DONE");
  digitalWrite(PIN_GSM_PWR, LOW);
  D(".LOW");
  delay(300);
  DLN(".INIT...");

interrupts();
  modem.init();
	yield();    
  modemConnect();
 
  
}

bool modemConnect() { // TODO: clean, move to websocket library?
  
  D("Waiting for network...");
  if (!modem.waitForNetwork()) {
    DLN(" fail, retry after 5 secs");
    delay(5000);
    return false;
  }
  DLN(" OK");

  D("Connecting to ");
  DSLN(apn);
  String ip;
  if (!modem.gprsConnect(apn, user, pass, ip)) {
    DLN(" fail, retry after 5 secs");
    delay(5000);
    return false;
  }
  D(" OK, Own IP address: ");
  DSLN(ip);

  // Commented: Connect instead in websocket library?
  D("Connecting to ");
  D(OMI_HOST);
  if (!client.connect(OMI_HOST, 80)) {
    DLN(" fail, retry after 5 secs");
    delay(5000);
    return false;
  }
  DLN(" CONNECT OK ");

  webSocket.onEvent(webSocketEvent);
  yield();
  webSocket.begin(&client, OMI_HOST, 80, OMI_PATH, "omi");
  yield();
  webSocket.connectedCb();
}


void reconnect(void){
  DLN("Initializing modem...");
  modem.init();
   
  modemConnect();
    yield();
}


void loop() {
	
	
  
  // if not connected try to connect
  if (!client.connected()) {
    //if (!reconnect()) return;
    reconnect();
    
  }



  for (int wait = 0; wait < 15 && !isConnected; wait++) {
    DLN("Waiting for connection.");
    auto time = millis() + 5000;
    while (millis() < time && !isConnected) {
      webSocket.loop();
    }
    yield();
  }


 
  while (isConnected) {
    webSocket.loop();
//DLN("===PART6=== ");
 yield();

   if (isXml) {
    		 
      isXml = false;
      String value;
      String item;
      unsigned tmp = 0, tmpb = 0;
      //GetValueOfItem(LidStatusName, responsePayload, value)
      if (findInfoItem(responsePayload, LidStatusName, item, tmp)) {
        if (findValue(item, value, tmpb)) {
          DFORMAT("Result: %s", value.c_str());
        }
        digitalWrite(PIN_LOCK, HIGH);
        delay(2000);
        digitalWrite(PIN_LOCK, LOW);
      }

    }
    
    //DFORMAT("RAM Memory left: %d\r\n", ESP.getFreeHeap());

 if (! isSubscribed) {
    
      webSocket.sendTXT(PoleSubscriptionRequest);
      isSubscribed = true; // TODO: Check response success
      
    }
 


  }
  yield();
}








