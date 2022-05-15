
/*********************** MQTT EMH eBZD Decoder by Orbis92 ********************************
 *                 Quick and Dirty EMH eBZD Decoder with MQTT transmitter                *
 *                                                                                       *
 *  - Decode meter reading in kWh (Wh). The resolution of the eBZD is 0.1Wh              *
 *  - If unlocked, current power in Watt is decoded, too. Check code if you do not have  *
 *    the power reading, you have to change "validData >= 3" to " >= 1"                  *                                                                 *
 *****************************************************************************************/

// #1 For the main part of the code a big thanks to the following author(s). 
// #2 Thanks for the ENC28J60 lib to replace the W5500 Phy, which is a pain to use, at least for me somehow...

/*
 *******************************************************************************
 *
 * Purpose: Example of using the Arduino MqttClient with EthernetClient.
 * Project URL: https://github.com/monstrenyatko/ArduinoMqtt
 *
 *******************************************************************************
 * Copyright Oleg Kovalenko 2017.
 *
 * Distributed under the MIT License.
 * (See accompanying file LICENSE or copy at http://opensource.org/licenses/MIT)
 *******************************************************************************
 */

#include <Arduino.h>
#include <EthernetENC.h>            //<EthernetENC.h> for ENC28J60   //<Ethernet.h>  for W5500 (not tested)

//MqttClient
#define MQTT_LOG_ENABLED 0
#include <MqttClient.h>

/******************************** Ethernet Client Setup ********************************/
byte mac[] =      {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xF5};         //must be unique
byte ip[] =       {192, 168, 1, 245 };                          //must be unique
byte gateway[] =  {192, 168, 1, 1 };    
byte subnet[] =   {255, 255, 255, 0 };  

/***************************************************************************************/

#define HW_UART_SPEED								115200L

/***************************** MQTT DEVICE LOCATION *************************************/
#define MQTT_LOC                    "Basement"
#define MQTT_ID										  "PowerMeter"
const char* MQTT_TOPIC =            "/" MQTT_LOC "/" MQTT_ID ;
const char* MQTT_TOPIC_P =          "/" MQTT_LOC "/" MQTT_ID "/P" ;   //power  (Watt)
const char* MQTT_TOPIC_E =          "/" MQTT_LOC "/" MQTT_ID "/E" ;   //energy (Wh, kWh)
/***************************************************************************************/

MqttClient *mqtt = NULL;
EthernetClient network;


//message "buffer"
String msgStr = "" ;       
#define STR_RES     850  //Reserved chars //1 message Byte stored as HEX number in 2 Chars
#define STR_LIM     800  //Reset before reserve is reached

//32bit max = 4,294,967,295
//raw meter value is 0.1Wh -> 32bit max 429496kWh (@50kWh/day -> 23years max) 
uint64_t energy = 0;                 //64bit  (101 billion years max...)
unsigned long int power = 0;         //4 Gigawatt max should be enough...

//============== MQTT logging ===================================
// #define LOG_PRINTFLN(fmt, ...)	printfln_P(PSTR(fmt), ##__VA_ARGS__)
// #define LOG_SIZE_MAX 128
// void printfln_P(const char *fmt, ...) {
// 	char buf[LOG_SIZE_MAX];
// 	va_list ap;
// 	va_start(ap, fmt);
// 	vsnprintf_P(buf, LOG_SIZE_MAX, fmt, ap);
// 	va_end(ap);
// 	Serial.println(buf);
// }

// ============== Object to supply system functions =============
class System: public MqttClient::System {
public:
	unsigned long millis() const {
		return ::millis();
	}
};

