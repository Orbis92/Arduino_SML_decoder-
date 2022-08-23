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
#include <fp64lib.h>        //from the Arduino Library Manager

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
#define MQTT_LOC                    "Technik"
#define MQTT_ID										  "Stromzaehler"
const char* MQTT_TOPIC =            "/" MQTT_LOC "/" MQTT_ID ;
const char* MQTT_TOPIC_P =          "/" MQTT_LOC "/" MQTT_ID "/P" ;   //power   (Watt)
const char* MQTT_TOPIC_E =          "/" MQTT_LOC "/" MQTT_ID "/E" ;   //energy  (Watthours)
/***************************************************************************************/

MqttClient *mqtt = NULL;
EthernetClient network;

//Status led
// #define       sLED       A0
// unsigned long ledTmr   = 0;
// unsigned char ledState = 0;

//some variables for the message "buffer"
String msgStr = "" ;       
#define STR_RES     850  //Reserved chars //1 message Byte stored as HEX number in 2 Chars
#define STR_LIM     800  //Reset before reserve is reached

unsigned long int power = 0; 
uint64_t energy = 0;               //64bit

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
  msgStr.reserve(STR_RES); //reserve some memory for the input string
  Serial3.begin(9600);
  Serial.print(".");

  // Connect ethernet
  Ethernet.init(10);                                  //CS pin of the Ethernet Chip
  //Ethernet.begin(mac);                              //for DHCP, uses a lot of both RAM and ROM
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  //static ip
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
    options.password.cstring = (char*)"password";
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)MQTT_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      //LOG_PRINTFLN("Connection error: %i", rc);
      Serial.println("Err");
      return;
    } else {
      Serial.println("ok"); 
    }         
	}   
  else  // start of "big else", MQQT connection good, "do normal loop stuff" here
  {
  {    
    //Blink status led
    // currentTime = millis();
    // if(ledTmr + 500 <= currentTime) {
    //   ledTmr = currentTime;
    //   ledState ^= 1;
    //   digitalWrite(sLED, ledState);
    // }

    // ------------------- Stromzähler ------------------------------------
    {
      //no valid data capurted so far, reset buffer before it gets to big
      if(msgStr.length() >= STR_LIM) {
        //Serial.println(msgStr); //debug only
        msgStr = ""; 
      }

      //capture data
      while(Serial3.available()) {                       
        unsigned char in = Serial3.read();    
        msgStr += bytetoHEX(in);             
      } 

      //try parsing
      unsigned char validData =  0;                       //0= no data, 1 = only energy, 3 = energy & power
      if(msgStr.length() >= 350) validData = parse();     //some bytes captures, try parse  

      //publish data
      if(validData >= 3) {                                // 3 -> enegry and power decoded
        msgStr = "";  //clear buf
        
        float64_t num = fp64_uint64_to_float64(energy);   //convert to float64
        num = fp64_div(num, fp64_sd(10.0));               //divide by 10.0 for Wh, 10000.0 for kWh
        String wh = fp64_to_string(num, 17, 15);          //Convert to String
        
        //publish energy
        {
          const char* buf = wh.c_str();
          MqttClient::Message message;
          message.qos = MqttClient::QOS0;
          message.retained = false;
          message.dup = false;
          message.payload = (void*) buf;
          message.payloadLen = strlen(buf);
          mqtt->publish(MQTT_TOPIC_E, message);      
        } 

        //publish power
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

        Serial.print(wh);
        Serial.print("Wh");
        Serial.print(", ");
        Serial.print(power);
        Serial.println("W") ;

      } //end valid data


		//idle 50ms
		mqtt->yield(50L);
	} //end "normal loop stuff" else
}  //end loop()

// ======================== DECODE SML DATA ===========================================
String SearchHead   = "77070100010800ff";     //PayloadHead
String SearchStartE = "621e52ff";             //8+2 Chars  = PayloadStartEnergy
String SearchEndE   = "0177070100100700ff";   //-1 Chars   = PayloadEndEnergy
String SearchStartP = "0101621b5200";         //12+2 Chars = PayloadStartPower
String SearchEndP   = "010101";               //-1 Chars   = PayloadEndPower

//short StartPos = String.indexOf(SearchForThisString, searchFromThisPosition)  //returns 0 if not found
//unsigned long int value = strtoul(aString.c_str(), NULL, 16);                 //hex String (16) to unsinged long int

inline unsigned char parse() {
  short index = msgStr.indexOf(SearchHead);  

  if(index > 0) {  //valid "head" found    
    unsigned char ret = 0;
    signed short sE = -1, eE = -1, sP = -1, eP = -1;

    //Get energy
    sE = msgStr.indexOf(SearchStartE, index);  
    if(sE > index) {    
      sE += 10; 
      eE = msgStr.indexOf(SearchEndE, sE);
      if(eE > sE) {  
        String payloadE = msgStr.substring(sE, eE);        
        //Serial.print("E_hex: ") ; Serial.println(payloadE) ;    //debug
        //energy = strtoul(payloadE.c_str(), NULL, 16);           //32bit 
        energy = hex2uint64(payloadE);                            //64bit
        ret += 1;   //bit0 = 1
      } else {
        return 0; //no valid data
      }         
    } else {
      return 0; //no valid data
    }

    //Get power
    sP = msgStr.indexOf(SearchStartP, eE);  
    if(sP > eE) {  
      sP += 14;
      eP = msgStr.indexOf(SearchEndP, sP);
      if(eP > sP) {   
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

// ================== Byte to HEX string =================================================
inline String bytetoHEX(byte in) {
  String str = "";
  if (in < 16) str += "0";
  str += String(in, HEX);
  return str;
}

//================== HexString to uint64_t =================================================
inline uint64_t hex2uint64(String in) {
  //Modified from: https://forum.arduino.cc/t/solved-how-can-i-convert-string-to-long-long/916176/18

  short digits = in.length(); //number of digits
  if(digits > 8) {
    uint64_t high = strtoul((in.substring(0, 8)).c_str(), NULL, 16);
    uint64_t low  = strtoul((in.substring(8)).c_str(), NULL, 16); 

    digits -= 8;  //first 8 digits are already converted
    for (short i = 0; i < digits; i++) {
      high <<= 4;   //multiply by 16 per digit
    }
    return high + low;
  }
  else {
    uint64_t temp = strtoul(in.c_str(), NULL, 16);
    return temp;
  }
}