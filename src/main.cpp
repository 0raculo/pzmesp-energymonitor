#include <PZEM004Tv30.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* Use software serial for the PZEM
 * Pin 11 Rx (Connects to the Tx pin on the PZEM)
 * Pin 12 Tx (Connects to the Rx pin on the PZEM)
*/
PZEM004Tv30 pzem(D6, D5);

const char* ssid = "ssid"; // Enter your WiFi name
const char* password =  "password"; // Enter WiFi password
const char* mqttServer = "192.168.69.2";
const int mqttPort = 1883;
const char* mqttUser = "emon_garagem";
const char* mqttPassword = "emon_garagem_pw";

//criar variaveis para MQTT client

const char* mqqt_client_name = "ESPPwMeterGaragem1";

long lastMsg = 0;

WiFiClient espClient;
PubSubClient client(espClient);
 
void callback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
 
  Serial.println();
  Serial.println("-----------------------");
 
}


void setup() {
 
  Serial.begin(115200);
 
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to the WiFi network");
 
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
 }
 
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
 
    if (client.connect(mqqt_client_name, mqttUser, mqttPassword )) {
      Serial.println("connected");  
    } else {
 
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
 
    }
  }

}
 


void process() {
    float voltage = pzem.voltage();
    if( !isnan(voltage) ){
        Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
        client.publish("emon/PowerMeter1/Voltage", String(voltage).c_str(), true);
    } else {
        Serial.println("Error reading voltage");
    }

    float current = pzem.current();
    if( !isnan(current) ){
        Serial.print("Current: "); Serial.print(current); Serial.println("A");
        client.publish("emon/PowerMeter1/Current", String(current).c_str(), true);
    } else {
        Serial.println("Error reading current");
    }

    float power = pzem.power();
    if( !isnan(power) ){
        Serial.print("Power: "); Serial.print(power); Serial.println("W");
        client.publish("emon/PowerMeter1/Power",  String(power).c_str(), true);
    } else {
        Serial.println("Error reading power");
    }

    float energy = pzem.energy();
    if( !isnan(energy) ){
        Serial.print("Energy: "); Serial.print(energy,3); Serial.println("kWh");
        client.publish("emon/PowerMeter1/Energy",  String(energy).c_str(), true);
    } else {
        Serial.println("Error reading energy");
    }

    float frequency = pzem.frequency();
    if( !isnan(frequency) ){
        Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
        client.publish("emon/PowerMeter1/Frequency",  String(frequency).c_str(), true);
    } else {
        Serial.println("Error reading frequency");
    }

    float pf = pzem.pf();
    if( !isnan(pf) ){
        Serial.print("PF: "); Serial.println(pf);
        client.publish("emon/PowerMeter1/PowerFactor",  String(pf).c_str(), true);
    } else {
        Serial.println("Error reading power factor");
    }

    Serial.println();
}
 
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    
    long now = millis();
    if (now - lastMsg > 5000) {
        // Wait a few seconds between measurements
    lastMsg = now;
    Serial.println("=======================");

    process();
    
    Serial.println("=======================");
    Serial.println("TERMINEI o processamento");

    }
}
