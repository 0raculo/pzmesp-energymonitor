#include <PZEM004Tv30.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>

#define mqtt_server       "192.168.69.2"
#define mqtt_port         "1883"
#define mqtt_user         "emon_garagem"
#define mqtt_pass         "emon_garagem_pw"
#define meter_name    "emon/PowerMeter1" //(TODO!)

#define mqqt_client_name  "ESPPwMeterGaragem1"

/* Use software serial for the PZEM
 * Pin 11 Rx (Connects to the Tx pin on the PZEM)
 * Pin 12 Tx (Connects to the Rx pin on the PZEM)
*/
PZEM004Tv30 pzem(D6, D5); // RX/TX pins

//flag for saving data
bool shouldSaveConfig = false;



/*
const char* ssid = "ssid"; // Enter your WiFi name
const char* password =  "password"; // Enter WiFi password
const char* mqttServer = "192.168.69.2";
const int mqttPort = 1883;
const char* mqttUser = "emon_garagem";
const char* mqttPassword = "emon_garagem_pw";
*/
//criar variaveis para MQTT client

long lastMsg = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
 
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

   //clean FS for testing 
   //  SPIFFS.format();

  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    Serial.println("reading config file");
    fs::File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
            Serial.println("opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            char buf [size+1];
            uint8_t i =0;

            while((configFile.available()))
            {
                buf[i]=configFile.read();
                i++;
            }
            buf[i]='\0';
            Serial.println(buf);
            DynamicJsonDocument doc(256);

            DeserializationError error = deserializeJson(doc, buf, strlen(buf));
            if (error)
            {
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(error.c_str());
                return;
            }
            Serial.println(error.c_str());
            JsonObject json = doc.to<JsonObject>();
            //serializeJson(doc, Serial);

            Serial.printf(doc["mqtt_server"] | "not found\n");
            if (json.isNull())
            {
                Serial.println("parsed json");

            }
            else Serial.println("error in json");

                if (error) {
                Serial.println("\nparsed json");
                    strlcpy(mqtt_server, doc["mqtt_server"] | "example.com", sizeof(mqtt_server));
                    strlcpy(mqtt_port, doc["mqtt_port"] | "1883", sizeof(mqtt_port));
                    strlcpy(mqtt_user, doc["mqtt_user"] | "emon_garagem", sizeof(mqtt_user));
                    strlcpy(mqtt_pass, doc["mqtt_pass"] | "emon_garagem_pw", sizeof(mqtt_pass));

                } else {
                Serial.println("failed to load json config");
                        }
            }
        }
        } else {
            Serial.println("failed to mount FS");
            }


    // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 20);
  WiFiManagerParameter custom_meter_name("metername", "meter name", meter_name, 30);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

// Reset Wifi settings for testing  
//  wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
//  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_meter_name);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(meter_name, custom_meter_name.getValue());

  
 // strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject json = jsonBuffer.to<JsonObject>();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["meter_name"] = meter_name;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json, configFile);
    
    configFile.close();
    //end save
  } 

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //const uint16_t mqtt_port_x = 12025; //TODO - confirmar que isto abaixo funciona.
  client.setServer(mqtt_server, atoi(mqtt_port));

  client.setCallback(callback);
 }
 
 
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
 
    if (client.connect(mqqt_client_name, mqtt_user, mqtt_pass)) {
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
