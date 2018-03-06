#include <Arduino.h>
#include "ESPNetworkManager.h"
#include <PubSubClient.h>
#include <DNSServer.h>
#include "Adafruit_VL53L0X.h"

//Module Vars
String ModuleID = String(ESP.getChipId());
String mcu_type = "output";
String myTopic = ("/" + mcu_type + "/" + ModuleID+ "/" +"data");
String settingsTopic= ("/" + mcu_type + "/" + ModuleID+ "/" +"settings");
String willTopic=("/" + mcu_type + "/" + ModuleID+ "/" +"lastwill");
String controlType="analog";

//Mqtt Client Setup
const char* micro_mqtt_broker = "educ.chem.auth.gr";
const char* mqtt_username ;
const char* mqtt_password ;
int retries=0;
WiFiClient sclient;
PubSubClient mqtt_client(sclient);
//NetworkManager
EspNetworkManager netmanager(ModuleID);
//Wifi Connection
String WiFiSSID;
String WiFiPassword;

// Adafruit_VL53L0X
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
int datasets=0;
long lastMsg = 0;
unsigned long last_time;
unsigned long currentMillis ;
//Functions
void  ExplicitRunNetworkManager(void);
void onMessageReceived(char*, byte*, unsigned int);
boolean ConnectToNetwork(void);
void MqttReconnect(void);



void setup() {
    EEPROM.begin(512);
    Serial.begin(115200);
 Serial.println("");
  if (!ConnectToNetwork()) {
    ExplicitRunNetworkManager();
  }

  mqtt_client.setServer("educ.chem.auth.gr", 1883);
  mqtt_client.setCallback(onMessageReceived);
  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }else{
    Serial.println("VL53L0X Success");
  }

}

void loop() {
  if (!mqtt_client.connected()) {
    MqttReconnect();
  }
  mqtt_client.loop();
   long now = millis();
   String message = "Out of Range";
   VL53L0X_RangingMeasurementData_t measure;
   lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
   if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    if (now - lastMsg > 50 && datasets<3 ) {
      lastMsg = now;
      Serial.print("Distance (mm): ");
      Serial.println(measure.RangeMilliMeter);
      Serial.print("Reading a measurement... ");
      message = (String) measure.RangeMilliMeter;
      mqtt_client.publish(myTopic.c_str(), message.c_str());
      datasets++;
    } else{
    datasets=0;
    }
  }
}


void MqttReconnect() {
	// Προσπάθησε να συνδεθείς μέχρι να τα καταφερεις
  Serial.print("Connecting to Push Service...");

		// Συνδέσου στον server που ορίσαμε με username:androiduse,password:and3125
		if (!mqtt_client.connect(ModuleID.c_str(),"androiduse","and3125")) {
      delay(1000);
      retries++;
      if (retries<=4) {
  			Serial.print("Failed to connect, mqtt_rc=");
  			Serial.print(mqtt_client.state());
  			Serial.println(" Retrying in 2 seconds");
        retries++;
      }else{
        retries=0;
        ExplicitRunNetworkManager();
      }
      return;
    }else{
      delay(500);
      Serial.println("Connected Sucessfully");
      //TODO PubSub Here
      mqtt_client.subscribe("/android/synchronize");
      mqtt_client.subscribe(myTopic.c_str());
      mqtt_client.subscribe(settingsTopic.c_str());
    }
}

void  ExplicitRunNetworkManager(){
  netmanager.begin();
  if (!netmanager.runManager()) {
    Serial.println("Manager Failed");
  }else{
    Serial.println("Credentials Received");
    Serial.println(netmanager.ReadSSID());
    Serial.println(netmanager.ReadPASSWORD());
  }
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
	//TODO: Add critical notification kai web setup signal
  Serial.print("Inoming Message [Topic: ");
  Serial.print(topic);
  Serial.print("] Message: ");
  String androidID;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    androidID += (char)payload[i];
  }
  Serial.println();
  if (strcmp(topic, "/android/synchronize") == 0)
  {
    Serial.println("Android asking for mC ID");
    String newAndroidTopic = ("/android/" + androidID + "/newmodules");
    String myInfo = (ModuleID + "&" + mcu_type+"&"+controlType+ "&" + "Distance Sensor");
    mqtt_client.publish((newAndroidTopic.c_str()), (myInfo.c_str()));
    delay(2);
  }
}

boolean ConnectToNetwork(){
  WiFi.mode(WIFI_STA);
	delay(10);
	Serial.println("WIFI>>:Attempting Connection to ROM Saved Network");
	String netssid = netmanager.ReadSSID();
	Serial.println("WIFI>>Network: "+netssid);
	String netpass = netmanager.ReadPASSWORD();
	WiFi.begin(netssid.c_str(),netpass.c_str());
	int attemps = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (WiFi.status() == WL_CONNECT_FAILED || attemps>21)
		{
			Serial.println("");
			Serial.println("WIFI>>:Connection Failed. Starting Network Manager.");
      //TODO Remove this delay at release
      delay(10);
			return false;
		}
		attemps++;
	}
	Serial.println("");
	Serial.println("WIFI>>:Connected");
	Serial.print("WIFI>>:IPAddress: ");
	Serial.println(WiFi.localIP());
  return true;
}