// ============== Setup all objects ==============================
void setup() {
  // Setup hardware serial for logging
  Serial.begin(HW_UART_SPEED);
	while (!Serial);
  Serial.print("Boot");

  //Read head
  msgStr.reserve(STR_RES); //reserve some memory for the string
  Serial3.begin(9600);
  Serial.print(".");

  // Connect ethernet
  Ethernet.init(10);                                  //CS pin of the Ethernet Chip
  //Ethernet.begin(mac);                              //DHCP, uses a lot of RAM and ROM
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  //static
  Serial.print(".");

  // Setup MqttClient
  MqttClient::System *mqttSystem = new System;
  MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl<HardwareSerial>(Serial);
  MqttClient::Network * mqttNetwork = new MqttClient::NetworkClientImpl<Client>(network, *mqttSystem);
  //// Make 128 bytes send buffer
  MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<128>();
  //// Make 128 bytes receive buffer
  MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<128>();
  //// Allow up to 2 subscriptions simultaneously
  MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<2>();
  //// Configure client options
  MqttClient::Options mqttOptions;
  ////// Set command timeout to 10 seconds
  mqttOptions.commandTimeoutMs = 10000;
  //// Make client object
  mqtt = new MqttClient (
    mqttOptions, *mqttLogger, *mqttSystem, *mqttNetwork, *mqttSendBuffer,
    *mqttRecvBuffer, *mqttMessageHandlers
  );

  Serial.println("ok");
}

// ============== Main loop ======================
void loop() {
	// Check connection status 
	if (!mqtt->isConnected()) {
    Serial.print("Con");

		// Re-establish TCP connection with MQTT broker
		network.stop();
		network.connect("192.168.1.16", 1883);         //("IP", Port) of your MQTT broker
		
    // Start new MQTT connection
		//LOG_PRINTFLN("Connecting");
    MqttClient::ConnectResult connectResult;
		Serial.print("."); 

    // Connect
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.username.cstring = (char*)"mqtt-user";    //Login to your MQTT broker
    options.password.cstring = (char*)"password";
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)MQTT_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      //LOG_PRINTFLN("Connection error: %i", rc);
      Serial.println("ERR");
      return;
    } else {
      // //publish ONLINE info
      // Serial.print("."); 
      // const char* buf = "ONLINE";
      // MqttClient::Message message;
      // message.qos = MqttClient::QOS0;
      // message.retained = false;
      // message.dup = false;
      // message.payload = (void*) buf;
      // message.payloadLen = strlen(buf);
      // mqtt->publish(MQTT_TOPIC, message);  
      
      // Add subscribe here if need  //runs once after succesful connection
      // Serial.print("."); 
      //   MqttClient::Error::type rc = mqtt->subscribe(MQTT_TOPIC_TEST, MqttClient::QOS0, processMessageTest);
      //   if (rc != MqttClient::Error::SUCCESS) {
      //     LOG_PRINTFLN("Subscribe error: %i", rc);
      //     LOG_PRINTFLN("Drop connection");
      //     mqtt->disconnect();
      //     return;
      //   }

      Serial.println("ok"); 
    }      
    
	}   
  else  // start big else, MQQT connection good, "normal loop stuff" here
  {
    //no valid data capurted so far, reset buffer before it gets to big
    if(msgStr.length() >= STR_LIM) {
      //Serial.println(msgStr);          //debug
      msgStr = "";
    }

    //capture data
    while(Serial3.available()) {            //not sure whether if() works better here
      unsigned char in = Serial3.read();    
      msgStr += bytetoHEX(in);
      //delay(10);  //wait for more bytes
    } 

    //try parsing
    unsigned char validData =  0;    //0 no header or energy data, 1 only energy, 3 energy & power
    if(msgStr.length() >= 300) validData = parse();         //some bytes captures, try parse    
    
    //printing values
    if(validData >= 3) {  //engery and power decoded
      msgStr = "";        //clear buffer

      float wh = (float)energy/10.0f;  //10000.0f for kWh  //energy is 64bit!

      // Serial.print(wh,1);
      // Serial.print("Wh");      
      // Serial.print(", ");
      // Serial.print(power);
      // Serial.println("W") ;

      //publish Watt
      {
        String temp = String(power);
        const char* buf = temp.c_str(); 
        MqttClient::Message message;
        message.qos = MqttClient::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*) buf;
        message.payloadLen = strlen(buf);
        mqtt->publish(MQTT_TOPIC_P, message);
      }        
		  
      //publish Watthours
      {
        String temp = String(wh, 1);    //1 decimal place
        const char* buf = temp.c_str(); 
        MqttClient::Message message;
        message.qos = MqttClient::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*) buf;
        message.payloadLen = strlen(buf);
        mqtt->publish(MQTT_TOPIC_E, message);      
      }      
    } //end valid data


		//idle 50ms
		mqtt->yield(50L);
	} //end "normal loop stuff" else
}  //end loop()


