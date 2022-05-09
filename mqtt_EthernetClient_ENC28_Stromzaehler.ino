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
#include <EthernetENC.h>

/******************** Ethernet Client Setup *******************/
byte mac[] =      {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xF5};         //must be unique   // see list of used addresses
byte ip[] =       {192, 168, 1, 245 };                          //must be unique
byte gateway[] =  {192, 168, 1, 1 };    
byte subnet[] =   {255, 255, 255, 0 }; 

//HardwareSerial Serial3(USART2);  //STM32

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 0
// Include library
#include <MqttClient.h>

//Print logfile
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

#define HW_UART_SPEED								115200L

/************** MQTT DEVICE LOCATION ***************/
#define MQTT_LOC                    "Technik"
#define MQTT_ID										  "Stromzaehler"
const char* MQTT_TOPIC =            "/" MQTT_LOC "/" MQTT_ID ;
const char* MQTT_TOPIC_P =          "/" MQTT_LOC "/" MQTT_ID "/P" ;
const char* MQTT_TOPIC_E =          "/" MQTT_LOC "/" MQTT_ID "/E" ; 
/***************************************************/

MqttClient *mqtt = NULL;
EthernetClient network;

//--------------- OWN STUFF -------------------

//String msgStr = "4D480177070100600100FF010101010B0A01454D48000096E0EF0177070100010800FF641C0104726201641044F5621E52FF6432537B0177070100100700FF0101621B5200530342010101632E2600760500";   //Test message
String msgStr = "" ;       
#define STR_RES     850  //Chars //1 message Byte stored as HEX number in 2 Chars
#define STR_LIM     800   
unsigned long int energy = 0;
unsigned long int power = 0;

//--------------- OWN STUFF END ---------------

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

  //-------------- OWN STUFF-----------------

  //Reader
  msgStr.reserve(STR_RES);
    Serial3.begin(9600); 

  Serial.print(".");
  //------------ OWN STUFF END -----------------

  Ethernet.init(10);    //53 Mega //10 Uno(Shield)  //PA4 STM32
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  //MAC address for the Arduino

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
    Serial.print("Connect");
		// Close connection if exists
		network.stop();
		// Re-establish TCP connection with MQTT broker
		network.connect("192.168.1.16", 1883);              //("IP", Port) of your MQTT broker
		// Start new MQTT connection
    Serial.print(".");  
		//LOG_PRINTFLN("Connecting to Broker");
    MqttClient::ConnectResult connectResult;
		// Connect
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.username.cstring = (char*)"mqtt-user";    //Login to your MQTT broker
    options.password.cstring = (char*)"h0m3455mqtt";
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)MQTT_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      //LOG_PRINTFLN("Connection error: %i", rc);
      Serial.println("CON ERR");
      return;
    } else {
      // Serial.print("."); 

      // //send ONLINE info
      // const char* buf = "ONLINE";
      // MqttClient::Message message;
      // message.qos = MqttClient::QOS0;
      // message.retained = false;
      // message.dup = false;
      // message.payload = (void*) buf;
      // message.payloadLen = strlen(buf);
      // mqtt->publish(MQTT_TOPIC, message);  

      Serial.println("ok"); 
    }      

    // Add subscribe here if need  //runs once after succesful connection

    //   MqttClient::Error::type rc = mqtt->subscribe(MQTT_TOPIC_TEST, MqttClient::QOS0, processMessageTest);
    //   if (rc != MqttClient::Error::SUCCESS) {
    //     LOG_PRINTFLN("Subscribe error: %i", rc);
    //     LOG_PRINTFLN("Drop connection");
    //     mqtt->disconnect();
    //     return;
    //   }
    
	}   
  else  // start big else, MQQT connection good, normal loop here
  {

    //no valid data capurted so far, reset buffer before it gets to big
    if(msgStr.length() >= STR_LIM) {
      //Serial.println(msgStr); //debug only
      msgStr = ""; 
    }

    //capture data
    while(Serial3.available()) {            //not sure whether if() works better here
      unsigned char in = Serial3.read();    
      msgStr += bytetoHEX(in);
      //delay(10);                   
    } 

    //try parsing
    signed char validData =  0;    //0 no header, -1 no energy data(fatal), 1 only energy, 3 both valid
    if(msgStr.length() >= 300) validData = parse();  //some bytes captures, try parse    
    
    //printing values
    if(validData >= 3) {  // with >= 1 it is sending multiple times per recv message
      float wh = (float)energy/10.0f;  //10000.0f for kWh

      Serial.print(wh,1);
      Serial.print("Wh");
      
      //if(validData >= 3) {	
        Serial.print(", ");
        Serial.print(power);
        Serial.println("W") ;

        //publish Watt
        {
          String temp = String(power);
          const char* buf = temp.c_str(); //"WATT";
          MqttClient::Message message;
          message.qos = MqttClient::QOS0;
          message.retained = false;
          message.dup = false;
          message.payload = (void*) buf;
          message.payloadLen = strlen(buf);
          mqtt->publish(MQTT_TOPIC_P, message);
        }
        msgStr = "";  //complete message parsed, clear buf
		  //} else Serial.println();

      //publish Watthours
      {
        String temp = String(wh,1);
        const char* buf = temp.c_str(); //"WATTHOURS";
        MqttClient::Message message;
        message.qos = MqttClient::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*) buf;
        message.payloadLen = strlen(buf);
        mqtt->publish(MQTT_TOPIC_E, message);      
      }
      
    } //end valid data


		//delay 20ms
		mqtt->yield(50L);
	} //end big else
}  //end loop()


//-------------------- DECODE DATA ---------------------------------------------
String SearchHead   = "77070100010800ff";     //PayloadHead
String SearchStartE = "621e52ff";             //8+2 Chars  = PayloadStartE
String SearchEndE   = "0177070100100700ff";   //-1 Chars   = PayloadEndE
String SearchStartP = "0101621b5200";         //12+2 Chars = PayloadStartP
String SearchEndP   = "010101";               //-1 Chars   = PayloadEndP

//short StartPos = aString.indexOf(SearchStr, searchFromPos)  //StartPos of SearchStr in aString, start looking at searchFrom
//unsigned long int x = strtoul(aString.c_str(), NULL, 16);   //hexString to unsinged long int

inline signed char parse() {
  short index = msgStr.indexOf(SearchHead);  

  if(index >= 0) {  //valid "head" found    
    signed char ret = 0;
    signed short sE = -1, eE = -1, sP = -1, eP = -1;

    sE = msgStr.indexOf(SearchStartE, index);  
    if(sE > 0) {
      sE += 10; 
      eE = msgStr.indexOf(SearchEndE, sE);
      if(eE > 0) {
        String payloadE = msgStr.substring(sE, eE);
        //Serial.print("E_hex: ") ; Serial.println(payloadE) ;
        energy = strtoul(payloadE.c_str(), NULL, 16);
        ret = 1;   //bit0 = 1
      } else {
        return -1; //fatal
      }         
    } else {
      return -1; //fatal
    }

    sP = msgStr.indexOf(SearchStartP, eE);  
    if(sP > 0) {
      sP += 14;
      eP = msgStr.indexOf(SearchEndP, sP);
      if(eP > 0) {   
        String payloadP = msgStr.substring(sP, eP);
        //Serial.print("P_hex: ") ; Serial.println(payloadP) ;
        power = strtoul(payloadP.c_str(), NULL, 16);
        ret += 2; //bit1 = 1
      }
    }
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