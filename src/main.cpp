#include <FS.h> 
#include <PZEM004Tv30.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <EasyButton.h>           //https://github.com/evert-arias/EasyButton
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <RemoteDebug.h>

char mqtt_server[40] =       "192.168.69.2";
char mqtt_port[6] =          "1883";
char mqtt_user[20] =         "emon_garagem";
char mqtt_pass[32] =         "emon_garagem_pw";
char meter_name[32] =        "emon/ChangeME";
char mqtt_client_name[32] =  "DefESPPwMeter";

RemoteDebug Debug;

#define HOST_NAME "remotedebug.local"

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
  Debug.println("Should save config");
  shouldSaveConfig = true;
}
 
void callback(char* topic, byte* payload, unsigned int length) {
 
  Debug.print("Message arrived in topic: ");
  Debug.println(topic);
 
  Debug.print("Message:");
  for (int i = 0; i < length; i++) {
    Debug.print((char)payload[i]);
  }
 
  Debug.println();
  Debug.println("-----------------------");
 
}


// Factory reset via GPIO FLASH Button
void onPressed() {
    Debug.println("Button has been pressed!");
    delay(1000);
    SPIFFS.remove("/config.json");
    ESP.eraseConfig();
    ESP.reset();
}

void setup() {
  
  Serial.begin(115200);

  String hostNameWifi = HOST_NAME;

  Debug.begin(HOST_NAME); // Initialize the WiFi server
  Debug.setSerialEnabled(true);
  Debug.setResetCmdEnabled(true); // Enable the reset command
	Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
	Debug.showColors(true); // Colors


  pinMode(0, INPUT_PULLUP);
   //clean FS for testing 
   //  SPIFFS.format();

  ArduinoOTA.setHostname("esp8266-testing");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
        Debug.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Debug.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Debug.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Debug.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Debug.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Debug.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Debug.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Debug.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Debug.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  button.begin();
  button.onPressedFor(4000, onPressed);
  Debug.println("mounting FS...");

  if (SPIFFS.begin()) {
    Debug.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Debug.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Debug.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonDocument doc(2048);
        auto deserializeError = deserializeJson(doc, buf.get());
        Serial.println("Imprimir JSON:");
        serializeJson(doc, Serial);
        Serial.println("FIM DE IMPRESSAO");

        if ( ! deserializeError ) {
        Debug.println("\nparsed json");
        strlcpy(mqtt_server, doc["mqtt_server"], sizeof(mqtt_server));
        strlcpy(mqtt_port, doc["mqtt_port"], sizeof(mqtt_port));
        strlcpy(mqtt_user, doc["mqtt_user"], sizeof(mqtt_user));
        strlcpy(mqtt_pass, doc["mqtt_pass"], sizeof(mqtt_pass));
        strlcpy(meter_name, doc["meter_name"], sizeof(meter_name));
        strlcpy(mqtt_client_name, doc["mqtt_client_name"], sizeof(mqtt_client_name));

        } else {
          Debug.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Debug.println("failed to mount FS");
  }

    // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("password", "mqtt pass", mqtt_pass, 32);
  WiFiManagerParameter custom_meter_name("metername", "meter name", meter_name, 32);
  WiFiManagerParameter custom_client_name("clientname", "client name", mqtt_client_name, 32);

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
    wifiManager.addParameter(&custom_client_name);
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
      Debug.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  //if you get here you have connected to the WiFi
  Debug.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(meter_name, custom_meter_name.getValue());
  strcpy(mqtt_client_name, custom_client_name.getValue());
  
 // strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Debug.println("saving config");
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject json = jsonBuffer.to<JsonObject>();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["meter_name"] = meter_name;
    json["mqtt_client_name"] = mqtt_client_name;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Debug.println("failed to open config file for writing");
    }

    serializeJson(json, configFile);
    Debug.println("Gravei o ficheiro");
    configFile.close();
    //end save
  } 

  Debug.println("local ip");
  Debug.println(WiFi.localIP());

  //const uint16_t mqtt_port_x = 12025; //TODO - confirmar que isto abaixo funciona.
  client.setServer(mqtt_server, atoi(mqtt_port));

  client.setCallback(callback);
 }
 

void reconnect() {
    Debug.println("Connecting to MQTT...");
    if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass)) {
      Debug.println("connected");
      mqtt_connected = true;
    } else {
      Debug.print("failed with state ");
      Debug.println(client.state());
      delay(2000);
      if(mqqt_con_retries_count==mqqt_con_retries){
        Debug.println("Tried reconnect multiple times, will reset");
        ESP.reset();
      }

  }
}
 


void process() {
Debug.print("Reading started.");
    float voltage = pzem.voltage();
    if( !isnan(voltage) ){
        Debug.print(voltage); Debug.print("V ");
        char  topic_volt [31];
        sprintf(topic_volt,"%s%s",meter_name,"/Voltage");
        client.publish(topic_volt, String(voltage).c_str(), true);
    } else {
        Debug.println("Error reading voltage");
    }

    float current = pzem.current();
    if( !isnan(current) ){
        Debug.print(current); Debug.print("A ");
        char  topic_current [31];
        sprintf(topic_current,"%s%s",meter_name,"/Current");
        client.publish(topic_current, String(current).c_str(), true);
    } else {
        Debug.println("Error reading current");
    }

    float power = pzem.power();
    if( !isnan(power) ){
        Debug.print(power); Debug.print("W ");
        char  topic_power [31];
        sprintf(topic_power,"%s%s",meter_name,"/Power");
        client.publish(topic_power,  String(power).c_str(), true);
    } else {
        Debug.println("Error reading power");
    }

    float energy = pzem.energy();
    if( !isnan(energy) ){
        Debug.print(energy,3); Debug.print("kWh ");
        char  topic_energy [31];
        sprintf(topic_energy,"%s%s",meter_name,"/Energy");
        client.publish(topic_energy,  String(energy).c_str(), true);
    } else {
        Debug.println("Error reading energy");
    }

    float frequency = pzem.frequency();
    if( !isnan(frequency) ){
        Debug.print(frequency, 1); Debug.print("Hz ");
        char  topic_freq [31];
        sprintf(topic_freq,"%s%s",meter_name,"/Frequency");
        client.publish(topic_freq,  String(frequency).c_str(), true);
    } else {
        Debug.println("Error reading frequency");
    }

    float pf = pzem.pf();
    if( !isnan(pf) ){
        Debug.print(" PF: "); Debug.print(pf);
        char  topic_pf [31];
        sprintf(topic_pf,"%s%s",meter_name,"/PowerFactor");
        client.publish(topic_pf,  String(pf).c_str(), true);
    } else {
        Debug.println("Error reading power factor");
    }

    Debug.println("Reading Finished");
}
 
void loop() {
  ArduinoOTA.handle();
  Debug.handle();

  button.read(); //check if FLASH button was pressed for clearing config file - while connected
  long now_read = millis();
  if (now_read - lastMsg > reading_delay) { //Attempt to connect and read
    lastMsg = now_read;
    if (!client.connected()) {
    Debug.println("MQTT Disconnected - entering IF");
    for (mqqt_con_retries_count = 0; mqqt_con_retries_count <= mqqt_con_retries; ++mqqt_con_retries_count){
      Debug.print("Retry number "); Debug.println(mqqt_con_retries_count);
        reconnect();
        button.read(); //if stuck on the for loop, check button
      }
    }

  client.loop();
  process();
  }

}