//================================ DECODE DATA ===============================================
//These are the fixed message parts to "zero in" on the payload areas...
String SearchHead   = "77070100010800ff";     //PayloadHead
String SearchStartE = "621e52ff";             //8+2 Chars  = PayloadStartEnergy
String SearchEndE   = "0177070100100700ff";   //-1 Chars   = PayloadEndEnergy
String SearchStartP = "0101621b5200";         //12+2 Chars = PayloadStartPower
String SearchEndP   = "010101";               //-1 Chars   = PayloadEndPower

//short StartPos = aString.indexOf(SearchStr, searchFromPos)    //StartPos of SearchStr in aString, starting at searchFrom
//unsigned long int num = strtoul(aString.c_str(), NULL, 16);   //hexString to unsinged long int

inline unsigned char parse() {
  short index = msgStr.indexOf(SearchHead);  

  if(index >= 0) {  //valid "head" found    
    signed char ret = 0;
    signed short sE = -1, eE = -1, sP = -1, eP = -1;

    sE = msgStr.indexOf(SearchStartE, index);  
    if(sE > 0) {    // > index
      sE += 10; 
      eE = msgStr.indexOf(SearchEndE, sE);
      if(eE > 0) {  // > sE
        String payloadE = msgStr.substring(sE, eE);
        //Serial.print("E_hex: ") ; Serial.println(payloadE) ;    //debug
        //energy = strtoul(payloadE.c_str(), NULL, 16);           //32bit 
        energy = strtoul(payloadE.c_str(), NULL, 16);     //64bit
        ret += 1;   //bit0 = 1
      } else {
        return 0;
      }         
    } else {
      return 0; 
    }

    sP = msgStr.indexOf(SearchStartP, eE);  
    if(sP > 0) {   // > eE
      sP += 14;
      eP = msgStr.indexOf(SearchEndP, sP);
      if(eP > 0) {   // > sP
        String payloadP = msgStr.substring(sP, eP);
        //Serial.print("P_hex: ") ; Serial.println(payloadP) ;      //debug
        power = strtoul(payloadP.c_str(), NULL, 16);     
        ret += 2; //bit1 = 1
      }
    }
    //debug
    //Serial.print("sE: "); Serial.println(sE); 
    //Serial.print("eE: "); Serial.println(eE);  
    //Serial.print("sP: "); Serial.println(sP);
    //Serial.print("eP: "); Serial.println(eP);

    return ret;
  }

  return 0;  //no header found
}

//Convert Byte to HEX string, add 0 for <= 15 (0F)
inline String bytetoHEX(byte in) {
  String str = "";
  if (in < 16) str += "0";
  str += String(in, HEX);
  return str;
}

// ============== Subscription callback ========================================
// void processMessageTest(MqttClient::MessageData& md) {
// 	const MqttClient::Message& msg = md.message;
// 	char payload[msg.payloadLen + 1];
// 	memcpy(payload, msg.payload, msg.payloadLen);
//   payload[msg.payloadLen] = '\0';

//  	LOG_PRINTFLN(
// 		"Message arrived: qos %d, retained %d, dup %d, packetid %d, payload:[%s]",
// 		msg.qos, msg.retained, msg.dup, msg.id, payload
// 	);

//   if(strcmp(payload,"TEST")) {
//     Serial.println("HW: LAMP ON");
//   } else {
//     Serial.println("Unkown Payload");
//   }
// }
