#include <FS.h> 
#include <PZEM004Tv30.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <EasyButton.h>           //https://github.com/evert-arias/EasyButton

// Defaults below
#define mqtt_server       "192.168.69.2"
#define mqtt_port         "1883"
#define mqtt_user         "emon_garagem"
#define mqtt_pass         "emon_garagem_pw"
#define meter_name    "emon/PowerMeter2"
#define mqqt_client_name  "ESPPwMeter2"
#define BUTTON_PIN 0      //button for flash format
PZEM004Tv30 pzem(D6, D5); // RX/TX pins
int mqqt_con_retries = 10; // number of retries for connecting to MQTT server
int mqqt_con_retries_delay = 5000; // seconds between retries

EasyButton button(BUTTON_PIN);

//flag for saving data
bool shouldSaveConfig = false;
int mqqt_con_retries_count = 0; // Number of tries counter
long lastMsg = 0;
long reading_delay = 5000;

bool mqtt_connected = false;


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

// Factory reset via GPIO FLASH Button
void onPressed() {
    Serial.println("Button has been pressed!");
    delay(1000);
    SPIFFS.remove("/config.json");
    ESP.eraseConfig();
    ESP.reset();
}

void setup() {
  
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
   //clean FS for testing 
   //  SPIFFS.format();
  button.begin();
  button.onPressed(onPressed);
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonDocument doc(1024);
        auto deserializeError = deserializeJson(doc, buf.get());
        serializeJson(doc, Serial);
        if ( ! deserializeError ) {
        Serial.println("\nparsed json");
        strlcpy(mqtt_server, doc["mqtt_server"] | "example.com", sizeof(mqtt_server));
        strlcpy(mqtt_port, doc["mqtt_port"] | "1883", sizeof(mqtt_port));
        strlcpy(mqtt_user, doc["mqtt_user"] | "emon_garagem", sizeof(mqtt_user));
        strlcpy(mqtt_pass, doc["mqtt_pass"] | "emon_garagem_pw", sizeof(mqtt_pass));

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
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
    Serial.println("Gravei o ficheiro");
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
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqqt_client_name, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      mqtt_connected = true;
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      if(mqqt_con_retries_count==mqqt_con_retries){
        Serial.println("Tried reconnect multiple times, will reset");
        ESP.reset();
      }

  }
}
 


void process() {
Serial.println("=======================\nINICIEI o processamento\n=======================");
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

    Serial.println("=======================\nTERMINEI o processamento\n=======================");
}
 
void loop() {
  button.read(); //check if FLASH button was pressed for clearing config file - while connected
  long now_read = millis();

  if (now_read - lastMsg > reading_delay) { //Attempt to connect and read
    lastMsg = now_read;
    if (!client.connected()) {
    for (mqqt_con_retries_count; mqqt_con_retries_count <= mqqt_con_retries; ++mqqt_con_retries_count){
        reconnect();
        button.read(); //if stuck on the for loop, check button
      }
    }

  client.loop();
  process();
  }

}
